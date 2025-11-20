#pragma once
#include "Core.h"
#include "Graphics/Buffers/Frame.h"
#include "Shaders/PBR.h"
#include "Shaders/Final.h"
#include "Shaders/SkyMap.h"
#include "Shaders/Skybox.h"
#include "Shaders/Bloom.h"
#include "Shaders/Shadow.h"
#include "GlobalConstants.h"

#include <memory>
#include <string>
#include <utility>

namespace Boom {

    struct GraphicsRenderer {
    public:
        GraphicsRenderer() = delete;

        BOOM_INLINE GraphicsRenderer(int32_t w, int32_t h)
        {
            // --- GL state that should be persistent for this context ---
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS); // smooth skybox edges

            // --- GLEW init ---
            glewExperimental = GL_TRUE;
            GLenum err = glewInit();
#ifdef BOOM_ENABLE_LOG
            if (GLEW_OK != err) {
                BOOM_FATAL("Unable to initialize GLEW - error: {} abort program.\n", GetGlewString(err, true));
                std::exit(EXIT_FAILURE);
            }
            if (GLEW_VERSION_4_5) {
                BOOM_INFO("Using glew version: {}", GetGlewString(GLEW_VERSION));
            }
            else {
                BOOM_WARN("Warning: The driver may lack full compatibility with OpenGL 4.5, potentially limiting access to advanced features.");
            }
            PrintSpecs();
#else
            (void)err;
#endif

            // --- Shaders / passes ---
            skyMapShader = std::make_unique<SkyMapShader>("skymap.glsl");
            skyBoxShader = std::make_unique<SkyboxShader>("skybox.glsl");
            finalShader = std::make_unique<FinalShader>("final.glsl", w, h);
            pbrShader = std::make_unique<PBRShader>("pbr.glsl");
            bloom = std::make_unique<BloomShader>("bloom.glsl", w, h);
            shadowShader = std::make_unique<ShadowShader>("shadow.glsl");

            // --- Framebuffers ---
            frame = std::make_unique<FrameBuffer>(w, h, /*lowPoly=*/false);
            lowPolyFrame = std::make_unique<FrameBuffer>(w, h, /*lowPoly=*/true);

            // --- Meshes ---
            skyboxMesh = CreateSkyboxMesh();

            // --- Internal bookkeeping ---
            m_Width = w;
            m_Height = h;
            m_AspectOverride = -1.0f;
            m_TouchViewport = true;     // default for non-Imgui presentation
        }

        BOOM_INLINE ~GraphicsRenderer() {}

    public: // ----------------------- Lights -----------------------
        // The PBR shader will ignore lights above MAX_LIGHTS (in-shader define)
        template <class TYPE>
        BOOM_INLINE void SetLight(TYPE const& light, Transform3D const& transform, uint32_t index) {
            pbrShader->SetLight<TYPE>(light, transform, index);
        }
        BOOM_INLINE void SetSpotLightCount(int32_t count) { pbrShader->SetSpotLightCount(count); }
        BOOM_INLINE void SetPointLightCount(int32_t count) { pbrShader->SetPointLightCount(count); }
        BOOM_INLINE void SetDirectionalLightCount(int32_t count) { pbrShader->SetDirectionalLightCount(count); }

        BOOM_INLINE void DrawShadow(Model3D& model, Transform3D& transform, std::vector<glm::mat4>& joints) {
            if (!joints.empty()) shadowShader->SetJoints(joints);
            //glCullFace(GL_FRONT);
            shadowShader->Draw(model, transform);
            //glCullFace(GL_BACK);
        }
        BOOM_INLINE void BeginShadowPass(const glm::vec3& LightDir)
        {
            // prepare projection and view mtx
            static auto proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 10.0f);
            auto view = glm::lookAt(LightDir, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            // compute light space
            auto lightSpaceMtx = proj * view;

            // set pbr shader light space mtx and depth map
            pbrShader->Use();
            pbrShader->SetLightSpaceMatrix(lightSpaceMtx);

            // begin depth rendering
            shadowShader->BeginFrame(lightSpaceMtx);
        }
        BOOM_INLINE void EndShadowPass()
        {
            shadowShader->EndFrame();
        }

