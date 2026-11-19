#include "core/ShowLog.hpp"
#include "core/Platform.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace SatyrAV{

namespace fs = std::filesystem;

namespace ShowLog{

std::string FolderFor(const std::string& projectPath){
	std::string path = projectPath;
	if(!path.empty() && path.back() != '/' && path.back() != '\\') path += '/';
	return path + "showlog";
}

std::string SafeName(const std::string& revyName){
	std::string out;
	for(char c : revyName){
		if(c == ' ' || c == '\t') continue;
		switch(c){
			case '<': case '>': case ':': case '"': case '/':
			case '\\': case '|': case '?': case '*':
				out += '_';
				break;
			default:
				// Control characters are refused too; UTF-8 bytes (æøå)
				// are kept as they are.
				if((unsigned char)c < 0x20) out += '_';
				else out += c;
		}
	}
	return out.empty() ? std::string("Show") : out;
}

std::string BaseNameFor(const std::string& revyName, const std::tm& date, int run){
	char buf[32];
	std::snprintf(buf, sizeof(buf), "-%04d-%02d-%02d-%d",
		date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, run);
	return SafeName(revyName) + buf;
}

int NextRunNumber(const std::string& folder, const std::string& revyName,
	const std::tm& date){
	std::error_code ec;
	for(int run = 1; run < 10000; run++){
		const std::string base = folder + "/" + BaseNameFor(revyName, date, run);
		if(!fs::exists(Platform::Utf8ToPath(base + ".log"), ec) &&
		   !fs::exists(Platform::Utf8ToPath(base + ".srt"), ec)){
			return run;
		}
	}
	return 10000;
}

std::string FormatLogLine(const ShowTime& t, const std::string& akt,
	const std::string& scene, const std::string& event){
	char ts[48];
	std::snprintf(ts, sizeof(ts), "%02d/%02d-%04d | %02d:%02d:%02d.%03d",
		t.local.tm_mday, t.local.tm_mon + 1, t.local.tm_year + 1900,
		t.local.tm_hour, t.local.tm_min, t.local.tm_sec, t.millis);
	std::string out = ts;
	if(!akt.empty())   out += " | " + akt;
	if(!scene.empty()) out += " | " + scene;
	out += " | " + event;
	return out;
}

std::string FormatSrtTime(double seconds){
	if(!(seconds > 0)) seconds = 0;
	long long ms = (long long)std::llround(seconds * 1000.0);
	const int h  = (int)(ms / 3600000); ms %= 3600000;
	const int m  = (int)(ms / 60000);   ms %= 60000;
	const int s  = (int)(ms / 1000);    ms %= 1000;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d,%03d", h, m, s, (int)ms);
	return buf;
}

std::string FormatDuration(double seconds){
	if(!(seconds > 0)) seconds = 0;
	long long total = (long long)seconds; // whole seconds, rounded down
	const int h = (int)(total / 3600);
	const int m = (int)((total / 60) % 60);
	const int s = (int)(total % 60);
	char buf[32];
	if(h > 0) std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
	else      std::snprintf(buf, sizeof(buf), "%d:%02d", m, s);
	return buf;
}

void Now(std::tm& local, int& millis){
	const auto now = std::chrono::system_clock::now();
	const std::time_t tt = std::chrono::system_clock::to_time_t(now);
	millis = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()).count() % 1000);
	if(millis < 0) millis += 1000;
	local = std::tm{};
#ifdef _WIN32
	localtime_s(&local, &tt);
#else
	localtime_r(&tt, &local);
#endif
}

} // namespace ShowLog

ShowLogWriter::~ShowLogWriter(){
	if(open) Close(pendingStart + kMaxSubtitleSeconds);
}

bool ShowLogWriter::IsOpen() const{ return open; }

bool ShowLogWriter::Open(const std::string& projectPath, const std::string& revyName,
	const std::tm& date, std::string* outBasePath){
	if(open) Close(pendingStart + kMaxSubtitleSeconds);

	const std::string folder = ShowLog::FolderFor(projectPath);
	std::error_code ec;
	fs::create_directories(Platform::Utf8ToPath(folder), ec);
	if(!fs::is_directory(Platform::Utf8ToPath(folder), ec)) return false;

	const int run = ShowLog::NextRunNumber(folder, revyName, date);
	const std::string base = folder + "/" + ShowLog::BaseNameFor(revyName, date, run);

	// Text mode, so both files get the platform's line endings and open
	// cleanly in Notepad and in editors that import .srt.
	logFile.open(Platform::Utf8ToPath(base + ".log"), std::ios::out | std::ios::trunc);
	srtFile.open(Platform::Utf8ToPath(base + ".srt"), std::ios::out | std::ios::trunc);
	if(!logFile.is_open() || !srtFile.is_open()){
		logFile.close();
		srtFile.close();
		return false;
	}

	open = true;
	srtIndex = 0;
	havePending = false;
	pendingStart = 0;
	pendingText.clear();
	if(outBasePath) *outBasePath = base;
	return true;
}

void ShowLogWriter::Write(const ShowTime& t, const std::string& akt,
	const std::string& scene, const std::string& event){
	if(!open) return;

	logFile << ShowLog::FormatLogLine(t, akt, scene, event) << '\n';
	logFile.flush();

	FlushPendingSubtitle(t.showSeconds);
	std::string where = akt;
	if(!scene.empty()) where += (where.empty() ? "" : " | ") + scene;
	pendingText = where.empty() ? event : where + "\n" + event;
	pendingStart = t.showSeconds;
	havePending = true;
}

void ShowLogWriter::FlushPendingSubtitle(double endSeconds){
	if(!havePending) return;
	double end = endSeconds;
	if(end > pendingStart + kMaxSubtitleSeconds) end = pendingStart + kMaxSubtitleSeconds;
	if(end < pendingStart) end = pendingStart;
	srtFile << ++srtIndex << '\n'
	        << ShowLog::FormatSrtTime(pendingStart) << " --> "
	        << ShowLog::FormatSrtTime(end) << '\n'
	        << pendingText << "\n\n";
	srtFile.flush();
	havePending = false;
}

void ShowLogWriter::Close(double endSeconds){
	if(!open) return;
	FlushPendingSubtitle(endSeconds);
	logFile.close();
	srtFile.close();
	open = false;
}

} // namespace SatyrAV
