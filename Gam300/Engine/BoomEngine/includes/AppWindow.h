#pragma once
#include "Core.h" //used for variables and error handling
#include "common/Events.h"
#include "GlobalConstants.h"
#include "Input/InputHandler.h"
#include "Graphics/Shaders/LoadingShader.h"

namespace Boom {
	struct AppWindow {
	public:
		AppWindow() = delete;
		AppWindow(AppWindow const&) = delete;

		//example for initializing with reference:
		//std::unique_ptr<AppWindow> w = std::make_unique<AppWindow>(&Dispatcher, 1900, 800, "BOOM");
		BOOM_INLINE AppWindow(EventDispatcher* disp, int32_t w, int32_t h, char const* windowTitle)
			: width{ w }
			, height{ h }
			, refreshRate{ 144 }
			, isFullscreen{ false }
			, monitorPtr{}
			, modePtr{}
			, windowPtr{}
			, dispatcher{ disp }
		{
			if (!glfwInit()) {
				BOOM_FATAL("AppWindow::Init() - glfwInit() failed.");
				std::exit(EXIT_FAILURE);
			}
			//glfwWindowHint(GLFW_OPENGL_CORE_PROFILE, GLFW_TRUE);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
			glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

			monitorPtr = glfwGetPrimaryMonitor();
			modePtr = glfwGetVideoMode(monitorPtr);
			glfwWindowHint(GLFW_REFRESH_RATE, modePtr->refreshRate);
			glfwWindowHint(GLFW_GREEN_BITS, modePtr->greenBits);
			glfwWindowHint(GLFW_BLUE_BITS, modePtr->blueBits);
			glfwWindowHint(GLFW_RED_BITS, modePtr->redBits);
			glfwWindowHint(GLFW_SAMPLES, 4);

			glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
			glfwWindowHint(GLFW_DEPTH_BITS, 24);

			glfwWindowHint(GLFW_MAXIMIZED, GL_FALSE);
			glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

			windowPtr = std::shared_ptr<GLFWwindow>(
				glfwCreateWindow(width, height, windowTitle, NULL, NULL),
				glfwDestroyWindow
			);
			if (windowPtr.get() == nullptr) {
				BOOM_FATAL("AppWindow::Init() - failed to init app window.");
				std::exit(EXIT_FAILURE);
			}

			// ADD THESE LINES:
			BOOM_INFO("AppWindow - Initial window size: {}x{}", width, height);
			glfwShowWindow(windowPtr.get());  // Make sure window is visible
			glfwFocusWindow(windowPtr.get()); // Bring to front

			int actualWidth, actualHeight;
			glfwGetWindowSize(windowPtr.get(), &actualWidth, &actualHeight);
			BOOM_INFO("AppWindow - Actual window size after creation: {}x{}", actualWidth, actualHeight);

			glfwMakeContextCurrent(windowPtr.get());
			GLFWwindow* current = glfwGetCurrentContext();

			if (current != windowPtr.get()) {
				BOOM_ERROR("Failed to make window context current in constructor!");
			}

			//user data
			glfwSetWindowUserPointer(windowPtr.get(), this);
			//enable a-vsync
			glfwSwapInterval(0);
			SetupCallbacks(windowPtr.get());

			//initial window color
			std::apply(glClearColor, CONSTANTS::DEFAULT_BACKGROUND_COLOR);

			glfwGetCursorPos(windowPtr.get(), &prevMousePos.x, &prevMousePos.y);

			//init
			camRegionW = width;
			camRegionH = height;
		}
		BOOM_INLINE ~AppWindow() {}

