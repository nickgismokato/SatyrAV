#include "core/Logger.hpp"
#include "core/Platform.hpp"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace SatyrAV{

namespace{
	// One mutex protects file rotation + the actual write. The logger is
	// touched from the main thread for the most part, but MediaPlayer
	// runs decode on a background thread and can fail there too — better
	// to be safe than to interleave bytes inside a log line.
	std::mutex g_mutex;

	const char* LevelTag(LogLevel l){
		switch(l){
			case LogLevel::Debug: return "DEBUG";
			case LogLevel::Info:  return "INFO ";
			case LogLevel::Warn:  return "WARN ";
			case LogLevel::Error: return "ERROR";
		}
		return "?    ";
	}

	bool LevelEnabled(LogLevel msgLevel, LogLevel fileLevel){
		return (int)msgLevel >= (int)fileLevel;
	}
}

Logger& Logger::Instance(){
	static Logger inst;
	return inst;
}

Logger::~Logger(){ Shutdown(); }

void Logger::Init(const std::string& logsDir){
	std::lock_guard<std::mutex> lock(g_mutex);
	dir = logsDir;
	Platform::EnsureDirectoryExists(dir);
	currentDay = -1; // force open on first message
}

void Logger::Shutdown(){
	std::lock_guard<std::mutex> lock(g_mutex);
	if(file){
		fclose((FILE*)file);
		file = nullptr;
	}
}

void Logger::SetFileLevel(LogLevel level){
	std::lock_guard<std::mutex> lock(g_mutex);
	fileLevel = level;
}

void Logger::RotateIfNeeded(){
	if(dir.empty()) return; // Init() not called yet — silently skip file IO

	std::time_t now = std::time(nullptr);
	std::tm local{};
#ifdef _WIN32
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	if(local.tm_mday == currentDay && file) return;

	if(file){
		fclose((FILE*)file);
		file = nullptr;
	}

	char nameBuf[64];
	std::snprintf(nameBuf, sizeof(nameBuf),
		"satyrav-%04d-%02d-%02d.log",
		local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);

#ifdef _WIN32
	const char sep = '\\';
#else
	const char sep = '/';
#endif
	std::string path = dir;
	if(!path.empty() && path.back() != '/' && path.back() != '\\') path += sep;
	path += nameBuf;

	// Append mode so multiple runs on the same day land in the same file.
	// Failure is non-fatal; we'll keep retrying once per message.
	file = std::fopen(path.c_str(), "a");
	currentDay = local.tm_mday;
}

void Logger::Log(LogLevel level, const std::string& msg){
	std::lock_guard<std::mutex> lock(g_mutex);

	std::time_t now = std::time(nullptr);
	std::tm local{};
#ifdef _WIN32
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	char ts[32];
	std::snprintf(ts, sizeof(ts), "%02d:%02d:%02d",
		local.tm_hour, local.tm_min, local.tm_sec);

	const char* tag = LevelTag(level);

	// Always write to stderr — preserves dev-time visibility regardless
	// of file-level filter. Newline-terminated and flushed via the impl.
	std::fprintf(stderr, "[%s %s] %s\n", ts, tag, msg.c_str());

	if(LevelEnabled(level, fileLevel)){
		RotateIfNeeded();
		if(file){
			std::fprintf((FILE*)file, "[%s %s] %s\n", ts, tag, msg.c_str());
			std::fflush((FILE*)file);
		}
	}
}

namespace{
	std::string Vformat(const char* fmt, std::va_list ap){
		std::va_list copy;
		va_copy(copy, ap);
		int n = std::vsnprintf(nullptr, 0, fmt, copy);
		va_end(copy);
		if(n <= 0) return std::string();
		std::string out;
		out.resize((size_t)n);
		std::vsnprintf(out.data(), (size_t)n + 1, fmt, ap);
		return out;
	}
}

void Logger::Debugf(const char* fmt, ...){
	std::va_list ap; va_start(ap, fmt);
	std::string s = Vformat(fmt, ap); va_end(ap);
	Log(LogLevel::Debug, s);
}
void Logger::Infof(const char* fmt, ...){
	std::va_list ap; va_start(ap, fmt);
	std::string s = Vformat(fmt, ap); va_end(ap);
	Log(LogLevel::Info, s);
}
void Logger::Warnf(const char* fmt, ...){
	std::va_list ap; va_start(ap, fmt);
	std::string s = Vformat(fmt, ap); va_end(ap);
	Log(LogLevel::Warn, s);
}
void Logger::Errorf(const char* fmt, ...){
	std::va_list ap; va_start(ap, fmt);
	std::string s = Vformat(fmt, ap); va_end(ap);
	Log(LogLevel::Error, s);
}

} // namespace SatyrAV
