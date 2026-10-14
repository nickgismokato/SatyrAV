#pragma once
#include "core/Backstage.hpp"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace SatyrAV{

// (2.0.2) Sends the backstage lineup without ever holding up the show.
//
// Post() only hands the body to a worker thread and returns. If a newer
// lineup arrives before an older one has gone out, the older one is
// dropped: the backstage screen only ever needs the latest. Each request
// times out after a few seconds, and nothing is retried — the next scene
// change sends a fresh lineup anyway.
//
// Every request and its outcome goes to the SatyrAV log; the last outcome
// is also kept for the Debug popup.
class BackstageSender{
public:
	BackstageSender() = default;
	~BackstageSender();
	BackstageSender(const BackstageSender&) = delete;
	BackstageSender& operator=(const BackstageSender&) = delete;

	// Empty = off. An invalid Url is logged and also leaves it off.
	// Returns whether posting is on afterwards.
	bool Configure(const std::string& url);
	bool Enabled() const;
	// The Url in canonical form, for logs and the Debug popup.
	std::string UrlText() const;

	// Queue `body` for sending. `label` names it in the log ("Scene 1").
	void Post(const std::string& body, const std::string& label);

	enum class State{ Off, Waiting, Sending, Sent, Failed };
	struct Status{
		State state = State::Off;
		std::string label;    // what was (or is being) sent
		std::string detail;   // "HTTP 200, 85 ms" or the error
		int64_t finishedMs = 0; // steady-clock ms when it finished; 0 = never
	};
	Status GetStatus() const;
	static int64_t NowMs();

	// Wait until nothing is queued or in flight. For tests and shutdown.
	bool WaitIdle(int timeoutMs);
	// Stop the worker. A request in flight finishes (bounded by the timeout).
	void Shutdown();

	static constexpr int kTimeoutMs = 4000;

private:
	void Run();

	mutable std::mutex mtx;
	std::condition_variable cv;
	std::thread worker;
	bool stop = false;
	bool enabled = false;
	Backstage::Url url;

	bool hasPending = false;
	bool busy = false;
	std::string pendingBody, pendingLabel;
	Status status;
};

} // namespace SatyrAV
