#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Windows <windows.h> (pulled in by <d3d11.h> via MediaPlayer) defines
// LoadImage as a macro expanding to LoadImageA/LoadImageW, which collides
// with our method name. Undefine it so the compiler sees the real symbol.
#ifdef LoadImage
#undef LoadImage
#endif

namespace SatyrAV{

struct CachedMedia{
	SDL_Texture* texture = nullptr; // frames[0] for animated GIFs; the lone tex for still images.
	int width  = 0;
	int height = 0;
	size_t sizeBytes = 0;
	std::string path;
	bool isImage = false;

	// (1.4) Animated GIF support. For still images `frames` is empty and
	// `texture` is the only tex; for animations `frames` holds every decoded
	// frame as an SDL texture and `frameDelaysMs` the matching per-frame
	// delay. `texture` is also kept pointing at `frames[0]` so every legacy
	// consumer that reads `texture` still gets a valid first frame.
	std::vector<SDL_Texture*> frames;
	std::vector<int> frameDelaysMs;
	bool isAnimation = false;
};

class MediaCache{
public:
	void PreloadFiles(const std::vector<std::string>& paths,
		const std::string& projectBase, SDL_Renderer* renderer);

	void PreloadForSceneTransition(
		const std::vector<std::string>& prevPaths,
		const std::vector<std::string>& currPaths,
		const std::vector<std::string>& nextPaths,
		const std::string& projectBase, SDL_Renderer* renderer);

	CachedMedia* Get(const std::string& path);
	CachedMedia* LoadImage(const std::string& path, SDL_Renderer* renderer);
	// (1.4) Decode an animated file (GIF) into frame textures. Falls back to
	// a single static frame if the installed SDL_image lacks IMG_LoadAnimation
	// or the file only has one frame.
	CachedMedia* LoadAnimation(const std::string& path, SDL_Renderer* renderer);

	void Clear();
	void Evict(const std::vector<std::string>& keepPaths);

	size_t GetTotalSize() const;
	int GetFileCount() const;

	std::string ResolvePath(const std::string& base, const std::string& rel);

private:
	std::unordered_map<std::string, CachedMedia> cache;

	static bool IsImageExt(const std::string& ext);
};

} // namespace SatyrAV
