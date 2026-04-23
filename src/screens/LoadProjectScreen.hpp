#pragma once
#include "screens/Screen.hpp"
#include "ui/ListView.hpp"

namespace SatyrAV{

class LoadProjectScreen : public Screen{
public:
	void OnEnter() override;
	void HandleInput(InputAction action) override;
	void Update(float deltaTime) override;
	void Draw(Renderer& r, TextRenderer& text) override;

private:
	ListView projectList;
	std::vector<std::string> projectPaths;

	void ScanProjects();
};

} // namespace SatyrAV
