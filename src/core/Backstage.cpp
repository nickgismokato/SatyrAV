#include "core/Backstage.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace SatyrAV{
namespace Backstage{

static std::string Trim(const std::string& s){
	size_t a = 0, b = s.size();
	while(a < b && std::isspace((unsigned char)s[a])) a++;
	while(b > a && std::isspace((unsigned char)s[b - 1])) b--;
	return s.substr(a, b - a);
}

static std::string Lower(std::string s){
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c){ return (char)std::tolower(c); });
	return s;
}

static std::string Unquote(const std::string& raw){
	std::string s = Trim(raw);
	if(s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()){
		return s.substr(1, s.size() - 2);
	}
	return s;
}

// Characters the ticker cannot take as they are. `;` and line breaks split
// a field; `<`, `>` and `&` are shown as HTML.
static void NoteAwkwardText(const std::string& text, std::string& warning){
	if(!warning.empty()) return;
	if(text.find(';') != std::string::npos){
		warning = "'" + text + "' contains ';', which separates the backstage "
			"screen's fields; it will be left out";
	} else if(text.find_first_of("<>&") != std::string::npos){
		warning = "'" + text + "' contains <, > or &, which the backstage "
			"screen shows as HTML";
	}
}

bool ParseExpectedTime(const std::string& text, int& seconds){
	std::string s = Lower(Unquote(text));
	s.erase(std::remove_if(s.begin(), s.end(),
		[](unsigned char c){ return std::isspace(c); }), s.end());
	if(s.empty()) return false;

	// h:mm:ss / m:ss
	if(s.find(':') != std::string::npos){
		int parts[3] = {0, 0, 0};
		int n = 0;
		size_t start = 0;
		while(true){
			size_t c = s.find(':', start);
			std::string part = s.substr(start, c == std::string::npos ? std::string::npos : c - start);
			if(part.empty() || n == 3) return false;
			for(char ch : part) if(!std::isdigit((unsigned char)ch)) return false;
			parts[n++] = std::stoi(part);
			if(c == std::string::npos) break;
			start = c + 1;
		}
		if(n < 2) return false;
		if(parts[n - 1] >= 60) return false;
		if(n == 3 && parts[1] >= 60) return false;
		seconds = (n == 3) ? parts[0] * 3600 + parts[1] * 60 + parts[2]
		                   : parts[0] * 60 + parts[1];
		return true;
	}

	// 2m40s / 2m / 40s / 1h5m / 160
	int total = 0;
	size_t i = 0;
	bool any = false;
	while(i < s.size()){
		size_t j = i;
		while(j < s.size() && std::isdigit((unsigned char)s[j])) j++;
		if(j == i || j - i > 6) return false;
		int value = std::stoi(s.substr(i, j - i));
		char unit = j < s.size() ? s[j] : 's';
		if(j == s.size() && any) return false; // `2m40` is ambiguous
		if(unit == 'h') total += value * 3600;
		else if(unit == 'm') total += value * 60;
		else if(unit == 's') total += value;
		else return false;
		any = true;
		i = (j < s.size()) ? j + 1 : j;
	}
	seconds = total;
	return true;
}

std::string FormatTime(int seconds){
	if(seconds < 0) return "";
	char buf[24];
	std::snprintf(buf, sizeof(buf), "%d:%02d", seconds / 60, seconds % 60);
	return buf;
}

std::vector<std::string> ParseList(const std::string& text){
	std::string s = Trim(text);
	if(!s.empty() && s.front() == '[') s.erase(0, 1);
	if(!s.empty() && s.back() == ']') s.pop_back();

	std::vector<std::string> out;
	std::string current;
	char quote = 0;
	auto flush = [&](){
		std::string item = Trim(current);
		current.clear();
		if(!item.empty()) out.push_back(item);
	};
	for(char c : s){
		if(quote){
			if(c == quote) quote = 0;
			else current += c;
		} else if(c == '"' || c == '\''){
			quote = c;
		} else if(c == ','){
			flush();
		} else{
			current += c;
		}
	}
	flush();
	return out;
}

