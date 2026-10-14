#pragma once
#include "core/Types.hpp"
#include <vector>

namespace SatyrAV{

// (2.0.2) Fixed text centre. By default the slave centres its text stack
// on the lines currently on screen, so every new line moves the block up
// by half a line. With `FixedTextCentre = true` the block is instead
// centred on every line it *will* hold before it is cleared, so each line
// appears where it will stay.
//
// This file decides which lines those are; SlaveWindow measures them.
namespace TextPlan{

// A cue that adds a line to the centred stack: `text`, `textbf`, `textit`,
// `typewrite`, and the primary line of `textD` — unless `pos(...)`,
// `move(...)` or `pathMove(...)` places it elsewhere. Mirrors the slave's
// own rule (RenderModifiers::InCentredStack).
bool AddsCentredLine(const Command& cmd);

// A cue that empties the centred stack: `clear` (no name), `clearText`,
// `clearAll`. `clear NAME` only drops one group, so it does not end a block.
bool EndsBlock(const Command& cmd);

// The cues that will put lines in the centred stack, from `start` (which
// must point into `scene.commands`, or into a compound's sub-commands)
// up to the next EndsBlock cue or the end of the scene, in firing order.
// Empty when `start` is not found in the scene.
std::vector<const Command*> PlanBlock(const Scene& scene, const Command* start);

// The text each planned cue will show, as the slave receives it.
std::vector<std::vector<TextRun>> PlannedRuns(const std::vector<const Command*>& plan);

} // namespace TextPlan
} // namespace SatyrAV
