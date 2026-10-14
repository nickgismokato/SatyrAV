#include "core/BackstageSender.hpp"
#include "core/HttpPost.hpp"
#include "core/Logger.hpp"
#include <chrono>
#include <vector>

namespace SatyrAV{

int64_t BackstageSender::NowMs(){
	return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

BackstageSender::~BackstageSender(){
	Shutdown();
}

bool BackstageSender::Configure(const std::string& text){
	std::lock_guard<std::mutex> lock(mtx);
	hasPending = false;
	status = Status{};
	enabled = false;
	if(text.empty()){
		return false;
	}
	Backstage::Url u;
	if(!Backstage::ParseUrl(text, u)){
		Logger::Instance().Warnf("Backstage: '%s' in schema.toml is not a usable Url "
			"— nothing will be sent to the backstage screen", text.c_str());
		return false;
	}
	url = u;
	enabled = true;
	status.state = State::Waiting;
	Logger::Instance().Infof("Backstage: scene changes will be sent to %s",
		Backstage::FormatUrl(url).c_str());
	return true;
}

bool BackstageSender::Enabled() const{
	std::lock_guard<std::mutex> lock(mtx);
	return enabled;
}

std::string BackstageSender::UrlText() const{
	std::lock_guard<std::mutex> lock(mtx);
	return enabled ? Backstage::FormatUrl(url) : std::string();
}

void BackstageSender::Post(const std::string& body, const std::string& label){
	std::lock_guard<std::mutex> lock(mtx);
	if(!enabled || stop) return;
	if(hasPending){
		Logger::Instance().Infof("Backstage: '%s' was replaced by '%s' before it was sent",
			pendingLabel.c_str(), label.c_str());
	}
	pendingBody = body;
	pendingLabel = label;
	hasPending = true;
	if(!worker.joinable()) worker = std::thread(&BackstageSender::Run, this);
	cv.notify_all();
}

BackstageSender::Status BackstageSender::GetStatus() const{
	std::lock_guard<std::mutex> lock(mtx);
	return status;
}

bool BackstageSender::WaitIdle(int timeoutMs){
	std::unique_lock<std::mutex> lock(mtx);
	return cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
		[&]{ return !hasPending && !busy; });
}

void BackstageSender::Shutdown(){
	{
		std::lock_guard<std::mutex> lock(mtx);
		stop = true;
		hasPending = false;
	}
	cv.notify_all();
	if(worker.joinable()) worker.join();
	std::lock_guard<std::mutex> lock(mtx);
	stop = false; // usable again after a later Configure + Post
}

// The body's lines, one per scene — logged one per log line, so the log
// shows exactly the lines the ticker receives.
static std::vector<std::string> Lines(const std::string& body){
	std::vector<std::string> out;
	size_t start = 0;
	while(start <= body.size()){
		size_t nl = body.find('\n', start);
		out.push_back(body.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
		if(nl == std::string::npos) break;
		start = nl + 1;
	}
	return out;
}

void BackstageSender::Run(){
	std::unique_lock<std::mutex> lock(mtx);
	while(true){
		cv.wait(lock, [&]{ return stop || hasPending; });
		if(stop) break;

		const std::string body = pendingBody;
		const std::string label = pendingLabel;
		const Backstage::Url target = url;
		hasPending = false;
		busy = true;
		status.state = State::Sending;
		status.label = label;
		status.detail.clear();
		lock.unlock();

		const std::string where = Backstage::FormatUrl(target);
		const auto lines = Lines(body);
		Logger::Instance().Infof("Backstage: sending '%s' to %s, %d line%s:",
			label.c_str(), where.c_str(), (int)lines.size(), lines.size() == 1 ? "" : "s");
		for(size_t i = 0; i < lines.size(); i++){
			Logger::Instance().Infof("Backstage:   %d  %s", (int)i + 1, lines[i].c_str());
		}
		HttpResult r = HttpPost(target, body,
			{{"sl-cmd", "waiting"}, {"Content-Type", "text/plain; charset=utf-8"}},
			kTimeoutMs);

		std::string detail;
		if(r.sent){
			detail = "HTTP " + std::to_string(r.status) + ", "
				+ std::to_string(r.elapsedMs) + " ms";
		} else{
			detail = r.error;
		}
		if(r.Ok()){
			Logger::Instance().Infof("Backstage: '%s' sent (%s)", label.c_str(), detail.c_str());
		} else if(r.sent){
			Logger::Instance().Warnf("Backstage: %s answered '%s' with %s",
				where.c_str(), label.c_str(), detail.c_str());
		} else{
			Logger::Instance().Warnf("Backstage: could not send '%s' to %s: %s",
				label.c_str(), where.c_str(), detail.c_str());
		}

		lock.lock();
		busy = false;
		status.state = r.Ok() ? State::Sent : State::Failed;
		status.detail = detail;
		status.finishedMs = NowMs();
		cv.notify_all();
	}
}

} // namespace SatyrAV
