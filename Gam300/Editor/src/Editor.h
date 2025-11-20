#pragma once
#include <memory>
#include "Application/Interface.h"  // For Boom::AppInterface complete type
#include "Vendors/imgui/imgui.h"    // For ImVec2
#include "Vendors/imGuizmo/ImGuizmo.h"
#include "Application/Application.h"
// Keep heavy headers out of here to avoid cycles.
// Just forward-declare the few types we need.
struct ImGuiContext;            // ImGui is global-namespace

namespace Boom {
    struct AppContext;          // Can still forward-declare this
}

namespace EditorUI {
    // Forward-declare all panels (their full defs live in each panel header).
    class MenuBarPanel;
    class HierarchyPanel;
    class InspectorPanel;
    struct ConsolePanel;
    class ResourcePanel;
    class DirectoryPanel;
    class AudioPanel;
    class PrefabBrowserPanel;
    class ViewportPanel;
    class PerformancePanel;
    class PlaybackControlsPanel;
	class NavmeshPanel;
    class AnimatorGraphPanel;
    // FIXED: Now inherits from AppInterface (complete definition included above)
    class Editor : public Boom::AppInterface {
    public:
        explicit Editor(ImGuiContext* imgui,
            entt::registry* registry,
            Boom::Application* app);
        ~Editor();                      // defined in .cpp after including all panels

        Editor(const Editor&) = delete;
        Editor& operator=(const Editor&) = delete;
        Editor(Editor&&) = delete;
        Editor& operator=(Editor&&) = delete;
        void RefreshSceneList(bool force = false);
        // lifecycle
        void Init();
        void Render();
        void Shutdown();

        // accessors used by panels
        Boom::AppContext* GetContext() const { return m_Context; }
        ImGuiContext* GetImGuiContext() const { return m_ImGuiContext; }
        entt::registry* GetRegistry() const { return m_Registry; }
        Boom::Application* GetApp() const { return m_App; }

        // NEW: Get viewport size for renderer resizing
        ImVec2 GetViewportSize() const;
        void RenderSceneDialogs();
        ViewportPanel* GetViewportPanel() const { return m_Viewport.get(); }
        AppInterface* GetAppInterface() const { return m_App; }

    public:
      
        char m_SceneNameBuffer[256] = {};
        ImGuiContext* m_ImGuiContext = nullptr;
        entt::registry* m_Registry = nullptr;
        Boom::Application* m_App = nullptr;
        void OnStart() override;
        void OnUpdate() override;
        bool m_ShowInspector = true;
        bool m_ShowHierarchy = true;
        bool m_ShowViewport = true;
        bool m_ShowPrefabBrowser = true;
        bool m_ShowPerformance = true;
        bool m_ShowPlaybackControls = true;
        bool m_ShowConsole = true;
        bool m_ShowAudio = true;
		bool m_ShowResources = true;
		bool m_ShowDirectory = true;
        bool m_ShowNavmesh = true;
        bool m_ShowAnimatorGraph = true;
        bool m_ShowSaveDialog = false;
        bool m_ShowLoadDialog = false;
        bool m_ShowSavePrefabDialog = false;
        std::unique_ptr<MenuBarPanel>           m_MenuBar;
        std::unique_ptr<HierarchyPanel>         m_Hierarchy;
        std::unique_ptr<InspectorPanel>         m_Inspector;
        std::unique_ptr<ConsolePanel>           m_Console;
        std::unique_ptr<ResourcePanel>          m_Resources;
        std::unique_ptr<DirectoryPanel>         m_Directory;
        std::unique_ptr<AudioPanel>             m_Audio;
        std::unique_ptr<PrefabBrowserPanel>     m_PrefabBrowser;
        std::unique_ptr<ViewportPanel>          m_Viewport;
        std::unique_ptr<PerformancePanel>       m_Performance;
        std::unique_ptr<PlaybackControlsPanel>  m_Playback;
		std::unique_ptr<NavmeshPanel>            m_Navmesh;
        std::unique_ptr<AnimatorGraphPanel>     m_AnimatorGraph;
    private:
        std::filesystem::path m_ScenesDir = std::filesystem::path("Scenes");
        std::unordered_map<std::string, std::filesystem::file_time_type> m_SceneStamp;
        std::vector<std::string> m_AvailableScenes;
        int m_SelectedSceneIndex = 0;

       
      
    };

} // namespace EditorUI