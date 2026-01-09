#include "Core.h"
#include "Video/VideoPlayer.h"
#include <iostream>

#include <GL/glew.h>

namespace Boom {

    // Make it non-static so it matches the 'friend' declaration.
    void VideoFrameCallback(plm_t*, plm_frame_t* frame, void* user) {
        ((Video*)user)->UploadFrame(frame);
    }

    Video::Video() {}

    Video::~Video() {
        Stop();

        if (m_TextureID) {
            GLuint tex = static_cast<GLuint>(m_TextureID);
            glDeleteTextures(1, &tex);
            m_TextureID = 0;
        }

        if (m_MPEG) {
            plm_destroy(m_MPEG);
            m_MPEG = nullptr;
        }
    }

    bool Video::Load(const std::string& filepath) {
        Stop();

        if (m_MPEG) {
            plm_destroy(m_MPEG);
            m_MPEG = nullptr;
            m_Video = nullptr;
        }

        m_MPEG = plm_create_with_filename(filepath.c_str());
        if (!m_MPEG) {
            std::cerr << "[Video] Failed to load: " << filepath << "\n";
            return false;
        }

        const int width = plm_get_width(m_MPEG);
        const int height = plm_get_height(m_MPEG);

        m_Size = { width, height };

        // allocate safe RGB buffer
        m_RGB.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);

        CreateTexture(width, height);

        // callback
        plm_set_video_decode_callback(m_MPEG, VideoFrameCallback, this);

        // Disable audio (0 instead of FALSE)
        plm_set_audio_enabled(m_MPEG, 0);

        return true;
    }

    void Video::Play() {
        if (!m_MPEG) return;
        m_Playing = true;
    }

    void Video::Pause() {
        m_Playing = false;
    }

    void Video::Stop() {
        m_Playing = false;
        if (m_MPEG) {
            plm_rewind(m_MPEG);
        }
    }

    void Video::Seek(float seconds) {
        if (!m_MPEG) return;
        plm_seek(m_MPEG, seconds, 0); // 0 instead of FALSE
    }

    void Video::Update(float deltaTime) {
        if (!m_Playing || !m_MPEG) return;
        plm_decode(m_MPEG, deltaTime);
    }

    void Video::UploadFrame(plm_frame_t* frame) {
        if (!frame || !m_TextureID) return;

        const int w = frame->width;
        const int h = frame->height;

        const size_t needed = (size_t)w * (size_t)h * 3;
        if (m_RGB.size() < needed) m_RGB.resize(needed);

        plm_frame_to_rgb(frame, m_RGB.data(), w * 3); // now matches signature

        GLuint tex = (GLuint)m_TextureID;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, m_RGB.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Video::CreateTexture(int width, int height) {
        GLuint tex = static_cast<GLuint>(m_TextureID);

        if (!tex) {
            glGenTextures(1, &tex);
            m_TextureID = static_cast<uint32_t>(tex);
        }

        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGB,
            width, height, 0,
            GL_RGB, GL_UNSIGNED_BYTE, nullptr
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Video::UploadFrame(plm_frame_t* frame)
    {
        if (!frame || m_TextureID == 0) return;

        const int w = frame->width;
        const int h = frame->height;

        const size_t needed = static_cast<size_t>(w) * static_cast<size_t>(h) * 3;
        if (m_RGB.size() < needed) {
            m_RGB.resize(needed);
        }

        // ✅ This now matches pl_mpeg's API
        plm_frame_to_rgb(frame, m_RGB.data(), w * 3);

        glBindTexture(GL_TEXTURE_2D, m_TextureID);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0, 0,
            w, h,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            m_RGB.data()
        );
        glBindTexture(GL_TEXTURE_2D, 0);
    }

} // namespace Boom
