#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

// pl_mpeg (C library)
extern "C" {
#include "Video/pl_mpeg.h"   // <-- FIXED PATH
}

namespace Boom {

    class Video {
    public:
        Video();
        ~Video();

        bool Load(const std::string& filepath);

        void Play();
        void Pause();
        void Stop();
        void Seek(float seconds);

        void Update(float deltaTime);
        

        // Rendering
        uint32_t   GetTextureID() const { return m_TextureID; }   // <-- avoid GLuint in header
        glm::ivec2 GetSize() const { return m_Size; }
        bool       IsPlaying() const { return m_Playing; }

    private:
        // Allow the C callback to call UploadFrame()
        friend void VideoFrameCallback(plm_t*, plm_frame_t*, void*);

        void CreateTexture(int width, int height);
        void UploadFrame(plm_frame_t* frame);

        // pl_mpeg objects
        plm_t* m_MPEG = nullptr;
        plm_video_t* m_Video = nullptr;

        // Video data
        uint32_t   m_TextureID = 0;      // OpenGL texture id
        glm::ivec2 m_Size{ 0, 0 };
        bool       m_Playing = false;

        // CPU staging buffer for RGB upload (safe sized)
        std::vector<uint8_t> m_RGB;
    };

} // namespace Boom