    public: // ----------------------- Skybox -----------------------
        BOOM_INLINE void InitSkybox(Skybox& sky, Texture const& tex, int32_t size) {
            sky.cubeMap = skyMapShader->Generate(tex, skyboxMesh, size);
        }
        BOOM_INLINE void DrawSkybox(Skybox const& sky, Transform3D const& transform) {
            skyBoxShader->Draw(skyboxMesh, sky.cubeMap, transform);
            pbrShader->SetEnvMaps(0, 0, 0, shadowShader->GetDepthMap());
        }

    public: // -------------------- Animator (skinning) -------------
        BOOM_INLINE void SetJoints(std::vector<glm::mat4>& transforms) {
            pbrShader->SetJoints(transforms);
        }

    public: // -------- Camera / draw (uses aspect override if set) --------
        BOOM_INLINE void SetCamera(Camera3D& cam, Transform3D const& transform) {
            const float aspect =
                (m_AspectOverride > 0.0f) ? m_AspectOverride
                : frame->Ratio();
            pbrShader->SetCamera(cam, transform, aspect);
            skyBoxShader->SetCamera(cam, transform, aspect);
            pbrShader->Use();
        }

        BOOM_INLINE void Draw(Mesh3D const& mesh, Transform3D const& transform) {
            pbrShader->Draw(mesh, transform);
        }

        BOOM_INLINE void Draw(Model3D const& model, Transform3D const& transform, PbrMaterial const& material = {}) {
            if (isDrawDebugMode) {
                pbrShader->DrawDebug(model, transform, material.albedo, showNormalTexture);
            }
            else {
                pbrShader->Draw(model, transform, material, showNormalTexture);
            }
        }

        BOOM_INLINE float Aspect() const { return frame->Ratio(); } // kept for backward compatibility

    public: // ---------------------- Frame lifecycle ----------------------
        BOOM_INLINE void NewFrame() {
            pbrShader->showDither = showLowPoly;
            if (showLowPoly) {
                lowPolyFrame->Begin();
            }
            else {
                frame->Begin();
            }
            pbrShader->Use();
        }

        BOOM_INLINE void EndFrame() {
            pbrShader->UnUse();
            if (showLowPoly) {
                lowPolyFrame->End();
                bloom->Compute(lowPolyFrame->GetBrightnessMap(), 10);
            }
            else {
                frame->End();
                bloom->Compute(frame->GetBrightnessMap(), 10);
            }
        }

        // NOTE: When embedding in ImGui, prefer calling Final pass yourself via ImGui::Image with the texture returned by GetFrame().
        // If you still call ShowFrame() while embedded, set SetTouchViewport(false) so we don't stomp ImGui's viewport.
        BOOM_INLINE void ShowFrame() {
            if (showLowPoly) {
                if (m_TouchViewport) glViewport(0, 0, lowPolyFrame->GetWidth(), lowPolyFrame->GetHeight());
                finalShader->Show(lowPolyFrame->GetTexture(), bloom->GetMap(), !isDrawDebugMode);
            }
            else {
                if (m_TouchViewport) glViewport(0, 0, frame->GetWidth(), frame->GetHeight());
                finalShader->Show(frame->GetTexture(), bloom->GetMap(), !isDrawDebugMode);
            }
        }

        BOOM_INLINE void ShowFrame(bool useFBO) {
            
            if (showLowPoly) {
                if (m_TouchViewport) glViewport(0, 0, lowPolyFrame->GetWidth(), lowPolyFrame->GetHeight());
                finalShader->Render(lowPolyFrame->GetTexture(), bloom->GetMap(), useFBO, enabledBloom);
            }
            else {
                if (m_TouchViewport) glViewport(0, 0, frame->GetWidth(), frame->GetHeight());
                //shadowShader->GetDepthMap() //frame->GetTexture()
                finalShader->Render(frame->GetTexture(), bloom->GetMap(), useFBO, enabledBloom); // toggle bloom inside final if needed
            }
        }

