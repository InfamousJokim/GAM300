#include "Panels/MenuBarPanel.h"
#include "Editor.h"        // to use Editor* in ctor
#include "Context/Context.h"      // Boom::AppContext (complete type)
#include "Context/DebugHelpers.h" // BOOM_INFO / BOOM_ERROR
#include "Vendors/imgui/imgui.h"
#include "Vendors/imGuizmo/ImGuizmo.h"

#include <string>
#include <cstdio>
#include <cstring> // strncpy_s on MSVC

namespace EditorUI {

    // --------------------------- Ctor ---------------------------------

    MenuBarPanel::MenuBarPanel(Editor* owner)
        : m_Owner(owner)
    {
        // If your Editor exposes getters, we can prefill a few pointers safely.
        // These calls are guarded to avoid compile/runtime issues if absent.
        if (m_Owner) {
            // Boom::AppContext* from Editor
            // Requires Editor to implement: Boom::AppContext* GetContext() const;
            if (auto* ctx = m_Owner->GetContext()) {
                m.ctx = ctx;
            }
            m.app = m_Owner->GetApp();

            // If your Editor has an Application* getter, set m.app here as well.
            // Example (uncomment/adapt if you have such API):
            // m.app = m_Owner->GetApplication();

            // If your Editor stores the "show panel" flags, wire them here.
            // Example (pseudo):
             m.showInspector        = &m_Owner->m_ShowInspector;
             m.showHierarchy        = &m_Owner->m_ShowHierarchy;
             m.showViewport         = &m_Owner->m_ShowViewport;
             m.showPrefabBrowser    = &m_Owner->m_ShowPrefabBrowser;
             m.showPerformance      = &m_Owner->m_ShowPerformance;
             m.showPlaybackControls = &m_Owner->m_ShowPlaybackControls;
             m.showConsole          = &m_Owner->m_ShowConsole;
             m.showAudio            = &m_Owner->m_ShowAudio;
			 m.showResources        = &m_Owner->m_ShowResources;
			 m.showDirectory        = &m_Owner->m_ShowDirectory;
             m.showAnimatorGraph    = &m_Owner->m_ShowAnimatorGraph;
             //Dialog flags & helpers can also be wired here if Editor exposes them.
             m.showSaveDialog = &m_Owner->m_ShowSaveDialog;
             m.showLoadDialog = &m_Owner->m_ShowLoadDialog;
             m.sceneNameBuffer = m_Owner->m_SceneNameBuffer;
             m.sceneNameBufferSize = sizeof(m_Owner->m_SceneNameBuffer);
             
             m.RefreshSceneList = [this](bool force){ m_Owner->RefreshSceneList(force); };
        }
    }

    // --------------------------- Helpers ---------------------------------

