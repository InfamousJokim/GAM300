#pragma once
#include "Core.h"
#include "Graphics/Buffers/Frame.h"
#include "Shaders/PBR.h"
#include "Shaders/Final.h"
#include "Shaders/SkyMap.h"
#include "Shaders/Skybox.h"
#include "Shaders/Bloom.h"
#include "GlobalConstants.h"
#include "Shaders/Shadow.h"

namespace Boom {
	struct GraphicsRenderer {
	public:
		GraphicsRenderer() = delete;
		BOOM_INLINE GraphicsRenderer(int32_t w, int32_t h)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS); //smooth skybox

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

			skyMapShader = std::make_unique<SkyMapShader>("skymap.glsl");
			skyBoxShader = std::make_unique<SkyboxShader>("skybox.glsl");
			finalShader = std::make_unique<FinalShader>("final.glsl", w, h);
			pbrShader = std::make_unique<PBRShader>("pbr.glsl");
			bloom = std::make_unique<BloomShader>("bloom.glsl", w, h);	
			frame = std::make_unique<FrameBuffer>(w, h, false);
			lowPolyFrame = std::make_unique<FrameBuffer>(w, h, true);

			skyboxMesh = CreateSkyboxMesh();
		}
		BOOM_INLINE ~GraphicsRenderer() {}

	public: //lights - the pbr shader will ignore any lights set above the maximum allowed by MAX_LIGHTS defined within shader
		template <class TYPE>
		BOOM_INLINE void SetLight(TYPE const& light, Transform3D const& transform, uint32_t index) {
			pbrShader->SetLight<TYPE>(light, transform, index);
		}
		BOOM_INLINE void SetSpotLightCount(int32_t count) {
			pbrShader->SetSpotLightCount(count);
		}
		BOOM_INLINE void SetPointLightCount(int32_t count) {
			pbrShader->SetPointLightCount(count);
		}
		BOOM_INLINE void SetDirectionalLightCount(int32_t count) {
			pbrShader->SetDirectionalLightCount(count);
		}

	public: //skybox
		BOOM_INLINE void InitSkybox(Skybox& sky, Texture const& tex, int32_t size) {
			sky.cubeMap = skyMapShader->Generate(tex, skyboxMesh, size);
		}
		BOOM_INLINE void DrawSkybox(Skybox const& sky, Transform3D const& transform) {
			skyBoxShader->Draw(skyboxMesh, sky.cubeMap, transform);
		}
	public: //animator
		BOOM_INLINE void SetJoints(std::vector<glm::mat4>& transforms)
		{
			pbrShader->SetJoints(transforms);
		}
	public: //shader uniforms and draw call
		BOOM_INLINE void SetCamera(Camera3D& cam, Transform3D const& transform) {
			float aspect{ frame->Ratio() };
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

		BOOM_INLINE float Aspect() const { return frame->Ratio(); }

	public: //helper functions
		BOOM_INLINE void Resize(int32_t w, int32_t h) {
			frame->Resize(w, h);
			lowPolyFrame->Resize(w, h);
		}
		BOOM_INLINE uint32_t GetFrame() {
			return finalShader->GetMap();
		}

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
		BOOM_INLINE void ShowFrame() {
			if (showLowPoly) {
				glViewport(0, 0, lowPolyFrame->GetWidth(), lowPolyFrame->GetHeight());
				finalShader->Show(lowPolyFrame->GetTexture(), bloom->GetMap(), !isDrawDebugMode);
			}
			else {
				glViewport(0, 0, frame->GetWidth(), frame->GetHeight());
				finalShader->Show(frame->GetTexture(), bloom->GetMap(), !isDrawDebugMode);
			}
		}

		BOOM_INLINE void ShowFrame(bool useFBO) {
			if (showLowPoly) {
				glViewport(0, 0, lowPolyFrame->GetWidth(), lowPolyFrame->GetHeight());
				finalShader->Render(lowPolyFrame->GetTexture(), bloom->GetMap(), useFBO, false);
			}
			else {
				glViewport(0, 0, frame->GetWidth(), frame->GetHeight());
				finalShader->Render(frame->GetTexture(), bloom->GetMap(), useFBO, false); //enable/disable bloom here
			}
		}

		BOOM_INLINE float& DitherThreshold() {
			return pbrShader->ditherThreshold;
		}
	private:
		BOOM_INLINE void PrintSpecs() {
			//?? missing enum?
			//BOOM_INFO("GPU Vendor: {}", GetGlewString(GL_VENDOR));
			//BOOM_INFO("GPU Renderer: {}", GetGlewString(GL_RENDERER));
			//BOOM_INFO("GPU Version: {}", GetGlewString(GL_VERSION));
			//BOOM_INFO("GPU Shader Version: {}", GetGlewString(GL_SHADING_LANGUAGE_VERSION));

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
		std::string GetGlewString(GLenum name, bool isError = false) {
			if (isError)
				return reinterpret_cast<char const*>(glewGetErrorString(name));
			else {
				char const* ret{ reinterpret_cast<char const*>(glewGetString(name)) };
				return ret ? ret : "Unknown glewGetString(" + std::to_string(name) + ')';
			}
		}
	private:
		std::unique_ptr<SkyMapShader> skyMapShader;
		std::unique_ptr<SkyboxShader> skyBoxShader;
		std::unique_ptr<FinalShader> finalShader;
		std::unique_ptr<PBRShader> pbrShader;
		std::unique_ptr<FrameBuffer> frame;
		std::unique_ptr<FrameBuffer> lowPolyFrame;
		std::unique_ptr<BloomShader> bloom;
		SkyboxMesh skyboxMesh;

	public: //imgui public references
		bool isDrawDebugMode{};
		bool showLowPoly{true};
		bool showNormalTexture{};
	};


}
