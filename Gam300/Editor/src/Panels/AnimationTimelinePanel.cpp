#include "Panels/AnimationTimelinePanel.h"
#include "Editor.h"
#include "Application/Interface.h"
#include "Application/Context.h"
#include "Commands/UndoRedo.h"
#include "ECS/ECS.hpp"
#include "Graphics/Models/Model.h"
#include "Graphics/Models/Animator.h"
#include "Graphics/Models/Animation.h"
#include "Graphics/Shaders/DebugLines.h"
#include "Graphics/Shaders/PBR.h"
#include "Graphics/Utilities/Data.h"
#include "Auxiliaries/Assets.h"
#include "Vendors/imgui/imgui.h"
#include "Vendors/imGuizmo/ImGuizmo.h"
#include "common/Core.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <cmath>
#include <functional>
#include <chrono>

using namespace EditorUI;

// ==================== BONE POSE UNDO/REDO COMMAND ====================

namespace EditorUI {
    // Command for bone pose manipulation with undo/redo support
    class BonePoseCommand : public ICommand {
    public:
        BonePoseCommand(AnimationTimelinePanel* panel,
                       const std::string& boneName,
                       const BonePose& oldPose,
                       const BonePose& newPose)
            : m_Panel(panel)
            , m_BoneName(boneName)
            , m_OldPose(oldPose)
            , m_NewPose(newPose)
        {
        }

        void Execute() override {
            if (!m_Panel) return;
            m_Panel->SetBonePose(m_BoneName, m_NewPose);
        }

        void Undo() override {
            if (!m_Panel) return;
            m_Panel->SetBonePose(m_BoneName, m_OldPose);
        }

        std::string GetDescription() const override {
            return "Bone Pose: " + m_BoneName;
        }

        // Public method to set the panel (for when panel needs to be updated)
        void SetPanel(AnimationTimelinePanel* panel) { m_Panel = panel; }

    private:
        AnimationTimelinePanel* m_Panel;
        std::string m_BoneName;
        BonePose m_OldPose;
        BonePose m_NewPose;
    };
}

AnimationTimelinePanel::AnimationTimelinePanel(Editor* owner)
    : m_Owner(owner)
    , m_App(owner ? static_cast<Boom::AppInterface*>(owner) : nullptr)
    , m_Ctx(m_App ? m_App->GetContext() : nullptr)
{
    // Initialize playback time
    m_LastFrameTime = (float)ImGui::GetTime();

    // Create framebuffer for 3D viewport
    glGenFramebuffers(1, &m_FramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);

    // Create color texture
    glGenTextures(1, &m_TextureID);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TextureID, 0);

    // Create depth buffer
    glGenRenderbuffers(1, &m_DepthBufferID);
    glBindRenderbuffer(GL_RENDERBUFFER, m_DepthBufferID);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthBufferID);

    // Check framebuffer status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        BOOM_ERROR("[AnimationTimelinePanel] Framebuffer is not complete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ResetCamera();
}

AnimationTimelinePanel::~AnimationTimelinePanel()
{
    if (m_FramebufferID) glDeleteFramebuffers(1, &m_FramebufferID);
    if (m_TextureID) glDeleteTextures(1, &m_TextureID);
    if (m_DepthBufferID) glDeleteRenderbuffers(1, &m_DepthBufferID);
}

void AnimationTimelinePanel::Render()
{
    if (!m_Owner || !m_App) return;

    // Create the animation editor window
    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
    ImGui::Begin("Animation Timeline", &m_Owner->m_ShowAnimationTimeline);

    // ===== PLAYBACK UPDATE: Independent timeline =====
    float currentFrameTime = (float)ImGui::GetTime();
    float deltaTime = currentFrameTime - m_LastFrameTime;

    // Clamp deltaTime to prevent huge first-frame jumps (e.g., window opened late)
    // Max 100ms (0.1s) per frame to avoid time spikes
    if (deltaTime > 0.1f || deltaTime < 0.0f)
    {
        deltaTime = 0.0f;  // Skip this frame's time update
    }

    m_LastFrameTime = currentFrameTime;

    // Update playback time (independent from game scene)
    if (m_Animator && m_SelectedClipIndex >= 0)
    {
        const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
        if (clip && clip->duration > 0.0f)
        {
            if (m_IsPlaying)
            {
                // CRITICAL: Manually advance time to control looping
                // The animator always loops in clip mode (Animator.h:128), so we control it here
                float newTime = m_CurrentTime + (deltaTime * m_PlaybackSpeed * clip->ticksPerSecond);

                if (m_Loop)
                {
                    // Loop enabled: wrap time
                    if (newTime >= clip->duration)
                    {
                        newTime = fmod(newTime, clip->duration);
                    }
                    m_CurrentTime = newTime;
                }
                else
                {
                    // Loop disabled: clamp and stop
                    if (newTime >= clip->duration)
                    {
                        m_CurrentTime = clip->duration;
                        m_IsPlaying = false;
                    }
                    else
                    {
                        m_CurrentTime = newTime;
                    }
                }

                // Update animator to match our timeline time
                m_Animator->SetTime(m_CurrentTime);
                m_Animator->UpdateJointsFromCurrentTime();
            }
            else
            {
                // When paused, ensure animator is synced to our scrubber time
                m_Animator->SetTime(m_CurrentTime);
                m_Animator->UpdateJointsFromCurrentTime();
            }
        }
    }

    // Get selected entity and load model/animator (only if not in standalone mode)
    auto selectedID = m_App->SelectedEntity();

    // Handle entity deselection (user clicked away in hierarchy)
    if (!m_StandaloneMode && selectedID == entt::null && m_SourceEntityID != entt::null)
    {
        // Entity was deselected - clear timeline to avoid using stale data
        BOOM_INFO("[AnimationTimeline] Entity deselected, clearing timeline");
        m_Animator.reset();
        m_SourceAnimator.reset();
        m_Model.reset();
        m_SourceEntityID = entt::null;
        m_HasModel = false;
        m_SelectedBoneName.clear();
        m_ManualBonePoses.clear();
        m_HasManualPoses = false;
        m_SelectedClipIndex = -1;
        m_CurrentTime = 0.0f;
        m_IsPlaying = false;
        return;  // Exit early
    }

    // Only try to load from entity if we're not in standalone mode
    if (!m_StandaloneMode && selectedID != entt::null)
    {
        // FIX 1: Validate entity before using it (scene reload can invalidate entities)
        auto& scene = m_App->GetContext()->scene;
        if (!scene.valid(selectedID))
        {
            // Entity ID is invalid (scene was reloaded) - treat like deselection
            BOOM_WARN("[AnimationTimeline] Entity ID {} is invalid after scene reload, clearing timeline", (uint32_t)selectedID);
            m_Animator.reset();
            m_SourceAnimator.reset();
            m_Model.reset();
            m_SourceEntityID = entt::null;
            m_HasModel = false;
            m_SelectedBoneName.clear();
            m_ManualBonePoses.clear();
            m_HasManualPoses = false;
            m_SelectedClipIndex = -1;
            m_CurrentTime = 0.0f;
            m_IsPlaying = false;
            return;  // Exit early
        }

        Boom::Entity selected(&scene, selectedID);

        // Get animator from AnimatorComponent (CLONE IT for independent timeline)
        if (selected.Has<Boom::AnimatorComponent>())
        {
            auto& animComp = selected.Get<Boom::AnimatorComponent>();
            if (animComp.animator)
            {
                // CRITICAL: Only clone when ENTITY changes, not when animator pointer might change
                // This prevents constant re-cloning during gameplay
                bool entityChanged = (m_SourceEntityID != selectedID);
                bool animatorPtrChanged = (m_SourceAnimator != animComp.animator);

                if (entityChanged)
                {
                    BOOM_INFO("[AnimationTimeline] Entity changed (old={}, new={}), cloning animator",
                        (uint32_t)m_SourceEntityID, (uint32_t)selectedID);

                    m_SourceEntityID = selectedID;
                    m_SourceAnimator = animComp.animator;

                    // Capture the entity's current clip BEFORE cloning
                    size_t entityCurrentClip = animComp.animator->GetCurrentClip();

                    m_Animator = animComp.animator->Clone();

                    // CRITICAL: Clear states to force clip mode (timeline doesn't use state machine)
                    m_Animator->GetStates().clear();

                    BOOM_INFO("[AnimationTimeline] Clone created: Original={}, Clone={} (states cleared for clip mode)",
                        (void*)animComp.animator.get(), (void*)m_Animator.get());

                    // Default to the entity's current clip (better UX)
                    if (entityCurrentClip < m_Animator->GetClipCount())
                    {
                        m_SelectedClipIndex = (int)entityCurrentClip;
                        m_Animator->PlayClip(entityCurrentClip);
                        BOOM_INFO("[AnimationTimeline] Defaulting to entity's current clip: {} (index {})",
                            m_Animator->GetClip(entityCurrentClip)->name, entityCurrentClip);
                    }
                    else
                    {
                        m_SelectedClipIndex = -1;
                    }

                    m_CurrentTime = 0.0f;
                    m_IsPlaying = false;

                    // Clear manual poses when switching entities
                    m_ManualBonePoses.clear();
                    m_HasManualPoses = false;
                    m_SelectedBoneName.clear();
                }
                else if (animatorPtrChanged)
                {
                    // FIX 2: Animator pointer changed (scene reload) - re-clone immediately
                    BOOM_INFO("[AnimationTimeline] Animator pointer changed (scene reload), re-cloning from entity");

                    // Update source reference
                    m_SourceAnimator = animComp.animator;

                    // Re-clone the animator
                    m_Animator = animComp.animator->Clone();
                    m_Animator->GetStates().clear();  // Force clip mode

                    // Restore playback state
                    if (m_SelectedClipIndex >= 0 && m_SelectedClipIndex < (int)m_Animator->GetClipCount())
                    {
                        m_Animator->PlayClip(m_SelectedClipIndex);
                        m_Animator->SetTime(m_CurrentTime);
                        BOOM_INFO("[AnimationTimeline] Restored clip {} at time {:.2f}", m_SelectedClipIndex, m_CurrentTime);
                    }

                    BOOM_INFO("[AnimationTimeline] Re-clone complete: Original={}, New Clone={}",
                        (void*)animComp.animator.get(), (void*)m_Animator.get());
                }
                // If entity AND animator didn't change, keep using our existing clone (don't re-clone!)
            }
            else
            {
                m_Animator.reset();
                m_SourceAnimator.reset();
                m_SourceEntityID = entt::null;
            }
        }
        else
        {
            m_Animator.reset();
            m_SourceAnimator.reset();
            m_SourceEntityID = entt::null;
        }

        // Get model from ModelComponent
        if (selected.Has<Boom::ModelComponent>())
        {
            auto& modelComp = selected.Get<Boom::ModelComponent>();

            if (modelComp.modelID != Boom::EMPTY_ASSET && m_Ctx && m_Ctx->assets)
            {
                Boom::ModelAsset* modelAsset = m_Ctx->assets->TryGet<Boom::ModelAsset>(modelComp.modelID);
                if (modelAsset && modelAsset->data)
                {
                    m_Model = modelAsset->data;
                    m_HasModel = true;
                }
                else
                {
                    if (!m_StandaloneMode)
                    {
                        m_Model.reset();
                        m_HasModel = false;
                    }
                    BOOM_ERROR("[AnimationTimeline] Failed to load model from asset registry");
                }
            }
            else
            {
                if (!m_StandaloneMode)
                {
                    m_Model.reset();
                    m_HasModel = false;
                }
            }
        }
        else
        {
            if (!m_StandaloneMode)
            {
                m_Model.reset();
                m_HasModel = false;
            }
        }
    }

    // NOTE: Animator change detection is now handled in the clone logic above
    // Auto-select first clip if we have an animator but no clip selected
    if (m_Animator && m_Animator->GetClipCount() > 0 && m_SelectedClipIndex < 0)
    {
        m_SelectedClipIndex = 0;
        m_Animator->PlayClip(0);
        m_CurrentTime = 0.0f;
        m_IsPlaying = false;  // Reset playback state
        BOOM_INFO("[AnimationTimeline] Auto-selected first clip: {}",
            m_Animator->GetClip(0)->name);
    }

    // --- LAYOUT: Four sections ---

    // Section 1: Control Bar (top - fixed height)
    RenderControlBar();

    ImGui::Separator();

    // Section 2: 3D Viewport (middle-top - resizable)
    RenderViewport();

    ImGui::Separator();

    // Section 3: Timeline Ruler (middle-bottom - fixed height)
    RenderTimelineRuler();

    ImGui::Separator();

    // Section 4: Track List (bottom - remaining space)
    RenderTrackList();

    ImGui::End();
}

