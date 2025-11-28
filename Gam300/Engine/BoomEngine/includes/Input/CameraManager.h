// CameraManager.h
#pragma once


#include "AppWindow.h"          // AppWindow, SetFOV(), IsMouseInCameraRegion()

namespace Boom {
    class CameraController {
    public:
        struct Config {
            float mouseSensitivityX = 0.25f;
            float mouseSensitivityY = 0.25f;
            float multiplierStep = 0.01f;
            float minFov = CONSTANTS::MIN_FOV;
            float maxFov = CONSTANTS::MAX_FOV;
            bool  gateToViewportRect = true;
            bool  gateToRMB = true;
            bool  normalizeDiagonal = true;
            bool  clampPitch = true;
            float minPitchDeg = -89.f;
            float maxPitchDeg = 89.f;
            
        };

        explicit CameraController(AppWindow* window, Config cfg = {})
            : m_app(window), m_cfg(cfg) {
        }
        BOOM_INLINE void attachCamera(Camera3D* cam) { m_cam = cam; }
        BOOM_INLINE void update(float /*dt*/) {
            if (!m_app) return;

            auto& input = m_app->input;
            const auto& s = input.current();
            const bool middleMouse = s.Mouse.test(GLFW_MOUSE_BUTTON_MIDDLE);
            const bool mmbPress = m_app->input.mousePressed(GLFW_MOUSE_BUTTON_MIDDLE);
            const bool inRegion = !m_cfg.gateToViewportRect || m_app->IsMouseInCameraRegion(m_app->Handle().get());
            const bool rmb = !m_cfg.gateToRMB || s.Mouse.test(GLFW_MOUSE_BUTTON_RIGHT);
            const bool rmbPress = m_app->input.mousePressed(GLFW_MOUSE_BUTTON_RIGHT);
            const bool canUse = m_app->camInputEnabled && inRegion && rmb;          // look + WASD
            const bool canPan = m_app->camInputEnabled && inRegion && middleMouse;  // MMB pan

            glm::dvec2 curPos = m_app->input.cursorPos();
            if (rmbPress) {
                m_prevLookPos = curPos;                // zero the baseline the moment RMB goes down
            }
            if (mmbPress) m_prevPanPos = curPos;

           glm::vec2 md = input.mouseDeltaLast();
           if (rmbPress) md = {};
            m_prevLookPos = curPos;
            glm::vec2 mdPan = middleMouse ? glm::vec2(curPos - m_prevPanPos) : glm::vec2(0.0f);
            if (glm::length2(mdPan) < 1.0f) mdPan = {};

            // WASD movement vector (camera-local intent)
            glm::vec3 movements{
                input.axis(GLFW_KEY_A, GLFW_KEY_D),
                input.axis(GLFW_KEY_Q, GLFW_KEY_E),
                input.axis(GLFW_KEY_S, GLFW_KEY_W)
            };
            if (m_cfg.normalizeDiagonal && glm::length2(movements) > 1e-6f)
                movements = glm::normalize(movements);

            const bool  running = s.Keys.test(GLFW_KEY_LEFT_SHIFT) || s.Keys.test(GLFW_KEY_RIGHT_SHIFT);
            const float base = CONSTANTS::CAM_PAN_SPEED * m_app->camMoveMultiplier;
            const float spd = base * (running ? CONSTANTS::CAM_RUN_MULTIPLIER : 1.f);
            const float fovDeg = (m_cam ? m_cam->FOV : CONSTANTS::MIN_FOV);
            // --- Give pan priority over look ---
            if (canPan) {
                // IMPORTANT: consume the delta every frame (Unity-like feel)
                glm::vec2 pixDelta = glm::vec2(curPos - m_prevPanPos);
                m_prevPanPos = curPos; 

               
                const float H = float(m_app->getHeight());
                const float d = panFocusDist; // pick your pivot distance; tune this
                const float wpp = 2.f * d * std::tan(fovDeg * 0.5f) / H; 

                // Camera-local right/up displacement (no forward, no speed multiplier)
                glm::vec3 localMove{
                    -pixDelta.x * wpp,    // right (flip sign if it feels backwards)
                     pixDelta.y * wpp,    // up
                     0.0f
                };

                m_app->camMoveDir = localMove; // treated as per-frame displacement
            }
            else if (canUse) {
                // Look (RMB) using the same md
                m_app->camRot.y = +md.x * (m_cfg.mouseSensitivityX * CONSTANTS::CAM_PAN_SPEED);
                m_app->camRot.x = -md.y * (m_cfg.mouseSensitivityY * CONSTANTS::CAM_PAN_SPEED);
                if (m_cfg.clampPitch)
                    m_app->camRot.x = std::clamp(m_app->camRot.x, m_cfg.minPitchDeg, m_cfg.maxPitchDeg);

                // Move (WASD) in camera-local forward/right/up
                const float yaw = m_cfg.mouseSensitivityX / float(m_app->getWidth());   // deg per pixel
                const float pitch = m_cfg.mouseSensitivityY / float(m_app->getHeight());  // deg per pixel
                const float sy = std::sinf(yaw), cy = std::cosf(yaw);
                const float sp = std::sinf(pitch), cp = std::cosf(pitch);

                const glm::vec3 forward{ cp * sy, sp, cp * cy };
                const glm::vec3 worldUp{ 0,1,0 };
                const glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
                const glm::vec3 up = glm::normalize(glm::cross(right, forward));

                glm::vec3 moveWorld = right * movements.x + up * movements.y + forward * movements.z;
                if (glm::length2(moveWorld) > 1e-6f) moveWorld = glm::normalize(moveWorld);
                m_app->camMoveDir = moveWorld * spd;
            }
            else {
                // Only clear when neither panning nor using look/move
                m_app->camMoveDir = {};
                m_app->camRot = {};
            }

            // Scroll: speed multiplier and FOV
            const glm::vec2 sd = input.scrollDelta();
            if (sd.y != 0.0f && inRegion) {
                if (rmb) {
                    float next = m_app->camMoveMultiplier + sd.y * m_cfg.multiplierStep;
                    m_app->camMoveMultiplier = std::clamp(next, 0.01f, 100.0f);
                }
                else if (m_cam) {
                    float nextFov = std::clamp(m_cam->FOV - sd.y, m_cfg.minFov, m_cfg.maxFov);
                    if (nextFov != m_cam->FOV) m_cam->SetFOV(nextFov);
                }
            }
          
        }


    private:
        AppWindow* m_app = nullptr;
        Config     m_cfg{};
        Camera3D*  m_cam = nullptr;
        bool        m_prevRmb = false;        // last-frame RMB state
        glm::dvec2  m_prevLookPos = {};           // last cursor pos used for look
        glm::dvec2  m_prevPanPos{};
        float panFocusDist = 1.0f;//adjust this if the pan dist is slow
    };
}