    void MenuBarPanel::PrefillSceneNameFromCurrent()
    {
        if (!m.app || !m.sceneNameBuffer || m.sceneNameBufferSize == 0) return;
        if (!m.app->IsSceneLoaded()) return;

        std::string currentPath = m.app->GetCurrentScenePath();
        if (currentPath.empty()) return;

        const size_t lastSlash = currentPath.find_last_of("/\\");
        const size_t lastDot = currentPath.find_last_of(".");
        if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash) {
            const std::string sceneName = currentPath.substr(lastSlash + 1, lastDot - lastSlash - 1);

#ifdef _MSC_VER
            strncpy_s(m.sceneNameBuffer, m.sceneNameBufferSize, sceneName.c_str(), _TRUNCATE);
#else
            std::snprintf(m.sceneNameBuffer, m.sceneNameBufferSize, "%s", sceneName.c_str());
#endif
        }
    }

    // --------------------------- UI ---------------------------------

    void MenuBarPanel::Render()
    {
        if (!ImGui::BeginMainMenuBar())
            return;

        // --------------------------- File ----------------------------------------
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                if (m.app) {
                    m.app->NewScene("UntitledScene");
                    if (m.RefreshSceneList) m.RefreshSceneList(true);
                    BOOM_INFO("[Editor] Created new scene");
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (m.showSaveDialog) *m.showSaveDialog = true;
                if (m.app && m.app->IsSceneLoaded()) {
                    if (m.RefreshSceneList) m.RefreshSceneList(true);
                    PrefillSceneNameFromCurrent();
                }
            }

            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                if (m.showSaveDialog) *m.showSaveDialog = true;
                if (m.sceneNameBuffer && m.sceneNameBufferSize) {
                    m.sceneNameBuffer[0] = '\0'; // fresh name
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Load Scene", "Ctrl+O")) {
                if (m.showLoadDialog) *m.showLoadDialog = true;
           
                if (m.RefreshSceneList) m.RefreshSceneList(false);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                if (m.app) m.app->Stop();
            }

            ImGui::EndMenu();
        }

        // --------------------------- View ----------------------------------------
        if (ImGui::BeginMenu("View")) {
            if (m.showInspector)        ImGui::MenuItem("Inspector", nullptr, m.showInspector);
            if (m.showHierarchy)        ImGui::MenuItem("Hierarchy", nullptr, m.showHierarchy);
            if (m.showViewport)         ImGui::MenuItem("Viewport", nullptr, m.showViewport);
            if (m.showPrefabBrowser)    ImGui::MenuItem("Prefab Browser", nullptr, m.showPrefabBrowser);
            if (m.showPerformance)      ImGui::MenuItem("Performance", nullptr, m.showPerformance);
            if (m.showPlaybackControls) ImGui::MenuItem("Playback Controls", nullptr, m.showPlaybackControls);
            if (m.showConsole)          ImGui::MenuItem("Debug Console", nullptr, m.showConsole);
            if (m.showAudio)            ImGui::MenuItem("Audio", nullptr, m.showAudio);
			if (m.showResources)     ImGui::MenuItem("Resources", nullptr, m.showResources);
			if (m.showDirectory)       ImGui::MenuItem("Directory", nullptr, m.showDirectory);
            if (m.showAnimatorGraph)    ImGui::MenuItem("Animator Graph", nullptr, m.showAnimatorGraph);
            ImGui::EndMenu();
        }

        // --------------------------- Options -------------------------------------
        if (ImGui::BeginMenu("Options"))
        {
            // Toggle your renderer's debug draw flag by *reference* if available.
            if (m.ctx && m.ctx->renderer) {
                ImGui::MenuItem("Debug Draw", nullptr, &m.ctx->renderer->isDrawDebugMode);
                ImGui::MenuItem("Normal View", nullptr, &m.ctx->renderer->showNormalTexture);
                if (ImGui::BeginMenu("Low Poly Mode")) {
                    ImGui::Checkbox("Enabled", &m.ctx->renderer->showLowPoly);
                    if (m.ctx->renderer->showLowPoly) {
                        ImGui::SliderFloat("Dither Threshold", &m.ctx->renderer->DitherThreshold(), 0.0f, 1.0f);
                    }
                    ImGui::EndMenu();
                }
                bool TEMPORARY_PLACEHOLDER_WIREFRAME_COLLISION{};
                ImGui::MenuItem("Collision Lines", nullptr, TEMPORARY_PLACEHOLDER_WIREFRAME_COLLISION);
				ImGui::MenuItem("Bloom", nullptr, &m.ctx->renderer->enabledBloom);
            }
            ImGui::EndMenu();
        }

        // --------------------------- GameObjects ---------------------------------
        if (ImGui::BeginMenu("GameObjects"))
        {
            if (ImGui::MenuItem("Create Empty Object"))
            {
                if (!m.ctx) {
                    BOOM_ERROR("[Editor] Create Empty Object failed: no context.");
                }
                else {
                    auto& reg = m.ctx->scene;

                    // Create entity (use whichever ctor your wrapper supports)
                    Entity go{ &reg };                      // if this constructs a new entt::entity
                    // Entity go{ &reg, reg.create() };     // use this form if your wrapper needs an explicit entity id

                    // Make a unique name: GameObject, GameObject (1), ...
                    auto nameExists = [&](const std::string& n) {
                        auto view = reg.view<InfoComponent>();
                        for (auto e : view) {
                            if (view.get<InfoComponent>(e).name == n) return true;
                        }
                        return false;
                        };
                    std::string name = "New Entity";
                    for (int i = 1; nameExists(name); ++i)
                        name = "New Entity (" + std::to_string(i) + ")";

                    // Attach components
                    go.Attach<InfoComponent>().name = name;
                    go.Attach<TransformComponent>(); // default transform

        

                    // Select the newly created object
                    m.selectedEntity = go.ID();

                    BOOM_INFO("[Editor] Created {}", name);
                }
            }

            if (ImGui::MenuItem("Create From Prefab...")) {
                if (m.showPrefabBrowser) *m.showPrefabBrowser = true;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save Selected as Prefab")) {
                if ( m.selectedEntity != entt::null) {
                    if (m.showSavePrefabDialog) *m.showSavePrefabDialog = true;
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete Selected")) {
                if (m.selectedEntity != entt::null) {
                    BOOM_INFO("[Editor] Requested: Delete Selected (delegate to Editor)");
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

} // namespace EditorUI