	private: //GLFW callbacks
		BOOM_INLINE static void SetupCallbacks(GLFWwindow* win) {
			glfwSetErrorCallback(AppWindow::OnError);
			glfwSetWindowMaximizeCallback(win, AppWindow::OnMaximized);
			glfwSetFramebufferSizeCallback(win, AppWindow::OnResize);
			glfwSetWindowIconifyCallback(win, AppWindow::OnIconify);
			glfwSetWindowCloseCallback(win, AppWindow::OnClose);
			glfwSetWindowFocusCallback(win, AppWindow::OnFocus);

			glfwSetScrollCallback(win, AppWindow::OnWheel);
			glfwSetMouseButtonCallback(win, AppWindow::OnMouse);
			glfwSetCursorPosCallback(win, AppWindow::OnMotion);
			glfwSetKeyCallback(win, AppWindow::OnKey);
		}
		BOOM_INLINE static void OnError(int32_t errorNo, char const* description) {
			BOOM_ERROR("[GLFW]: [{}] {}", errorNo, description);
		}
		BOOM_INLINE static void OnMaximized(GLFWwindow* win, int32_t action) {
			AppWindow* self{ GetUserData(win) };
			if (action) {
				self->isFullscreen = true;
				//GetUserData(win)->dispatcher->PostEvent<WindowMaximizeEvent>();
			}
			else {
				self->isFullscreen = false;
				//enable input + unpause system + change to non fullscreen or not + etc...
				//GetUserData(win)->dispatcher->PostEvent<WindowRestoreEvent>();
			}
		}
		BOOM_INLINE static void OnResize(GLFWwindow* win, int32_t w, int32_t h) {
			AppWindow* self{ GetUserData(win) };
			if (!self->isEditor) {
				self->dispatcher->PostEvent<WindowResizeEvent>(w, h);
				self->camRegionW = w; 
				self->camRegionH = h;
			}
		}
		BOOM_INLINE static void OnIconify(GLFWwindow* win, int32_t action) {
			(void)win;
			//AppWindow* self{ GetUserData(win) };
			if (action) {
				//disable input + pause system + etc...
				//self->dispatcher->PostEvent<WindowIconifyEvent>();
			}
			else {
				//enable input + unpause system + check fullscreen or not + etc...
				//self->dispatcher->PostEvent<WindowRestoreEvent>();
			}
		}
		BOOM_INLINE static void OnClose(GLFWwindow* win) {
			(void)win;
			//AppWindow* self{ GetUserData(win) };
			//self->dispatcher->PostEvent<WindowCloseEvent>();
			//this event struct can be implemented to contain other needed system logic upon closing window
			//GetUserData(win)->dispatcher->PostEvent<WindowCloseEvent>();
		}
		BOOM_INLINE static void OnFocus(GLFWwindow* win, int32_t focused) {
			(void)win; (void)focused;
			//implemented due to gam250 requirement of pausing the game when unfocused
			//assuming might carry over
		}

		// Scroll
		BOOM_INLINE static void OnWheel(GLFWwindow* win, double sx, double sy) {
			auto* self = GetUserData(win);
			if (!self) return;
			self->input.onScroll(sx, sy);
			// Optional: also emit your event type
			if (self->dispatcher) self->dispatcher->PostEvent<MouseWheelEvent>(sx, sy);
		}

		// Mouse buttons
		BOOM_INLINE static void OnMouse(GLFWwindow* win, int32_t button, int32_t action, int32_t mods) {
			auto* self = GetUserData(win);
			if (!self) return;
			self->input.onMouseButton(button, action, mods);

			// Optional: your event types
			if (!self->dispatcher) return;
			if (action == GLFW_PRESS)   self->dispatcher->PostEvent<MouseDownEvent>(button);
			if (action == GLFW_RELEASE) self->dispatcher->PostEvent<MouseReleaseEvent>(button);
		}

		// Mouse motion
		BOOM_INLINE static void OnMotion(GLFWwindow* win, double x, double y) {
			auto* self = GetUserData(win);
			if (!self) return;
			self->input.onCursorPos(x, y);
			// Optional: motion/drag events
			self->dispatcher->PostEvent<MouseMotionEvent>(x, y);
			if (self->input.current().Mouse.any()) {
				// per-event delta (since last motion). If you want exact deltas, track last x/y here.
				self->dispatcher->PostEvent<MouseDragEvent>(0.0, 0.0);
			}
		}

		// Keys
		BOOM_INLINE static void OnKey(GLFWwindow* win, int32_t key, int32_t sc, int32_t action, int32_t mods) {
			auto* self = GetUserData(win);
			if (!self) return;

			//if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
				//glfwSetWindowShouldClose(win, GLFW_TRUE);
				//return;
			//}

			self->input.onKey(key, sc, action, mods);

			// Optional: keep your key events
			if (!self->dispatcher) return;
			switch (action) {
			case GLFW_PRESS:   self->dispatcher->PostEvent<KeyPressEvent>(key);   break;
			case GLFW_RELEASE: self->dispatcher->PostEvent<KeyReleaseEvent>(key); break;
			case GLFW_REPEAT:  self->dispatcher->PostEvent<KeyRepeatEvent>(key);  break;
			}
		}

		//to be called explicitly to reference window from application
		//example: GetUserData(win)->isFullscreen 
		BOOM_INLINE static AppWindow* GetUserData(GLFWwindow* window) {
			return static_cast<AppWindow*>(glfwGetWindowUserPointer(window));
		}
	public:
		BOOM_INLINE void SetWindowTitle(std::string const& title) {
			glfwSetWindowTitle(windowPtr.get(), title.c_str());
		}

		[[nodiscard]] BOOM_INLINE int32_t& Width() noexcept {
			return width;
		}
		[[nodiscard]] BOOM_INLINE int32_t& Height() noexcept {
			return height;
		}
		BOOM_INLINE std::shared_ptr<GLFWwindow> Handle() const {
			return windowPtr;
		}

