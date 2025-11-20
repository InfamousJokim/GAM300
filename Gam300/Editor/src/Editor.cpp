// src/Editor/Editor.cpp
#include "Editor.h"

// Bring in the full AppContext definition here (not in the header)
#include "Context/Context.h"

// Panels (full definitions MUST be included here before ~Editor and method calls)
#include "Panels/MenuBarPanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/Inspector/InspectorPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/ResourcePanel.h"
#include "Panels/DirectoryPanel.h"
#include "Panels/AudioPanel.h"
#include "Panels/PrefabBrowserPanel.h"
#include "Panels/ViewportPanel.h"
#include "Panels/PerformancePanel.h"
#include "Panels/PlaybackControlsPanel.h"
#include "Panels/NavMeshPanel.h"
#include "Panels/Inspector/AnimatorGraphPanel.h"
#include "Panels/RayCast.h"
#include "BoomEngine.h"

// Gizmo
#include "Vendors/imGuizmo/ImGuizmo.h"

// GL loader must be included somewhere before GL calls.
// If your project uses glad/glew, include it *once* in a common cpp.
// Here we rely on your existing setup.
#include <GLFW/glfw3.h>
#include "Vendors/imgui/backends/imgui_impl_glfw.h"
#include "Vendors/imgui/backends/imgui_impl_opengl3.h"


namespace {
    namespace fs = std::filesystem;

    inline std::string trim_copy(std::string s) {
        auto issp = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
        while (!s.empty() && issp(s.back())) s.pop_back();
        size_t b = 0; while (b < s.size() && issp(s[b])) ++b;
        if (b) s.erase(0, b);
        return s;
    }

    inline fs::path ResolveScenePath(const std::string& userText,
        const fs::path& baseDir,
        const char* defaultExt /* ".scene" or ".yaml" */)
    {
        std::string raw = trim_copy(userText);
        if (raw.empty()) raw = "UntitledScene";

        fs::path p(raw);
        // If user didn’t type a directory, save under baseDir
        fs::path dst = (p.is_absolute() || p.has_parent_path()) ? p : (baseDir / p);

        // Add/normalize the extension
        auto ext = dst.extension().string();
        if (ext.empty()) {
            dst.replace_extension(defaultExt);
        }
        else if (ext != ".scene" && ext != ".yaml") {
            // Force the format you want
            dst.replace_extension(defaultExt);
        }
        // Normalize the path for logs/consistency
        return dst.lexically_normal();
    }
    // ---------------- Helpers local to this translation unit ----------------

    void BeginImguiFrame(ImGuiContext* ctx)
    {
        if (ctx) ImGui::SetCurrentContext(ctx);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ctx) ImGuizmo::SetImGuiContext(ctx);
        ImGuizmo::BeginFrame();
    }

    void EndImguiFrame()
    {
        ImGui::Render();
        if (ImDrawData* dd = ImGui::GetDrawData(); dd && dd->Valid)
        {
            ImGui_ImplOpenGL3_RenderDrawData(dd);
            glFlush();
        }
    }

    void CreateMainDockSpace()
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
            | ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("DockSpace", nullptr, flags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceID = ImGui::GetID("MainDockspace");
        ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        ImGui::End();
    }

} // anonymous namespace

// ---------------------------- EditorUI::Editor ----------------------------

namespace EditorUI {

    Editor::Editor(ImGuiContext* imgui, entt::registry* registry, Boom::Application* app)
        : m_ImGuiContext(imgui), m_Registry(registry), m_App(app) {
    }

    // Define after all panel headers are included so the types are complete.
    Editor::~Editor() = default;
    namespace fs = std::filesystem;
    void Editor::Init()
    {
        { //load assets
            DataSerializer serializer;
            serializer.Deserialize(*m_Context->assets, "AssetsProp/assets.yaml", m_App->GetWindowHandle().get());
        }

        // Construct panels here; they persist across frames.
        // We pass `this` so panels can call owner->GetContext() etc.
        m_MenuBar = std::make_unique<MenuBarPanel>(this);
        m_Inspector = std::make_unique<InspectorPanel>(this);
        m_Hierarchy = std::make_unique<HierarchyPanel>(this);
        m_Console = std::make_unique<ConsolePanel>(this);
        m_Resources = std::make_unique<ResourcePanel>(this);
        m_Directory = std::make_unique<DirectoryPanel>(this);
        m_Audio = std::make_unique<AudioPanel>(this);
        m_PrefabBrowser = std::make_unique<PrefabBrowserPanel>(this);
        m_Viewport = std::make_unique<ViewportPanel>(this);
        m_Performance = std::make_unique<PerformancePanel>(this);
        m_Playback = std::make_unique<PlaybackControlsPanel>(this, m_App);
		m_Navmesh = std::make_unique<NavmeshPanel>(this);
        m_AnimatorGraph = std::make_unique<AnimatorGraphPanel>(this);
        // Panel-specific init
        if (m_Directory) m_Directory->Init();
    }
    void Editor::OnStart()
    {
        // AppInterface already filled m_Context before this call.
        // Make sure ImGui uses the context we created in main.cpp.
        if (m_ImGuiContext)
            ImGui::SetCurrentContext(m_ImGuiContext);

        BOOM_INFO("Editor::OnStart");
        Init();   // build all panels here (you already wrote this)
    }

