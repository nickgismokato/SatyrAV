#include "render/ParticleSystem.hpp"
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace SatyrAV{

static float RandF(float min, float max){
	return min + (float)(rand() % 10000) / 10000.0f * (max - min);
}

// (1.4) Draw a uniform sample in (0, 1) for distribution sampling.
static float UniformU01(){
	float u = (float)(rand() % 100000) / 100000.0f;
	if(u <= 0.0f) u = 1e-6f;
	if(u >= 1.0f) u = 1.0f - 1e-6f;
	return u;
}

// (1.4) Box-Muller — one standard normal sample.
static float SampleStdNormal(){
	float u1 = UniformU01();
	float u2 = UniformU01();
	return std::sqrt(-2.0f * std::log(u1)) * std::cos(6.28318f * u2);
}

// (1.4) Sample a distribution, returning a value in [0, 1] suitable for
// multiplying by a screen dimension. Unknown / degenerate specs fall back
// to uniform.
static float SampleDist01(const DistSpec& spec){
	switch(spec.type){
		case Distribution::Normal:{
			float v = spec.p1 + spec.p2 * SampleStdNormal();
			return std::clamp(v, 0.0f, 1.0f);
		}
		case Distribution::LogNormal:{
			float z = spec.p1 + spec.p2 * SampleStdNormal();
			float v = std::exp(z);
			// Soft normalise: divide by (1 + v) so the result lands in (0, 1).
			return std::clamp(v / (1.0f + v), 0.0f, 1.0f);
		}
		case Distribution::Binomial:{
			int n = std::max(1, (int)spec.p1);
			float p = std::clamp(spec.p2, 0.0f, 1.0f);
			int successes = 0;
			for(int i = 0; i < n; i++){
				if(UniformU01() < p) successes++;
			}
			return (float)successes / (float)n;
		}
		case Distribution::Bernoulli:{
			float p = std::clamp(spec.p1, 0.0f, 1.0f);
			return (UniformU01() < p) ? 1.0f : 0.0f;
		}
		case Distribution::Gamma:{
			// Marsaglia & Tsang, for shape >= 1. For shape < 1 we use the
			// boost technique: sample with shape+1 and rescale by U^(1/k).
			float k = std::max(0.01f, spec.p1);
			float theta = std::max(0.0001f, spec.p2);
			float boost = 1.0f;
			if(k < 1.0f){
				boost = std::pow(UniformU01(), 1.0f / k);
				k += 1.0f;
			}
			float d = k - 1.0f / 3.0f;
			float c = 1.0f / std::sqrt(9.0f * d);
			float v;
			for(;;){
				float x = SampleStdNormal();
				float t = 1.0f + c * x;
				if(t <= 0.0f) continue;
				v = t * t * t;
				float u = UniformU01();
				if(u < 1.0f - 0.0331f * x * x * x * x) break;
				if(std::log(u) < 0.5f * x * x + d * (1.0f - v + std::log(v))) break;
			}
			float sample = boost * d * v * theta;
			return std::clamp(sample / (1.0f + sample), 0.0f, 1.0f);
		}
		case Distribution::Uniform:
		default:
			return UniformU01();
	}
}

// (1.5) Shape a velocity sample within a motion model's built-in [lo, hi]
// range using the configured distribution. Uniform reproduces the exact
// 1.4 path (same `RandF` call), so scenes without VX/VY tunings are
// byte-identical to 1.4.
static float SampleV(float lo, float hi, const DistSpec& d){
	if(d.type == Distribution::Uniform) return RandF(lo, hi);
	float u = SampleDist01(d);
	return lo + u * (hi - lo);
}

// (1.5) Downward acceleration applied to every integrating particle type
// when `bounceEdges` is on. Tuned so RAIN (spawn vy 300–600 px/s) reaches
// the floor visibly faster and bounces ~2–3 times before settling on a
// 1080p display, which gives a readable pile-up effect.
static constexpr float GRAVITY_PX_PER_S2 = 900.0f;

// (1.5) Below this combined |vx|+|vy| the particle is considered at rest
// when it touches the floor, and gets locked by `ReflectAndSettle`.
// Too low and floor hits keep bouncing imperceptibly; too high and drops
// visibly snap into place mid-bounce.
static constexpr float SETTLE_SPEED_PX_PER_S = 55.0f;

// (1.5) Add a gravity tick to a free particle. No-op when settled, and
// callers skip it for formula-driven types whose position is not derived
// from vx/vy (Orbit, Noise). Oscillation uses vy for its linear descent,
// so gravity speeds that descent up naturally.
static inline void ApplyGravity(Particle& p, float dt, bool on){
	if(on && !p.settled) p.vy += GRAVITY_PX_PER_S2 * dt;
}

// (1.4) Cheap hash-based 2D value noise for the Noise flow field.
// Returns a float in [-1, 1] that varies smoothly with (x, y).
static float Hash2(int x, int y){
	uint32_t h = (uint32_t)(x * 374761393) ^ (uint32_t)(y * 668265263);
	h = (h ^ (h >> 13)) * 1274126177u;
	return (float)((h & 0xFFFF) / 32768.0f) - 1.0f;
}
static float Fade(float t){ return t * t * (3.0f - 2.0f * t); }
static float ValueNoise2D(float x, float y){
	int xi = (int)std::floor(x);
	int yi = (int)std::floor(y);
	float xf = x - (float)xi;
	float yf = y - (float)yi;
	float u = Fade(xf);
	float v = Fade(yf);
	float a = Hash2(xi,     yi);
	float b = Hash2(xi + 1, yi);
	float c = Hash2(xi,     yi + 1);
	float d = Hash2(xi + 1, yi + 1);
	float ab = a + u * (b - a);
	float cd = c + u * (d - c);
	return ab + v * (cd - ab);
}

void ParticleSystem::Start(ParticleType pType, SDL_Texture* tex,
	int tw, int th, int sw, int sh, const RenderModifiers& mods){
	type = pType;
	textures.clear();
	textures.push_back({tex, tw, th});
	screenW = sw; screenH = sh;
	baseMods = mods;
	active = true;
	spawnTimer = 0.0f;
	particles.clear();

	ConfigureSpawnRate();
}

void ParticleSystem::StartPool(ParticleType pType,
	const std::vector<ParticleTex>& pool,
	int sw, int sh, const RenderModifiers& mods){
	type = pType;
	textures = pool;
	// Drop empties just in case.
	textures.erase(
		std::remove_if(textures.begin(), textures.end(),
			[](const ParticleTex& t){ return t.texture == nullptr; }),
		textures.end());
	if(textures.empty()){
		active = false;
		return;
	}
	screenW = sw; screenH = sh;
	baseMods = mods;
	active = true;
	spawnTimer = 0.0f;
	particles.clear();

	ConfigureSpawnRate();
}

void ParticleSystem::ConfigureSpawnRate(){
	// Tune spawn rate by type
	switch(type){
		case ParticleType::Rain:        spawnRate = 0.02f; break;
		case ParticleType::Snow:        spawnRate = 0.08f; break;
		case ParticleType::Confetti:    spawnRate = 0.04f; break;
		case ParticleType::Rise:        spawnRate = 0.06f; break;
		case ParticleType::Scatter:     spawnRate = 0.005f; break;
		// (1.4) stubs — tuned later in phase 3b
		case ParticleType::Brownian:    spawnRate = 0.05f; break;
		case ParticleType::Oscillation: spawnRate = 0.05f; break;
		case ParticleType::Orbit:       spawnRate = 0.05f; break;
		case ParticleType::Noise:       spawnRate = 0.05f; break;
		case ParticleType::LifeCurve:   spawnRate = 0.05f; break;
		default:                        spawnRate = 0.04f; break;
	}

	// Scale by intensity (lower intensity = longer between spawns)
	float intensity = std::clamp(baseMods.particleIntensity, 0.01f, 1.0f);
	spawnRate /= intensity;

	// (1.4) Apply per-type density multiplier from the tuning cascade.
	// Clamp to avoid runaway rates when authors pass 0 or a huge value.
	float dens = std::clamp(baseMods.densityMul, 0.01f, 20.0f);
	spawnRate /= dens;
}

void ParticleSystem::Stop(){
	active = false;
	stoppingSmoothly = false;
	particles.clear();
	textures.clear();
}

void ParticleSystem::StopSmoothly(){
	// Flip into "no more spawns" mode. Update() will keep integrating the
	// existing particles and Draw() keeps rendering them; once the live
	// count drains to zero, Update flips `active` to false so the owner
	// can garbage-collect the entry.
	if(!active) return;
	stoppingSmoothly = true;
}

bool ParticleSystem::IsActive() const{ return active; }
bool ParticleSystem::IsStoppingSmoothly() const{ return stoppingSmoothly; }

float ParticleSystem::SpawnX(int pw) const{
	if(baseMods.xDist.type == Distribution::Uniform){
		return RandF(-(float)pw, (float)screenW);
	}
	float u = SampleDist01(baseMods.xDist);
	return u * (float)std::max(0, screenW - pw);
}

void ParticleSystem::SpawnParticle(){
	if(textures.empty()) return;

	Particle p;

	// Pick a texture for this particle.
	p.texIdx = (int)(rand() % textures.size());
	const ParticleTex& chosen = textures[p.texIdx];
	int texW = chosen.w;
	int texH = chosen.h;

	// Determine size
	int pw = texW, ph = texH;
	if(baseMods.useRandomSize){
		pw = baseMods.minWidth + rand() % std::max(1, baseMods.maxWidth - baseMods.minWidth);
		if(baseMods.keepAspectRatio && texW > 0){
			float ratio = (float)texH / (float)texW;
			ph = (int)(pw * ratio);
		} else{
			ph = baseMods.minHeight + rand() % std::max(1, baseMods.maxHeight - baseMods.minHeight);
		}
	} else if(baseMods.width > 0 && baseMods.height > 0){
		pw = baseMods.width;
		ph = baseMods.height;
	}

	p.texW = pw;
	p.texH = ph;
	p.scale = 1.0f;
	p.alpha = baseMods.transparency;

	switch(type){
		case ParticleType::Rain:
			p.x = SpawnX(pw);
			p.y = -(float)ph;
			p.vx = SampleV(-20.0f, 20.0f, baseMods.vxDist);
			p.vy = SampleV(300.0f, 600.0f, baseMods.vyDist);
			p.rotation = RandF(-5.0f, 5.0f);
			p.rotationSpeed = 0.0f;
			p.maxLifetime = p.lifetime = (float)screenH / p.vy + 1.0f;
			break;

		case ParticleType::Snow:
			p.x = SpawnX(pw);
			p.y = -(float)ph;
			p.vx = SampleV(-40.0f, 40.0f, baseMods.vxDist);
			p.vy = SampleV(40.0f, 120.0f, baseMods.vyDist);
			p.rotation = RandF(0.0f, 360.0f);
			p.rotationSpeed = RandF(-30.0f, 30.0f);
			p.maxLifetime = p.lifetime = (float)screenH / p.vy + 2.0f;
			break;

		case ParticleType::Confetti:
			p.x = SpawnX(pw);
			p.y = -(float)ph;
			p.vx = SampleV(-60.0f, 60.0f, baseMods.vxDist);
			p.vy = SampleV(100.0f, 300.0f, baseMods.vyDist);
			p.rotation = RandF(0.0f, 360.0f);
			p.rotationSpeed = RandF(-180.0f, 180.0f);
			p.maxLifetime = p.lifetime = (float)screenH / p.vy + 1.0f;
			break;

		case ParticleType::Rise:
			p.x = SpawnX(pw);
			p.y = (float)(screenH + ph);
			p.vx = SampleV(-15.0f, 15.0f, baseMods.vxDist);
			p.vy = SampleV(-200.0f, -80.0f, baseMods.vyDist);
			p.rotation = RandF(0.0f, 360.0f);
			p.rotationSpeed = RandF(-20.0f, 20.0f);
			p.maxLifetime = p.lifetime = (float)screenH / std::abs(p.vy) + 1.0f;
			break;

		case ParticleType::Scatter:
			p.x = (float)screenW / 2.0f;
			p.y = (float)screenH / 2.0f;
			{
				float angle = RandF(0.0f, 6.28318f);
				float speed = RandF(100.0f, 500.0f);
				p.vx = std::cos(angle) * speed;
				p.vy = std::sin(angle) * speed;
			}
			p.rotation = RandF(0.0f, 360.0f);
			p.rotationSpeed = RandF(-120.0f, 120.0f);
			p.maxLifetime = p.lifetime = RandF(1.5f, 3.0f);
			break;

		// (1.4) Brownian — random-walk drift, spawn across the screen.
		// No directional gravity; velocity is nudged each frame in
		// ApplyPhysics. Lifetime is moderate so the cloud stays bounded.
		case ParticleType::Brownian:
			p.x = SpawnX(pw);
			p.y = RandF(0.0f, (float)screenH);
			p.vx = SampleV(-30.0f, 30.0f, baseMods.vxDist);
			p.vy = SampleV(-30.0f, 30.0f, baseMods.vyDist);
			p.rotation = RandF(0.0f, 360.0f);
			p.rotationSpeed = RandF(-30.0f, 30.0f);
			p.maxLifetime = p.lifetime = RandF(4.0f, 8.0f);
			break;

		// (1.4) Oscillation — sinusoidal X around a vertical column,
		// linear Y drift downward. Each particle gets its own amp/freq/phase
		// so the swarm doesn't move in lock-step.
		case ParticleType::Oscillation:
			p.auxD = SpawnX(pw);                    // baseX
			p.auxA = RandF(30.0f, 120.0f);          // amplitude px
			p.auxB = RandF(1.5f, 4.0f);             // freq rad/s
			p.auxC = RandF(0.0f, 6.28318f);         // phase
			p.x = p.auxD;
			p.y = -(float)ph;
			p.vx = 0.0f;
			p.vy = SampleV(80.0f, 180.0f, baseMods.vyDist);
			p.rotation = 0.0f;
			p.rotationSpeed = 0.0f;
			p.maxLifetime = p.lifetime = (float)screenH / p.vy + 1.0f;
			break;

		// (1.4) Orbit — circular motion around the screen centre.
		// Each particle gets a random radius and angular velocity; we
		// store the current angle and integrate it in ApplyPhysics.
		case ParticleType::Orbit:
			p.auxA = RandF(80.0f, (float)std::min(screenW, screenH) * 0.45f); // radius
			p.auxB = RandF(0.3f, 1.2f);             // angular vel rad/s
			if(rand() % 2) p.auxB = -p.auxB;        // random direction
			p.auxC = RandF(0.0f, 6.28318f);         // initial angle
			p.x = (float)screenW / 2.0f + p.auxA * std::cos(p.auxC);
			p.y = (float)screenH / 2.0f + p.auxA * std::sin(p.auxC);
			p.vx = 0.0f; p.vy = 0.0f;
			p.rotation = 0.0f;
			p.rotationSpeed = RandF(-60.0f, 60.0f);
			p.maxLifetime = p.lifetime = RandF(6.0f, 12.0f);
			break;

		// (1.4) Noise — velocity driven by a 2D value-noise flow field.
		// Particles are seeded across the screen; ApplyPhysics samples
		// the field at the current position and steers the velocity.
		case ParticleType::Noise:
			p.x = SpawnX(pw);
			p.y = RandF(0.0f, (float)screenH);
			p.vx = 0.0f;
			p.vy = 0.0f;
			p.rotation = RandF(0.0f, 360.0f);
			p.rotationSpeed = 0.0f;
			p.maxLifetime = p.lifetime = RandF(5.0f, 10.0f);
			break;

		// (1.4) LifeCurve — linear motion; scale and alpha follow a bell
		// curve over the particle's lifetime (grow-in, shrink-out).
		case ParticleType::LifeCurve:
			p.x = SpawnX(pw);
			p.y = RandF(0.0f, (float)screenH);
			p.vx = SampleV(-40.0f, 40.0f, baseMods.vxDist);
			p.vy = SampleV(-40.0f, 40.0f, baseMods.vyDist);
			p.rotation = RandF(0.0f, 360.0f);
			p.rotationSpeed = RandF(-30.0f, 30.0f);
			p.auxA = 1.0f; // base scale
			p.maxLifetime = p.lifetime = RandF(2.0f, 4.0f);
			break;

		default:
			p.x = RandF(0.0f, (float)screenW);
			p.y = -(float)ph;
			p.vx = 0; p.vy = 200;
			p.rotation = 0; p.rotationSpeed = 0;
			p.maxLifetime = p.lifetime = 3.0f;
			break;
	}

	// (1.4) Apply the tuning's speed multiplier to the spawn velocity.
	// Formula-driven types (Orbit, Oscillation) set vx/vy to 0 and drive
	// position directly, so the multiplier is a no-op for them. For those
	// we scale the corresponding aux fields instead so the user still sees
	// a faster system.
	float sp = std::clamp(baseMods.speedMul, 0.01f, 20.0f);
	p.vx *= sp;
	p.vy *= sp;
	if(type == ParticleType::Orbit){
		p.auxB *= sp;       // angular velocity
	} else if(type == ParticleType::Oscillation){
		p.auxB *= sp;       // oscillation frequency
	}

	particles.push_back(p);
}

// (1.4/1.5) Reflect a particle off the four screen edges. Applied after
// the per-type integration so even formula-driven types clamp inside
// bounds. The reflected velocity component loses 20% energy per hit.
// (1.5) When the particle hits the floor with both |vx| and |vy| below
// `SETTLE_SPEED_PX_PER_S`, it is locked in place — velocity and rotation
// speed zero — so RAIN droplets (and friends) pile up instead of dying
// mid-flight. Lifetime keeps ticking, so the pile fades out naturally.
// Settling only fires on the floor; ceiling and side hits still bounce.
static void ReflectEdges(Particle& p, int sw, int sh){
	if(p.settled) return;
	const float loss = 0.8f;
	float maxX = (float)(sw - p.texW);
	float maxY = (float)(sh - p.texH);
	if(p.x < 0.0f){         p.x = 0.0f;  if(p.vx < 0) p.vx = -p.vx * loss; }
	else if(p.x > maxX){    p.x = maxX;  if(p.vx > 0) p.vx = -p.vx * loss; }
	if(p.y < 0.0f){         p.y = 0.0f;  if(p.vy < 0) p.vy = -p.vy * loss; }
	else if(p.y > maxY){
		p.y = maxY;
		if(p.vy > 0) p.vy = -p.vy * loss;
		// (1.5) Floor rest check — once both components drop below the
		// settle threshold, freeze the particle so gravity can't keep
		// jittering it on the floor.
		if(std::abs(p.vx) < SETTLE_SPEED_PX_PER_S
			&& std::abs(p.vy) < SETTLE_SPEED_PX_PER_S){
			p.settled = true;
			p.vx = 0.0f;
			p.vy = 0.0f;
			p.rotationSpeed = 0.0f;
		}
	}
}

void ParticleSystem::ApplyPhysics(Particle& p, float dt){
	// (1.5) Settled particles skip all physics — ReflectAndSettle zeroed
	// their velocity and rotation speed on the bounce that parked them.
	// Lifetime keeps ticking so they fade and eventually die as normal.
	if(p.settled){
		p.lifetime -= dt;
		return;
	}

	// Default integration — per-model branches below may override.
	switch(type){
		// (1.4) Brownian — random-walk velocity kicks each frame plus
		// mild damping so the cloud stays bounded. No position teleport.
		case ParticleType::Brownian:{
			ApplyGravity(p, dt, baseMods.bounceEdges);
			p.vx += RandF(-200.0f, 200.0f) * dt;
			p.vy += RandF(-200.0f, 200.0f) * dt;
			p.vx *= std::max(0.0f, 1.0f - 0.8f * dt);
			p.vy *= std::max(0.0f, 1.0f - 0.8f * dt);
			p.x  += p.vx * dt;
			p.y  += p.vy * dt;
			p.rotation += p.rotationSpeed * dt;
			p.lifetime -= dt;
			p.alpha = baseMods.transparency * (p.lifetime / p.maxLifetime);
			return;
		}

		// (1.4) Oscillation — X is baseX + amplitude * sin(freq*t + phase).
		// Y drifts linearly at the spawn-time vy. t is derived from
		// (maxLifetime - lifetime) so each particle has its own clock.
		case ParticleType::Oscillation:{
			ApplyGravity(p, dt, baseMods.bounceEdges);
			float t = p.maxLifetime - p.lifetime;
			p.x = p.auxD + p.auxA * std::sin(p.auxB * t + p.auxC);
			p.y += p.vy * dt;
			p.rotation += p.rotationSpeed * dt;
			p.lifetime -= dt;
			return;
		}

		// (1.4) Orbit — integrate angle at angular velocity, recompute
		// position on the circle around screen centre.
		case ParticleType::Orbit:{
			p.auxC += p.auxB * dt;
			p.x = (float)screenW / 2.0f + p.auxA * std::cos(p.auxC);
			p.y = (float)screenH / 2.0f + p.auxA * std::sin(p.auxC);
			p.rotation += p.rotationSpeed * dt;
			p.lifetime -= dt;
			return;
		}

		// (1.4) Noise — sample a 2D value-noise field at the particle's
		// position (scaled) to steer its velocity. Small grid step means
		// nearby particles follow similar currents.
		case ParticleType::Noise:{
			const float scale = 1.0f / 150.0f;
			float n1 = ValueNoise2D(p.x * scale,         p.y * scale);
			float n2 = ValueNoise2D(p.x * scale + 31.7f, p.y * scale + 17.3f);
			// (1.4) Multiply base 120 px/s by the tuning's speed.
			float speed = 120.0f * std::clamp(baseMods.speedMul, 0.01f, 20.0f);
			p.vx = n1 * speed;
			p.vy = n2 * speed;
			p.x += p.vx * dt;
			p.y += p.vy * dt;
			p.rotation += p.rotationSpeed * dt;
			p.lifetime -= dt;
			return;
		}

		// (1.4) LifeCurve — linear motion; scale rises to ~1.5x at mid-life
		// then falls back, alpha does a matching fade in/out. Uses a
		// simple triangular envelope (sin(pi*t) would also work).
		case ParticleType::LifeCurve:{
			ApplyGravity(p, dt, baseMods.bounceEdges);
			p.x += p.vx * dt;
			p.y += p.vy * dt;
			p.rotation += p.rotationSpeed * dt;
			p.lifetime -= dt;
			float lt = 1.0f - (p.lifetime / p.maxLifetime); // 0 → 1
			float env = (lt < 0.5f) ? (lt * 2.0f) : ((1.0f - lt) * 2.0f);
			p.scale = p.auxA * (0.5f + env);
			p.alpha = baseMods.transparency * env;
			return;
		}

		default: break;
	}

	// Legacy types (Rain, Snow, Confetti, Rise, Scatter): shared linear
	// integration. (1.5) Gravity ticks here so all five inherit settling.
	ApplyGravity(p, dt, baseMods.bounceEdges);
	p.x += p.vx * dt;
	p.y += p.vy * dt;
	p.rotation += p.rotationSpeed * dt;
	p.lifetime -= dt;

	// Snow sway
	if(type == ParticleType::Snow){
		p.vx += RandF(-80.0f, 80.0f) * dt;
		p.vx = std::clamp(p.vx, -60.0f, 60.0f);
	}

	// Scatter fade out
	if(type == ParticleType::Scatter){
		p.alpha = baseMods.transparency * (p.lifetime / p.maxLifetime);
	}
}

void ParticleSystem::Update(float dt){
	if(!active) return;

	// (1.4) Smooth-stop: skip spawning entirely so the live particles
	// drain naturally. Once they're all gone, deactivate so the owner
	// can drop this entry.
	if(stoppingSmoothly){
		if(particles.empty()){
			active = false;
			textures.clear();
			return;
		}
	} else{
		// Spawn new particles
		spawnTimer += dt;
		while(spawnTimer >= spawnRate){
			SpawnParticle();
			spawnTimer -= spawnRate;
		}
	}

	// Update existing
	for(auto& p : particles){
		ApplyPhysics(p, dt);

		// (1.4) Boundary reflection — applied here so it runs after every
		// per-type branch in ApplyPhysics, including formula-driven ones.
		if(baseMods.bounceEdges){
			ReflectEdges(p, screenW, screenH);
		}

		// (1.4.1) Age existing samples and drop expired ones. We rebuild
		// the ring as a plain vector each frame — N is small (≤ 64), and
		// the bookkeeping for an in-place ring with age-based eviction
		// would be more code than this.
		int N = baseMods.trailLength;
		if(N > 0){
			float maxAge = std::max(0.5f, (float)N * 0.08f);
			if(!p.trail.empty()){
				std::vector<TrailSample> kept;
				kept.reserve(p.trail.size() + 1);
				int M = (int)p.trail.size();
				for(int i = 0; i < M; i++){
					int ring = (p.trailHead + i) % M;
					TrailSample s = p.trail[ring];
					s.age += dt;
					if(s.age <= maxAge) kept.push_back(s);
				}
				p.trail = std::move(kept);
				p.trailHead = 0;
			}

			// Distance-gated push — new sample only when the particle has
			// moved at least `minDist` pixels since the last one. Stops
			// slow particles from stacking copies on top of themselves.
			float minDist = std::max(2.0f, (float)std::min(p.texW, p.texH) * 0.25f);
			bool shouldPush = !p.lastSampleValid;
			if(p.lastSampleValid){
				float dx = p.x - p.lastSampleX;
				float dy = p.y - p.lastSampleY;
				if(dx * dx + dy * dy >= minDist * minDist) shouldPush = true;
			}
			if(shouldPush){
				TrailSample s{p.x, p.y, p.rotation, p.scale, p.alpha, 0.0f};
				if((int)p.trail.size() < N){
					p.trail.push_back(s);
				} else{
					// Drop oldest, append newest.
					p.trail.erase(p.trail.begin());
					p.trail.push_back(s);
				}
				p.lastSampleX = p.x;
				p.lastSampleY = p.y;
				p.lastSampleValid = true;
			}
		}
	}

	// Remove dead particles
	particles.erase(
		std::remove_if(particles.begin(), particles.end(),
			[](const Particle& p){ return p.lifetime <= 0.0f; }),
		particles.end());
}

void ParticleSystem::Draw(SDL_Renderer* renderer){
	if(!active || !renderer || textures.empty()) return;

	for(auto& p : particles){
		int idx = (p.texIdx >= 0 && p.texIdx < (int)textures.size()) ? p.texIdx : 0;
		SDL_Texture* tex = textures[idx].texture;
		if(!tex) continue;

		// (1.4.1) Trail — three render modes:
		//   Ghost   — fade-and-shrink past samples behind the particle.
		//   Stretch — single velocity-aligned quad from oldest sample to live.
		//   Glow    — Ghost with additive blending for a bloom look.
		if(!p.trail.empty()){
			int N = baseMods.trailLength;
			float maxAge = std::max(0.5f, (float)N * 0.08f);

			if(baseMods.trailMode == RenderModifiers::TrailMode::Stretch){
				// Single elongated quad from the oldest live sample to the
				// particle's current position. Rotated along that vector.
				const TrailSample& tail = p.trail.front();
				float headX = p.x;
				float headY = p.y;
				float dx = headX - tail.x;
				float dy = headY - tail.y;
				float len = std::sqrt(dx * dx + dy * dy);
				if(len > 1.0f){
					float midX = (headX + tail.x) * 0.5f + (float)p.texW * 0.5f;
					float midY = (headY + tail.y) * 0.5f + (float)p.texH * 0.5f;
					int sw = (int)len + p.texW;
					int sh = p.texH;
					SDL_Rect sdst = {(int)midX - sw / 2, (int)midY - sh / 2, sw, sh};
					double angDeg = std::atan2(dy, dx) * 57.2957795f;
					uint8_t sa = (uint8_t)(std::clamp(p.alpha * 0.5f, 0.0f, 1.0f) * 255.0f);
					SDL_SetTextureAlphaMod(tex, sa);
					SDL_RenderCopyEx(renderer, tex, nullptr, &sdst,
						angDeg, nullptr, SDL_FLIP_NONE);
				}
			} else{
				bool glow = (baseMods.trailMode == RenderModifiers::TrailMode::Glow);
				if(glow) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);

				const float decayRate = 4.0f / maxAge;
				int M = (int)p.trail.size();
				for(int i = 0; i < M; i++){
					const TrailSample& s = p.trail[i];
					float t = std::clamp(s.age / maxAge, 0.0f, 1.0f);
					float scl = s.scale * std::max(0.1f, 1.0f - t);
					int tdw = (int)(p.texW * scl);
					int tdh = (int)(p.texH * scl);
					int tdx = (int)s.x - (tdw - p.texW) / 2;
					int tdy = (int)s.y - (tdh - p.texH) / 2;
					SDL_Rect tdst = {tdx, tdy, tdw, tdh};
					float fade = std::exp(-s.age * decayRate);
					uint8_t ta = (uint8_t)(std::clamp(s.alpha * fade, 0.0f, 1.0f) * 255.0f);
					SDL_SetTextureAlphaMod(tex, ta);
					SDL_RenderCopyEx(renderer, tex, nullptr, &tdst,
						(double)s.rotation, nullptr, SDL_FLIP_NONE);
				}

				if(glow) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
			}
		}

		// (1.4) Apply per-particle scale, centred on (p.x, p.y) so scaling
		// doesn't pull the particle toward its top-left corner.
		int dw = (int)(p.texW * p.scale);
		int dh = (int)(p.texH * p.scale);
		int dx = (int)p.x - (dw - p.texW) / 2;
		int dy = (int)p.y - (dh - p.texH) / 2;
		SDL_Rect dst = {dx, dy, dw, dh};

		uint8_t alpha = (uint8_t)(std::clamp(p.alpha, 0.0f, 1.0f) * 255.0f);
		SDL_SetTextureAlphaMod(tex, alpha);
		SDL_RenderCopyEx(renderer, tex, nullptr, &dst,
			(double)p.rotation, nullptr, SDL_FLIP_NONE);
	}

	// Reset alpha on every texture we touched.
	for(auto& t : textures){
		if(t.texture) SDL_SetTextureAlphaMod(t.texture, 255);
	}
}

} // namespace SatyrAV
