// BoomEngine.cpp : Defines the functions for the static library.
//
//Build BoomEngine before running debuger or building Gam300
#include "BoomEngine.h"
#include "Core.h"
#include "framework.h"
#include "common/Events.h"

#include "AppWindow.h"
#include "Graphics/Renderer.h"
#include "Graphics/Shaders/Color.h"
#include "Graphics/Buffers/Frame.h"
#include "ECS/ECS.hpp"
#include <iostream>
#include "Audio/Audio.hpp"
using namespace std;
using namespace Boom;
#define LOG_EVENT(MSG) std::cout << "[Event] " << MSG << '\n'

void MyEngineClass::whatup() {

	// Example of using the logger
    #ifdef BOOM_ENABLE_LOG
        BOOM_INFO("Logger is active!");
        BOOM_DEBUG("DEBUG TEST");
        BOOM_WARN("WARNING TEST");
    #else
        std::cout << "BOOM_ENABLE_LOG not defined." << std::endl;
    #endif
        static EventDispatcher dispatcher;          // one per library instance

     
        struct WindowResizeEvent
        {
            int w, h;
           
            WindowResizeEvent(int width, int height) : w(width), h(height) {}
        };
        struct QuitEvent {};

        uint32_t resizeID = 42;
        dispatcher.AttachCallback<WindowResizeEvent>(
            [](const WindowResizeEvent& e)
            { LOG_EVENT("Resize -> " << e.w << 'x' << e.h); },
            resizeID);

        dispatcher.AttachCallback<QuitEvent>(
            [](const QuitEvent&) { LOG_EVENT("Quit requested"); },
            /*listenerID=*/7);

      
        dispatcher.PostEvent<WindowResizeEvent>(1280, 720);
        dispatcher.PostEvent<QuitEvent>();
        dispatcher.PostTask([] { LOG_EVENT("Frame task ran"); });

    
        dispatcher.PollEvents();

        
        dispatcher.DetachCallback<WindowResizeEvent>(resizeID);
        dispatcher.PostEvent<WindowResizeEvent>(1920, 1080);
        dispatcher.PollEvents();

        std::cout << "Dispatcher smoketest finished inside MyEngineClass::whatup().\n";

        {
            auto& se = SoundEngine::Instance();
            if (se.Init()) {
                //se.PlaySound("startup", "Resources/Audio/vboom.wav", false);

                // Simple loop to update FMOD for 2 seconds
                auto start = std::chrono::high_resolution_clock::now();
                while (std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start).count() < 2.0f) {
                    se.Update();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }

            SoundEngine::Instance().Shutdown();
        }
}

std::unique_ptr<Application>MyEngineClass::CreateApp() {
    return std::make_unique<Application>();
}

namespace {
    std::string GetGlewString(GLenum name, bool isError = false) {
        if (isError)
            return reinterpret_cast<char const*>(glewGetErrorString(name));
        else {
            char const* ret{ reinterpret_cast<char const*>(glewGetString(name)) };
            return ret ? ret : "Unknown glewGetString(" + std::to_string(name) + ')';
        }
    }
    void PrintSpecs() {
        BOOM_INFO("GPU Vendor: {}", GetGlewString(GL_VENDOR));
        BOOM_INFO("GPU Renderer: {}", GetGlewString(GL_RENDERER));
        BOOM_INFO("GPU Version: {}", GetGlewString(GL_VERSION));
        BOOM_INFO("GPU Shader Version: {}", GetGlewString(GL_SHADING_LANGUAGE_VERSION));

        GLint ver[2];
        glGetIntegerv(GL_MAJOR_VERSION, &ver[0]);
        glGetIntegerv(GL_MINOR_VERSION, &ver[1]);
        BOOM_INFO("GL Version: {}.{}", ver[0], ver[1]);

        GLboolean isDB;
        glGetBooleanv(GL_DOUBLEBUFFER, &isDB);
        if (isDB)
            BOOM_INFO("Current OpenGL Context is double-buffered");
        else
            BOOM_INFO("Current OpenGL Context is not double-buffered");

        GLint output;
        glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &output);
        BOOM_INFO("Maximum Vertex Count: {}", output);
        glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &output);
        BOOM_INFO("Maximum Indicies Count: {}", output);
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &output);
        BOOM_INFO("Maximum texture size: {}", output);

        GLint viewport[2];
        glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport);
        BOOM_INFO("Maximum Viewport Dimensions: {} x {}", viewport[0], viewport[1]);

        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &output);
        BOOM_INFO("Maximum generic vertex attributes: {}", output);
        glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &output);
        BOOM_INFO("Maximum vertex buffer bindings: {}\n", output);
    }
}