		BOOM_INLINE bool PollEvents() {
			input.beginFrame();
			glfwMakeContextCurrent(windowPtr.get());
			glfwPollEvents();
			dispatcher->PollEvents();
			glfwSwapBuffers(windowPtr.get());
			return !glfwWindowShouldClose(windowPtr.get());
		}
		BOOM_INLINE bool IsKey(int32_t key) const {
			return key >= 0 && key <= GLFW_KEY_LAST
				&& inputs.Keys.test(static_cast<size_t>(key));
		}
		BOOM_INLINE bool IsMouse(int32_t button) const {
			if (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST)
				return true; //inputs.Mouse.test(button);
			return false;
		}
		BOOM_INLINE int IsExit() const {
			return glfwWindowShouldClose(windowPtr.get());
		}
		
		BOOM_INLINE void SetCameraInputRegion(double x, double y, double w, double h, bool enabled) {
			camRegionX = x; camRegionY = y; camRegionW = w; camRegionH = h; camInputEnabled = enabled;
		}

		BOOM_INLINE float GetViewportX() const { return (float)camRegionX; }
		BOOM_INLINE float GetViewportY() const { return (float)camRegionY; }
		BOOM_INLINE float GetViewportW() const { return (float)camRegionW; }
		BOOM_INLINE float GetViewportH() const { return (float)camRegionH; }

		BOOM_INLINE bool IsMouseInCameraRegion(GLFWwindow* win) const {
			double mx, my; glfwGetCursorPos(win, &mx, &my); // client-area coords
			return (mx >= camRegionX && mx <= camRegionX + camRegionW &&
				my >= camRegionY && my <= camRegionY + camRegionH);
		}
		BOOM_INLINE void SetViewportKeyboardFocus(bool allow) { allowViewportKeyboard = allow; }

		// NEW: point-in-rect test (window client coords)
		BOOM_INLINE bool IsPointInCameraRect(double x, double y) const {
			return (x >= camRegionX && x <= camRegionX + camRegionW &&
				y >= camRegionY && y <= camRegionY + camRegionH);
		}

		// NEW: allow mouse even if ImGui wants it (used by bridge callbacks)
		BOOM_INLINE bool AllowCameraMouseNow(double x, double y) const {
			return camInputEnabled && IsPointInCameraRect(x, y);
		}
		BOOM_INLINE int getWidth() {
			return width;
		}
		BOOM_INLINE int getHeight() {
			return height;
		}	
		//accesssors
		BOOM_INLINE EventDispatcher* GetDispatcher() const { return dispatcher; }
		BOOM_INLINE InputSystem& GetInputSystem() { return input; }

		BOOM_INLINE static void RenderLoading(GLFWwindow* win, float percentProgress) {
			static auto loadingShader{ std::make_unique<LoadingShader>("loading.glsl") };
			int w, h;
			glfwGetFramebufferSize(win, &w, &h);
			auto proj = glm::ortho(0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);
			std::apply(glClearColor, CONSTANTS::DEFAULT_BACKGROUND_COLOR);
			glClear(GL_COLOR_BUFFER_BIT);

			// track (dark background)
			const float barY = h * 0.45f;
			const float barH = h * 0.10f;
			const float trackX = w * 0.5f;
			const float trackW = w * 0.4f;
			loadingShader->SetColor({ 0.12f, 0.12f, 0.12f, 1.f });
			loadingShader->SetTransform({trackX, barY + barH * 0.5f}, { trackW, barH }, 0.f);
			loadingShader->Show(proj);

			// fill (bright color)
			const float fillW = trackW * percentProgress;
			loadingShader->SetColor({ 0.0f, 0.7f, 1.f, 1.f });
			loadingShader->SetTransform({trackX - trackW + fillW, barY + barH * 0.5f }, { fillW, barH }, 0.f);
			loadingShader->Show(proj);

			glfwSwapBuffers(win);
			glfwPollEvents();
		}
	private:
		int32_t width{};
		int32_t height{};
		int32_t refreshRate;
		bool isFullscreen;

		//these are kept raw as they are lightweight and non-owning
		GLFWmonitor* monitorPtr;
		GLFWvidmode const* modePtr;
		std::shared_ptr<GLFWwindow> windowPtr;
		EventDispatcher* dispatcher;
		WindowInputs inputs;

	public: //usage for basic glew input to move camera in editor
		bool isRightClickDown{};   // optional legacy flags if you still want them
		bool isMiddleClickDown{};
		bool isShiftDown{};
		bool allowViewportKeyboard{ false };
		// x - strafe right/left, y - hover up/down, z - forward/back
		glm::vec3  camMoveDir{};

		// Kept for completeness; not needed if you use InputSystem mouse deltas
		glm::dvec2 prevMousePos{};

		// around x/y (x=pitch, y=yaw) in degrees
		glm::vec2  camRot{};


		float camMoveMultiplier{ 0.05f };
		// Making sure that the rotation on happens in the viewport
		double camRegionX{ 0.0 }, camRegionY{ 0.0 }, camRegionW{ 0.0 }, camRegionH{ 0.0 };
		bool   camInputEnabled{ false };
		bool isEditor{}; //needed due to imgui's weird resizing bug

		InputSystem input;
	};
}
