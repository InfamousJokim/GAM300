#pragma once

#include "Core.h"
#include <string>
#include <memory>

namespace Boom {

// Forward declare non-nested implementation to avoid nested-class lookup issues
struct VideoPlayerImpl;

// Lightweight runtime video player API. Implementation may be a stub when
// FFmpeg/FMOD are not available. Use `BOOM_ENABLE_FFMPEG` to enable full
// implementation (requires linking FFmpeg + FMOD and including headers).

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    // Open a file path. Returns true if opened (or stub accepted).
    bool Open(const std::string& path);

    void Play();
    void Pause();
    void Stop();
    void Close();

    // Upload decoded frame to GPU / present. Must be called on main thread.
    void Update();

    // Returns a texture handle suitable for ImGui::Image (backend dependent)
    void* GetTextureHandle() const;

    double GetDuration() const;
    double GetPosition() const;

    bool IsPlaying() const;

private:
    // Use an out-of-line Impl type to avoid nested type lookup issues in some build setups
    VideoPlayerImpl* m_Impl{nullptr};
};

using VideoPlayerPtr = std::unique_ptr<VideoPlayer>;

} // namespace Boom
