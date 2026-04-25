#include "core/Project.hpp"
#include "core/SchemaParser.hpp"
#include "core/SceneParser.hpp"
#include "core/AssetPath.hpp"
#include <filesystem>
#include <fstream>
#include <toml++/toml.hpp>

namespace fs = std::filesystem;

namespace SatyrAV{

ProjectData Project::Create(
	RevyType type, int year,
	const std::string& creatorName,
	const std::string& projectPath,
	const std::string& revyName){
	ProjectData data;
	data.revyType = type;
	// If no explicit display name was supplied (e.g. older callers),
	// fall back to the enum's built-in name so the schema is still valid.
	data.revyName = revyName.empty() ? RevyTypeName(type) : revyName;
	data.year = year;
	data.creatorName = creatorName;
	data.projectPath = projectPath;

	// Create directory structure
	fs::create_directories(projectPath);
	fs::create_directories(projectPath + "/scenes");
	fs::create_directories(projectPath + "/movies");
	fs::create_directories(projectPath + "/sound");
	fs::create_directories(projectPath + "/pictures");

	// Copy the logo as an example image
	std::string logoSrc = FindAsset("satyr-logo-body.png");
	std::string logoDst = projectPath + "/pictures/satyr-logo.png";
	if(fs::exists(logoSrc)){
		fs::copy_file(logoSrc, logoDst, fs::copy_options::skip_existing);
	}

	// Generate default schema and example scene
	GenerateDefaultSchema(projectPath + "/schema.toml", data);
	GenerateExampleScene(projectPath + "/scenes/ExampleScene.ngk");

	// Reload the project so it's fully populated
	return Load(projectPath);
}

ProjectData Project::Load(const std::string& projectPath){
	ProjectData data;
	data.projectPath = projectPath;

	std::string schemaPath = projectPath + "/schema.toml";
	if(!fs::exists(schemaPath)) return data;

	data.schema = SchemaParser::ParseFile(schemaPath);
	// (1.6.2) Mirror creator up to ProjectData so callers reading the
	// top-level field see the persisted value. revyName/year stay split
	// across `data` (bare name + int year, set at create-time) and
	// `schema` (combined "Name YYYY" string + parsed int) intentionally
	// — the LoadProject screen reads the schema fields directly.
	data.creatorName = data.schema.creatorName;
	if(data.schema.year > 0) data.year = data.schema.year;

	// Load all scene files
	for(auto& akt : data.schema.akts){
		for(auto& sceneName : akt.sceneNames){
			std::string scenePath = projectPath + "/scenes/" + sceneName + ".ngk";
			Scene scene = SceneParser::ParseFile(scenePath);
			if(scene.name.empty()){
				scene.name = sceneName;
			}
			akt.scenes.push_back(std::move(scene));
		}
	}

	return data;
}

void Project::Save(const ProjectData& data){
	// Round-trip the schema TOML: load → mutate → write. Only the fields
	// we actively manage from inside the app are updated; everything else
	// (Options, RevyData, Structure.*) is preserved byte-for-byte by toml++
	// on emit. Note: toml++ does not preserve stand-alone comments — users
	// who hand-edit schema.toml with # notes will lose them here.
	std::string schemaPath = data.projectPath + "/schema.toml";
	if(!fs::exists(schemaPath)) return;

	toml::table tbl;
	try{
		tbl = toml::parse_file(schemaPath);
	} catch(const toml::parse_error&){
		return;
	}

	// Ensure [Display] exists, then update its keys from the in-memory schema.
	if(!tbl.contains("Display") || !tbl["Display"].is_table()){
		tbl.insert_or_assign("Display", toml::table{});
	}
	auto& disp = *tbl["Display"].as_table();
	const auto& td = data.schema.targetedDisplay;
	disp.insert_or_assign("Width",    (int64_t)td.width);
	disp.insert_or_assign("Height",   (int64_t)td.height);
	disp.insert_or_assign("CenterX",  (int64_t)td.centerX);
	disp.insert_or_assign("CenterY",  (int64_t)td.centerY);
	disp.insert_or_assign("Rotation", (double)td.rotation);

	std::ofstream out(schemaPath);
	if(!out.is_open()) return;
	out << tbl;
}

void Project::GenerateDefaultSchema(const std::string& path, const ProjectData& data){
	std::ofstream out(path);
	out << "# Project options — these apply to all scenes unless overridden\n";
	out << "[Options]\n";
	out << "FontSize = 25\n";
	out << "TextOutline = 4        # Black outline thickness (pt), 0 = disabled\n";
	out << "# Font = \"\"           # Path to .ttf font file\n";
	out << "# BackgroundColour = black  # Default slave background\n";
	out << "\n";
	out << "[RevyData]\n";
	// data.revyName already includes the Jubi suffix and/or custom name
	// chosen by the user in the New Project screen.
	out << "Revy = \"" << data.revyName << " " << data.year << "\"\n";
	// (1.6.2) Persist the creator so the Load Project screen can show
	// authorship in its second column. Empty string is acceptable for
	// users who skip the field; older projects can backfill by hand.
	out << "Creator = \"" << data.creatorName << "\"\n\n";
	out << "[Structure]\n";
	out << "akter = 1\n";
	out << "[Structure.Akt1]\n";
	out << "schema = [\n";
	out << "\t\"ExampleScene\"\n";
	out << "]\n\n";
	out << "# [Display] — targeted display rect within the physical secondary screen.\n";
	out << "# Adjust interactively from Debug popup (D) → S, then ESC to save.\n";
	out << "# [Display]\n";
	out << "# Width    = 1920\n";
	out << "# Height   = 1080\n";
	out << "# CenterX  = 960\n";
	out << "# CenterY  = 540\n";
	out << "# Rotation = 0.0\n";
}

void Project::GenerateExampleScene(const std::string& path){
	std::ofstream out(path);
	out << "# =============================================\n";
	out << "# ExampleScene.ngk — A tutorial scene\n";
	out << "# =============================================\n";
	out << "# Each line under [Commands] is one cue.\n";
	out << "# Press ENTER to advance through them.\n";
	out << "#\n";
	out << "# Commands:\n";
	out << "#   text \"message\"       — Display text on screen\n";
	out << "#   clear                — Clear all text and images\n";
	out << "#   show file.png        — Display an image\n";
	out << "#   play file.mp3        — Play audio or video\n";
	out << "#   stop                 — Stop all media\n";
	out << "#   stop file.mp3        — Stop specific media\n";
	out << "#   clear & text \"Hi\"    — Combine with & for one cue\n";
	out << "#\n";
	out << "# Loops:\n";
	out << "#   loop(count, clearEvery){\n";
	out << "#       commands...\n";
	out << "#   }\n";
	out << "#\n";
	out << "# Per-scene options override project options:\n";
	out << "#   bc = black/blue/red/green/white\n";
	out << "#   FontSize = 30\n";
	out << "#   font = path/to/font.ttf\n";
	out << "\n";
	out << "[Options]\n";
	out << "bc = black\n";
	out << "# FontSize = 30  # Uncomment to override project font size\n";
	out << "\n";
	out << "[Preload]\n";
	out << "satyr-logo.png\n";
	out << "\n";
	out << "[Commands]\n";
	out << "text \"Welcome to SatyrAV!\"\n";
	out << "text \"This is your first scene.\"\n";
	out << "text \"Each line is a cue - press ENTER to advance.\"\n";
	out << "clear\n";
	out << "text \"Let's show the SaTyR logo:\"\n";
	out << "show satyr-logo.png\n";
	out << "clear                                #Clear everything\n";
	out << "text \"You can combine commands:\"\n";
	out << "clear & text \"Like this!\"            #Clear and show text in one cue\n";
	out << "clear\n";
	out << "text \"Now let's try a loop:\"\n";
	out << "clear\n";
	out << "loop(3, 1){\n";
	out << "\ttext \"Loop iteration!\"\n";
	out << "\ttext \"This repeats 3 times, clearing each time.\"\n";
	out << "}\n";
	out << "text \"Loop finished!\"\n";
	out << "clear\n";
	out << "text \"That's the basics!\"\n";
	out << "text \"Edit this .ngk file to create your own scenes.\"\n";
	out << "clear\n";
}

} // namespace SatyrAV
