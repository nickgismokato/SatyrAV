#include "core/TexParser.hpp"
#include "core/Logger.hpp"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <string>
#include <unordered_set>

namespace SatyrAV{

namespace{

enum class StopAt{
	EndOfInput,
	CloseBrace,
	CloseBracket,
	EndEnvironment
};

bool IsLetter(char c){
	return std::isalpha((unsigned char)c) != 0;
}

// Environments whose body is kept as raw source instead of being parsed.
/* Not needed for this 
bool IsVerbatimEnvironment(const std::string& name){
	return name == "verbatim" || name == "verbatim*" || name == "lstlisting"
		|| name == "minted" || name == "comment";
}
*/

// Single-use recursive-descent reader over one source string.
class Reader{
public:
	Reader(const std::string& source, const std::unordered_map<std::string, int>& argCounts)
		: s(source), argCounts(argCounts){}

	std::vector<TexNode> ParseDocument(){
		return ParseList(StopAt::EndOfInput, "", 1);
	}

private:
	const std::string& s;
	const std::unordered_map<std::string, int>& argCounts;
	size_t i = 0;
	int line = 1;

	bool AtEnd() const{ return i >= s.size(); }
	char Peek() const{ return i < s.size() ? s[i] : '\0'; }
	bool LooksAt(const std::string& text) const{ return s.compare(i, text.size(), text) == 0; }

	char Advance(){
		char c = s[i++];
		if(c == '\n') ++line;
		return c;
	}

	void Warn(int atLine, const std::string& msg){
		Logger::Instance().Warnf("TexParser: line %d: %s", atLine, msg.c_str());
	}

	// `%` to end of line, plus the newline and the next line's leading
	// whitespace — same as TeX, so `foo%\n  bar` reads as `foobar`.
	void SkipComment(){
		while(!AtEnd() && Peek() != '\n') Advance();
		if(!AtEnd()) Advance();
		while(Peek() == ' ' || Peek() == '\t') Advance();
	}

	// Spaces and tabs, plus at most one newline when `allowNewline`. A blank
	// line is a paragraph break and is never skipped.
	void SkipInlineSpace(bool allowNewline = true){
		bool sawNewline = !allowNewline;
		while(!AtEnd()){
			char c = Peek();
			if(c == ' ' || c == '\t' || c == '\r') Advance();
			else if(c == '\n' && !sawNewline){ sawNewline = true; Advance(); }
			else break;
		}
	}

	// True (positioned on the opener) if `opener` follows after optional
	// whitespace; otherwise the position is left untouched.
	bool PeekArgumentOpener(char opener, bool allowNewline){
		size_t savedI = i;
		int savedLine = line;
		SkipInlineSpace(allowNewline);
		if(Peek() == opener) return true;
		i = savedI;
		line = savedLine;
		return false;
	}

	// Name after a backslash: a run of letters (with an optional trailing
	// `*`) or a single non-letter character. Spaces after a letter name are
	// swallowed, as TeX does.
	std::string ReadControlName(){
		if(AtEnd()) return "";
		if(!IsLetter(Peek())) return std::string(1, Advance());
		std::string name;
		while(!AtEnd() && IsLetter(Peek())) name += Advance();
		if(Peek() == '*') name += Advance();
		SkipInlineSpace();
		return name;
	}

	// `{name}` read verbatim — used for \begin and \end.
	std::string ReadBraceRaw(){
		SkipInlineSpace();
		if(Peek() != '{'){
			Warn(line, "expected '{' after \\begin or \\end");
			return "";
		}
		int openLine = line;
		Advance();
		std::string out;
		while(!AtEnd() && Peek() != '}') out += Advance();
		if(AtEnd()) Warn(openLine, "unclosed '{'");
		else Advance();
		return out;
	}

	std::string ReadRawUntil(const std::string& terminator, int startLine){
		size_t end = s.find(terminator, i);
		if(end == std::string::npos){
			Warn(startLine, "missing '" + terminator + "'");
			end = s.size();
		}
		std::string raw = s.substr(i, end - i);
		while(i < end) Advance();
		i = std::min(end + terminator.size(), s.size());
		return raw;
	}

	std::string ReadMathUntil(const std::string& closer, int startLine){
		std::string out;
		while(!AtEnd()){
			if(LooksAt(closer)){
				i += closer.size();
				return out;
			}
			if(Peek() == '%'){ SkipComment(); continue; }
			if(Peek() == '\\'){
				// Keep escapes like `\$` together so they can't close the math.
				out += Advance();
				if(!AtEnd()) out += Advance();
				continue;
			}
			out += Advance();
		}
		Warn(startLine, "unclosed math, expected '" + closer + "'");
		return out;
	}

