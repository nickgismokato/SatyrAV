#include "screens/LoadProjectScreen.hpp"
#include "App.hpp"
#include "core/Config.hpp"
#include "core/Project.hpp"
#include <filesystem>

namespace SatyrAV{

void LoadProjectScreen::OnEnter(){
	ScanProjects();
}

void LoadProjectScreen::HandleInput(InputAction action){
	switch(action){
		case InputAction::NavUp:    projectList.MoveUp(); break;
		case InputAction::NavDown:  projectList.MoveDown(); break;
		case InputAction::JumpUp:   projectList.MoveUp(JUMP_STEP); break;
		case InputAction::JumpDown: projectList.MoveDown(JUMP_STEP); break;
		case InputAction::JumpTop:  projectList.JumpToTop(); break;
		case InputAction::JumpBottom: projectList.JumpToBottom(); break;
		case InputAction::Execute:{
			int idx = projectList.GetSelectedIndex();
			if(idx >= 0 && idx < (int)projectPaths.size()){
				ProjectData data = Project::Load(projectPaths[idx]);
				app->OpenProject(data);
			}
			break;
		}
		case InputAction::NavLeft:
		case InputAction::Quit:
			app->SwitchScreen(ScreenID::MainMenu);
			break;
		default: break;
	}
}

void LoadProjectScreen::Update(float deltaTime){
	(void)deltaTime;
}

void LoadProjectScreen::Draw(Renderer& r, TextRenderer& text){
	r.ClearMaster(Colours::BACKGROUND);
	auto* renderer = r.GetMaster();

	text.DrawTextCentered(renderer, "Load Project",
		r.GetMasterWidth() / 2, 30, Colours::WHITE);

	projectList.x = 50;
	projectList.y = 80;
	projectList.w = r.GetMasterWidth() - 100;
	projectList.h = r.GetMasterHeight() - 120;
	projectList.focused = true;
	projectList.Draw(r, text);

}

void LoadProjectScreen::ScanProjects(){
	auto& config = Config::Instance();
	projectPaths.clear();
	std::vector<std::string> names;

	if(!config.projectsDir.empty() && std::filesystem::exists(config.projectsDir)){
		for(auto& entry : std::filesystem::directory_iterator(config.projectsDir)){
			if(entry.is_directory()){
				auto schemaPath = entry.path() / "schema.toml";
				if(std::filesystem::exists(schemaPath)){
					projectPaths.push_back(entry.path().string());
					names.push_back(entry.path().filename().string());
				}
			}
		}
	}

	projectList.SetItems(names);
}

} // namespace SatyrAV
