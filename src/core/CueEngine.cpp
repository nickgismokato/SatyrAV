#include "core/CueEngine.hpp"
#include "core/SceneParser.hpp"
#include <algorithm>

namespace SatyrAV{

void CueEngine::LoadProject(const ProjectData& project){
	projectData = project;
	currentAktIdx = -1;
	currentSceneIdx = -1;
	currentCmdIdx = -1;
}

void CueEngine::ReloadScenes(){
	// Re-parse every scene .ngk without touching the schema or resetting
	// navigation. Bound to the U key during performance so the user can
	// tweak a cue file and pull it back in without leaving the project.
	const std::string base = projectData.projectPath + "/scenes/";
	for(auto& akt : projectData.schema.akts){
		akt.scenes.clear();
		for(auto& sceneName : akt.sceneNames){
			Scene scene = SceneParser::ParseFile(base + sceneName + ".ngk");
			if(scene.name.empty()) scene.name = sceneName;
			akt.scenes.push_back(std::move(scene));
		}
	}

	// Clamp selection so it still points at something valid. The akt
	// structure doesn't change, so only the command index can go stale.
	if(currentAktIdx >= 0 && currentSceneIdx >= 0){
		int cmdCount = GetCommandCountForScene(currentAktIdx, currentSceneIdx);
		if(cmdCount <= 0){
			currentCmdIdx = -1;
		} else if(currentCmdIdx >= cmdCount){
			currentCmdIdx = cmdCount - 1;
		}
		// Snap off any comment row the reload may have landed us on.
		if(currentCmdIdx >= 0){
			int snap = NextExecutableIndex(currentAktIdx, currentSceneIdx, currentCmdIdx);
			if(snap < 0) snap = PrevExecutableIndex(currentAktIdx, currentSceneIdx, currentCmdIdx);
			if(snap >= 0) currentCmdIdx = snap;
		}
	}
}

void CueEngine::SetSlaveCallback(SlaveUpdateFn callback){
	slaveCallback = std::move(callback);
}

void CueEngine::SelectAkt(int index){
	if(index >= 0 && index < (int)projectData.schema.akts.size()){
		currentAktIdx = index;
		currentSceneIdx = -1;
		currentCmdIdx = -1;
	}
}

void CueEngine::SelectScene(int index){
	if(currentAktIdx < 0) return;
	auto& akt = projectData.schema.akts[currentAktIdx];
	if(index >= 0 && index < (int)akt.scenes.size()){
		currentSceneIdx = index;
		currentCmdIdx = -1;
	}
}

void CueEngine::SetCommandIndex(int index){
	currentCmdIdx = index;
}

void CueEngine::ExecuteCurrentCommand(){
	auto* scene = GetCurrentScene();
	if(!scene || currentCmdIdx < 0) return;
	if(currentCmdIdx >= (int)scene->commands.size()) return;

	ExecuteCommand(scene->commands[currentCmdIdx]);
}

void CueEngine::ExecuteCommand(const Command& cmd){
	// Comments are non-executable — they live in the navigator for the operator
	// but never fire on the slave.
	if(cmd.type == CommandType::Comment) return;

	if(cmd.type == CommandType::Compound){
		// Execute all sub-commands in sequence
		for(auto& sub : cmd.subCommands){
			if(sub.type == CommandType::Comment) continue;
			if(slaveCallback) slaveCallback(sub);
		}
		return;
	}

	if(slaveCallback){
		slaveCallback(cmd);
	}
}

int CueEngine::GetCurrentAktIndex() const{ return currentAktIdx; }
int CueEngine::GetCurrentSceneIndex() const{ return currentSceneIdx; }
int CueEngine::GetCurrentCommandIndex() const{ return currentCmdIdx; }
int CueEngine::GetAktCount() const{ return (int)projectData.schema.akts.size(); }

int CueEngine::GetSceneCount() const{
	if(currentAktIdx < 0) return 0;
	return (int)projectData.schema.akts[currentAktIdx].scenes.size();
}

int CueEngine::GetCommandCount() const{
	auto* scene = GetCurrentScene();
	return scene ? (int)scene->commands.size() : 0;
}

const Akt* CueEngine::GetCurrentAkt() const{
	if(currentAktIdx < 0) return nullptr;
	return &projectData.schema.akts[currentAktIdx];
}

const Scene* CueEngine::GetCurrentScene() const{
	auto* akt = GetCurrentAkt();
	if(!akt || currentSceneIdx < 0) return nullptr;
	if(currentSceneIdx >= (int)akt->scenes.size()) return nullptr;
	return &akt->scenes[currentSceneIdx];
}

const Command* CueEngine::GetCurrentCommand() const{
	auto* scene = GetCurrentScene();
	if(!scene || currentCmdIdx < 0) return nullptr;
	if(currentCmdIdx >= (int)scene->commands.size()) return nullptr;
	return &scene->commands[currentCmdIdx];
}

const Scene* CueEngine::GetNextScene() const{
	if(currentAktIdx < 0 || currentSceneIdx < 0) return nullptr;
	auto& akts = projectData.schema.akts;
	auto& akt = akts[currentAktIdx];

	// Next scene in same akt
	if(currentSceneIdx + 1 < (int)akt.scenes.size()){
		return &akt.scenes[currentSceneIdx + 1];
	}

	// First scene in next akt
	if(currentAktIdx + 1 < (int)akts.size()){
		auto& nextAkt = akts[currentAktIdx + 1];
		if(!nextAkt.scenes.empty()){
			return &nextAkt.scenes[0];
		}
	}

	return nullptr;
}

bool CueEngine::IsLastScene() const{
	return GetNextScene() == nullptr;
}

void CueEngine::StopAllMedia(){
	// TODO: Stop all playing audio/video
}

void CueEngine::ToggleVideo(){
	// TODO: Pause/resume current video
}

void CueEngine::ToggleMusic(){
	// TODO: Pause/resume current audio
}

const Akt* CueEngine::GetAkt(int index) const{
	if(index < 0 || index >= (int)projectData.schema.akts.size()) return nullptr;
	return &projectData.schema.akts[index];
}

const Scene* CueEngine::GetScene(int aktIdx, int sceneIdx) const{
	auto* akt = GetAkt(aktIdx);
	if(!akt || sceneIdx < 0 || sceneIdx >= (int)akt->scenes.size()) return nullptr;
	return &akt->scenes[sceneIdx];
}

const Command* CueEngine::GetCommand(int aktIdx, int sceneIdx, int cmdIdx) const{
	auto* scene = GetScene(aktIdx, sceneIdx);
	if(!scene || cmdIdx < 0 || cmdIdx >= (int)scene->commands.size()) return nullptr;
	return &scene->commands[cmdIdx];
}

int CueEngine::GetSceneCountForAkt(int aktIdx) const{
	auto* akt = GetAkt(aktIdx);
	return akt ? (int)akt->scenes.size() : 0;
}

int CueEngine::GetCommandCountForScene(int aktIdx, int sceneIdx) const{
	auto* scene = GetScene(aktIdx, sceneIdx);
	return scene ? (int)scene->commands.size() : 0;
}

const std::string& CueEngine::GetRevyName() const{
	return projectData.schema.revyName;
}

int CueEngine::GetRevyYear() const{
	return projectData.year;
}

int CueEngine::GetEffectiveFontSize() const{
	// Scene -> Project -> Global
	auto* scene = GetCurrentScene();
	if(scene && scene->fontSize > 0) return scene->fontSize;
	if(projectData.schema.fontSize > 0) return projectData.schema.fontSize;
	return 25; // absolute fallback
}

Colour CueEngine::GetEffectiveBackgroundColour() const{
	auto* scene = GetCurrentScene();
	if(scene) return scene->backgroundColor;
	return projectData.schema.backgroundColor;
}

int CueEngine::GetTextOutline() const{
	return projectData.schema.textOutline;
}

int CueEngine::GetProjectFontSize() const{
	return projectData.schema.fontSize;
}

int CueEngine::GetSceneFontSize() const{
	auto* scene = GetCurrentScene();
	return scene ? scene->fontSize : 0;
}

bool CueEngine::GetEffectiveCapitalize() const{
	auto* scene = GetCurrentScene();
	if(scene && scene->capitalize) return true;
	return projectData.schema.capitalize;
}

ParticleTuning CueEngine::GetEffectiveParticleTuning(ParticleType type) const{
	ParticleTuning out; // built-in defaults (1.0, 1.0, Uniform)

	// Project-level first so scene can override it.
	auto projIt = projectData.schema.particleTunings.find((int)type);
	if(projIt != projectData.schema.particleTunings.end()){
		const auto& t = projIt->second;
		if(t.haveSpeed)   { out.speed   = t.speed;   out.haveSpeed   = true; }
		if(t.haveDensity) { out.density = t.density; out.haveDensity = true; }
		if(t.haveXDist)   { out.xDist   = t.xDist;   out.haveXDist   = true; }
		if(t.haveVxDist)  { out.vxDist  = t.vxDist;  out.haveVxDist  = true; }
		if(t.haveVyDist)  { out.vyDist  = t.vyDist;  out.haveVyDist  = true; }
	}

	// Scene-level.
	auto* scene = GetCurrentScene();
	if(scene){
		auto sceneIt = scene->particleTunings.find((int)type);
		if(sceneIt != scene->particleTunings.end()){
			const auto& t = sceneIt->second;
			if(t.haveSpeed)   { out.speed   = t.speed;   out.haveSpeed   = true; }
			if(t.haveDensity) { out.density = t.density; out.haveDensity = true; }
			if(t.haveXDist)   { out.xDist   = t.xDist;   out.haveXDist   = true; }
			if(t.haveVxDist)  { out.vxDist  = t.vxDist;  out.haveVxDist  = true; }
			if(t.haveVyDist)  { out.vyDist  = t.vyDist;  out.haveVyDist  = true; }
		}
	}

	return out;
}

int CueEngine::NextExecutableIndex(int aktIdx, int sceneIdx, int from) const{
	auto* scene = GetScene(aktIdx, sceneIdx);
	if(!scene) return -1;
	int n = (int)scene->commands.size();
	for(int i = std::max(0, from); i < n; i++){
		if(scene->commands[i].type != CommandType::Comment) return i;
	}
	return -1;
}

int CueEngine::PrevExecutableIndex(int aktIdx, int sceneIdx, int from) const{
	auto* scene = GetScene(aktIdx, sceneIdx);
	if(!scene) return -1;
	int n = (int)scene->commands.size();
	int start = std::min(from, n - 1);
	for(int i = start; i >= 0; i--){
		if(scene->commands[i].type != CommandType::Comment) return i;
	}
	return -1;
}

int CueEngine::FirstExecutableIndex(int aktIdx, int sceneIdx) const{
	return NextExecutableIndex(aktIdx, sceneIdx, 0);
}

int CueEngine::LastExecutableIndex(int aktIdx, int sceneIdx) const{
	auto* scene = GetScene(aktIdx, sceneIdx);
	if(!scene || scene->commands.empty()) return -1;
	return PrevExecutableIndex(aktIdx, sceneIdx, (int)scene->commands.size() - 1);
}

} // namespace SatyrAV