	std::string ReadText(StopAt stop){
		std::string out;
		while(!AtEnd()){
			char c = Peek();
			if(c == '\\' || c == '{' || c == '}' || c == '$' || c == '%') break;
			if(c == ']' && stop == StopAt::CloseBracket) break;
			out += Advance();
		}
		return out;
	}

	static void AppendText(std::vector<TexNode>& nodes, const std::string& text){
		if(text.empty()) return;
		if(!nodes.empty() && nodes.back().kind == TexNodeKind::Text){
			nodes.back().text += text;
			return;
		}
		TexNode node;
		node.kind = TexNodeKind::Text;
		node.text = text;
		nodes.push_back(std::move(node));
	}

	// Optional `[...]` arguments first, then mandatory `{...}` ones, up to
	// the registered arg count (unlimited if none is registered).
	void ParseArguments(TexNode& node, bool allowNewline){
		int maxArgs = -1;
		auto it = argCounts.find(node.name);
		if(it != argCounts.end()) maxArgs = it->second;

		int taken = 0;
		while(maxArgs < 0 || taken < maxArgs){
			if(node.args.empty() && PeekArgumentOpener('[', allowNewline)){
				int openLine = line;
				Advance();
				node.optArgs.push_back(ParseList(StopAt::CloseBracket, "", openLine));
			}else if(PeekArgumentOpener('{', allowNewline)){
				int openLine = line;
				Advance();
				node.args.push_back(ParseList(StopAt::CloseBrace, "", openLine));
			}else{
				break;
			}
			++taken;
		}
	}

	TexNode ParseCommand(const std::string& name, int startLine){
		TexNode cmd;
		cmd.kind = TexNodeKind::Command;
		cmd.name = name;
		cmd.line = startLine;
		if(IsLetter(name[0])){
			ParseArguments(cmd, true);
		}else if(name == "\\" && PeekArgumentOpener('[', false)){
			// `\\[2pt]` — the only control symbol that takes an argument.
			int openLine = line;
			Advance();
			cmd.optArgs.push_back(ParseList(StopAt::CloseBracket, "", openLine));
		}
		return cmd;
	}

	TexNode ParseEnvironment(int startLine){
		TexNode env;
		env.kind = TexNodeKind::Environment;
		env.line = startLine;
		env.name = ReadBraceRaw();
		// Arguments must start on the \begin line, so a body that opens
		// with `{` or `[` on the next line isn't mistaken for one.
		ParseArguments(env, false);
		
		/*Old, Not needed
		if(IsVerbatimEnvironment(env.name))
			env.text = ReadRawUntil("\\end{" + env.name + "}", startLine);
		else
		*/
		
		env.children = ParseList(StopAt::EndEnvironment, env.name, startLine);
		return env;
	}

	TexNode ParseDollarMath(){
		TexNode math;
		math.kind = TexNodeKind::Math;
		math.line = line;
		Advance();
		math.display = Peek() == '$';
		if(math.display) Advance();
		math.text = ReadMathUntil(math.display ? "$$" : "$", math.line);
		return math;
	}

