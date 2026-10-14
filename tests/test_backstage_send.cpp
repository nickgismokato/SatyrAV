// (2.0.2) Sending the backstage lineup, against a real socket.
//
// A tiny HTTP server runs on 127.0.0.1 inside the test, so these check the
// exact bytes the ticker receives (method, path, the `sl-cmd: waiting`
// header, the body), plus the sender's promises: it never blocks the
// caller, only the newest lineup matters, and failures are reported rather
// than thrown. POSIX only — on Windows the request goes through WinHTTP,
// which is exercised by running the app against the real ticker.

#include <doctest/doctest.h>
#include "core/BackstageSender.hpp"
#include "core/HttpPost.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace SatyrAV;

namespace{

// Accepts connections until stopped; records each request; answers with
// `status` after `delayMs`.
class TestServer{
public:
	explicit TestServer(int status = 200, int delayMs = 0)
		: status(status), delayMs(delayMs){
		fd = socket(AF_INET, SOCK_STREAM, 0);
		int one = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
		sockaddr_in a{};
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		a.sin_port = 0;
		bind(fd, (sockaddr*)&a, sizeof(a));
		socklen_t len = sizeof(a);
		getsockname(fd, (sockaddr*)&a, &len);
		port = ntohs(a.sin_port);
		listen(fd, 8);
		thread = std::thread([this]{ Loop(); });
	}
	~TestServer(){
		running = false;
		shutdown(fd, SHUT_RDWR);
		close(fd);
		if(thread.joinable()) thread.join();
	}
	std::vector<std::string> Requests(){
		std::lock_guard<std::mutex> l(m);
		return requests;
	}
	int port = 0;

private:
	void Loop(){
		while(running){
			int c = accept(fd, nullptr, nullptr);
			if(c < 0) return;
			std::string req;
			char buf[1024];
			size_t need = std::string::npos;
			while(true){
				ssize_t n = recv(c, buf, sizeof(buf), 0);
				if(n <= 0) break;
				req.append(buf, (size_t)n);
				auto he = req.find("\r\n\r\n");
				if(he != std::string::npos && need == std::string::npos){
					auto cl = req.find("Content-Length: ");
					size_t len = cl == std::string::npos ? 0
						: (size_t)std::atoi(req.c_str() + cl + 16);
					need = he + 4 + len;
				}
				if(need != std::string::npos && req.size() >= need) break;
			}
			{
				std::lock_guard<std::mutex> l(m);
				requests.push_back(req);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
			std::string resp = "HTTP/1.1 " + std::to_string(status)
				+ " X\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok";
			send(c, resp.data(), resp.size(), MSG_NOSIGNAL);
			close(c);
		}
	}
	int fd = -1;
	int status, delayMs;
	std::atomic<bool> running{true};
	std::thread thread;
	std::mutex m;
	std::vector<std::string> requests;
};

std::string BodyOf(const std::string& req){
	auto he = req.find("\r\n\r\n");
	return he == std::string::npos ? std::string() : req.substr(he + 4);
}

std::string LocalUrl(int port){
	return "http://127.0.0.1:" + std::to_string(port) + "/set-bck";
}

} // namespace

TEST_CASE("HttpPost sends what the ticker expects"){
	TestServer server;
	Backstage::Url url;
	REQUIRE(Backstage::ParseUrl(LocalUrl(server.port), url));

	const std::string body = "Scene 1;2:40;actors;A;B\nScene 2;1:00";
	auto r = HttpPost(url, body, {{"sl-cmd", "waiting"}}, 2000);
	CHECK(r.sent);
	CHECK(r.status == 200);
	CHECK(r.Ok());

	auto reqs = server.Requests();
	REQUIRE(reqs.size() == 1);
	CHECK(reqs[0].rfind("POST /set-bck HTTP/1.1\r\n", 0) == 0);
	CHECK(reqs[0].find("\r\nsl-cmd: waiting\r\n") != std::string::npos);
	CHECK(reqs[0].find("\r\nHost: 127.0.0.1:" + std::to_string(server.port) + "\r\n") != std::string::npos);
	CHECK(BodyOf(reqs[0]) == body);
}

TEST_CASE("HttpPost reports failures instead of hanging"){
	Backstage::Url url;
	SUBCASE("nothing listening"){
		int closedPort;
		{ TestServer s; closedPort = s.port; }   // bound, then released
		REQUIRE(Backstage::ParseUrl(LocalUrl(closedPort), url));
		auto r = HttpPost(url, "x", {}, 1000);
		CHECK_FALSE(r.sent);
		CHECK_FALSE(r.error.empty());
		CHECK(r.elapsedMs < 1500);
	}
	SUBCASE("server error is a response, not a success"){
		TestServer server(500);
		REQUIRE(Backstage::ParseUrl(LocalUrl(server.port), url));
		auto r = HttpPost(url, "x", {}, 1000);
		CHECK(r.sent);
		CHECK(r.status == 500);
		CHECK_FALSE(r.Ok());
	}
	SUBCASE("https needs the Windows build"){
		REQUIRE(Backstage::ParseUrl("https://127.0.0.1/set-bck", url));
		auto r = HttpPost(url, "x", {}, 1000);
		CHECK_FALSE(r.sent);
		CHECK(r.error.find("Windows") != std::string::npos);
	}
}

TEST_CASE("BackstageSender: off without a Url, and an invalid Url stays off"){
	BackstageSender s;
	CHECK_FALSE(s.Configure(""));
	CHECK_FALSE(s.Enabled());
	CHECK(s.GetStatus().state == BackstageSender::State::Off);
	s.Post("ignored", "x");                       // no thread, no crash
	CHECK(s.WaitIdle(100));
	CHECK_FALSE(s.Configure("ftp://nope"));
	CHECK_FALSE(s.Enabled());
}

TEST_CASE("BackstageSender: Post returns at once and the request arrives"){
	TestServer server(200, 300);                   // a slow ticker
	BackstageSender s;
	REQUIRE(s.Configure(LocalUrl(server.port)));
	CHECK(s.UrlText() == LocalUrl(server.port));

	auto t0 = BackstageSender::NowMs();
	s.Post("A;1:00", "A");
	CHECK(BackstageSender::NowMs() - t0 < 100);   // the show is not held up
	REQUIRE(s.WaitIdle(3000));

	auto st = s.GetStatus();
	CHECK(st.state == BackstageSender::State::Sent);
	CHECK(st.label == "A");
	CHECK(st.detail.rfind("HTTP 200", 0) == 0);
	auto reqs = server.Requests();
	REQUIRE(reqs.size() == 1);
	CHECK(BodyOf(reqs[0]) == "A;1:00");
	CHECK(reqs[0].find("\r\nsl-cmd: waiting\r\n") != std::string::npos);
}

TEST_CASE("BackstageSender: three scenes arrive as three lines"){
	TestServer server;
	BackstageSender s;
	REQUIRE(s.Configure(LocalUrl(server.port)));

	std::vector<Akt> akts(1);
	for(const char* n : {"One", "Two", "Three"}){
		Scene sc;
		sc.name = n;
		sc.backstage.present = true;
		sc.backstage.name = n;
		sc.backstage.actors = {std::string(n) + " actor"};
		akts[0].scenes.push_back(sc);
	}
	s.Post(Backstage::FormatBody(Backstage::BuildLineup(akts, 0, 0)), "One");
	REQUIRE(s.WaitIdle(3000));

	auto reqs = server.Requests();
	REQUIRE(reqs.size() == 1);
	CHECK(BodyOf(reqs[0]) ==
		"One;;actors;One actor\n"
		"Two;;actors;Two actor\n"
		"Three;;actors;Three actor");
}

TEST_CASE("BackstageSender: only the newest lineup matters"){
	TestServer server(200, 300);
	BackstageSender s;
	REQUIRE(s.Configure(LocalUrl(server.port)));
	s.Post("first", "1");
	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // "1" in flight
	s.Post("second", "2");   // replaced before it goes out
	s.Post("third", "3");
	REQUIRE(s.WaitIdle(3000));

	auto reqs = server.Requests();
	REQUIRE(reqs.size() == 2);
	CHECK(BodyOf(reqs[0]) == "first");
	CHECK(BodyOf(reqs[1]) == "third");
	CHECK(s.GetStatus().label == "3");
}

TEST_CASE("BackstageSender: a dead ticker is reported, and shutdown is clean"){
	int closedPort;
	{ TestServer t; closedPort = t.port; }
	BackstageSender s;
	REQUIRE(s.Configure(LocalUrl(closedPort)));
	s.Post("x", "Scene 1");
	REQUIRE(s.WaitIdle(BackstageSender::kTimeoutMs + 1000));
	auto st = s.GetStatus();
	CHECK(st.state == BackstageSender::State::Failed);
	CHECK_FALSE(st.detail.empty());
	CHECK(st.finishedMs > 0);
	s.Shutdown();
	s.Shutdown();   // twice is fine
}

#endif // !_WIN32