    public: // ---------------------- Utilities / helpers -------------------
        BOOM_INLINE void Resize(int32_t w, int32_t h) {
            if (w <= 0 || h <= 0) return;

            m_Width = w;
            m_Height = h;

            frame->Resize(w, h);
            lowPolyFrame->Resize(w, h);

            // If FinalShader / BloomShader expose resizing, call them.
            if (finalShader) finalShader->Resize(w, h);
            if (bloom)       bloom->Resize(w, h);
        }

        BOOM_INLINE uint32_t GetFrame() const {
            return finalShader->GetMap();
        }

        BOOM_INLINE float& DitherThreshold() { return pbrShader->ditherThreshold; }

        BOOM_INLINE float AspectRatio() const { return frame->Ratio(); }

        // --- Editor-facing toggles for embedded rendering ---
        BOOM_INLINE void SetAspectOverride(float aspect) { m_AspectOverride = aspect; } // <= set panel aspect here
        BOOM_INLINE void ClearAspectOverride() { m_AspectOverride = -1.0f; }
        BOOM_INLINE void SetTouchViewport(bool on) { m_TouchViewport = on; }
        BOOM_INLINE std::pair<int32_t, int32_t> BackbufferSize() const { return { m_Width, m_Height }; }

    private:
        BOOM_INLINE void PrintSpecs() {
            GLint ver[2];
            glGetIntegerv(GL_MAJOR_VERSION, &ver[0]);
            glGetIntegerv(GL_MINOR_VERSION, &ver[1]);
            BOOM_INFO("GL Version: {}.{}", ver[0], ver[1]);

            GLboolean isDB;
            glGetBooleanv(GL_DOUBLEBUFFER, &isDB);
            if (isDB) BOOM_INFO("Current OpenGL Context is double-buffered");
            else      BOOM_INFO("Current OpenGL Context is not double-buffered");

            GLint output;
            glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &output);
            BOOM_INFO("Maximum Vertex Count: {}", output);
            glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &output);
            BOOM_INFO("Maximum Indices Count: {}", output);
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

        BOOM_INLINE std::string GetGlewString(GLenum name, bool isError = false) {
            if (isError) {
                return reinterpret_cast<char const*>(glewGetErrorString(name));
            }
            else {
                char const* ret{ reinterpret_cast<char const*>(glewGetString(name)) };
                return ret ? ret : "Unknown glewGetString(" + std::to_string(name) + ')';
            }
        }

    private: // ---------------------- Owned resources ----------------------
        std::unique_ptr<SkyMapShader>  skyMapShader;
        std::unique_ptr<SkyboxShader>  skyBoxShader;
        std::unique_ptr<FinalShader>   finalShader;
        std::unique_ptr<PBRShader>     pbrShader;
        std::unique_ptr<ShadowShader>  shadowShader;
        std::unique_ptr<FrameBuffer>   frame;
        std::unique_ptr<FrameBuffer>   lowPolyFrame;
        std::unique_ptr<BloomShader>   bloom;
        SkyboxMesh                     skyboxMesh;

    private: // ---------------------- Internal state -----------------------
        int32_t m_Width{};
        int32_t m_Height{};
        float   m_AspectOverride{ -1.0f }; // < 0.0f => use FBO ratio
        bool    m_TouchViewport{ true };    // false when embedded in ImGui

    public:  // ---------------------- ImGui-exposed toggles ----------------
        bool isDrawDebugMode{};
        bool showLowPoly{};
        bool showNormalTexture{};
        bool enabledBloom{};
    };

} // namespace Boom