bool ApplyKey(BackstageInfo& info, const std::string& key,
	const std::string& value, std::string& warning){
	warning.clear();
	const std::string k = Lower(Trim(key));
	if(k == "name"){
		info.name = Unquote(value);
		NoteAwkwardText(info.name, warning);
		return true;
	}
	if(k == "expectedtime" || k == "time"){
		int sec = 0;
		if(!ParseExpectedTime(value, sec)){
			warning = "ExpectedTime '" + Trim(value) + "' is not a time "
				"(use 2m40s, 2:40 or seconds)";
			return false;
		}
		info.expectedSeconds = sec;
		return true;
	}
	if(k == "actors" || k == "ninjas"){
		auto& list = (k == "actors") ? info.actors : info.ninjas;
		list = ParseList(value);
		for(const auto& person : list) NoteAwkwardText(person, warning);
		return true;
	}
	warning = "Unknown [Backstage] key '" + Trim(key)
		+ "' (expected Name, ExpectedTime, Actors or Ninjas)";
	return false;
}

std::vector<const BackstageInfo*> BuildLineup(const std::vector<Akt>& akts,
	int aktIdx, int sceneIdx){
	std::vector<const BackstageInfo*> lineup;
	if(aktIdx < 0 || aktIdx >= (int)akts.size()) return lineup;
	const auto& scenes = akts[aktIdx].scenes;
	if(sceneIdx < 0 || sceneIdx >= (int)scenes.size()) return lineup;
	if(!scenes[sceneIdx].backstage.present) return lineup;

	lineup.push_back(&scenes[sceneIdx].backstage);
	int a = aktIdx, s = sceneIdx + 1;
	while(lineup.size() < 3 && a < (int)akts.size()){
		if(s >= (int)akts[a].scenes.size()){ a++; s = 0; continue; }
		const auto& info = akts[a].scenes[s].backstage;
		if(info.present) lineup.push_back(&info);
		s++;
	}
	return lineup;
}

static std::string Field(const std::string& raw){
	std::string s;
	s.reserve(raw.size());
	for(char c : raw){
		if(c == ';' || c == '\r' || c == '\n') continue;
		s += c;
	}
	return Trim(s);
}

std::string FormatLine(const BackstageInfo& info){
	std::string line = Field(info.name) + ";" + FormatTime(info.expectedSeconds);
	auto appendGroup = [&](const char* label, const std::vector<std::string>& people){
		std::string names;
		for(const auto& p : people){
			std::string f = Field(p);
			if(!f.empty()) names += ";" + f;
		}
		if(!names.empty()) line += std::string(";") + label + names;
	};
	appendGroup("actors", info.actors);
	appendGroup("ninjas", info.ninjas);
	return line;
}

std::string FormatBody(const std::vector<const BackstageInfo*>& lineup){
	std::string body;
	for(size_t i = 0; i < lineup.size(); i++){
		if(i) body += "\n";
		body += FormatLine(*lineup[i]);
	}
	return body;
}

bool ParseUrl(const std::string& text, Url& out){
	std::string s = Trim(text);
	Url u;
	const std::string lower = Lower(s);
	if(lower.rfind("https://", 0) == 0){ u.https = true;  s = s.substr(8); }
	else if(lower.rfind("http://", 0) == 0){ u.https = false; s = s.substr(7); }
	else if(lower.find("://") != std::string::npos) return false;

	size_t slash = s.find('/');
	std::string hostPort = s.substr(0, slash);
	if(slash != std::string::npos && slash + 1 < s.size()) u.path = s.substr(slash);

	u.port = u.https ? 443 : 80;
	size_t colon = hostPort.rfind(':');
	if(colon != std::string::npos){
		std::string portText = hostPort.substr(colon + 1);
		if(portText.empty() || portText.size() > 5) return false;
		for(char c : portText) if(!std::isdigit((unsigned char)c)) return false;
		u.port = std::stoi(portText);
		if(u.port < 1 || u.port > 65535) return false;
		hostPort = hostPort.substr(0, colon);
	}
	if(hostPort.empty() || hostPort.find_first_of(" \t") != std::string::npos) return false;
	u.host = hostPort;
	out = u;
	return true;
}

std::string FormatUrl(const Url& url){
	std::string s = url.https ? "https://" : "http://";
	s += url.host;
	if(url.port != (url.https ? 443 : 80)) s += ":" + std::to_string(url.port);
	s += url.path;
	return s;
}

} // namespace Backstage
} // namespace SatyrAV
