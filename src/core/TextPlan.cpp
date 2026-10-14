#include "core/TextPlan.hpp"

namespace SatyrAV{
namespace TextPlan{

bool AddsCentredLine(const Command& cmd){
	switch(cmd.type){
		case CommandType::Text:
		case CommandType::TextBf:
		case CommandType::TextIt:
		case CommandType::Typewrite:
		case CommandType::TextD:
			break;
		default:
			return false;
	}
	if(cmd.runs.empty() && cmd.argument.empty()) return false;
	return cmd.mods.InCentredStack();
}

bool EndsBlock(const Command& cmd){
	return (cmd.type == CommandType::Clear && cmd.argument.empty())
		|| cmd.type == CommandType::ClearText
		|| cmd.type == CommandType::ClearAll;
}

std::vector<const Command*> PlanBlock(const Scene& scene, const Command* start){
	// The scene in firing order: a compound's sub-commands fire in place of
	// it (CueEngine::ExecuteCommand), and comments never fire.
	std::vector<const Command*> order;
	for(const auto& c : scene.commands){
		if(c.type == CommandType::Compound){
			for(const auto& sub : c.subCommands){
				if(sub.type != CommandType::Comment) order.push_back(&sub);
			}
		} else if(c.type != CommandType::Comment){
			order.push_back(&c);
		}
	}

	std::vector<const Command*> plan;
	size_t i = 0;
	while(i < order.size() && order[i] != start) i++;
	if(i == order.size()) return plan;
	for(; i < order.size(); i++){
		const Command* c = order[i];
		if(c != start && EndsBlock(*c)) break;
		if(AddsCentredLine(*c)) plan.push_back(c);
	}
	return plan;
}

std::vector<std::vector<TextRun>> PlannedRuns(const std::vector<const Command*>& plan){
	std::vector<std::vector<TextRun>> out;
	out.reserve(plan.size());
	for(const Command* c : plan){
		if(!c->runs.empty()){
			out.push_back(c->runs);
		} else{
			// The single-string form, as SlaveWindow::PushText builds it.
			TextRun r;
			r.text = c->argument;
			out.push_back({r});
		}
	}
	return out;
}

} // namespace TextPlan
} // namespace SatyrAV
