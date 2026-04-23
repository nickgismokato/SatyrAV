#include "render/MediaCache.hpp"
#include <SDL2/SDL_image.h>
#include <filesystem>
#include <cstdio>
#include <algorithm>

namespace SatyrAV{

std::string MediaCache::ResolvePath(const std::string& base, const std::string& rel){
	std::string full = base + "/" + rel;
	if(std::filesystem::exists(full)) return full;
	const char* SUBDIRS[] = {"pictures", "movies", "sound", nullptr};
	for(int i = 0; SUBDIRS[i]; i++){
		std::string tryPath = base + "/" + SUBDIRS[i] + "/" + rel;
		if(std::filesystem::exists(tryPath)) return tryPath;
	}
	return full;
}

bool MediaCache::IsImageExt(const std::string& ext){
	return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
		ext == ".bmp" || ext == ".gif" || ext == ".webp";
}

static bool IsAnimatedExt(const std::string& ext){
	// GIF is the only animated raster format we decode via SDL_image. WebP
	// animations also exist but require extra build flags; leave that for
	// a future phase. Other image extensions always take the static path.
	return ext == ".gif";
}

static std::string LowerExt(const std::string& path){
	auto dot = path.find_last_of('.');
	if(dot == std::string::npos) return "";
	std::string ext = path.substr(dot);
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	return ext;
}

void MediaCache::PreloadFiles(const std::vector<std::string>& paths,
	const std::string& projectBase, SDL_Renderer* renderer){
	for(auto& relPath : paths){
		std::string full = ResolvePath(projectBase, relPath);
		if(cache.count(full)) continue; // already cached

		std::string ext = LowerExt(full);

		if(IsAnimatedExt(ext)){
			// Animated GIF — decode every frame ahead of playback.
			LoadAnimation(full, renderer);
		} else if(IsImageExt(ext)){
			// Load image texture into cache
			LoadImage(full, renderer);
		} else{
			// Register non-image file (audio/video) — track size but no texture
			CachedMedia entry;
			entry.path = full;
			entry.isImage = false;
			if(std::filesystem::exists(full)){
				try{
					entry.sizeBytes = (size_t)std::filesystem::file_size(full);
				} catch(...){}
			}
			cache[full] = entry;
		}
	}
}

void MediaCache::PreloadForSceneTransition(
	const std::vector<std::string>& prevPaths,
	const std::vector<std::string>& currPaths,
	const std::vector<std::string>& nextPaths,
	const std::string& projectBase, SDL_Renderer* renderer){

	// Build the set of paths to keep
	std::vector<std::string> keepPaths;
	auto addResolved = [&](const std::vector<std::string>& paths){
		for(auto& p : paths){
			keepPaths.push_back(ResolvePath(projectBase, p));
		}
	};
	addResolved(prevPaths);
	addResolved(currPaths);
	addResolved(nextPaths);

	Evict(keepPaths);

	PreloadFiles(prevPaths, projectBase, renderer);
	PreloadFiles(currPaths, projectBase, renderer);
	PreloadFiles(nextPaths, projectBase, renderer);
}

CachedMedia* MediaCache::Get(const std::string& path){
	auto it = cache.find(path);
	if(it != cache.end()) return &it->second;
	return nullptr;
}

// Destroy every texture owned by the entry. For animations `texture` aliases
// `frames[0]`, so it must not be destroyed a second time.
static void DestroyTextures(CachedMedia& entry){
	if(entry.isAnimation){
		for(auto* f : entry.frames){
			if(f) SDL_DestroyTexture(f);
		}
		entry.frames.clear();
		entry.frameDelaysMs.clear();
		entry.texture = nullptr;
	} else if(entry.texture){
		SDL_DestroyTexture(entry.texture);
		entry.texture = nullptr;
	}
}

CachedMedia* MediaCache::LoadImage(const std::string& path, SDL_Renderer* renderer){
	// GIF always goes through the animation path — even single-frame GIFs
	// benefit from the consistent code path, and real animations would
	// otherwise silently render only the first frame.
	if(LowerExt(path) == ".gif"){
		return LoadAnimation(path, renderer);
	}

	auto* existing = Get(path);
	if(existing && existing->texture) return existing;

	SDL_Surface* surface = IMG_Load(path.c_str());
	if(!surface){
		fprintf(stderr, "MediaCache: cannot load '%s': %s\n", path.c_str(), IMG_GetError());
		return nullptr;
	}

	CachedMedia entry;
	entry.path = path;
	entry.width = surface->w;
	entry.height = surface->h;
	entry.sizeBytes = (size_t)surface->w * surface->h * surface->format->BytesPerPixel;
	entry.texture = SDL_CreateTextureFromSurface(renderer, surface);
	entry.isImage = true;
	SDL_FreeSurface(surface);

	if(!entry.texture) return nullptr;

	cache[path] = entry;
	return &cache[path];
}

CachedMedia* MediaCache::LoadAnimation(const std::string& path, SDL_Renderer* renderer){
	auto* existing = Get(path);
	if(existing && (existing->texture || !existing->frames.empty())) return existing;

	// IMG_LoadAnimation is SDL_image 2.6+. We pin 2.8.4 in setup-mingw-deps
	// so this call should always succeed; if somebody ever builds against an
	// older SDL_image the .gif falls through to the static path below.
	IMG_Animation* anim = IMG_LoadAnimation(path.c_str());
	if(!anim || anim->count <= 0){
		if(anim) IMG_FreeAnimation(anim);
		// Fallback: treat as a static image (first frame only).
		SDL_Surface* surface = IMG_Load(path.c_str());
		if(!surface){
			fprintf(stderr, "MediaCache: cannot load '%s': %s\n", path.c_str(), IMG_GetError());
			return nullptr;
		}
		CachedMedia entry;
		entry.path = path;
		entry.width = surface->w;
		entry.height = surface->h;
		entry.sizeBytes = (size_t)surface->w * surface->h * surface->format->BytesPerPixel;
		entry.texture = SDL_CreateTextureFromSurface(renderer, surface);
		entry.isImage = true;
		SDL_FreeSurface(surface);
		if(!entry.texture) return nullptr;
		cache[path] = entry;
		return &cache[path];
	}

	CachedMedia entry;
	entry.path = path;
	entry.width = anim->w;
	entry.height = anim->h;
	entry.isImage = true;
	entry.isAnimation = (anim->count > 1);

	entry.frames.reserve(anim->count);
	entry.frameDelaysMs.reserve(anim->count);
	for(int i = 0; i < anim->count; i++){
		SDL_Surface* s = anim->frames[i];
		if(!s) continue;
		SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
		if(!t) continue;
		entry.frames.push_back(t);
		int delay = anim->delays[i];
		// SDL_image reports 0 for frames with no delay; give them a sane
		// default (10 fps) so the animation actually advances.
		if(delay <= 0) delay = 100;
		entry.frameDelaysMs.push_back(delay);
		entry.sizeBytes += (size_t)s->w * s->h * s->format->BytesPerPixel;
	}
	IMG_FreeAnimation(anim);

	if(entry.frames.empty()) return nullptr;

	// `texture` aliases frame 0 so legacy consumers that only know about
	// `texture` still see something valid (the first frame).
	entry.texture = entry.frames.front();

	// Single-frame GIFs collapse to the static path at draw time — we still
	// store them in `frames` for uniformity but flip isAnimation off.
	if(entry.frames.size() == 1) entry.isAnimation = false;

	cache[path] = std::move(entry);
	return &cache[path];
}

void MediaCache::Clear(){
	for(auto& [path, entry] : cache){
		DestroyTextures(entry);
	}
	cache.clear();
}

void MediaCache::Evict(const std::vector<std::string>& keepPaths){
	std::vector<std::string> toRemove;
	for(auto& [path, entry] : cache){
		bool keep = false;
		for(auto& kp : keepPaths){
			if(kp == path){ keep = true; break; }
		}
		if(!keep) toRemove.push_back(path);
	}
	for(auto& path : toRemove){
		DestroyTextures(cache[path]);
		cache.erase(path);
	}
}

size_t MediaCache::GetTotalSize() const{
	size_t total = 0;
	for(auto& [_, entry] : cache) total += entry.sizeBytes;
	return total;
}

int MediaCache::GetFileCount() const{
	return (int)cache.size();
}

} // namespace SatyrAV
