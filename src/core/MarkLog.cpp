#include "core/MarkLog.hpp"
#include "core/Platform.hpp"
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace SatyrAV{
namespace MarkLog{

namespace fs = std::filesystem;

static std::string TrimCopy(const std::string& s){
	size_t a = 0, b = s.size();
	while(a < b && std::isspace((unsigned char)s[a])) a++;
	while(b > a && std::isspace((unsigned char)s[b - 1])) b--;
	return s.substr(a, b - a);
}

std::string FolderFor(const std::string& projectPath){
	std::string path = projectPath;
	if(!path.empty() && path.back() != '/' && path.back() != '\\') path += '/';
	return path + "fix-it";
}

bool EnsureFolder(const std::string& projectPath){
	std::error_code ec;
	const fs::path dir = Platform::Utf8ToPath(FolderFor(projectPath));
	fs::create_directories(dir, ec);
	return fs::is_directory(dir, ec);
}

std::string FileNameFor(const std::tm& date){
	char buf[48];
	std::snprintf(buf, sizeof(buf), "ErrorInFiles-%04d-%02d-%02d.LOG",
		date.tm_year + 1900, date.tm_mon + 1, date.tm_mday);
	return buf;
}

MarkedLine MarkFor(const Scene& scene, const Command& cmd){
	MarkedLine m;
	m.fileName = scene.fileName;
	m.lineNumber = cmd.lineNumber;
	if(cmd.lineNumber >= 1 && cmd.lineNumber <= (int)scene.sourceLines.size()){
		m.source = TrimCopy(scene.sourceLines[cmd.lineNumber - 1]);
	}
	return m;
}

std::string FormatEntry(const std::tm& when, const MarkedLine& mark){
	char ts[16];
	std::snprintf(ts, sizeof(ts), "%02d:%02d:%02d",
		when.tm_hour, when.tm_min, when.tm_sec);
	std::string out = ts;
	out += "  ";
	out += mark.fileName.empty() ? std::string("(unknown file)") : mark.fileName;
	out += "  line ";
	out += mark.lineNumber > 0 ? std::to_string(mark.lineNumber) : std::string("?");
	out += ":  ";
	out += mark.source;
	return out;
}

bool Append(const std::string& projectPath, const MarkedLine& mark,
	const std::tm& when, std::string* outPath){
	if(!EnsureFolder(projectPath)) return false;
	const std::string path = FolderFor(projectPath) + "/" + FileNameFor(when);
	// Text mode, so the file has the platform's line endings and opens
	// cleanly in Notepad. Append mode creates the file on the first mark.
	std::ofstream file(Platform::Utf8ToPath(path), std::ios::app);
	if(!file.is_open()) return false;
	file << FormatEntry(when, mark) << '\n';
	file.flush();
	if(!file.good()) return false;
	if(outPath) *outPath = path;
	return true;
}

bool Append(const std::string& projectPath, const MarkedLine& mark,
	std::string* outPath){
	std::time_t now = std::time(nullptr);
	std::tm local{};
#ifdef _WIN32
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	return Append(projectPath, mark, local, outPath);
}

} // namespace MarkLog
} // namespace SatyrAV
