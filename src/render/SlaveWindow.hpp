#pragma once
#include "render/Renderer.hpp"
#include "render/TextRenderer.hpp"
#include "render/MediaPlayer.hpp"
#include "render/ParticleSystem.hpp"
#include "render/MediaCache.hpp"
#include "core/Types.hpp"
#include <string>
#include <vector>

namespace SatyrAV{

struct SlaveTextEntry{
	std::vector<TextRun> runs;
	RenderModifiers mods;

	// Per-entry animation state (set on PushText, read during DrawText).
	float spawnTimeSec = 0.0f; // wall-clock seconds at push
	float currentSpin  = 0.0f; // accumulated degrees from spin(...)

	// Group tag — empty = ungrouped. Grouped entries survive plain `clear`
	// and are only dropped by `clear <name>`, `clearText`, or `clearAll`.
	std::string groupName;

	// (1.6.1) Subtitle anchor — when true, layout overrides the X axis to
	// keep the line horizontally centred on the canvas while honouring
	// `mods.posY` for vertical placement. Used by `textD`'s secondary
	// translation line. Independent of grouping/clear semantics.
	bool subtitleAnchor = false;
};

struct SlaveImageEntry{
	SDL_Texture* tex = nullptr; // not owned (cache or MediaPlayer)
	int w = 0, h = 0;
	RenderModifiers mods;
	float spawnTimeSec = 0.0f;
	float currentSpin  = 0.0f;
	std::string groupName;

	// (1.4) Animated GIF playback. When `frames` is non-empty, `tex` is
	// ignored and the draw path picks `frames[frameIdx]` instead, wrapping
	// through `frameDelaysMs` as elapsed time crosses each delay. Pointers
	// here are borrowed from the MediaCache and MUST NOT be freed.
	std::vector<SDL_Texture*> frames;
	std::vector<int> frameDelaysMs;
	int frameIdx = 0;
	float frameElapsedSec = 0.0f;
};

struct SlaveParticleEntry{
	ParticleSystem system;
	std::string groupName;
};

class SlaveWindow{
public:
	void Clear(Renderer& r, Colour bg = Colours::BLACK);
	void DrawText(Renderer& r, TextRenderer& text);
	void DrawMedia(Renderer& r, MediaPlayer& media);
	void DrawParticles(SDL_Renderer* renderer, float dt);
	void Update(Renderer& r, MediaPlayer& media, float dt);

	// State mutators
	void PushText(const std::string& line, const RenderModifiers& mods = {},
		const std::string& groupName = "");
	// Multi-run variant — used when a text line contains `+` concatenation.
	void PushTextRuns(const std::vector<TextRun>& runs, const RenderModifiers& mods,
		const std::string& groupName = "");
	// Append runs to the most recently pushed text entry. If no prior entry
	// exists, behaves like PushTextRuns. Used by `textCont`.
	void AppendToLastText(const std::vector<TextRun>& runs, const RenderModifiers& mods,
		const std::string& groupName = "");
	// (1.6.1) Push a subtitle entry — same plumbing as PushTextRuns but the
	// resulting entry has `subtitleAnchor=true`, so DrawText centres it
	// horizontally and uses `mods.posY` (in percent of canvas) for the
	// vertical position regardless of the default centred-stack layout.
	// Used by `textD`'s second argument.
	void PushSubtitleRuns(const std::vector<TextRun>& runs, const RenderModifiers& mods,
		const std::string& groupName = "");
	// Remove all ungrouped text entries (preserves traditional `clear` behaviour).
	void ClearText();
	// Remove every text entry regardless of group (used by `clearText` / `clearAll`).
	void ClearTextAll();
	void SetBackgroundColour(Colour c);
	void SetCapitalize(bool cap);

	// Image with modifiers. Both ungrouped and grouped shows append, so a
	// scene can stack multiple `show` calls on screen. `clear` /
	// `clearImages` still drop every ungrouped image at once.
	void ShowImage(SDL_Texture* tex, int w, int h, const RenderModifiers& mods,
		const std::string& groupName = "");
	// (1.4) Animated GIF variant. `frames` and `delaysMs` must be the same
	// length; the entry cycles through frames by wall time at draw. Pointers
	// are borrowed (owned by MediaCache) and must outlive the entry.
	void ShowAnimation(const std::vector<SDL_Texture*>& frames,
		const std::vector<int>& delaysMs, int w, int h,
		const RenderModifiers& mods, const std::string& groupName = "");
	// Remove ungrouped images.
	void ClearImage();
	// Remove every image entry regardless of group.
	void ClearImageAll();

	// Particle system. Ungrouped starts replace any existing ungrouped
	// particles; grouped starts append.
	void StartParticles(ParticleType type, SDL_Texture* tex,
		int texW, int texH, int screenW, int screenH,
		const RenderModifiers& mods,
		const std::string& groupName = "");
	// Pool variant — every spawn picks a new random texture.
	// Used by randomImages(...) + particle(...).
	void StartParticlesPool(ParticleType type,
		const std::vector<ParticleTex>& pool,
		int screenW, int screenH, const RenderModifiers& mods,
		const std::string& groupName = "");
	// Stop ungrouped particle systems. Smooth-stopping systems are
	// preserved so a later `stop` doesn't cut their ending short.
	void StopParticles();
	// Stop every particle system regardless of group. Same protection
	// for smooth-stopping systems applies.
	void StopParticlesAll();
	// (1.4) Mark matching systems as smooth-stopping — no more spawns
	// but live particles continue until they die naturally. Empty
	// `groupName` matches ungrouped systems; otherwise matches that group.
	void StopParticlesSmoothly(const std::string& groupName = "");

	// Group / global clear helpers.
	// Drop every text/image/particle entry tagged with groupName.
	void ClearGroup(const std::string& groupName);
	// Drop all text, images, and particles regardless of group.
	void ClearAll();

private:
	std::vector<SlaveTextEntry> textEntries;
	Colour bgColour = Colours::BLACK;
	bool capitalizeText = false;

	// Active images (previously a single texture; now a list so grouped
	// shows can stack and ungrouped shows replace their own kind only).
	std::vector<SlaveImageEntry> imageEntries;

	// Active particle systems, each with its own group tag.
	std::vector<SlaveParticleEntry> particleEntries;

	// Delta-time stashed by Update() so draw helpers can integrate spin.
	float lastDt = 0.0f;

	// If spawnTimeSec > 0 and mods.hasMove, the position is lerped from
	// from→to over moveDurationSec. If currentSpin != nullptr and
	// mods.spinDegPerSec != 0, the accumulator is advanced by lastDt and
	// added to the static rotation.
	void RenderTextureFitted(SDL_Renderer* r, SDL_Texture* tex,
		int texW, int texH, int screenW, int screenH,
		const RenderModifiers& mods,
		float spawnTimeSec = 0.0f, float* currentSpin = nullptr);
};

} // namespace SatyrAV
