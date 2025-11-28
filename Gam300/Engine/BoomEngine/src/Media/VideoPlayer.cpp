#include "Core.h"
#include "Media/VideoPlayer.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>

namespace Boom {

struct VideoPlayerImpl {
    VideoPlayerImpl() {}
    ~VideoPlayerImpl() {}

    std::atomic<bool> playing{false};
    double duration{0.0};
    double position{0.0};
    uintptr_t texHandle{0};
};

} // namespace Boom

// Use fully-qualified definitions to avoid lookup issues
Boom::VideoPlayer::VideoPlayer()
    : m_Impl(new Boom::VideoPlayerImpl())
{
}

Boom::VideoPlayer::~VideoPlayer()
{
    if (m_Impl) {
        Close();
        delete m_Impl;
        m_Impl = nullptr;
    }
}

bool Boom::VideoPlayer::Open(const std::string& path)
{
    if (!m_Impl) return false;
    std::string lower = path;
    for (auto &c : lower) c = (char)tolower(c);
    if (lower.find(".mp4") == std::string::npos && lower.find(".mov") == std::string::npos && lower.find(".mkv") == std::string::npos && lower.find(".webm") == std::string::npos) {
        BOOM_WARN("VideoPlayer: unsupported extension for '%s', stub will still accept it.", path.c_str());
    }
    m_Impl->opened = true;
    m_Impl->playing = false;
    m_Impl->duration = 0.0;
    m_Impl->position = 0.0;
    m_Impl->texHandle = 0;
    BOOM_INFO("VideoPlayer: Opened (stub) '%s'", path.c_str());
    return true;
}

void Boom::VideoPlayer::Play()
{
    if (!m_Impl) return;
    if (!m_Impl->opened) return;
    m_Impl->playing = true;
}

void Boom::VideoPlayer::Pause()
{
    if (!m_Impl) return;
    m_Impl->playing = false;
}

void Boom::VideoPlayer::Stop()
{
    if (!m_Impl) return;
    m_Impl->playing = false;
    m_Impl->position = 0.0;
}

void Boom::VideoPlayer::Close()
{
    if (!m_Impl) return;
    m_Impl->playing = false;
    m_Impl->opened = false;
    m_Impl->position = 0.0;
    m_Impl->texHandle = 0;
}

void Boom::VideoPlayer::Update()
{
    if (!m_Impl) return;
    if (!m_Impl->opened) return;
    if (m_Impl->playing) {
        m_Impl->position += 1.0 / 60.0; // advance approx. 60fps in stub
    }
}

void* Boom::VideoPlayer::GetTextureHandle() const
{
    if (!m_Impl) return nullptr;
    return reinterpret_cast<void*>((intptr_t)m_Impl->texHandle);
}

double Boom::VideoPlayer::GetDuration() const { return m_Impl ? m_Impl->duration : 0.0; }

double Boom::VideoPlayer::GetPosition() const { return m_Impl ? m_Impl->position : 0.0; }

bool Boom::VideoPlayer::IsPlaying() const { return m_Impl ? m_Impl->playing.load() : false; }
