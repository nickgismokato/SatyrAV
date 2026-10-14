#pragma once
#include "ui/Widget.hpp"
#include <string>

namespace SatyrAV{

class TextField : public Widget{
public:
	void Draw(Renderer& r, TextRenderer& text) override;

	void SetText(const std::string& t);
	const std::string& GetText() const;
	void AppendText(const std::string& ch);
	void RemoveLastChar();
	void Clear();

	std::string label;
	int maxLength = 256;

private:
	std::string content;
};

} // namespace SatyrAV
