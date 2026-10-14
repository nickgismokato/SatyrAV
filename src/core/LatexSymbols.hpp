#pragma once
#include <string>

namespace SatyrAV{

// (1.8.2) Expand LaTeX-style symbol macros in displayed text into the
// UTF-8 characters they name: `\alpha` → α, `\subseteq` → ⊆,
// `\mathbb{R}` → ℝ.
//
// Runs at parse time on text-command runs and on `comment` text, so the
// slave renderer, the master navigator, and the word-wrap measuring code
// all deal in real characters — nothing downstream needs to know this
// feature exists. Filenames, group names, and section keys are never
// touched.
//
// This is a lookup table, not a LaTeX engine. Two consequences worth
// knowing when authoring scenes:
//
//   - **Unknown macros are left byte-for-byte.** `\foo` stays `\foo`.
//     That is deliberate: it is what keeps the renderer's `\n` line-break
//     marker working, since `n` is not a symbol name.
//   - **No space is swallowed after a macro.** Real LaTeX eats it, so
//     `\alpha x` renders as `αx`; here it renders as `α x`, which is
//     almost always what the author meant. Write `\alpha\beta` (no
//     separator needed) when two symbols should touch.
//
// The recognised set is documented in docs/commands.md.
std::string ExpandLatexSymbols(const std::string& text);

} // namespace SatyrAV
