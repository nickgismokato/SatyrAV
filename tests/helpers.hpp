#pragma once
// (1.8.1) Shared test scaffolding: a self-cleaning scratch directory and
// a helper that runs a .ngk source string through the real parser.

#include "core/SceneParser.hpp"
#include <doctest/doctest.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace SatyrTest{

namespace fs = std::filesystem;

// RAII scratch directory under the platform temp dir. Each instance gets
// a unique name so leftovers from a crashed run can never be picked up by
// a later one, and it removes itself on destruction.
class TempDir{
public:
	explicit TempDir(const std::string& tag = "t"){
		static std::atomic<unsigned long> counter{0};
		const auto stamp = std::chrono::steady_clock::now()
			.time_since_epoch().count();
		dir = fs::temp_directory_path()
			/ ("satyrav-" + tag + "-" + std::to_string(stamp)
			   + "-" + std::to_string(counter++));
		fs::create_directories(dir);
	}
	~TempDir(){
		std::error_code ec;
		fs::remove_all(dir, ec); // best effort — never throw from a dtor
	}

	TempDir(const TempDir&)            = delete;
	TempDir& operator=(const TempDir&) = delete;

	const fs::path& Path() const{ return dir; }
	std::string Str() const{ return dir.string(); }
	std::string File(const std::string& name) const{
		return (dir / name).string();
	}

	// Writes in binary mode on purpose: the tests control line endings
	// exactly, instead of inheriting the host's text-mode translation.
	std::string Write(const std::string& name, const std::string& content) const{
		auto p = dir / name;
		fs::create_directories(p.parent_path());
		std::ofstream out(p, std::ios::binary);
		out << content;
		out.close();
		return p.string();
	}

private:
	fs::path dir;
};

// Run a .ngk source string through the parser. SceneParser only exposes
// ParseFile, so the text goes to a real file first — which is also what
// the application does, so no part of the code path is faked or bypassed.
inline SatyrAV::Scene ParseScene(const std::string& text){
	TempDir tmp("scene");
	return SatyrAV::SceneParser::ParseFile(tmp.Write("Scene.ngk", text));
}

// Convenience for the very common "one command, no sections" case.
inline SatyrAV::Command ParseOneCommand(const std::string& commandLine){
	auto scene = ParseScene("[Commands]\n" + commandLine + "\n");
	REQUIRE(scene.commands.size() == 1);
	return scene.commands[0];
}

} // namespace SatyrTest