	std::vector<TexNode> ParseList(StopAt stop, const std::string& envName, int openLine){
		std::vector<TexNode> nodes;
		while(!AtEnd()){
			char c = Peek();
			if(c == '%'){
				SkipComment();
				continue;
			}
			if(c == '}'){
				Advance();
				if(stop == StopAt::CloseBrace) return nodes;
				Warn(line, "unmatched '}' ignored");
				continue;
			}
			if(c == ']' && stop == StopAt::CloseBracket){
				Advance();
				return nodes;
			}
			if(c == '{'){
				TexNode group;
				group.kind = TexNodeKind::Group;
				group.line = line;
				Advance();
				group.children = ParseList(StopAt::CloseBrace, "", group.line);
				nodes.push_back(std::move(group));
				continue;
			}
			if(c == '$'){
				nodes.push_back(ParseDollarMath());
				continue;
			}
			if(c == '\\'){
				int startLine = line;
				Advance();
				std::string name = ReadControlName();
				std::unordered_set<std::string> notNeeded = {
					"documentclass", "usepackage", "revyname", "revyyear", "version", "status", "author", "melody", "maketitle", "responsible", "writtenbyname", "melodyname", "responsibletext", "rolename"
				};
				if(name.empty()){
					AppendText(nodes, "\\"); // lone backslash at end of file
				}else if(notNeeded.find(name) != notNeeded.end()){
					continue;
				}else if(name == "begin"){
					nodes.push_back(ParseEnvironment(startLine));
				}else if(name == "end"){
					std::string endName = ReadBraceRaw();
					if(stop == StopAt::EndEnvironment && endName == envName) return nodes;
					Warn(startLine, "\\end{" + endName + "} without matching \\begin, ignored");
				}else if(name == "role"){
					continue;
					/*
					For now we do nothing. But this is where we do the logic to find the actor.
					*/
				}else if(name == "sings"){
					continue;
					/*
					For now we do nothing. But this is where we do the logic to write the 'text "This is a lyrics"'.
					*/
				}else if(name == "(" || name == "["){
					TexNode math;
					math.kind = TexNodeKind::Math;
					math.line = startLine;
					math.display = name == "[";
					math.text = ReadMathUntil(math.display ? "\\]" : "\\)", startLine);
					nodes.push_back(std::move(math));
				}else{
					nodes.push_back(ParseCommand(name, startLine));
				}
				continue;
			}
			AppendText(nodes, ReadText(stop));
		}

		if(stop == StopAt::CloseBrace) Warn(openLine, "unclosed '{'");
		else if(stop == StopAt::CloseBracket) Warn(openLine, "unclosed '['");
		else if(stop == StopAt::EndEnvironment) Warn(openLine, "missing \\end{" + envName + "}");
		return nodes;
	}
};

} // namespace

TexParser::TexParser(){
	// `\item {x}` is body text, not an argument; `\item[label]` still works.
	argCounts["item"] = 1;
}

bool TexParser::ConvertFile(const std::string& texPath, const std::string& ngkPath){
	std::ifstream in(texPath, std::ios::binary);
	if(!in){
		Logger::Instance().Warnf("TexParser: can't open '%s'", texPath.c_str());
		return false;
	}
	std::stringstream buffer;
	buffer << in.rdbuf();

	std::string output = Convert(Parse(buffer.str()));

	std::ofstream out(ngkPath, std::ios::binary);
	if(!out){
		Logger::Instance().Warnf("TexParser: can't write '%s'", ngkPath.c_str());
		return false;
	}
	out << output;
	return true;
}

std::vector<TexNode> TexParser::Parse(const std::string& source) const{
	Reader reader(source, argCounts);
	return reader.ParseDocument();
}

std::string TexParser::Convert(const std::vector<TexNode>& nodes){
	std::string out;
	EmitAll(nodes, out);
	return out;
}

void TexParser::SetCommandHandler(const std::string& name, Handler handler){
	commandHandlers[name] = std::move(handler);
}

void TexParser::SetEnvironmentHandler(const std::string& name, Handler handler){
	environmentHandlers[name] = std::move(handler);
}

void TexParser::SetMathHandler(Handler handler){
	mathHandler = std::move(handler);
}

void TexParser::SetArgCount(const std::string& name, int count){
	argCounts[name] = count;
}

void TexParser::Emit(const TexNode& node, std::string& out){
	switch(node.kind){
	case TexNodeKind::Text:
		out += node.text;
		return;

	case TexNodeKind::Group:
		EmitAll(node.children, out);
		return;

	case TexNodeKind::Math:
		if(mathHandler) mathHandler(*this, node, out);
		else out += node.text;
		return;

	case TexNodeKind::Environment:{
		auto it = environmentHandlers.find(node.name);
		if(it != environmentHandlers.end()){
			it->second(*this, node, out);
			return;
		}
		out += node.text; // verbatim body, empty otherwise
		EmitAll(node.children, out);
		return;
	}

	case TexNodeKind::Command:{
		auto it = commandHandlers.find(node.name);
		if(it != commandHandlers.end()){
			it->second(*this, node, out);
			return;
		}
		if(node.name == "\\"){
			out += '\n';
			return;
		}
		if(node.name.size() == 1 && std::string("%{}$&#_ ").find(node.name[0]) != std::string::npos){
			out += node.name;
			return;
		}
		if(node.name == "\n"){
			out += ' ';
			return;
		}
		for(const auto& arg : node.args) EmitAll(arg, out);
		return;
	}
	}
}

void TexParser::EmitAll(const std::vector<TexNode>& nodes, std::string& out){
	for(const auto& node : nodes) Emit(node, out);
}

} // namespace SatyrAV