void AnimationTimelinePanel::RenderControlBar()
{
    // Top bar with model loading, playback controls, and visualization options
    ImGui::BeginGroup();

    // Model loading button
    if (ImGui::Button("Load Model", ImVec2(100, 0)))
    {
        ImGui::OpenPopup("SelectModelPopup");
    }

    if (ImGui::BeginPopup("SelectModelPopup"))
    {
        ImGui::Text("Select a model:");
        ImGui::Separator();

        if (m_Ctx && m_Ctx->assets)
        {
            auto& modelMap = m_Ctx->assets->GetMap<Boom::ModelAsset>();

            for (auto& [assetID, assetPtr] : modelMap)
            {
                if (assetID == Boom::EMPTY_ASSET) continue;

                auto* modelAsset = dynamic_cast<Boom::ModelAsset*>(assetPtr.get());
                if (modelAsset && modelAsset->data)
                {
                    std::string displayName = modelAsset->name;
                    if (modelAsset->hasJoints)
                    {
                        displayName += " (Skeletal)";
                    }

                    if (ImGui::Selectable(displayName.c_str()))
                    {
                        LoadModel(modelAsset->name);
                        ImGui::CloseCurrentPopup();
                    }

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::Text("Source: %s", modelAsset->source.c_str());
                        ImGui::Text("Has Joints: %s", modelAsset->hasJoints ? "Yes" : "No");
                        ImGui::EndTooltip();
                    }
                }
            }
        }
        else
        {
            ImGui::TextDisabled("No models available");
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(60, 0)) && m_HasModel)
    {
        ClearModel();
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Playback controls (Unity-style layout)
    ImGui::BeginDisabled(!m_HasModel || !m_Animator || m_SelectedClipIndex < 0);

    // First Frame button
    if (ImGui::Button("|<")) {
        m_CurrentTime = 0.0f;
        m_IsPlaying = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("First Frame");

    // Previous Keyframe button
    ImGui::SameLine();
    if (ImGui::Button("<K")) {
        // Jump to previous keyframe
        if (m_Animator && m_SelectedClipIndex >= 0) {
            float prevTime = 0.0f;
            const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
            if (clip) {
                // Find previous keyframe across all bones
                for (const auto& [boneName, track] : clip->tracks) {
                    for (const auto& kf : track) {
                        if (kf.timeStamp < m_CurrentTime - 0.001f && kf.timeStamp > prevTime) {
                            prevTime = kf.timeStamp;
                        }
                    }
                }
                if (prevTime > 0.0f) m_CurrentTime = prevTime;
            }
        }
        m_IsPlaying = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Previous Keyframe");

    ImGui::SameLine();
    if (ImGui::Button(m_IsPlaying ? "Pause" : "Play")) {
        m_IsPlaying = !m_IsPlaying;
    }

    // Next Keyframe button
    ImGui::SameLine();
    if (ImGui::Button("K>")) {
        // Jump to next keyframe
        if (m_Animator && m_SelectedClipIndex >= 0) {
            const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
            if (clip) {
                float nextTime = clip->duration;
                // Find next keyframe across all bones
                for (const auto& [boneName, track] : clip->tracks) {
                    for (const auto& kf : track) {
                        if (kf.timeStamp > m_CurrentTime + 0.001f && kf.timeStamp < nextTime) {
                            nextTime = kf.timeStamp;
                        }
                    }
                }
                if (nextTime < clip->duration) m_CurrentTime = nextTime;
            }
        }
        m_IsPlaying = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Next Keyframe");

    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        m_IsPlaying = false;
        m_CurrentTime = 0.0f;
    }

    ImGui::SameLine();
    ImGui::Checkbox("Loop", &m_Loop);

    // Playback speed control
    ImGui::SameLine();
    ImGui::Text("Speed:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::SliderFloat("##PlaybackSpeed", &m_PlaybackSpeed, 0.1f, 3.0f, "%.2fx");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Playback speed multiplier");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("1x")) {
        m_PlaybackSpeed = 1.0f;
    }
    ImGui::EndDisabled();

    // Undo/Redo buttons
    ImGui::SameLine(0, 20);
    ImGui::Separator();
    ImGui::SameLine(0, 20);

    ImGui::BeginDisabled(m_UndoStack.empty());
    if (ImGui::Button("Undo"))
    {
        Undo();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Undo last keyframe edit (Ctrl+Z)");
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(m_RedoStack.empty());
    if (ImGui::Button("Redo"))
    {
        Redo();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Redo keyframe edit (Ctrl+Y)");
    }
    ImGui::EndDisabled();

    // Keyboard shortcuts
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyShift)
    {
        Undo();
    }
    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyShift)))
    {
        Redo();
    }

    // Gizmo mode keyboard shortcuts (W/E/R/T/K) - only when viewport is focused
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
            // Only allow translate if not in rotation-only mode
            if (!m_RotationOnlyMode) {
                m_GizmoOperation = 7;  // ImGuizmo::TRANSLATE
            }
            else {
                BOOM_WARN("[Gizmo] Translation disabled in Rotation-Only mode. Uncheck 'Rotation Only' to enable.");
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
            m_GizmoOperation = 120;  // ImGuizmo::ROTATE
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            // Only allow scale if not in rotation-only mode
            if (!m_RotationOnlyMode) {
                m_GizmoOperation = 896;  // ImGuizmo::SCALE
            }
            else {
                BOOM_WARN("[Gizmo] Scaling disabled in Rotation-Only mode. Uncheck 'Rotation Only' to enable.");
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_T, false)) {
            m_GizmoMode = (m_GizmoMode == 0) ? 1 : 0;  // Toggle LOCAL (0) / WORLD (1)
            BOOM_INFO("[Gizmo] Toggled to {} space", m_GizmoMode == 1 ? "WORLD" : "LOCAL");
        }

        // K key - Add keyframe at current time for selected bone
        if (ImGui::IsKeyPressed(ImGuiKey_K, false))
        {
            if (!m_SelectedBoneName.empty() && m_Animator && m_SelectedClipIndex >= 0)
            {
                // Capture current bone transform
                Boom::KeyFrame kf = CaptureCurrentBoneTransform(m_SelectedBoneName);

                if (kf.timeStamp >= 0.0f)  // Valid capture
                {
                    // Create and execute ADD command
                    KeyframeCommand cmd;
                    cmd.type = KeyframeCommand::ADD;
                    cmd.boneName = m_SelectedBoneName;
                    cmd.keyframe = kf;
                    ExecuteCommand(cmd);

                    BOOM_INFO("[Keyframe Record] Captured pose for bone '{}' at time {:.2f}s",
                              m_SelectedBoneName.c_str(), (double)kf.timeStamp);
                }
                else
                {
                    BOOM_WARN("[Keyframe Record] Failed to capture bone transform for '{}'", m_SelectedBoneName.c_str());
                }
            }
            else if (m_SelectedBoneName.empty())
            {
                BOOM_WARN("[Keyframe Record] No bone selected - select a bone first!");
            }
            else if (!m_Animator)
            {
                BOOM_WARN("[Keyframe Record] No animator loaded");
            }
            else if (m_SelectedClipIndex < 0)
            {
                BOOM_WARN("[Keyframe Record] No clip selected");
            }
        }
    }

    ImGui::SameLine(0, 20);
    ImGui::Separator();
    ImGui::SameLine(0, 20);

    // Gizmo mode buttons
    const char* gizmoModeText = "";
    if (m_GizmoOperation == 7) gizmoModeText = "Move (W)";
    else if (m_GizmoOperation == 120) gizmoModeText = "Rotate (E)";
    else if (m_GizmoOperation == 896) gizmoModeText = "Scale (R)";

    const char* gizmoSpaceText = (m_GizmoMode == 1) ? "World" : "Local";

    ImGui::Text("Gizmo:");
    ImGui::SameLine();
    if (ImGui::Button("Move (W)")) m_GizmoOperation = 7;
    ImGui::SameLine();
    if (ImGui::Button("Rotate (E)")) m_GizmoOperation = 120;
    ImGui::SameLine();
    if (ImGui::Button("Scale (R)")) m_GizmoOperation = 896;
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    if (ImGui::Button("Toggle Space (T)"))
    {
        m_GizmoMode = (m_GizmoMode == 0) ? 1 : 0;
    }
    ImGui::SameLine();
    ImGui::Text("[%s - %s]", gizmoModeText, gizmoSpaceText);

    // Keyframe recording hint
    ImGui::SameLine(0, 20);
    ImGui::Separator();
    ImGui::SameLine(0, 20);

    // Show keyframe recording status
    if (!m_SelectedBoneName.empty())
    {
        ImGui::Text("Add Keyframe:");
        ImGui::SameLine();
        if (ImGui::Button("K"))
        {
            // Trigger K key action manually via button
            if (m_Animator && m_SelectedClipIndex >= 0)
            {
                Boom::KeyFrame kf = CaptureCurrentBoneTransform(m_SelectedBoneName);
                if (kf.timeStamp >= 0.0f)
                {
                    KeyframeCommand cmd;
                    cmd.type = KeyframeCommand::ADD;
                    cmd.boneName = m_SelectedBoneName;
                    cmd.keyframe = kf;
                    ExecuteCommand(cmd);
                }
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Add keyframe for '%s' at current time (%.2fs)\nOr press K key",
                            m_SelectedBoneName.c_str(), m_CurrentTime);
        }
    }
    else
    {
        ImGui::TextDisabled("Add Keyframe: K");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Select a bone first, then press K to add a keyframe");
        }
    }

    ImGui::SameLine(0, 20);
    ImGui::Separator();
    ImGui::SameLine(0, 20);

    // Visualization toggles
    ImGui::Checkbox("Show Skeleton", &m_ShowSkeleton);
    ImGui::SameLine();
    ImGui::Checkbox("Show Grid", &m_ShowGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Wireframe", &m_ShowWireframe);

    ImGui::SameLine(0, 20);
    ImGui::Separator();
    ImGui::SameLine(0, 10);

    // Gizmo manipulation mode toggle
    if (ImGui::Checkbox("Rotation Only", &m_RotationOnlyMode))
    {
        // If switching to rotation-only mode, force rotate gizmo
        if (m_RotationOnlyMode)
        {
            m_GizmoOperation = 120;  // ImGuizmo::ROTATE
            BOOM_INFO("[Gizmo] Rotation-Only Mode ENABLED - translation disabled to prevent bone stretching");
        }
        else
        {
            BOOM_INFO("[Gizmo] Rotation-Only Mode DISABLED - translation allowed (may cause bone stretching)");
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("When enabled, only rotation gizmo is allowed.\nPrevents bone length stretching from translation.");
    }

    // Show current gizmo mode indicator
    ImGui::SameLine();
    const char* gizmoModeLabel = "Unknown";
    if (m_GizmoOperation == 7) gizmoModeLabel = "Translate (W)";
    else if (m_GizmoOperation == 120) gizmoModeLabel = "Rotate (E)";
    else if (m_GizmoOperation == 896) gizmoModeLabel = "Scale (R)";
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%s]", gizmoModeLabel);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Current gizmo mode. Press W/E/R to switch.\nPress T to toggle World/Local space.");
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset Camera")) {
        ResetCamera();
    }

    if (m_HasModel) {
        ImGui::SameLine();
        if (ImGui::Button("Frame Model")) {
            FrameModel();
        }

        // Clear bone poses button (only show if we have manual poses)
        if (m_HasManualPoses && !m_SelectedBoneName.empty())
        {
            ImGui::SameLine();
            if (ImGui::Button("Clear Bone Pose"))
            {
                ClearBonePose(m_SelectedBoneName);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Clear manual pose for selected bone '%s'\n(Ctrl+Z to undo)",
                                m_SelectedBoneName.c_str());
            }
        }

        if (m_HasManualPoses)
        {
            ImGui::SameLine();
            if (ImGui::Button("Clear All Poses"))
            {
                ClearAllBonePoses();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Clear all manual bone poses\n(Returns to animation keyframes)");
            }
        }
    }

    // Scale controls (new row)
    if (m_HasModel)
    {
        ImGui::Text("Preview Scale:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);

        // Use logarithmic slider for better control at small values
        if (ImGui::SliderFloat("##ModelScale", &m_ModelScale, 0.001f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
        {
            // Scale changed - optionally re-frame
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset Scale"))
        {
            m_ModelScale = 1.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("0.01x"))
        {
            m_ModelScale = 0.01f;
        }
        ImGui::SameLine();
        if (ImGui::Button("0.1x"))
        {
            m_ModelScale = 0.1f;
        }

        // Show info
        ImGui::SameLine();
        if (m_Model)
        {
            ImGui::TextDisabled("| Asset Scale: (%.2f, %.2f, %.2f) | Camera Dist: %.2f",
                m_Model->modelTransform.scale.x,
                m_Model->modelTransform.scale.y,
                m_Model->modelTransform.scale.z,
                m_CameraDistance);
        }
    }

    // ===== CHUNK 2: Animation Clip Selection & Info Display =====
    if (m_Animator && m_Animator->GetClipCount() > 0)
    {
        ImGui::Separator();
        ImGui::Text("Animation Clip:");
        ImGui::SameLine();

        // Animation clip dropdown
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##AnimClip",
            m_SelectedClipIndex >= 0 ? m_Animator->GetClip(m_SelectedClipIndex)->name.c_str() : "Select clip..."))
        {
            for (size_t i = 0; i < m_Animator->GetClipCount(); ++i)
            {
                const auto* clip = m_Animator->GetClip(i);
                if (!clip) continue;

                bool isSelected = (m_SelectedClipIndex == (int)i);
                if (ImGui::Selectable(clip->name.c_str(), isSelected))
                {
                    m_SelectedClipIndex = (int)i;
                    m_Animator->PlayClip(i);  // Switch to this clip
                    m_CurrentTime = 0.0f;     // Reset time
                    m_IsPlaying = false;      // Stop playback when switching clips
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Clip management buttons
        ImGui::SameLine();
        if (ImGui::Button("New"))
        {
            ImGui::OpenPopup("CreateClipPopup");
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Create a new animation clip");
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedClipIndex < 0);
        if (ImGui::Button("Duplicate"))
        {
            if (m_Animator && m_SelectedClipIndex >= 0 && m_SelectedClipIndex < (int)m_Animator->GetClipCount())
            {
                auto* sourceClip = m_Animator->GetClipMutable(m_SelectedClipIndex);
                if (sourceClip)
                {
                    // Create a deep copy of the clip
                    auto newClip = std::make_shared<Boom::AnimationClip>();
                    newClip->name = sourceClip->name + " Copy";
                    newClip->duration = sourceClip->duration;
                    newClip->ticksPerSecond = sourceClip->ticksPerSecond;
                    newClip->filePath = sourceClip->filePath;
                    newClip->tracks = sourceClip->tracks;  // Deep copy tracks

                    // Add to animator
                    m_Animator->AddClip(newClip);
                    m_SelectedClipIndex = (int)m_Animator->GetClipCount() - 1;
                    m_Animator->PlayClip(m_SelectedClipIndex);
                    m_CurrentTime = 0.0f;
                    m_IsPlaying = false;

                    BOOM_INFO("[AnimTimeline] Duplicated clip '{}' as '{}'",
                              sourceClip->name, newClip->name);
                }
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Duplicate selected animation clip");
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedClipIndex < 0);
        if (ImGui::Button("Rename"))
        {
            if (m_Animator && m_SelectedClipIndex >= 0)
            {
                ImGui::OpenPopup("RenameClipPopup");
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Rename selected animation clip");
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedClipIndex < 0 || (m_Animator && m_Animator->GetClipCount() <= 1));
        if (ImGui::Button("Delete"))
        {
            if (m_Animator && m_SelectedClipIndex >= 0)
            {
                ImGui::OpenPopup("DeleteClipConfirm");
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Delete selected animation clip");
        }
        ImGui::EndDisabled();

        // Create clip popup
        if (ImGui::BeginPopup("CreateClipPopup"))
        {
            static char clipName[128] = "New Clip";
            static float clipDuration = 1.0f;
            static float clipFPS = 30.0f;

            ImGui::Text("Create New Animation Clip");
            ImGui::Separator();

            ImGui::InputText("Name", clipName, sizeof(clipName));
            ImGui::InputFloat("Duration (s)", &clipDuration, 0.1f, 1.0f, "%.2f");
            ImGui::InputFloat("FPS", &clipFPS, 1.0f, 10.0f, "%.1f");

            if (clipDuration < 0.01f) clipDuration = 0.01f;
            if (clipFPS < 1.0f) clipFPS = 1.0f;

            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                if (m_Animator)
                {
                    auto newClip = std::make_shared<Boom::AnimationClip>();
                    newClip->name = std::string(clipName);
                    newClip->duration = clipDuration;
                    newClip->ticksPerSecond = clipFPS;
                    newClip->filePath = "";  // No source file

                    // Add to animator
                    m_Animator->AddClip(newClip);
                    m_SelectedClipIndex = (int)m_Animator->GetClipCount() - 1;
                    m_Animator->PlayClip(m_SelectedClipIndex);
                    m_CurrentTime = 0.0f;
                    m_IsPlaying = false;

                    BOOM_INFO("[AnimTimeline] Created new clip '{}'", newClip->name);

                    // Reset form
                    strcpy_s(clipName, sizeof(clipName), "New Clip");
                    clipDuration = 1.0f;
                    clipFPS = 30.0f;
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Rename clip popup
        if (ImGui::BeginPopup("RenameClipPopup"))
        {
            static char newName[128] = "";
            static bool firstOpen = true;

            if (firstOpen)
            {
                if (m_Animator && m_SelectedClipIndex >= 0)
                {
                    auto* clip = m_Animator->GetClipMutable(m_SelectedClipIndex);
                    if (clip)
                    {
                        strncpy_s(newName, sizeof(newName), clip->name.c_str(), _TRUNCATE);
                    }
                }
                firstOpen = false;
            }

            ImGui::Text("Rename Animation Clip");
            ImGui::Separator();

            ImGui::InputText("New Name", newName, sizeof(newName));

            if (ImGui::Button("Rename", ImVec2(120, 0)))
            {
                if (m_Animator && m_SelectedClipIndex >= 0)
                {
                    auto* clip = m_Animator->GetClipMutable(m_SelectedClipIndex);
                    if (clip && strlen(newName) > 0)
                    {
                        std::string oldName = clip->name;
                        clip->name = std::string(newName);
                        BOOM_INFO("[AnimTimeline] Renamed clip '{}' to '{}'", oldName, clip->name);
                    }
                }
                firstOpen = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                firstOpen = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Delete clip confirmation popup
        if (ImGui::BeginPopupModal("DeleteClipConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (m_Animator && m_SelectedClipIndex >= 0)
            {
                const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
                if (clip)
                {
                    ImGui::Text("Are you sure you want to delete clip:");
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "  %s", clip->name.c_str());
                    ImGui::Text("This action cannot be undone!");
                }
            }

            ImGui::Separator();

            if (ImGui::Button("Delete", ImVec2(120, 0)))
            {
                if (m_Animator && m_SelectedClipIndex >= 0 && m_Animator->GetClipCount() > 1)
                {
                    const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
                    std::string clipName = clip ? clip->name : "Unknown";

                    m_Animator->RemoveClip(m_SelectedClipIndex);
                    BOOM_INFO("[AnimTimeline] Deleted clip '{}'", clipName);

                    // Select the first remaining clip
                    if (m_Animator->GetClipCount() > 0)
                    {
                        m_SelectedClipIndex = 0;
                        m_Animator->PlayClip(0);
                        m_CurrentTime = 0.0f;
                        m_IsPlaying = false;
                    }
                    else
                    {
                        m_SelectedClipIndex = -1;
                    }
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Display clip information
        if (m_SelectedClipIndex >= 0 && m_SelectedClipIndex < (int)m_Animator->GetClipCount())
        {
            const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
            if (clip)
            {
                ImGui::SameLine();
                ImGui::Text("|");

                // Duration display
                ImGui::SameLine();
                ImGui::Text("Duration: %.2fs", clip->duration);

                // FPS / Ticks per second
                ImGui::SameLine();
                ImGui::Text("| FPS: %.1f", clip->ticksPerSecond);

                // Frame count (approximate)
                int frameCount = (int)(clip->duration * clip->ticksPerSecond);
                ImGui::SameLine();
                ImGui::Text("| Frames: %d", frameCount);

                // Current time / Total time
                ImGui::SameLine();
                ImGui::Text("| Time: %.2f / %.2f", m_CurrentTime, clip->duration);

                // DEBUG: Playback state
                ImGui::SameLine();
                ImGui::TextColored(m_IsPlaying ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                    "| %s", m_IsPlaying ? "PLAYING" : "PAUSED");

                ImGui::SameLine();
                ImGui::TextColored(m_Loop ? ImVec4(0, 1, 1, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1),
                    "| Loop: %s", m_Loop ? "ON" : "OFF");
            }
        }
    }
    else if (m_Animator)
    {
        ImGui::Separator();
        ImGui::TextDisabled("No animation clips available");
    }

    ImGui::EndGroup();
}

void AnimationTimelinePanel::RenderViewport()
{
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    // Take half of remaining space for viewport
    ImVec2 viewportSize = ImVec2(availableSize.x, availableSize.y * 0.5f);

    // Resize framebuffer if viewport size changed
    if (viewportSize.x != m_ViewportSize.x || viewportSize.y != m_ViewportSize.y)
    {
        if (viewportSize.x > 0 && viewportSize.y > 0)
        {
            m_ViewportSize = viewportSize;

            // Resize color texture
            glBindTexture(GL_TEXTURE_2D, m_TextureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

            // Resize depth buffer
            glBindRenderbuffer(GL_RENDERBUFFER, m_DepthBufferID);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y);
        }
    }

    // Update camera
    UpdateCamera();

    // ===== SAVE RENDERER STATE (to prevent timeline from affecting game scene) =====
    // CRITICAL: The renderer is SHARED between timeline and game scene
    // Render order: Game scene FIRST, then Timeline SECOND
    // We must save and restore state to prevent cross-contamination
    float savedAmbient = 0.0f;
    if (m_Ctx && m_Ctx->renderer)
    {
        savedAmbient = m_Ctx->renderer->AmbientStrength();
    }

    // Render to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);

    // CRITICAL: Check if framebuffer is still valid after scene reload
    GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        BOOM_ERROR("[AnimationTimeline] Framebuffer invalid (status: 0x{:X}) - recreating!", fbStatus);

        // Unbind before deleting
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Delete old framebuffer resources
        if (m_FramebufferID) glDeleteFramebuffers(1, &m_FramebufferID);
        if (m_TextureID) glDeleteTextures(1, &m_TextureID);
        if (m_DepthBufferID) glDeleteRenderbuffers(1, &m_DepthBufferID);

        // Recreate framebuffer
        glGenFramebuffers(1, &m_FramebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);

        // Recreate color texture
        glGenTextures(1, &m_TextureID);
        glBindTexture(GL_TEXTURE_2D, m_TextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TextureID, 0);

        // Recreate depth buffer
        glGenRenderbuffers(1, &m_DepthBufferID);
        glBindRenderbuffer(GL_RENDERBUFFER, m_DepthBufferID);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthBufferID);

        // Verify new framebuffer
        GLenum newStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (newStatus == GL_FRAMEBUFFER_COMPLETE)
        {
            BOOM_INFO("[AnimationTimeline] Framebuffer recreated successfully!");
        }
        else
        {
            BOOM_ERROR("[AnimationTimeline] Failed to recreate framebuffer (status: 0x{:X})", newStatus);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;  // Abort rendering if framebuffer is broken
        }
    }

    glViewport(0, 0, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y);

    // Clear
    glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Set up lighting for preview (temporary - will restore after)
    if (m_HasModel && m_Ctx && m_Ctx->renderer)
    {
        std::vector<Boom::GPUDirLight> dirLights(1);
        dirLights[0].dir_intensity = glm::vec4(glm::normalize(glm::vec3(1.0f, -1.0f, 1.0f)), 1.0f);
        dirLights[0].radiance = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_Ctx->renderer->UploadDirLights(dirLights, 1);
        m_Ctx->renderer->AmbientStrength() = 0.3f;
    }

    // Set wireframe mode if requested
    if (m_ShowWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // Render scene
    if (m_ShowGrid) RenderGrid();
    if (m_HasModel) RenderModel();
    if (m_HasModel && m_ShowSkeleton) RenderSkeleton();

    // Reset polygon mode
    if (m_ShowWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // ===== RESTORE RENDERER STATE (critical to prevent leaking to game scene) =====
    if (m_Ctx && m_Ctx->renderer)
    {
        // CRITICAL: Clear joint transforms to prevent affecting game scene models
        // Must use identity matrices (like Application.cpp does), NOT empty vector!
        // Empty vector doesn't upload to GPU, leaving stale timeline joints
        // NOTE: NOT static - create fresh each time to force GPU upload
        std::vector<glm::mat4> identityPalette(100, glm::mat4(1.0f));
        m_Ctx->renderer->SetJoints(identityPalette);

        // Restore ambient strength
        m_Ctx->renderer->AmbientStrength() = savedAmbient;

        // Note: Directional lights will be re-uploaded by game scene on next frame
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Display framebuffer texture in ImGui
    ImVec2 viewportPos = ImGui::GetCursorScreenPos();
    ImGui::Image(
        (ImTextureID)(intptr_t)m_TextureID,
        m_ViewportSize,
        ImVec2(0, 1), // UV top-left
        ImVec2(1, 0)  // UV bottom-right (flip Y)
    );

    // Show message overlay if no model loaded
    if (!m_HasModel)
    {
        ImVec2 textPos = ImVec2(
            viewportPos.x + m_ViewportSize.x * 0.5f - 150,
            viewportPos.y + m_ViewportSize.y * 0.5f - 40
        );
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Draw semi-transparent background
        drawList->AddRectFilled(
            ImVec2(textPos.x - 10, textPos.y - 10),
            ImVec2(textPos.x + 310, textPos.y + 90),
            IM_COL32(0, 0, 0, 200)
        );

        // Draw text
        drawList->AddText(ImVec2(textPos.x, textPos.y),
                         IM_COL32(200, 200, 200, 255),
                         "No model loaded");
        drawList->AddText(ImVec2(textPos.x, textPos.y + 25),
                         IM_COL32(150, 150, 150, 255),
                         "Click 'Load Model' to select a model");
        drawList->AddText(ImVec2(textPos.x, textPos.y + 50),
                         IM_COL32(150, 150, 150, 255),
                         "OR select an entity with AnimatorComponent");
    }

    // Handle camera controls AFTER image is rendered
    HandleCameraControls();

    // Handle bone picking for selection in 3D viewport
    HandleBonePicking();

    // Render transform gizmo at selected bone
    // NOTE: Gizmo draws on top of the image, but we already rendered with updated poses
    HandleGizmo(viewportPos, m_ViewportSize);
}

void AnimationTimelinePanel::RenderTimelineRuler()
{
    ImGui::BeginGroup();

    // Get animation duration
    float duration = 1.0f;  // Default
    if (m_Animator && m_SelectedClipIndex >= 0 && static_cast<size_t>(m_SelectedClipIndex) < m_Animator->GetClipCount())
    {
        const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
        if (clip)
        {
            duration = clip->duration;
        }
    }

    // Timeline ruler dimensions
    const float rulerHeight = 50.0f;
    ImVec2 rulerSize = ImVec2(ImGui::GetContentRegionAvail().x, rulerHeight);
    ImVec2 rulerPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw ruler background with subtle gradient
    drawList->AddRectFilledMultiColor(
        rulerPos,
        ImVec2(rulerPos.x + rulerSize.x, rulerPos.y + rulerSize.y),
        IM_COL32(55, 55, 55, 255),  // Top-left
        IM_COL32(55, 55, 55, 255),  // Top-right
        IM_COL32(45, 45, 45, 255),  // Bottom-right
        IM_COL32(45, 45, 45, 255)   // Bottom-left
    );

    // Draw top border (separator from viewport)
    drawList->AddLine(
        rulerPos,
        ImVec2(rulerPos.x + rulerSize.x, rulerPos.y),
        IM_COL32(80, 80, 80, 255),
        1.0f
    );

    // Draw bottom border (separator from bone tracks)
    drawList->AddLine(
        ImVec2(rulerPos.x, rulerPos.y + rulerSize.y),
        ImVec2(rulerPos.x + rulerSize.x, rulerPos.y + rulerSize.y),
        IM_COL32(100, 100, 100, 255),
        2.0f
    );

    // Draw time markers with dynamic spacing based on available width
    if (duration > 0.0f && rulerSize.x > 0.0f)
    {
        // Calculate pixels per second
        float pixelsPerSecond = rulerSize.x / duration;

        // Choose marker interval based on zoom level
        // We want major markers roughly every 60-120 pixels
        float majorInterval = 1.0f;  // Default: 1 second
        float minorInterval = 0.5f;  // Default: 0.5 seconds

        if (pixelsPerSecond < 30.0f)  // Very zoomed out (< 30px per second)
        {
            majorInterval = 10.0f;
            minorInterval = 5.0f;
        }
        else if (pixelsPerSecond < 60.0f)  // Zoomed out (30-60px per second)
        {
            majorInterval = 5.0f;
            minorInterval = 1.0f;
        }
        else if (pixelsPerSecond < 100.0f)  // Medium (60-100px per second)
        {
            majorInterval = 2.0f;
            minorInterval = 1.0f;
        }
        else if (pixelsPerSecond < 150.0f)  // Medium-close (100-150px per second)
        {
            majorInterval = 1.0f;
            minorInterval = 0.5f;
        }
        else if (pixelsPerSecond < 300.0f)  // Close (150-300px per second)
        {
            majorInterval = 0.5f;
            minorInterval = 0.1f;
        }
        else  // Very zoomed in (> 300px per second)
        {
            majorInterval = 0.1f;
            minorInterval = 0.05f;
        }

        // Draw major markers
        for (float t = 0.0f; t <= duration; t += majorInterval)
        {
            float normalizedTime = t / duration;
            float x = rulerPos.x + normalizedTime * rulerSize.x;

            // Major marker (longer line + time label)
            drawList->AddLine(
                ImVec2(x, rulerPos.y + rulerHeight - 20.0f),
                ImVec2(x, rulerPos.y + rulerHeight),
                IM_COL32(200, 200, 200, 255),
                1.5f
            );

            // Time label with smart formatting
            char timeLabel[16];
            if (majorInterval >= 1.0f)
            {
                snprintf(timeLabel, sizeof(timeLabel), "%.0fs", t);  // No decimals for >= 1s intervals
            }
            else
            {
                snprintf(timeLabel, sizeof(timeLabel), "%.1fs", t);  // 1 decimal for < 1s intervals
            }

            // Calculate text size for proper centering and edge clamping
            ImVec2 textSize = ImGui::CalcTextSize(timeLabel);
            float textX = x - (textSize.x * 0.5f);  // Center text on tick mark

            // Clamp text to stay within ruler bounds
            textX = (textX < rulerPos.x) ? rulerPos.x : textX;  // Don't go off left edge
            textX = (textX + textSize.x > rulerPos.x + rulerSize.x) ? (rulerPos.x + rulerSize.x - textSize.x) : textX;  // Don't go off right edge

            drawList->AddText(
                ImVec2(textX, rulerPos.y + 5.0f),
                IM_COL32(220, 220, 220, 255),
                timeLabel
            );
        }

        // Draw minor markers (only if there's enough space)
        if (pixelsPerSecond > 40.0f)  // Only show minor markers when not too cramped
        {
            for (float t = 0.0f; t <= duration; t += minorInterval)
            {
                // Skip if this is already a major marker
                if (fmod(t, majorInterval) < 0.001f) continue;

                float normalizedTime = t / duration;
                float x = rulerPos.x + normalizedTime * rulerSize.x;

                // Minor marker (shorter line)
                drawList->AddLine(
                    ImVec2(x, rulerPos.y + rulerHeight - 10.0f),
                    ImVec2(x, rulerPos.y + rulerHeight),
                    IM_COL32(150, 150, 150, 255),
                    1.0f
                );
            }
        }
    }

    // Draw playhead (red vertical line)
    if (duration > 0.0f)
    {
        float normalizedTime = m_CurrentTime / duration;
        normalizedTime = (normalizedTime < 0.0f) ? 0.0f : (normalizedTime > 1.0f) ? 1.0f : normalizedTime;
        float playheadX = rulerPos.x + normalizedTime * rulerSize.x;

        // Playhead line
        drawList->AddLine(
            ImVec2(playheadX, rulerPos.y),
            ImVec2(playheadX, rulerPos.y + rulerHeight),
            IM_COL32(255, 80, 80, 255),
            3.0f
        );

        // Playhead triangle (at top)
        ImVec2 triangleTop(playheadX, rulerPos.y);
        ImVec2 triangleLeft(playheadX - 6.0f, rulerPos.y + 12.0f);
        ImVec2 triangleRight(playheadX + 6.0f, rulerPos.y + 12.0f);
        drawList->AddTriangleFilled(triangleTop, triangleLeft, triangleRight, IM_COL32(255, 80, 80, 255));

        // Current time display (Unity-style: frame number + seconds)
        const float fps = 30.0f;  // Standard animation framerate
        int currentFrame = (int)(m_CurrentTime * fps);
        int totalFrames = (int)(duration * fps);

        char currentTimeLabel[64];
        snprintf(currentTimeLabel, sizeof(currentTimeLabel), "Frame %d / %d  (%.2fs / %.2fs)",
                 currentFrame, totalFrames, m_CurrentTime, duration);

        ImVec2 timeDisplaySize = ImGui::CalcTextSize(currentTimeLabel);
        float timeDisplayX = rulerPos.x + rulerSize.x - timeDisplaySize.x - 5.0f;  // 5px padding from right edge

        // Draw semi-transparent background for readability
        drawList->AddRectFilled(
            ImVec2(timeDisplayX - 3.0f, rulerPos.y + rulerHeight - 20.0f),
            ImVec2(timeDisplayX + timeDisplaySize.x + 3.0f, rulerPos.y + rulerHeight - 2.0f),
            IM_COL32(0, 0, 0, 150)
        );

        drawList->AddText(
            ImVec2(timeDisplayX, rulerPos.y + rulerHeight - 18.0f),
            IM_COL32(255, 255, 255, 255),
            currentTimeLabel
        );
    }

    // Make the ruler interactive (invisible button over the entire area)
    ImGui::SetCursorScreenPos(rulerPos);
    ImGui::InvisibleButton("TimelineRuler", rulerSize);

    // Handle timeline scrubbing
    if (ImGui::IsItemHovered())
    {
        // Change cursor to indicate interactivity
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        // Click to jump to time
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_IsDraggingTimeline = true;
            m_IsPlaying = false;  // Pause playback when scrubbing
        }
    }

    // Handle dragging
    if (m_IsDraggingTimeline)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            // Calculate time from mouse position
            ImVec2 mousePos = ImGui::GetMousePos();
            float normalizedTime = (mousePos.x - rulerPos.x) / rulerSize.x;
            normalizedTime = (normalizedTime < 0.0f) ? 0.0f : (normalizedTime > 1.0f) ? 1.0f : normalizedTime;

            m_CurrentTime = normalizedTime * duration;

            // Update animator to this time (if we have one)
            if (m_Animator)
            {
                m_Animator->SetTime(m_CurrentTime);
                m_Animator->Animate(0.0f);  // Update with 0 dt to just apply the time
            }
        }
        else
        {
            // Mouse released - stop dragging
            m_IsDraggingTimeline = false;
        }
    }

    ImGui::EndGroup();
}

void AnimationTimelinePanel::RenderTrackList()
{
    ImGui::BeginGroup();

    ImGui::Text("BONE TRACKS");

    // Check if we have a valid animator with skeleton
    if (!m_Animator || !m_HasModel)
    {
        ImGui::TextDisabled("No model loaded");
        ImGui::EndGroup();
        return;
    }

    // Get animation duration for timeline scaling
    float duration = 1.0f;  // Default
    if (m_SelectedClipIndex >= 0 && static_cast<size_t>(m_SelectedClipIndex) < m_Animator->GetClipCount())
    {
        const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
        if (clip)
        {
            duration = clip->duration;
        }
    }

    if (ImGui::BeginChild("TrackListScroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        // Setup two columns: bone names (left) and timeline tracks (right)
        const float boneNameWidth = 250.0f;
        ImGui::Columns(2, "BoneTrackColumns", true);
        ImGui::SetColumnWidth(0, boneNameWidth);

        // Get the root joint from animator
        const Boom::Joint& root = m_Animator->GetRoot();

        // Render bone hierarchy starting from root
        RenderBoneTrack(root, duration);

        // End columns
        ImGui::Columns(1);
    }
    ImGui::EndChild();

    ImGui::EndGroup();
}

void AnimationTimelinePanel::RenderBoneTrack(const Boom::Joint& joint, float duration)
{
    // === COLUMN 0: Bone Name (with tree hierarchy) ===

    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                             | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    // Highlight if selected
    if (joint.name == m_SelectedBoneName)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // If no children, make it a leaf node
    if (joint.children.empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // Default to open for root and first level
    if (joint.index == 0 || joint.index == 1)
    {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    // Store row position for timeline drawing
    ImVec2 rowStartPos = ImGui::GetCursorScreenPos();
    float rowHeight = ImGui::GetTextLineHeightWithSpacing();

    // Display bone name with tree node in COLUMN 0
    std::string label = joint.name + " [" + std::to_string(joint.index) + "]";
    bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);

    // Handle selection
    if (ImGui::IsItemClicked())
    {
        m_SelectedBoneName = joint.name;
        // Sync with global context for viewport highlighting
        if (m_Ctx)
        {
            m_Ctx->SelectedBoneName = joint.name;
        }
    }

    // Tooltip with bone info
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Bone: %s", joint.name.c_str());
        ImGui::Text("Index: %d", joint.index);
        ImGui::Text("Children: %zu", joint.children.size());
        ImGui::EndTooltip();
    }

    // === COLUMN 1: Timeline Track (perfectly aligned) ===
    ImGui::NextColumn();

    // Get the timeline area dimensions
    ImVec2 timelineStartPos = ImGui::GetCursorScreenPos();
    float timelineWidth = ImGui::GetColumnWidth(1) - 10.0f; // Leave some padding

    // Adjust vertical position to match the tree node row
    timelineStartPos.y = rowStartPos.y;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Timeline background (dark gray)
    ImVec2 timelineMin = timelineStartPos;
    ImVec2 timelineMax(timelineMin.x + timelineWidth, timelineMin.y + rowHeight);
    drawList->AddRectFilled(timelineMin, timelineMax, IM_COL32(40, 40, 40, 255));

    // Draw grid lines for time markers (every second)
    if (duration > 0.0f)
    {
        for (float t = 0.0f; t <= duration; t += 1.0f)
        {
            float x = timelineMin.x + (t / duration) * timelineWidth;
            drawList->AddLine(
                ImVec2(x, timelineMin.y),
                ImVec2(x, timelineMax.y),
                IM_COL32(80, 80, 80, 255)
            );
        }
    }

    // Draw current time indicator (red line)
    if (duration > 0.0f && m_CurrentTime >= 0.0f)
    {
        float normalizedTime = m_CurrentTime / duration;
        normalizedTime = (normalizedTime < 0.0f) ? 0.0f : (normalizedTime > 1.0f) ? 1.0f : normalizedTime;
        float x = timelineMin.x + normalizedTime * timelineWidth;
        drawList->AddLine(
            ImVec2(x, timelineMin.y),
            ImVec2(x, timelineMax.y),
            IM_COL32(255, 0, 0, 255),
            2.0f
        );
    }

    // Draw and interact with keyframe diamonds
    if (m_Animator && m_SelectedClipIndex >= 0 && duration > 0.0f)
    {
        const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
        if (clip)
        {
            const auto* keyframes = clip->GetTrack(joint.name);
            if (keyframes && !keyframes->empty())
            {
                ImVec2 mousePos = ImGui::GetMousePos();

                // Track if we're hovering any keyframe on THIS bone (local to this iteration)
                bool hoveredAnyKeyframe = false;

                // Draw diamond at each keyframe timestamp
                for (size_t i = 0; i < keyframes->size(); ++i)
                {
                    const auto& kf = (*keyframes)[i];

                    // Calculate X position based on timestamp
                    float normalizedTime = kf.timeStamp / duration;
                    float x = timelineMin.x + normalizedTime * timelineWidth;

                    // If we're dragging this keyframe, use mouse position for X
                    if (m_IsDraggingKeyframe && m_DraggedBoneName == joint.name && m_DraggedKeyframeIndex == i)
                    {
                        x = mousePos.x;
                        x = std::max(timelineMin.x, std::min(x, timelineMin.x + timelineWidth)); // Clamp to timeline bounds
                    }

                    // Diamond center (vertically centered in row)
                    ImVec2 center(x, timelineMin.y + rowHeight * 0.5f);
                    float size = 3.0f;
                    float hitTestSize = 6.0f; // Larger hit area for easier clicking

                    // Diamond vertices (rotated square)
                    ImVec2 top(center.x, center.y - size);
                    ImVec2 right(center.x + size, center.y);
                    ImVec2 bottom(center.x, center.y + size);
                    ImVec2 left(center.x - size, center.y);

                    // Check if mouse is hovering over this keyframe
                    bool isHovered = (mousePos.x >= center.x - hitTestSize && mousePos.x <= center.x + hitTestSize &&
                                      mousePos.y >= center.y - hitTestSize && mousePos.y <= center.y + hitTestSize);

                    // Determine color based on state
                    ImU32 fillColor = IM_COL32(255, 200, 0, 255);  // Default gold
                    ImU32 outlineColor = IM_COL32(200, 150, 0, 255);

                    if (m_IsDraggingKeyframe && m_DraggedBoneName == joint.name && m_DraggedKeyframeIndex == i)
                    {
                        // Being dragged - bright cyan
                        fillColor = IM_COL32(0, 255, 255, 255);
                        outlineColor = IM_COL32(0, 200, 200, 255);
                        size = 4.0f; // Slightly larger when dragging
                    }
                    else if (isHovered)
                    {
                        // Hovered - brighter yellow
                        fillColor = IM_COL32(255, 255, 100, 255);
                        outlineColor = IM_COL32(255, 200, 0, 255);
                        size = 4.0f; // Slightly larger when hovered
                    }

                    // Recalculate vertices with potentially new size
                    top = ImVec2(center.x, center.y - size);
                    right = ImVec2(center.x + size, center.y);
                    bottom = ImVec2(center.x, center.y + size);
                    left = ImVec2(center.x - size, center.y);

                    // Draw filled diamond
                    drawList->AddQuadFilled(top, right, bottom, left, fillColor);

                    // Draw outline for better visibility
                    drawList->AddQuad(top, right, bottom, left, outlineColor, 1.5f);

                    // Handle mouse interactions
                    if (isHovered && !m_IsDraggingKeyframe)
                    {
                        // Mark that we're hovering a keyframe on this bone
                        hoveredAnyKeyframe = true;

                        // Set hover state
                        m_HoveredKeyframeIndex = (int)i;
                        m_HoveredBoneName = joint.name;

                        // Show tooltip
                        ImGui::SetTooltip("Keyframe at %.2fs\nLeft-click to drag\nRight-click to delete", kf.timeStamp);

                        // Start dragging on left-click
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            m_IsDraggingKeyframe = true;
                            m_DraggedBoneName = joint.name;
                            m_DraggedKeyframeIndex = i;
                        }

                        // Delete on right-click
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                        {
                            // Create and execute remove command
                            KeyframeCommand cmd;
                            cmd.type = KeyframeCommand::REMOVE;
                            cmd.boneName = joint.name;
                            cmd.keyframeIndex = i;
                            cmd.keyframe = kf; // Store the keyframe data for undo
                            ExecuteCommand(cmd);
                            break; // Exit loop since we modified the array
                        }
                    }
                }

                // Handle drag release
                if (m_IsDraggingKeyframe && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    // Calculate new timestamp from mouse position
                    float newTime = ((mousePos.x - timelineMin.x) / timelineWidth) * duration;
                    newTime = std::max(0.0f, std::min(newTime, duration)); // Clamp to clip duration

                    // Get the old timestamp before moving
                    auto* track = m_Animator->GetTrackMutable(m_SelectedClipIndex, m_DraggedBoneName);
                    if (track && m_DraggedKeyframeIndex < track->size())
                    {
                        float oldTime = (*track)[m_DraggedKeyframeIndex].timeStamp;

                        // Only create command if time actually changed
                        if (std::abs(oldTime - newTime) > 0.001f)
                        {
                            // Create and execute move command
                            KeyframeCommand cmd;
                            cmd.type = KeyframeCommand::MOVE;
                            cmd.boneName = m_DraggedBoneName;
                            cmd.keyframeIndex = m_DraggedKeyframeIndex;
                            cmd.oldTime = oldTime;
                            cmd.newTime = newTime;
                            ExecuteCommand(cmd);
                        }
                    }

                    m_IsDraggingKeyframe = false;
                }

                // NOTE: "Click to add keyframe" feature disabled
                // This is not Unity-like, and without bone manipulation capability,
                // adding keyframes with identity transforms would break animations.
                // TODO: Re-enable once we can capture actual bone transforms from 3D viewport
            }
        }
    }

    // Add invisible dummy item to properly extend window bounds
    ImGui::Dummy(ImVec2(timelineWidth, rowHeight));

    // Return to COLUMN 0 for next bone
    ImGui::NextColumn();

    // === Recurse to children if node is open ===
    if (nodeOpen && !joint.children.empty())
    {
        for (const auto& child : joint.children)
        {
            RenderBoneTrack(child, duration);
        }
        ImGui::TreePop();
    }
}

// ========== Camera Functions ==========

void AnimationTimelinePanel::UpdateCamera()
{
    // Calculate camera position based on spherical coordinates
    float x = m_CameraDistance * cos(m_CameraPitch) * sin(m_CameraYaw);
    float y = m_CameraDistance * sin(m_CameraPitch);
    float z = m_CameraDistance * cos(m_CameraPitch) * cos(m_CameraYaw);

    m_CameraPosition = m_CameraTarget + glm::vec3(x, y, z);
}

void AnimationTimelinePanel::HandleCameraControls()
{
    // Only process input if the viewport is actually hovered
    if (!ImGui::IsItemHovered()) return;

    // Check if Animation Timeline window is focused (prevents input bleed-through)
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        // Another window is focused on top - don't process input
        m_IsOrbitingCamera = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Right mouse button: orbit camera
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        if (!m_IsOrbitingCamera)
        {
            m_IsOrbitingCamera = true;
            m_LastMousePos = ImGui::GetMousePos();
        }

        ImVec2 currentMousePos = ImGui::GetMousePos();
        ImVec2 delta = ImVec2(
            currentMousePos.x - m_LastMousePos.x,
            currentMousePos.y - m_LastMousePos.y
        );

        // Update camera angles
        m_CameraYaw -= delta.x * 0.005f;
        m_CameraPitch -= delta.y * 0.005f;

        // Clamp pitch to avoid flipping
        m_CameraPitch = glm::clamp(m_CameraPitch, -1.5f, 1.5f);

        m_LastMousePos = currentMousePos;
    }
    else
    {
        m_IsOrbitingCamera = false;
    }

    // Mouse wheel: zoom
    if (io.MouseWheel != 0.0f)
    {
        m_CameraDistance -= io.MouseWheel * 0.5f;
        m_CameraDistance = glm::clamp(m_CameraDistance, 0.1f, 100.0f);
    }
}

void AnimationTimelinePanel::HandleBonePicking()
{
    // CRITICAL: Don't pick bones while gizmo is being used OR hovered (prevents click-through)
    if (ImGuizmo::IsUsing()) return;
    if (ImGuizmo::IsOver()) return;  // Also block when mouse is over gizmo (even if not dragging yet)

    // Only process if viewport is hovered and window is focused
    if (!ImGui::IsItemHovered()) return;
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    if (!m_Animator || !m_HasModel) return;

    // Get mouse position relative to viewport
    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 viewportScreenPos = ImGui::GetItemRectMin();  // Top-left corner of viewport
    m_ViewportMousePos = ImVec2(
        mousePos.x - viewportScreenPos.x,
        mousePos.y - viewportScreenPos.y
    );

    // Convert to normalized device coordinates [-1, 1]
    float ndcX = (m_ViewportMousePos.x / m_ViewportSize.x) * 2.0f - 1.0f;
    float ndcY = 1.0f - (m_ViewportMousePos.y / m_ViewportSize.y) * 2.0f;  // Flip Y

    // Compute ray in world space
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(m_CameraPosition, m_CameraTarget, glm::vec3(0, 1, 0));
    glm::mat4 invProjView = glm::inverse(proj * view);

    // Near and far points in world space
    glm::vec4 rayStartNDC(ndcX, ndcY, -1.0f, 1.0f);  // Near plane
    glm::vec4 rayEndNDC(ndcX, ndcY, 1.0f, 1.0f);     // Far plane

    glm::vec4 rayStartWorld = invProjView * rayStartNDC;
    glm::vec4 rayEndWorld = invProjView * rayEndNDC;

    rayStartWorld /= rayStartWorld.w;
    rayEndWorld /= rayEndWorld.w;

    glm::vec3 rayOrigin = glm::vec3(rayStartWorld);
    glm::vec3 rayDir = glm::normalize(glm::vec3(rayEndWorld) - glm::vec3(rayStartWorld));

    // Get bone positions - use manual poses if available for accurate picking during gizmo manipulation
    auto boneLines = m_Animator->GetSkeletonLines();  // Get default lines first

    if (m_HasManualPoses)
    {
        // Rebuild bone lines with manual poses (same logic as RenderSkeleton)
        boneLines.clear();  // Clear and rebuild with manual poses

        const Boom::Joint& root = m_Animator->GetRoot();

        std::function<void(const Boom::Joint&, const glm::vec3&)> buildBoneLinesFunc;
        buildBoneLinesFunc = [&](const Boom::Joint& joint, const glm::vec3& parentPos)
        {
            glm::mat4 worldTransform = GetBoneWorldTransform(joint.name);
            glm::vec3 bonePos = glm::vec3(worldTransform[3]);

            Boom::Animator::BoneLine boneLine;  // Correct type
            boneLine.start = parentPos;
            boneLine.end = bonePos;
            boneLine.boneName = joint.name;
            boneLines.push_back(boneLine);

            for (const auto& child : joint.children)
            {
                buildBoneLinesFunc(child, bonePos);
            }
        };

        glm::mat4 rootTransform = GetBoneWorldTransform(root.name);
        glm::vec3 rootPos = glm::vec3(rootTransform[3]);

        for (const auto& child : root.children)
        {
            buildBoneLinesFunc(child, rootPos);
        }
    }
    // else: keep the default boneLines from GetSkeletonLines()

    if (boneLines.empty()) return;

    // Apply same transform as skeleton rendering
    glm::mat4 boneTransform = glm::scale(glm::mat4(1.0f), glm::vec3(m_ModelScale));

    // Find closest bone to ray
    float closestDist = FLT_MAX;
    std::string closestBone;

    // IMPROVED: Smaller, more precise selection radius
    // Use a fixed screen-space equivalent radius that scales with camera distance
    // But clamp it to prevent huge selection areas when far away
    float baseRadius = m_CameraDistance * 0.03f;  // Reduced from 0.08 for better precision
    const float selectionRadius = glm::clamp(baseRadius, 0.05f, 0.3f);  // Clamp between 5cm and 30cm

    for (const auto& boneLine : boneLines)
    {
        // Transform bone positions (same as RenderSkeleton)
        glm::vec4 start4 = boneTransform * glm::vec4(boneLine.start, 1.0f);
        glm::vec4 end4 = boneTransform * glm::vec4(boneLine.end, 1.0f);
        glm::vec3 boneStart = glm::vec3(start4);
        glm::vec3 boneEnd = glm::vec3(end4);

        // Compute distance from ray to bone LINE SEGMENT (not just start point)
        // This makes selection work for entire bone length, not just the joint
        glm::vec3 boneDir = boneEnd - boneStart;
        float boneLength = glm::length(boneDir);

        // Handle zero-length bones (shouldn't happen but be safe)
        if (boneLength < 0.001f)
        {
            boneDir = glm::vec3(0, 1, 0);
            boneLength = 0.001f;
        }
        else
        {
            boneDir /= boneLength;  // Normalize
        }

        // Line-segment to line distance (closest approach between ray and bone segment)
        // Based on: http://geomalgorithms.com/a07-_distance.html
        glm::vec3 w0 = rayOrigin - boneStart;
        float a = glm::dot(rayDir, rayDir);        // Always 1 (ray is normalized)
        float b = glm::dot(rayDir, boneDir);
        float c = glm::dot(boneDir, boneDir);      // Always 1 (bone dir normalized)
        float d = glm::dot(rayDir, w0);
        float e = glm::dot(boneDir, w0);

        float denom = a * c - b * b;
        float sc, tc;

        // Compute parameters for closest points
        if (denom < 0.001f)
        {
            // Lines are parallel
            sc = 0.0f;
            tc = (b > c ? d / b : e / c);
        }
        else
        {
            sc = (b * e - c * d) / denom;
            tc = (a * e - b * d) / denom;
        }

        // Clamp tc to bone segment [0, boneLength]
        tc = glm::clamp(tc, 0.0f, boneLength);

        // Compute closest points
        glm::vec3 closestOnRay = rayOrigin + sc * rayDir;
        glm::vec3 closestOnBone = boneStart + tc * boneDir;

        float dist = glm::length(closestOnRay - closestOnBone);

        // Check if within selection radius and closer than previous bones
        if (dist < selectionRadius && dist < closestDist)
        {
            closestDist = dist;
            closestBone = boneLine.boneName;
        }
    }

    // Update hover state
    m_HoveredBoneNameViewport = closestBone;

    // Handle mouse click for selection
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !closestBone.empty())
    {
        m_SelectedBoneName = closestBone;

        // Sync with global context for track list highlighting (same as RenderBoneTrack does)
        if (m_Ctx)
        {
            m_Ctx->SelectedBoneName = closestBone;
        }
    }
}

glm::mat4 AnimationTimelinePanel::GetBoneWorldTransform(const std::string& boneName)
{
    // Walk the bone hierarchy and accumulate transforms to get world matrix
    // This matches how GetSkeletonLines() computes bone positions

    if (!m_Animator) return glm::mat4(1.0f);

    // Helper lambda to recursively find bone and compute world transform
    std::function<bool(const Boom::Joint&, const glm::mat4&, glm::mat4&)> findBone;
    findBone = [&](const Boom::Joint& joint, const glm::mat4& parentWorld, glm::mat4& outWorld) -> bool
    {
        // Compute local transform for this bone
        glm::mat4 localTransform = glm::mat4(1.0f);

        // Check if we have manual override for this bone
        auto it = m_ManualBonePoses.find(joint.name);
        if (it != m_ManualBonePoses.end())
        {
            // Use manual pose
            const BonePose& pose = it->second;
            localTransform = glm::translate(glm::mat4(1.0f), pose.position);
            localTransform *= glm::mat4_cast(pose.rotation);
            localTransform = glm::scale(localTransform, pose.scale);
        }
        else
        {
            // Get transform from animation
            if (m_SelectedClipIndex >= 0 && m_SelectedClipIndex < (int)m_Animator->GetClipCount())
            {
                const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
                const auto* keys = clip->GetTrack(joint.name);

                if (keys && keys->size() >= 2)
                {
                    // Find previous and next keyframes manually (GetPreviousAndNextFrames is private)
                    Boom::KeyFrame prev = (*keys)[0];
                    Boom::KeyFrame next = (*keys)[keys->size() - 1];

                    for (size_t i = 0; i < keys->size() - 1; ++i)
                    {
                        if ((*keys)[i].timeStamp <= m_CurrentTime && (*keys)[i + 1].timeStamp >= m_CurrentTime)
                        {
                            prev = (*keys)[i];
                            next = (*keys)[i + 1];
                            break;
                        }
                    }

                    float progression = 0.0f;
                    float dt = next.timeStamp - prev.timeStamp;
                    if (dt > 0.0f)
                    {
                        progression = (m_CurrentTime - prev.timeStamp) / dt;
                    }

                    // Interpolate between keyframes
                    glm::vec3 pos = glm::mix(prev.position, next.position, progression);
                    glm::quat rot = glm::slerp(prev.rotation, next.rotation, progression);
                    glm::vec3 scl = glm::mix(prev.scale, next.scale, progression);

                    localTransform = glm::translate(glm::mat4(1.0f), pos);
                    localTransform *= glm::mat4_cast(rot);
                    localTransform = glm::scale(localTransform, scl);
                }
            }
        }

        // Compute world transform
        glm::mat4 worldTransform = parentWorld * localTransform;

        // Is this the bone we're looking for?
        if (joint.name == boneName)
        {
            outWorld = worldTransform;
            return true;
        }

        // Search children
        for (const auto& child : joint.children)
        {
            if (findBone(child, worldTransform, outWorld))
                return true;
        }

        return false;
    };

    glm::mat4 result;
    const Boom::Joint& root = m_Animator->GetRoot();
    if (findBone(root, glm::mat4(1.0f), result))
    {
        return result;
    }

    return glm::mat4(1.0f);
}

std::string AnimationTimelinePanel::GetParentBoneName(const std::string& boneName)
{
    if (!m_Animator) return "";

    const Boom::Joint& root = m_Animator->GetRoot();

    // Lambda to search for parent
    std::function<bool(const Boom::Joint&, std::string&)> findParent;
    findParent = [&](const Boom::Joint& joint, std::string& parentName) -> bool
    {
        // Check if any direct child matches our bone
        for (const auto& child : joint.children)
        {
            if (child.name == boneName)
            {
                parentName = joint.name;
                return true;
            }

            // Recurse into children
            if (findParent(child, parentName))
                return true;
        }
        return false;
    };

    std::string parent;
    if (findParent(root, parent))
        return parent;

    return "";  // No parent (root bone)
}

Boom::KeyFrame AnimationTimelinePanel::CaptureCurrentBoneTransform(const std::string& boneName)
{
    Boom::KeyFrame kf;
    kf.timeStamp = -1.0f;  // Invalid by default

    if (!m_Animator || boneName.empty() || m_SelectedClipIndex < 0)
    {
        return kf;  // Invalid
    }

    kf.timeStamp = m_CurrentTime;  // Capture at current timeline time

    // Priority 1: Check if we have a manual override from gizmo manipulation
    auto it = m_ManualBonePoses.find(boneName);
    if (it != m_ManualBonePoses.end())
    {
        // Use the manually posed transform (already in LOCAL space)
        const BonePose& pose = it->second;
        kf.position = pose.position;
        kf.rotation = pose.rotation;
        kf.scale = pose.scale;

        BOOM_INFO("[Keyframe Capture] Using manual pose for bone '{}'", boneName.c_str());
        return kf;
    }

    // Priority 2: Get current bone transform from animation at current time
    // We need to extract LOCAL space transform (relative to parent)

    // Get the bone's world transform at current time
    glm::mat4 boneWorld = GetBoneWorldTransform(boneName);

    // Get parent's world transform
    std::string parentName = GetParentBoneName(boneName);
    glm::mat4 parentWorld = glm::mat4(1.0f);

    if (!parentName.empty())
    {
        parentWorld = GetBoneWorldTransform(parentName);
    }

    // Convert to local space: local = inverse(parent) * world
    glm::mat4 localTransform = glm::inverse(parentWorld) * boneWorld;

    // Decompose local transform into position/rotation/scale
    glm::vec3 scale, translation, skew;
    glm::quat rotation;
    glm::vec4 perspective;
    glm::decompose(localTransform, scale, rotation, translation, skew, perspective);

    kf.position = translation;
    kf.rotation = rotation;
    kf.scale = scale;

    BOOM_INFO("[Keyframe Capture] Captured animated pose for bone '{}' - pos:({:.2f}, {:.2f}, {:.2f})",
              boneName.c_str(), translation.x, translation.y, translation.z);

    return kf;
}

// ========== Bone Pose Manipulation ==========

void AnimationTimelinePanel::SetBonePose(const std::string& boneName, const BonePose& pose)
{
    m_ManualBonePoses[boneName] = pose;
    m_HasManualPoses = !m_ManualBonePoses.empty();
}

void AnimationTimelinePanel::ClearBonePose(const std::string& boneName)
{
    m_ManualBonePoses.erase(boneName);
    m_HasManualPoses = !m_ManualBonePoses.empty();
}

void AnimationTimelinePanel::ClearAllBonePoses()
{
    m_ManualBonePoses.clear();
    m_HasManualPoses = false;
}

void AnimationTimelinePanel::ApplyManualBonePosesToTransforms(std::vector<glm::mat4>& transforms)
{
    // Apply manual bone poses to the transform matrices for skinning
    // This makes the model mesh move with the manually posed bones
    // CRITICAL: This must match GetBoneWorldTransform() logic exactly for consistency

    if (!m_Animator || m_ManualBonePoses.empty()) return;

    const Boom::Joint& root = m_Animator->GetRoot();
    const auto* clip = (m_SelectedClipIndex >= 0 && m_SelectedClipIndex < (int)m_Animator->GetClipCount())
                       ? m_Animator->GetClip(m_SelectedClipIndex)
                       : nullptr;

    // Recursively rebuild all transforms using manual poses where applicable
    std::function<void(const Boom::Joint&, const glm::mat4&, bool)> rebuild;
    rebuild = [&](const Boom::Joint& joint, const glm::mat4& parentWorld, bool parentHadManualPose)
    {
        glm::mat4 localTransform = glm::mat4(1.0f);
        bool hasManualPose = false;

        // Check if this bone has a manual pose override
        auto it = m_ManualBonePoses.find(joint.name);
        if (it != m_ManualBonePoses.end())
        {
            // Use manual local transform (from gizmo manipulation)
            const BonePose& pose = it->second;
            localTransform = glm::translate(glm::mat4(1.0f), pose.position);
            localTransform *= glm::mat4_cast(pose.rotation);
            localTransform = glm::scale(localTransform, pose.scale);
            hasManualPose = true;
        }
        else
        {
            // Get transform from animation at current time
            if (clip)
            {
                const auto* keys = clip->GetTrack(joint.name);
                if (keys && keys->size() >= 2)
                {
                    // Find keyframes for interpolation
                    Boom::KeyFrame prev = (*keys)[0];
                    Boom::KeyFrame next = (*keys)[keys->size() - 1];

                    for (size_t i = 0; i < keys->size() - 1; ++i)
                    {
                        if ((*keys)[i].timeStamp <= m_CurrentTime && (*keys)[i + 1].timeStamp >= m_CurrentTime)
                        {
                            prev = (*keys)[i];
                            next = (*keys)[i + 1];
                            break;
                        }
                    }

                    float progression = 0.0f;
                    float dt = next.timeStamp - prev.timeStamp;
                    if (dt > 0.0f)
                    {
                        progression = (m_CurrentTime - prev.timeStamp) / dt;
                    }

                    // Interpolate
                    glm::vec3 pos = glm::mix(prev.position, next.position, progression);
                    glm::quat rot = glm::slerp(prev.rotation, next.rotation, progression);
                    glm::vec3 scl = glm::mix(prev.scale, next.scale, progression);

                    localTransform = glm::translate(glm::mat4(1.0f), pos);
                    localTransform *= glm::mat4_cast(rot);
                    localTransform = glm::scale(localTransform, scl);
                }
            }
        }

        // Compute world transform
        glm::mat4 worldTransform = parentWorld * localTransform;

        // Update skinning transform for GPU: skinning = world * offset
        transforms[joint.index] = worldTransform * joint.offset;

        // Recurse to children
        for (const auto& child : joint.children)
        {
            rebuild(child, worldTransform, hasManualPose || parentHadManualPose);
        }
    };

    rebuild(root, glm::mat4(1.0f), false);  // false = root has no parent with manual pose
}

void AnimationTimelinePanel::HandleGizmo(const ImVec2& viewportMin, const ImVec2& viewportSize)
{
    // Only show gizmo if we have a selected bone
    if (m_SelectedBoneName.empty() || !m_Animator || !m_HasModel) return;

    // Get current bone world transform (includes animation + manual overrides)
    glm::mat4 boneWorldMatrix = GetBoneWorldTransform(m_SelectedBoneName);

    // Apply model scale
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(m_ModelScale));
    boneWorldMatrix = scaleMatrix * boneWorldMatrix;

    // Setup ImGuizmo
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewportMin.x, viewportMin.y, viewportSize.x, viewportSize.y);
    ImGuizmo::SetGizmoSizeClipSpace(0.15f);

    // Enable ImGuizmo to receive input
    ImGuizmo::Enable(true);
    ImGuizmo::AllowAxisFlip(true);

    // Setup view and projection matrices
    glm::mat4 view = glm::lookAt(m_CameraPosition, m_CameraTarget, glm::vec3(0, 1, 0));
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // Store the original matrix before manipulation
    glm::mat4 originalMatrix = boneWorldMatrix;

    // Render the gizmo (modifies boneWorldMatrix if user drags it)
    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        (ImGuizmo::OPERATION)m_GizmoOperation,
        (ImGuizmo::MODE)m_GizmoMode,
        glm::value_ptr(boneWorldMatrix),
        nullptr,
        m_UseSnap ? m_SnapValues : nullptr
    );

    // Check if user is manipulating the gizmo
    // IMPORTANT: Only process gizmo input if our viewport is hovered/focused
    // This prevents the main viewport from also responding to the same gizmo
    bool isUsing = ImGuizmo::IsUsing();
    bool isOver = ImGuizmo::IsOver();
    bool isOurGizmo = isOver || m_GizmoWasUsing;  // IsOver or we started the drag

    // UNITY-STYLE BONE MANIPULATION
    // Key difference: Apply changes in REAL-TIME during drag, not just on release

    // Capture pose when starting manipulation (for undo)
    if (isUsing && !m_GizmoWasUsing && isOurGizmo)
    {
        // User just started manipulating - capture the BEFORE state
        m_BoneBeingManipulated = m_SelectedBoneName;
        m_HasPoseBeforeManipulation = false;

        // Try to get existing manual pose
        auto it = m_ManualBonePoses.find(m_SelectedBoneName);
        if (it != m_ManualBonePoses.end())
        {
            // Bone already has manual pose - use that as the before state
            m_BonePoseBeforeManipulation = it->second;
            m_HasPoseBeforeManipulation = true;
        }
        else
        {
            // No manual pose yet - capture current animated pose as before state
            glm::mat4 boneWorld = GetBoneWorldTransform(m_SelectedBoneName);
            std::string parentName = GetParentBoneName(m_SelectedBoneName);
            glm::mat4 parentWorld = glm::mat4(1.0f);
            if (!parentName.empty())
            {
                parentWorld = GetBoneWorldTransform(parentName);
            }

            glm::mat4 localTransform = glm::inverse(parentWorld) * boneWorld;
            glm::vec3 scale, translation, skew;
            glm::quat rotation;
            glm::vec4 perspective;
            glm::decompose(localTransform, scale, rotation, translation, skew, perspective);

            m_BonePoseBeforeManipulation.position = translation;
            m_BonePoseBeforeManipulation.rotation = rotation;
            m_BonePoseBeforeManipulation.scale = scale;
            m_HasPoseBeforeManipulation = true;
        }
    }

    // REAL-TIME UPDATE: Apply transformation during drag (UNITY BEHAVIOR)
    if (isUsing && isOurGizmo)
    {
        // Check if the matrix actually changed - check BOTH position AND rotation
        glm::vec3 oldPos = glm::vec3(originalMatrix[3]);
        glm::vec3 newPos = glm::vec3(boneWorldMatrix[3]);
        float posDiff = glm::distance(oldPos, newPos);

        // Extract rotation from matrices
        glm::quat oldRot = glm::quat_cast(originalMatrix);
        glm::quat newRot = glm::quat_cast(boneWorldMatrix);
        float rotDot = glm::abs(glm::dot(oldRot, newRot));
        bool rotChanged = (rotDot < 0.9999f);  // Quaternions differ if dot product isn't ~1.0

        float detDiff = glm::abs(glm::determinant(originalMatrix) - glm::determinant(boneWorldMatrix));

        bool matrixChanged = (posDiff > 0.00001f || rotChanged || detDiff > 0.00001f);

        if (matrixChanged)
        {
            // CRITICAL FIX: Remove model scale to get actual bone world transform
            glm::mat4 boneWorld = glm::inverse(scaleMatrix) * boneWorldMatrix;

            // Get parent bone's world transform
            std::string parentName = GetParentBoneName(m_SelectedBoneName);
            glm::mat4 parentWorld = glm::mat4(1.0f);

            if (!parentName.empty())
            {
                // Temporarily remove our bone's manual pose to get parent's clean transform
                auto it = m_ManualBonePoses.find(m_SelectedBoneName);
                BonePose tempPose = {};  // Initialize to avoid warning
                bool hadPose = false;
                if (it != m_ManualBonePoses.end())
                {
                    tempPose = it->second;
                    hadPose = true;
                    m_ManualBonePoses.erase(it);
                }

                // Get parent's world transform
                parentWorld = GetBoneWorldTransform(parentName);

                // Restore our pose
                if (hadPose)
                {
                    m_ManualBonePoses[m_SelectedBoneName] = tempPose;
                }
            }

            // Convert to local space: local = inverse(parent) * world
            glm::mat4 localTransform = glm::inverse(parentWorld) * boneWorld;

            // Extract transform components
            glm::vec3 scale, translation, skew;
            glm::quat rotation;
            glm::vec4 perspective;
            bool decomposed = glm::decompose(localTransform, scale, rotation, translation, skew, perspective);

            if (decomposed)
            {
                // Normalize rotation to prevent drift and ensure valid quaternion
                rotation = glm::normalize(rotation);

                // Ensure scale is reasonable (prevent collapse or explosion)
                for (int i = 0; i < 3; i++)
                {
                    if (scale[i] < 0.001f) scale[i] = 0.001f;
                    if (scale[i] > 1000.0f) scale[i] = 1000.0f;
                }

                // Create new pose and apply immediately for real-time feedback
                BonePose newPose;
                newPose.position = translation;
                newPose.rotation = rotation;
                newPose.scale = scale;

                m_ManualBonePoses[m_SelectedBoneName] = newPose;
                m_HasManualPoses = true;
            }
        }
    }

    // On release: Create undo command
    if (!isUsing && m_GizmoWasUsing && isOurGizmo)
    {
        BOOM_INFO("[AnimTimeline Gizmo] User released gizmo on bone: {}", m_SelectedBoneName);

        // Get the final pose that was applied
        auto it = m_ManualBonePoses.find(m_SelectedBoneName);
        if (it != m_ManualBonePoses.end() && m_HasPoseBeforeManipulation)
        {
            BonePose finalPose = it->second;

            // Check if pose actually changed
            const float posThreshold = 0.0001f;
            const float rotThreshold = 0.0001f;
            const float scaleThreshold = 0.0001f;

            bool poseChanged =
                glm::distance(m_BonePoseBeforeManipulation.position, finalPose.position) > posThreshold ||
                glm::abs(glm::dot(m_BonePoseBeforeManipulation.rotation, finalPose.rotation)) < (1.0f - rotThreshold) ||
                glm::distance(m_BonePoseBeforeManipulation.scale, finalPose.scale) > scaleThreshold;

            if (poseChanged && m_Owner && m_Owner->GetCommandHistory())
            {
                auto cmd = std::make_unique<BonePoseCommand>(
                    this,
                    m_SelectedBoneName,
                    m_BonePoseBeforeManipulation,
                    finalPose
                );

                // Temporarily set the old pose, then Execute() will apply the new one
                m_ManualBonePoses[m_SelectedBoneName] = m_BonePoseBeforeManipulation;
                m_Owner->GetCommandHistory()->Execute(std::move(cmd));
            }
        }
    }

    m_GizmoWasUsing = isUsing;
}

void AnimationTimelinePanel::ResetCamera()
{
    m_CameraDistance = 3.0f;
    m_CameraYaw = 0.0f;
    m_CameraPitch = 0.3f;
    m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);
    UpdateCamera();
}

void AnimationTimelinePanel::FrameModel()
{
    if (!m_Model) return;

    float radius = 2.0f * m_ModelScale;
    m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);

    float fovRadians = glm::radians(45.0f);
    m_CameraDistance = (radius * 2.5f) / std::tan(fovRadians * 0.5f);
    m_CameraDistance = glm::clamp(m_CameraDistance, 0.5f, 100.0f);

    UpdateCamera();
}

// ========== 3D Rendering Functions ==========

void AnimationTimelinePanel::RenderModel()
{
    if (!m_Ctx || !m_Ctx->renderer || !m_Model)
    {
        return;
    }

    // Set aspect ratio for viewport
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    m_Ctx->renderer->SetAspectOverride(aspect);

    // Set up camera
    Boom::Camera3D camera{};
    camera.FOV = 45.0f;
    camera.nearPlane = 0.01f;
    camera.farPlane = 1000.0f;

    // Compute camera transform
    glm::vec3 direction = glm::normalize(m_CameraTarget - m_CameraPosition);
    float yaw = atan2(-direction.x, -direction.z);
    float pitch = asin(direction.y);
    float roll = 0.0f;

    glm::vec3 eulerAngles = glm::vec3(glm::degrees(pitch), glm::degrees(yaw), glm::degrees(roll));

    Boom::Transform3D cameraTransform{};
    cameraTransform.translate = m_CameraPosition;
    cameraTransform.rotate = eulerAngles;
    cameraTransform.scale = glm::vec3(1.0f);

    m_Ctx->renderer->SetCamera(camera, cameraTransform);

    // Set joints if model has skeleton
    if (m_Animator)
    {
        // Sync animator to timeline time and update joint transforms
        m_Animator->SetTime(m_CurrentTime);
        m_Animator->UpdateJointsFromCurrentTime();

        // Get the updated transforms
        auto& transforms = m_Animator->GetJoints();

        // Apply manual bone poses if we're manipulating bones with gizmo
        if (m_HasManualPoses)
        {
            ApplyManualBonePosesToTransforms(transforms);
        }

        m_Ctx->renderer->SetJoints(transforms);
    }

    // CRITICAL: Call SetCamera again right before Draw in case something overwrote it
    m_Ctx->renderer->SetCamera(camera, cameraTransform);

    // Model transform (with preview scale)
    glm::mat4 desiredMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(m_ModelScale));
    glm::mat4 assetMatrix = m_Model->modelTransform.Matrix();
    glm::mat4 finalMatrix = desiredMatrix * glm::inverse(assetMatrix);

    glm::vec3 finalTranslate = glm::vec3(finalMatrix[3]);
    glm::vec3 finalScale;
    finalScale.x = glm::length(glm::vec3(finalMatrix[0]));
    finalScale.y = glm::length(glm::vec3(finalMatrix[1]));
    finalScale.z = glm::length(glm::vec3(finalMatrix[2]));

    glm::mat4 rotMatrix = finalMatrix;
    rotMatrix[0] /= finalScale.x;
    rotMatrix[1] /= finalScale.y;
    rotMatrix[2] /= finalScale.z;
    glm::quat modelRotQuat = glm::quat_cast(rotMatrix);
    glm::vec3 finalRotate = glm::degrees(glm::eulerAngles(modelRotQuat));

    Boom::Transform3D modelTransform{};
    modelTransform.translate = finalTranslate;
    modelTransform.rotate = finalRotate;
    modelTransform.scale = finalScale;

    // Default material
    Boom::PbrMaterial material{};
    material.albedo = glm::vec3(0.7f, 0.7f, 0.7f);
    material.roughness = 0.5f;
    material.metallic = 0.0f;

    // Draw model
    m_Ctx->renderer->Draw(m_Model, modelTransform, material);

    m_Ctx->renderer->ClearAspectOverride();
}

void AnimationTimelinePanel::RenderSkeleton()
{
    if (!m_Animator || !m_Ctx || !m_Owner) return;

    auto debugShader = m_Owner->GetDebugLinesShader();
    if (!debugShader) return;

    glm::mat4 view = glm::lookAt(m_CameraPosition, m_CameraTarget, glm::vec3(0, 1, 0));
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // If we have manual poses, rebuild bone lines to show real-time updates during gizmo drag
    auto boneLines = m_Animator->GetSkeletonLines();  // Get default lines first

    if (m_HasManualPoses)
    {
        // REAL-TIME UPDATE: Rebuild bone lines using manual poses
        boneLines.clear();  // Clear and rebuild with manual poses

        const Boom::Joint& root = m_Animator->GetRoot();

        // Lambda to recursively build bone lines with manual pose overrides
        std::function<void(const Boom::Joint&, const glm::vec3&)> buildBoneLinesFunc;
        buildBoneLinesFunc = [&](const Boom::Joint& joint, const glm::vec3& parentPos)
        {
            // Get bone's world position (respects manual overrides)
            glm::mat4 worldTransform = GetBoneWorldTransform(joint.name);
            glm::vec3 bonePos = glm::vec3(worldTransform[3]);

            // Create line from parent to this bone
            Boom::Animator::BoneLine boneLine;  // Correct type
            boneLine.start = parentPos;
            boneLine.end = bonePos;
            boneLine.boneName = joint.name;
            boneLines.push_back(boneLine);

            // Recurse to children
            for (const auto& child : joint.children)
            {
                buildBoneLinesFunc(child, bonePos);
            }
        };

        // Start from root
        glm::mat4 rootTransform = GetBoneWorldTransform(root.name);
        glm::vec3 rootPos = glm::vec3(rootTransform[3]);

        for (const auto& child : root.children)
        {
            buildBoneLinesFunc(child, rootPos);
        }
    }
    // else: keep the default boneLines from GetSkeletonLines()

    if (boneLines.empty()) return;

    std::vector<Boom::LineVert> normalBones;
    std::vector<Boom::LineVert> hoveredBones;
    std::vector<Boom::LineVert> selectedBones;
    normalBones.reserve(boneLines.size() * 2);

    glm::vec4 boneColor = m_Ctx->BoneColor;
    glm::vec4 selectedBoneColor = m_Ctx->SelectedBoneColor;
    glm::vec4 hoveredBoneColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow for hover

    glm::mat4 boneTransform = glm::scale(glm::mat4(1.0f), glm::vec3(m_ModelScale));

    for (const auto& boneLine : boneLines)
    {
        bool isSelected = (!m_SelectedBoneName.empty() && boneLine.boneName == m_SelectedBoneName);
        bool isHovered = (!m_HoveredBoneNameViewport.empty() && boneLine.boneName == m_HoveredBoneNameViewport);

        glm::vec4 start4 = boneTransform * glm::vec4(boneLine.start, 1.0f);
        glm::vec4 end4 = boneTransform * glm::vec4(boneLine.end, 1.0f);
        glm::vec3 scaledStart = glm::vec3(start4);
        glm::vec3 scaledEnd = glm::vec3(end4);

        if (isSelected)
        {
            selectedBones.push_back({ scaledStart, selectedBoneColor });
            selectedBones.push_back({ scaledEnd, selectedBoneColor });
        }
        else if (isHovered)
        {
            hoveredBones.push_back({ scaledStart, hoveredBoneColor });
            hoveredBones.push_back({ scaledEnd, hoveredBoneColor });
        }
        else
        {
            normalBones.push_back({ scaledStart, boneColor });
            normalBones.push_back({ scaledEnd, boneColor });
        }
    }

    // Render in order: normal first, then hovered (thicker), then selected (thickest)
    if (!normalBones.empty())
    {
        debugShader->Draw(view, proj, normalBones, m_Ctx->BoneLineWidth, true);
    }

    if (!hoveredBones.empty())
    {
        debugShader->Draw(view, proj, hoveredBones, m_Ctx->BoneLineWidth * 2.0f, true);
    }

    if (!selectedBones.empty())
    {
        debugShader->Draw(view, proj, selectedBones, m_Ctx->BoneLineWidth * 3.0f, true);
    }
}

void AnimationTimelinePanel::RenderGrid()
{
    if (!m_Owner || !m_Ctx) return;

    auto debugShader = m_Owner->GetDebugLinesShader();
    if (!debugShader) return;

    glm::mat4 view = glm::lookAt(m_CameraPosition, m_CameraTarget, glm::vec3(0, 1, 0));
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    std::vector<Boom::LineVert> gridLines;
    const int gridSize = 10;
    const float gridStep = 1.0f;
    const glm::vec4 gridColor(0.3f, 0.3f, 0.3f, 1.0f);
    const glm::vec4 axisXColor(0.6f, 0.2f, 0.2f, 1.0f);
    const glm::vec4 axisZColor(0.2f, 0.2f, 0.6f, 1.0f);

    // Grid lines parallel to Z axis
    for (int i = -gridSize; i <= gridSize; ++i)
    {
        float x = i * gridStep;
        glm::vec3 start(x, 0.0f, -gridSize * gridStep);
        glm::vec3 end(x, 0.0f, gridSize * gridStep);
        glm::vec4 color = (i == 0) ? axisZColor : gridColor;
        gridLines.push_back({ start, color });
        gridLines.push_back({ end, color });
    }

    // Grid lines parallel to X axis
    for (int i = -gridSize; i <= gridSize; ++i)
    {
        float z = i * gridStep;
        glm::vec3 start(-gridSize * gridStep, 0.0f, z);
        glm::vec3 end(gridSize * gridStep, 0.0f, z);
        glm::vec4 color = (i == 0) ? axisXColor : gridColor;
        gridLines.push_back({ start, color });
        gridLines.push_back({ end, color });
    }

    if (!gridLines.empty())
    {
        debugShader->Draw(view, proj, gridLines, 1.0f, false);
    }
}

// ========== Model Loading (Standalone Mode) ==========

void AnimationTimelinePanel::LoadModel(const std::string& modelPath)
{
    if (!m_Ctx || !m_Ctx->assets) return;

    BOOM_INFO("[AnimationTimeline] Loading model in standalone mode: {}", modelPath);

    // Clear previous model
    ClearModel();

    // Search asset registry for model matching the path
    auto& modelMap = m_Ctx->assets->GetMap<Boom::ModelAsset>();

    Boom::ModelAsset* foundAsset = nullptr;
    Boom::AssetID foundID = Boom::EMPTY_ASSET;

    for (auto& [assetID, assetPtr] : modelMap)
    {
        if (assetID == Boom::EMPTY_ASSET) continue;

        auto* modelAsset = dynamic_cast<Boom::ModelAsset*>(assetPtr.get());
        if (modelAsset && (modelAsset->source == modelPath || modelAsset->name == modelPath))
        {
            foundAsset = modelAsset;
            foundID = assetID;
            break;
        }
    }

    if (!foundAsset || !foundAsset->data)
    {
        BOOM_ERROR("[AnimationTimeline] Model not found in asset registry: {}", modelPath);
        return;
    }

    // Load the model
    m_Model = foundAsset->data;
    m_LoadedModelPath = modelPath;
    m_HasModel = true;
    m_StandaloneMode = true;

    BOOM_INFO("[AnimationTimeline] Model loaded successfully in standalone mode: {} (HasJoints: {})",
              foundAsset->name, foundAsset->hasJoints);

    // Get animator from skeletal model if it has skeleton (CLONE for independence)
    if (foundAsset->hasJoints && m_Model->HasJoint())
    {
        auto skeletalModel = std::dynamic_pointer_cast<Boom::SkeletalModel>(m_Model);
        if (skeletalModel)
        {
            auto sourceAnimator = skeletalModel->GetAnimator();
            if (sourceAnimator)
            {
                // CRITICAL: Clone the animator for independent timeline
                m_SourceAnimator = sourceAnimator;
                m_Animator = sourceAnimator->Clone();

                // CRITICAL: Clear states to force clip mode (timeline doesn't use state machine)
                m_Animator->GetStates().clear();

                BOOM_INFO("[AnimationTimeline] Cloned animator from skeletal model for independent preview (states cleared)");

                // Auto-select first animation clip if available
                if (m_Animator->GetClipCount() > 0)
                {
                    m_SelectedClipIndex = 0;
                    m_Animator->PlayClip(0);
                    m_CurrentTime = 0.0f;
                    m_IsPlaying = false;  // Reset playback state
                    BOOM_INFO("[AnimationTimeline] Auto-selected first clip: {}",
                        m_Animator->GetClip(0)->name);
                }
                else
                {
                    m_SelectedClipIndex = -1;
                    m_IsPlaying = false;
                    BOOM_WARN("[AnimationTimeline] Animator has no clips");
                }
            }
            else
            {
                BOOM_WARN("[AnimationTimeline] Model has joints but no animator");
                m_Animator.reset();
                m_SourceAnimator.reset();
                m_SelectedClipIndex = -1;
            }
        }
        else
        {
            BOOM_WARN("[AnimationTimeline] Model has joints but is not SkeletalModel");
            m_Animator.reset();
            m_SourceAnimator.reset();
            m_SelectedClipIndex = -1;
        }
    }
    else
    {
        m_Animator.reset();
        m_SourceAnimator.reset();
        m_SelectedClipIndex = -1;
        BOOM_INFO("[AnimationTimeline] No animator needed (static model)");
    }

    // Reset preview scale when loading new model
    m_ModelScale = 1.0f;

    // Auto-frame the model to fit it in view
    FrameModel();
}

void AnimationTimelinePanel::ClearModel()
{
    m_Model.reset();
    m_Animator.reset();
    m_SourceAnimator.reset();
    m_SourceEntityID = entt::null;
    m_HasModel = false;
    m_StandaloneMode = false;
    m_LoadedModelPath.clear();
    m_SelectedBoneName.clear();
    m_SelectedClipIndex = -1;
    m_CurrentTime = 0.0f;
    m_IsPlaying = false;

    BOOM_INFO("[AnimationTimeline] Model cleared");
}

// ========== Undo/Redo System ==========

void AnimationTimelinePanel::ExecuteCommand(const KeyframeCommand& cmd)
{
    if (!m_Animator || m_SelectedClipIndex < 0) return;

    // Push to undo stack
    m_UndoStack.push_back(cmd);
    if (m_UndoStack.size() > MAX_UNDO_HISTORY)
    {
        m_UndoStack.erase(m_UndoStack.begin()); // Remove oldest
    }

    // Clear redo stack (new action invalidates redo history)
    m_RedoStack.clear();

    // Execute the command
    switch (cmd.type)
    {
    case KeyframeCommand::ADD:
        m_Animator->AddKeyframe(m_SelectedClipIndex, cmd.boneName, cmd.keyframe);
        BOOM_INFO("[Keyframes] Added keyframe at {:.2f}s on bone '{}'", (double)cmd.keyframe.timeStamp, cmd.boneName.c_str());
        break;

    case KeyframeCommand::REMOVE:
        m_Animator->RemoveKeyframe(m_SelectedClipIndex, cmd.boneName, cmd.keyframeIndex);
        BOOM_INFO("[Keyframes] Removed keyframe at index {} on bone '{}'", (int)cmd.keyframeIndex, cmd.boneName.c_str());
        break;

    case KeyframeCommand::MOVE:
        m_Animator->UpdateKeyframeTime(m_SelectedClipIndex, cmd.boneName, cmd.keyframeIndex, cmd.newTime);
        BOOM_INFO("[Keyframes] Moved keyframe on bone '{}' from {:.2f}s to {:.2f}s",
                  cmd.boneName.c_str(), (double)cmd.oldTime, (double)cmd.newTime);
        break;
    }
}

void AnimationTimelinePanel::Undo()
{
    if (m_UndoStack.empty() || !m_Animator || m_SelectedClipIndex < 0) return;

    KeyframeCommand cmd = m_UndoStack.back();
    m_UndoStack.pop_back();

    // Reverse the command
    switch (cmd.type)
    {
    case KeyframeCommand::ADD:
        // Undo add = remove the keyframe
        {
            auto* track = m_Animator->GetTrackMutable(m_SelectedClipIndex, cmd.boneName);
            if (track)
            {
                // Find the keyframe we just added
                for (size_t i = 0; i < track->size(); ++i)
                {
                    if (std::abs((*track)[i].timeStamp - cmd.keyframe.timeStamp) < 0.001f)
                    {
                        m_Animator->RemoveKeyframe(m_SelectedClipIndex, cmd.boneName, i);
                        BOOM_INFO("[Undo] Removed added keyframe at {:.2f}s on bone '{}'",
                                  (double)cmd.keyframe.timeStamp, cmd.boneName.c_str());
                        break;
                    }
                }
            }
        }
        break;

    case KeyframeCommand::REMOVE:
        // Undo remove = add it back
        m_Animator->AddKeyframe(m_SelectedClipIndex, cmd.boneName, cmd.keyframe);
        BOOM_INFO("[Undo] Restored removed keyframe at {:.2f}s on bone '{}'",
                  (double)cmd.keyframe.timeStamp, cmd.boneName.c_str());
        break;

    case KeyframeCommand::MOVE:
        // Undo move = move it back to old time
        {
            auto* track = m_Animator->GetTrackMutable(m_SelectedClipIndex, cmd.boneName);
            if (track)
            {
                // Find the keyframe by timestamp
                for (size_t i = 0; i < track->size(); ++i)
                {
                    if (std::abs((*track)[i].timeStamp - cmd.newTime) < 0.001f)
                    {
                        m_Animator->UpdateKeyframeTime(m_SelectedClipIndex, cmd.boneName, i, cmd.oldTime);
                        BOOM_INFO("[Undo] Moved keyframe on bone '{}' back from {:.2f}s to {:.2f}s",
                                  cmd.boneName.c_str(), (double)cmd.newTime, (double)cmd.oldTime);
                        break;
                    }
                }
            }
        }
        break;
    }

    // Push to redo stack
    m_RedoStack.push_back(cmd);
}

void AnimationTimelinePanel::Redo()
{
    if (m_RedoStack.empty() || !m_Animator || m_SelectedClipIndex < 0) return;

    KeyframeCommand cmd = m_RedoStack.back();
    m_RedoStack.pop_back();

    // Re-execute the command
    switch (cmd.type)
    {
    case KeyframeCommand::ADD:
        m_Animator->AddKeyframe(m_SelectedClipIndex, cmd.boneName, cmd.keyframe);
        BOOM_INFO("[Redo] Re-added keyframe at {:.2f}s on bone '{}'",
                  (double)cmd.keyframe.timeStamp, cmd.boneName.c_str());
        break;

    case KeyframeCommand::REMOVE:
        m_Animator->RemoveKeyframe(m_SelectedClipIndex, cmd.boneName, cmd.keyframeIndex);
        BOOM_INFO("[Redo] Re-removed keyframe at index {} on bone '{}'",
                  (int)cmd.keyframeIndex, cmd.boneName.c_str());
        break;

    case KeyframeCommand::MOVE:
        {
            auto* track = m_Animator->GetTrackMutable(m_SelectedClipIndex, cmd.boneName);
            if (track)
            {
                // Find the keyframe by old timestamp
                for (size_t i = 0; i < track->size(); ++i)
                {
                    if (std::abs((*track)[i].timeStamp - cmd.oldTime) < 0.001f)
                    {
                        m_Animator->UpdateKeyframeTime(m_SelectedClipIndex, cmd.boneName, i, cmd.newTime);
                        BOOM_INFO("[Redo] Re-moved keyframe on bone '{}' from {:.2f}s to {:.2f}s",
                                  cmd.boneName.c_str(), (double)cmd.oldTime, (double)cmd.newTime);
                        break;
                    }
                }
            }
        }
        break;
    }

    // Push back to undo stack
    m_UndoStack.push_back(cmd);
}
