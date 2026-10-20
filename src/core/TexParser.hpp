#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace SatyrAV{

// Parses a practical subset of LaTeX into a node tree and writes it out as
// a `.ngk` text file. This is NOT a TeX engine — there is no macro
// expansion and no catcode handling. It understands:
//   \command[opt]{arg}...   control words (letters, optional trailing `*`)
//   \\  \%  \{  \$ ...      control symbols (one non-letter character)
//   {...}                   groups
//   \begin{env} ... \end{env}
//   $...$  $$...$$  \(...\)  \[...\]   math, kept as raw source
//   % comments              dropped
//   verbatim / lstlisting / comment environments, kept as raw source
//
// Commands do not need to be known in advance: by default a command takes
// every `[...]` and `{...}` that directly follows it (optional arguments
// first, then mandatory ones). Use SetArgCount to cap that for commands
// commands where a following `{` is body text rather than an argument
// (`\item` is pre-set to 1). Arguments after `\begin{env}` must start on the
// same line, and the same SetArgCount table applies to environment names.
//
// What ends up in the `.ngk` file is decided by the handlers. Anything
// without a handler falls back to the defaults described on Emit.

enum class TexNodeKind{
	Text,         // plain text in `text`
	Command,      // `name` + optArgs/args
	Group,        // `{...}` — contents in `children`
	Environment,  // `name` + optArgs/args + `children` (or raw `text` for verbatim)
	Math          // raw source in `text`; `display` for $$ and \[
};

struct TexNode{
	TexNodeKind kind = TexNodeKind::Text;
	std::string name;
	std::string text;
	bool display = false;
	std::vector<std::vector<TexNode>> optArgs;
	std::vector<std::vector<TexNode>> args;
	std::vector<TexNode> children;
	int line = 0;
};

class TexParser{
public:
	// Writes the converted output for `node` into `out`. Handlers can call
	// parser.Emit / parser.EmitAll to recurse into arguments and children.
	using Handler = std::function<void(TexParser& parser, const TexNode& node, std::string& out)>;

	TexParser();

	// Read `texPath`, parse it and write the result to `ngkPath`.
	// Returns false if either file can't be opened.
	bool ConvertFile(const std::string& texPath, const std::string& ngkPath);

	// Parse a source string. Malformed input is logged and recovered from,
	// never thrown.
	std::vector<TexNode> Parse(const std::string& source) const;

	// Convert an already-parsed tree into `.ngk` text.
	std::string Convert(const std::vector<TexNode>& nodes);

	// Handler registration. `name` is without the backslash
	// ("section", "section*", "\\", "%") or the environment name ("itemize").
	void SetCommandHandler(const std::string& name, Handler handler);
	void SetEnvironmentHandler(const std::string& name, Handler handler);
	void SetMathHandler(Handler handler);

	// Max number of arguments (optional + mandatory) a command consumes.
	// Commands without an entry consume greedily.
	void SetArgCount(const std::string& name, int count);

	// Defaults when no handler is registered:
	//   Text         written as-is
	//   Group        children emitted
	//   Command      control symbols \% \{ \} \$ \& \# \_ write their
	//                character, `\\` writes a newline, anything else emits
	//                its mandatory `{...}` arguments in order
	//   Environment  children (or raw text) emitted
	//   Math         raw source written as-is
	void Emit(const TexNode& node, std::string& out);
	void EmitAll(const std::vector<TexNode>& nodes, std::string& out);

private:
	std::unordered_map<std::string, Handler> commandHandlers;
	std::unordered_map<std::string, Handler> environmentHandlers;
	Handler mathHandler;
	std::unordered_map<std::string, int> argCounts;
};

} // namespace SatyrAV
