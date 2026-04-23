#pragma once
#include "core/Types.hpp"
#include <SDL2/SDL.h>
#include <vector>
#include <string>

namespace SatyrAV{

struct ParticleTex{
	SDL_Texture* texture = nullptr;
	int w = 0, h = 0;
};

// (1.4) One past-frame sample for the trail ring buffer.
// (1.4.1) `age` is wall-time seconds since the sample was pushed — used
// for exponential alpha decay and age-shrink scale.
struct TrailSample{
	float x, y;
	float rotation;
	float scale;
	float alpha;
	float age;
};

struct Particle{
	float x, y;          // position in pixels
	float vx, vy;        // velocity
	float rotation;      // degrees
	float rotationSpeed; // degrees/sec
	float lifetime;      // remaining seconds
	float maxLifetime;
	float scale;
	float alpha;         // 0-1
	int texW, texH;      // final draw size for this particle
	int texIdx;          // index into the texture pool for this particle

	// (1.4) Per-model scratch fields — meaning depends on ParticleType.
	// Oscillation: auxA=amplitude(px), auxB=freq(rad/s), auxC=phase, auxD=baseX
	// Orbit:       auxA=radius(px),    auxB=angularVel,    auxC=angle, auxD=unused
	// LifeCurve:   auxA=baseScale,     auxB=unused,        auxC=unused, auxD=unused
	// Brownian / Noise: unused (state is the velocity itself).
	float auxA = 0.0f;
	float auxB = 0.0f;
	float auxC = 0.0f;
	float auxD = 0.0f;

	// (1.5) True once the particle has come to rest on the floor under
	// gravity + edge bounce. Settled particles skip all per-type physics,
	// keep their lifetime countdown, and fade/expire naturally. Reset to
	// false on spawn.
	bool settled = false;

	// (1.4) Trail ring buffer — oldest sample at `trailHead` when the
	// buffer is full. Empty when trailLength (baseMods) is 0.
	std::vector<TrailSample> trail;
	int trailHead = 0;
	// (1.4.1) Last position a trail sample was pushed at — used for
	// distance-gating so slow particles don't stack copies on themselves.
	// `lastSampleValid` flips true once the first sample lands.
	float lastSampleX = 0.0f;
	float lastSampleY = 0.0f;
	bool  lastSampleValid = false;
};

class ParticleSystem{
public:
	// Single-texture start (all particles share one image).
	void Start(ParticleType type, SDL_Texture* texture,
		int texW, int texH, int screenW, int screenH,
		const RenderModifiers& mods);
	// Pool start — every spawn picks a fresh random texture from the pool.
	// Used by randomImages() combined with particle().
	void StartPool(ParticleType type, const std::vector<ParticleTex>& pool,
		int screenW, int screenH, const RenderModifiers& mods);
	void Stop();
	// (1.4) Stop spawning, but keep updating and drawing the particles
	// that are currently alive so they finish their animation naturally.
	// Once the live-particle count hits zero the system deactivates itself.
	void StopSmoothly();
	void Update(float dt);
	void Draw(SDL_Renderer* renderer);

	bool IsActive() const;
	// (1.4) True once `StopSmoothly()` has been called. Regular `stop`,
	// `clear`, and group clears leave smooth-stopping systems alone so
	// the ending cue isn't cut short by a later hard-stop command.
	bool IsStoppingSmoothly() const;

private:
	ParticleType type = ParticleType::None;
	std::vector<ParticleTex> textures; // at least 1 entry while active
	int screenW = 0, screenH = 0;
	float spawnTimer = 0.0f;
	float spawnRate  = 0.05f; // seconds between spawns
	bool active = false;
	bool stoppingSmoothly = false;

	RenderModifiers baseMods;
	std::vector<Particle> particles;

	void ConfigureSpawnRate();
	void SpawnParticle();
	void ApplyPhysics(Particle& p, float dt);

	// (1.4) Pick a spawn X respecting `baseMods.xDist`. For Uniform, keeps
	// the legacy `-pw → screenW` range so particles can slide in from the
	// left edge. For non-Uniform, the sample maps to [0, screenW - pw].
	float SpawnX(int pw) const;
};

} // namespace SatyrAV
