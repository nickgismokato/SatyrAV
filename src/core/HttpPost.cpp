#include "core/HttpPost.hpp"
#include <chrono>
#include <cstdlib>

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	#include <winhttp.h>
#else
	#include <cerrno>
	#include <cstring>
	#include <fcntl.h>
	#include <netdb.h>
	#include <poll.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <sys/types.h>
	#include <unistd.h>
#endif

namespace SatyrAV{

namespace{
	using Clock = std::chrono::steady_clock;
	int MsSince(Clock::time_point t0){
		return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
			Clock::now() - t0).count();
	}
}

#ifdef _WIN32

static std::wstring Widen(const std::string& s){
	if(s.empty()) return std::wstring();
	int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
	return w;
}

// The WinHTTP errors an operator can actually meet, in words; the number is
// kept so anything else can still be looked up.
static std::string WinHttpError(DWORD code){
	const char* what = nullptr;
	switch(code){
		case ERROR_WINHTTP_TIMEOUT:                 what = "timed out"; break;
		case ERROR_WINHTTP_NAME_NOT_RESOLVED:       what = "host name not found (no network, or a typo in the Url?)"; break;
		case ERROR_WINHTTP_CANNOT_CONNECT:          what = "could not connect (server down, or wrong port?)"; break;
		case ERROR_WINHTTP_CONNECTION_ERROR:        what = "connection dropped"; break;
		case ERROR_WINHTTP_SECURE_FAILURE:          what = "https certificate problem"; break;
		case ERROR_WINHTTP_INVALID_URL:             what = "invalid Url"; break;
		case ERROR_WINHTTP_UNRECOGNIZED_SCHEME:     what = "unrecognised Url scheme"; break;
		default: break;
	}
	std::string s = what ? what : "WinHTTP error";
	return s + " (" + std::to_string((unsigned long)code) + ")";
}

HttpResult HttpPost(const Backstage::Url& url, const std::string& body,
	const HttpHeaders& headers, int timeoutMs){
	HttpResult r;
	const auto t0 = Clock::now();

	HINTERNET session = WinHttpOpen(L"SatyrAV", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	HINTERNET connect = nullptr, request = nullptr;
	auto fail = [&](){
		r.error = WinHttpError(GetLastError());
	};

	if(!session){ fail(); }
	else{
		WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
		connect = WinHttpConnect(session, Widen(url.host).c_str(),
			(INTERNET_PORT)url.port, 0);
		if(!connect) fail();
	}
	if(connect){
		request = WinHttpOpenRequest(connect, L"POST", Widen(url.path).c_str(),
			nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			url.https ? WINHTTP_FLAG_SECURE : 0);
		if(!request) fail();
	}
	if(request){
		std::wstring head;
		for(const auto& h : headers) head += Widen(h.first + ": " + h.second + "\r\n");
		BOOL okSend = WinHttpSendRequest(request,
			head.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : head.c_str(),
			head.empty() ? 0 : (DWORD)-1L,
			(LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
		if(!okSend || !WinHttpReceiveResponse(request, nullptr)){
			fail();
		} else{
			DWORD status = 0, size = sizeof(status);
			if(WinHttpQueryHeaders(request,
				WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)){
				r.sent = true;
				r.status = (int)status;
			} else{
				fail();
			}
		}
	}

	if(request) WinHttpCloseHandle(request);
	if(connect) WinHttpCloseHandle(connect);
	if(session) WinHttpCloseHandle(session);
	r.elapsedMs = MsSince(t0);
	return r;
}

#else // POSIX — plain http only

HttpResult HttpPost(const Backstage::Url& url, const std::string& body,
	const HttpHeaders& headers, int timeoutMs){
	HttpResult r;
	const auto t0 = Clock::now();
	if(url.https){
		r.error = "https is only supported in the Windows build (use an http:// Url here)";
		return r;
	}

	addrinfo hints{};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	addrinfo* res = nullptr;
	const std::string port = std::to_string(url.port);
	if(getaddrinfo(url.host.c_str(), port.c_str(), &hints, &res) != 0 || !res){
		r.error = "host name not found";
		r.elapsedMs = MsSince(t0);
		return r;
	}

	int fd = -1;
	std::string lastError = "could not connect";
	for(addrinfo* ai = res; ai && fd < 0; ai = ai->ai_next){
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if(fd < 0) continue;
		// Non-blocking connect, so a dead host costs timeoutMs, not minutes.
		int flags = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
		int c = connect(fd, ai->ai_addr, ai->ai_addrlen);
		bool connected = (c == 0);
		if(c < 0 && errno == EINPROGRESS){
			pollfd p{fd, POLLOUT, 0};
			if(poll(&p, 1, timeoutMs) == 1){
				int err = 0;
				socklen_t len = sizeof(err);
				getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
				connected = (err == 0);
				if(err) lastError = std::string("could not connect (") + std::strerror(err) + ")";
			} else{
				lastError = "timed out connecting";
			}
		}
		if(!connected){ close(fd); fd = -1; continue; }
		fcntl(fd, F_SETFL, flags);
	}
	freeaddrinfo(res);
	if(fd < 0){
		r.error = lastError;
		r.elapsedMs = MsSince(t0);
		return r;
	}

	timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	std::string req = "POST " + url.path + " HTTP/1.1\r\n"
		"Host: " + url.host + (url.port != 80 ? ":" + port : std::string()) + "\r\n";
	for(const auto& h : headers) req += h.first + ": " + h.second + "\r\n";
	req += "Content-Length: " + std::to_string(body.size()) + "\r\n"
		"Connection: close\r\n\r\n" + body;

	size_t off = 0;
	while(off < req.size()){
		ssize_t n = send(fd, req.data() + off, req.size() - off, MSG_NOSIGNAL);
		if(n <= 0){ r.error = "connection dropped while sending"; break; }
		off += (size_t)n;
	}

	if(r.error.empty()){
		std::string resp;
		char buf[512];
		while(resp.find("\r\n") == std::string::npos && resp.size() < 4096){
			ssize_t n = recv(fd, buf, sizeof(buf), 0);
			if(n <= 0) break;
			resp.append(buf, (size_t)n);
		}
		// "HTTP/1.1 200 OK"
		auto sp = resp.find(' ');
		if(resp.rfind("HTTP/", 0) == 0 && sp != std::string::npos && sp + 4 <= resp.size()){
			r.sent = true;
			r.status = std::atoi(resp.c_str() + sp + 1);
		} else{
			r.error = resp.empty() ? "no response (timed out?)" : "not an HTTP response";
		}
	}
	close(fd);
	r.elapsedMs = MsSince(t0);
	return r;
}

#endif

} // namespace SatyrAV
