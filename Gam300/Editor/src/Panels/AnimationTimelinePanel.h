#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include "Vendors/imgui/imgui.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <GL/glew.h>
#include <entt/entity/entity.hpp>
#include "Graphics/Models/Animation.h"  // For Boom::KeyFrame

namespace Boom {
    struct AppContext;
    struct AppInterface;
    struct Model;
    struct Animator;
    struct Joint;
}

namespace EditorUI {
    class Editor;
    class CommandHistory;  // Forward declaration

    // Undo/Redo command for keyframe operations
    struct KeyframeCommand {
        enum Type { ADD, REMOVE, MOVE };

        Type type;
        std::string boneName;
        size_t keyframeIndex = 0;
        Boom::KeyFrame keyframe;  // The keyframe data
        float oldTime = 0.0f;     // For move operations
        float newTime = 0.0f;     // For move operations
    };

    // Bone pose for undo/redo
    struct BonePose {
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
    };

    /**
     * @brief Animation Timeline Editor - Unity-style animation editing with integrated 3D preview
     *
     * Features:
     * - Integrated 3D viewport for animation preview
     * - Timeline scrubber and keyframe editing
     * - Playback controls
     * - Bone track visualization
     */
    class AnimationTimelinePanel {
    public:
        explicit AnimationTimelinePanel(Editor* owner);
        ~AnimationTimelinePanel();

        void Render();

        // Bone pose manipulation (public for undo/redo commands)
        void SetBonePose(const std::string& boneName, const BonePose& pose);  // Set bone pose (for undo/redo)
        void ClearBonePose(const std::string& boneName);  // Clear bone pose override
        void ClearAllBonePoses();  // Clear all bone pose overrides

    private:
        // UI Sections
        void RenderControlBar();      // Model selection, clip selection, playback controls
        void RenderViewport();        // 3D preview viewport
        void RenderTimelineRuler();   // Horizontal time ruler with scrubber
        void RenderTrackList();       // Bone tracks and keyframes

        // Bone track helpers
        void RenderBoneTrack(const Boom::Joint& joint, float duration);

        // 3D Viewport Rendering
        void UpdateCamera();
        void HandleCameraControls();
        void HandleBonePicking();  // Mouse picking for bone selection in viewport
        void HandleGizmo(const ImVec2& viewportMin, const ImVec2& viewportSize);  // Transform gizmo for bone manipulation
        void RenderModel();
        void RenderSkeleton();
        void RenderGrid();

        // Camera orbit controls
        void ResetCamera();
        void FrameModel(); // Auto-adjust camera to fit model in view

        // Model loading (standalone mode)
        void LoadModel(const std::string& modelPath);
        void ClearModel();

        // Bone manipulation helpers
        glm::mat4 GetBoneWorldTransform(const std::string& boneName);  // Get bone's world matrix
        std::string GetParentBoneName(const std::string& boneName);    // Find parent bone name
        void ApplyManualBonePosesToTransforms(std::vector<glm::mat4>& transforms);  // Apply manual poses to skinning transforms

        // Keyframe recording
        Boom::KeyFrame CaptureCurrentBoneTransform(const std::string& boneName);  // Capture bone's current pose

    private:
        Editor* m_Owner = nullptr;
        Boom::AppInterface* m_App = nullptr;
        Boom::AppContext* m_Ctx = nullptr;

        // Loaded model (from selected entity OR standalone)
        std::shared_ptr<Boom::Model> m_Model;
        std::shared_ptr<Boom::Animator> m_Animator;  // Our independent cloned animator
        std::shared_ptr<Boom::Animator> m_SourceAnimator;  // The original animator we cloned from (for change detection)
        entt::entity m_SourceEntityID = entt::null;  // Track which entity we cloned from
        bool m_HasModel = false;
        bool m_StandaloneMode = false; // true if model loaded directly (not from entity)
        std::string m_LoadedModelPath;

        // Viewport framebuffer
        GLuint m_FramebufferID = 0;
        GLuint m_TextureID = 0;
        GLuint m_DepthBufferID = 0;
        ImVec2 m_ViewportSize = { 800.0f, 400.0f };

        // Orbit camera
        glm::vec3 m_CameraPosition = glm::vec3(0.0f, 1.5f, 3.0f);
        glm::vec3 m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);
        float m_CameraDistance = 3.0f;
        float m_CameraYaw = 0.0f;      // Horizontal rotation
        float m_CameraPitch = 0.0f;    // Vertical rotation
        bool m_IsOrbitingCamera = false;
        ImVec2 m_LastMousePos = { 0.0f, 0.0f };

        // Visualization settings
        bool m_ShowSkeleton = true;
        bool m_ShowGrid = true;
        bool m_ShowWireframe = false;
        float m_ModelScale = 1.0f;  // Preview scale adjustment

        // Timeline state
        float m_CurrentTime = 0.0f;
        float m_TimelineZoom = 1.0f;
        bool m_IsPlaying = false;
        bool m_Loop = true;  // Loop animation playback
        float m_PlaybackSpeed = 1.0f;  // Animation playback speed multiplier
        int m_SelectedClipIndex = -1;  // Currently selected animation clip (-1 = none)
        float m_LastFrameTime = 0.0f;  // For delta time calculation
        bool m_IsDraggingTimeline = false;  // Is user scrubbing the timeline?

        // Selected bone (for keyframe editing later)
        std::string m_SelectedBoneName;

        // Bone picking state (viewport 3D interaction)
        std::string m_HoveredBoneNameViewport;  // Bone currently hovered in 3D viewport
        ImVec2 m_ViewportMousePos = { 0.0f, 0.0f };  // Mouse position relative to viewport

        // Transform gizmo state
        int m_GizmoOperation = 7;  // ImGuizmo::TRANSLATE (7 = translate)
        int m_GizmoMode = 1;       // ImGuizmo::WORLD (1 = world space, 0 = local space)
        bool m_GizmoWasUsing = false;  // Track if gizmo was being used last frame
        bool m_UseSnap = false;    // Snap to grid
        float m_SnapValues[3] = { 0.1f, 0.1f, 0.1f };  // Snap grid size
        bool m_RotationOnlyMode = true;  // Rotation-only mode (prevents translation warping)

        // Manual bone pose overrides (for gizmo manipulation)
        std::map<std::string, BonePose> m_ManualBonePoses;  // Overrides animation data
        bool m_HasManualPoses = false;  // Flag to know if we need to apply overrides

        // Bone manipulation state (for undo/redo)
        std::string m_BoneBeingManipulated;  // Bone name currently being manipulated
        BonePose m_BonePoseBeforeManipulation;  // Pose before gizmo manipulation started
        bool m_HasPoseBeforeManipulation = false;  // Flag to track if we captured the before state

        // Keyframe interaction state
        bool m_IsDraggingKeyframe = false;
        std::string m_DraggedBoneName;
        size_t m_DraggedKeyframeIndex = 0;
        int m_HoveredKeyframeIndex = -1;  // -1 = no hover
        std::string m_HoveredBoneName;

        // Undo/Redo system
        std::vector<KeyframeCommand> m_UndoStack;
        std::vector<KeyframeCommand> m_RedoStack;
        const size_t MAX_UNDO_HISTORY = 50;

        void ExecuteCommand(const KeyframeCommand& cmd);
        void Undo();
        void Redo();
    };

} // namespace EditorUI