    void Editor::OnUpdate()
    {
        // draw one ImGui frame of the editor
        Render(); // you already wrote this to NewFrame(), draw panels, RenderDrawData()
    }

    void Editor::Render()
    {
        // Minimal GL state for editor UI pass
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (ImGuiViewport* vp = ImGui::GetMainViewport())
            glViewport(0, 0, (GLsizei)vp->Size.x, (GLsizei)vp->Size.y);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // --- Start frame ---
        BeginImguiFrame(m_ImGuiContext);

        // --- Layout root dockspace ---
        CreateMainDockSpace();
		
        // --- Panels (menu first, then windows) ---
        if (m_MenuBar)        m_MenuBar->Render();
        RenderSceneDialogs();
        if (m_ShowViewport && m_Viewport)      m_Viewport->Render();
        if (m_ShowHierarchy && m_Hierarchy)     m_Hierarchy->Render();
        if (m_ShowInspector && m_Inspector)     m_Inspector->Render();
        if (m_ShowResources && m_Resources)     m_Resources->OnShow();   
        if (m_ShowDirectory && m_Directory)     m_Directory->OnShow();
        if (m_ShowPrefabBrowser && m_PrefabBrowser) m_PrefabBrowser->Render();
        if (m_ShowConsole && m_Console)       m_Console->Render();
        if (m_ShowAudio && m_Audio)         m_Audio->Render();
        if (m_ShowPerformance && m_Performance)  m_Performance->Render();
        if (m_ShowPlaybackControls && m_Playback) m_Playback->OnShow();
		if (m_ShowNavmesh && m_Navmesh)       m_Navmesh->Render();
        if (m_ShowAnimatorGraph && m_AnimatorGraph) m_AnimatorGraph->Render();
        // --- End frame / draw ---
        EndImguiFrame();
    }
    void Editor::RefreshSceneList(bool force) {
        namespace fs = std::filesystem;

        if (!fs::exists(m_ScenesDir)) {
#ifdef DEBUG
            BOOM_WARN("[Editor] '{}' directory doesn't exist, creating it...", m_ScenesDir.string());
#endif DEBUG
            fs::create_directory(m_ScenesDir);
        }

        std::unordered_map<std::string, fs::file_time_type> newStamp;

        auto accept = [](const fs::path& p) {
            auto ext = p.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (ext != ".yaml" && ext != ".scene")
                return false;

            // ignore *_assets.yaml
            std::string stem = p.stem().string();
            if (stem.size() > 7 && stem.rfind("_assets") == stem.size() - 7)
                return false;

            return true;
            };

        for (const auto& entry : fs::directory_iterator(m_ScenesDir)) {
            if (!entry.is_regular_file()) continue;
            if (!accept(entry.path()))    continue;

            const std::string stem = entry.path().stem().string();
            newStamp[stem] = fs::last_write_time(entry.path());
        }

        bool changed = force || (newStamp.size() != m_SceneStamp.size());
        if (!changed) {
            for (auto& [name, ts] : newStamp) {
                auto it = m_SceneStamp.find(name);
                if (it == m_SceneStamp.end() || it->second != ts) { changed = true; break; }
            }
        }
        if (!changed) return;

        m_SceneStamp = std::move(newStamp);
        m_AvailableScenes.clear();
        m_AvailableScenes.reserve(m_SceneStamp.size());
        for (auto& [name, _] : m_SceneStamp) m_AvailableScenes.push_back(name);
        std::sort(m_AvailableScenes.begin(), m_AvailableScenes.end());

        if (m_SelectedSceneIndex >= static_cast<int>(m_AvailableScenes.size()))
            m_SelectedSceneIndex = static_cast<int>(m_AvailableScenes.size()) - 1;
        if (m_SelectedSceneIndex < 0)
            m_SelectedSceneIndex = 0;
#ifdef DEBUG
        BOOM_INFO("[Editor] Scene list refreshed ({} items).", static_cast<int>(m_AvailableScenes.size()));
#endif DEBUG

    }
    static std::string ToBaseName(const char* buf) {
        namespace fs = std::filesystem;
        fs::path p = (buf && buf[0]) ? fs::path(buf) : fs::path("UntitledScene");
        std::string stem = p.stem().string();
        if (stem.empty()) stem = "UntitledScene";
        return stem; // no folders, no extension
    }
    void Editor::RenderSceneDialogs()
    {
        namespace fs = std::filesystem;

        // --- SAVE (triggered when MenuBar sets m_ShowSaveDialog = true) ---
        if (m_ShowSaveDialog) {
            ImGui::OpenPopup("Save Scene");
            m_ShowSaveDialog = false;
        }

        if (ImGui::BeginPopupModal("Save Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Scene name (omit folders and extension):");
            ImGui::InputText("##scene_name", m_SceneNameBuffer, IM_ARRAYSIZE(m_SceneNameBuffer));

            ImGui::Separator();

            // For preview only (editor UI): show where it will end up.
            // NOTE: Your Application::SaveScene builds "Scenes/" + name + ".yaml".
            // If your app uses a different folder, change this preview accordingly.
            const std::string baseName = ToBaseName(m_SceneNameBuffer);
            const fs::path preview = fs::path("Scenes") / (baseName + ".yaml");
            ImGui::Text("Will save to:\n%s", preview.lexically_normal().string().c_str());

            if (ImGui::Button("Save", ImVec2(120, 0)))
            {
                if (!m_App) {
                    BOOM_ERROR("[Editor] SaveScene failed: m_App is null");
                }
                else {
                    try {
                        // OPTION B: pass ONLY the stem; Application::SaveScene builds folder + ".yaml"
                        m_App->SaveScene(baseName);
                        BOOM_INFO("[Editor] Requested save of scene '{}'", baseName);

                        RefreshSceneList(true);
                        ImGui::CloseCurrentPopup();
                    }
                    catch (const std::exception& e) {
#ifdef DEBUG
                        BOOM_ERROR("[Editor] SaveScene exception: {}", e.what());
#endif // DEBUG
						std::cerr << "SaveScene exception: " << e.what() << std::endl;
                       
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // --- LOAD ---
        if (m_ShowLoadDialog) {
            RefreshSceneList(false);
            ImGui::OpenPopup("Load Scene");
            m_ShowLoadDialog = false;

        }

        if (ImGui::BeginPopupModal("Load Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Select a scene to load (.yaml):");
            ImGui::Separator();

            ImGui::BeginChild("##scene_list", ImVec2(420, 260), true);
            for (int i = 0; i < static_cast<int>(m_AvailableScenes.size()); ++i) {
                const bool selected = (i == m_SelectedSceneIndex);
                if (ImGui::Selectable(m_AvailableScenes[(size_t)i].c_str(), selected))
                    m_SelectedSceneIndex = i;
            }
            ImGui::EndChild();

            ImGui::Separator();

            if (ImGui::Button("Load", ImVec2(120, 0)))
            {
                if (!m_App) {
                    BOOM_ERROR("[Editor] LoadScene failed: m_App is null");
                }
                else if (m_SelectedSceneIndex >= 0 &&
                    m_SelectedSceneIndex < (int)m_AvailableScenes.size())
                {
                    try {
                        const std::string& baseName = m_AvailableScenes[(size_t)m_SelectedSceneIndex];
                        // If Application::LoadScene expects a full path, keep this:
                        const fs::path src = fs::path("Scenes") / (baseName + ".yaml");
                        if (!fs::exists(src)) {
#ifdef DEBUG
                            BOOM_WARN("[Editor] LoadScene: file not found '{}'", src.string());
#endif DEBUG
                        }
                        else {
                            m_App->LoadScene(baseName);
#ifdef DEBUG
                            BOOM_INFO("[Editor] Loaded scene: {}", src.string());
#endif DEBUG
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    catch (const std::exception& e) {
                    #ifdef DEBUG
                        BOOM_ERROR("[Editor] LoadScene exception: {}", e.what());
                    #endif DEBUG
						std::cerr << "LoadScene exception: " << e.what() << std::endl;
                    }
                }
                else {
                    BOOM_WARN("[Editor] LoadScene: no selection");
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }


    void Editor::Shutdown()
    {
        // unique_ptr members will clean up automatically.
    }

} 
