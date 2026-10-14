#pragma once
#include <string>

namespace SatyrAV{

// (1.6.3) Application-wide logger. Writes every message to BOTH the
// terminal (stderr — preserves the existing dev-time visibility) and a
// per-day rotating file in `Platform::GetDefaultLogsDir()`. Filenames
// follow `satyrav-YYYY-MM-DD.log`; rotation happens on the first
// message of each calendar day.
//
// The logger is a process-wide singleton initialised once from
// `App::Init`. Calls before `Init` simply skip the file write but still
// hit stderr, so static-init paths can log safely.
//
// Levels follow the usual progression. The `level` filter applies only
// to *file* writes — stderr always shows everything for development.
enum class LogLevel{
	Debug,
	Info,
	Warn,
	Error
};

class Logger{
public:
	static Logger& Instance();

	// Open the log file for today's date. Safe to call multiple times;
	// re-opens if the day has rolled over. Failures are non-fatal — file
	// writes will be skipped but stderr output continues.
	void Init(const std::string& logsDir);
	void Shutdown();

	void Log(LogLevel level, const std::string& msg);

	// Convenience wrappers — printf-style formatting.
	void Debugf(const char* fmt, ...);
	void Infof(const char* fmt, ...);
	void Warnf(const char* fmt, ...);
	void Errorf(const char* fmt, ...);

	// Drop file writes below this level. Stderr is unaffected. Default Info.
	void SetFileLevel(LogLevel level);

private:
	Logger() = default;
	~Logger();

	// Re-evaluates `currentDay` and reopens the file when the day has
	// changed. No-op if still the same day.
	void RotateIfNeeded();

	void* file = nullptr; // FILE* — kept type-erased to keep the header light
	std::string dir;
	int currentDay = -1;  // tm_mday of the open file (1-31), -1 = never opened
	LogLevel fileLevel = LogLevel::Info;
};

} // namespace SatyrAV
