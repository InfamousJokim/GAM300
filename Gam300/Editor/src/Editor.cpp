// src/Editor/Editor.cpp
#include "Editor.h"
#pragma warning(disable : 4834) // Disable [[nodiscard]] warnings for logging

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
#include "Panels/AnimatorGraphPanel.h"
#include "Panels/ModelPreviewPanel.h"
#include "Panels/AnimationTimelinePanel.h"
#include "BoomEngine.h"

// Undo/Redo
#include "Commands/UndoRedo.h"

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
        { //load assets - using multithreaded loading for faster startup
            DataSerializer serializer;
            serializer.DeserializeAsync(*m_Context->assets, "AssetsProp/assets.yaml", m_App->GetWindowHandle().get());
            // Note: Pass 0 as numThreads to auto-detect CPU cores, or specify a number like 4 or 8
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
        m_ModelPreview = std::make_unique<ModelPreviewPanel>(this);
        m_AnimationTimeline = std::make_unique<AnimationTimelinePanel>(this);

        // Initialize Undo/Redo system
        m_CommandHistory = std::make_unique<CommandHistory>(100); // Max 100 undo levels

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

        // --- Global keyboard shortcuts (processed before panels) ---
        HandleGlobalShortcuts();

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
        if (m_ShowModelPreview && m_ModelPreview) m_ModelPreview->Render();
        if (m_ShowAnimationTimeline && m_AnimationTimeline) m_AnimationTimeline->Render();
        // --- End frame / draw ---
        EndImguiFrame();
    }

    void Editor::HandleGlobalShortcuts()
    {
        if (!m_App) return;

        ImGuiIO& io = ImGui::GetIO();
        bool ctrl = io.KeyCtrl;
        bool shift = io.KeyShift;
        bool alt = io.KeyAlt;

        // ===== Undo/Redo Shortcuts =====

        // Ctrl+Z: Undo
        if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            if (m_CommandHistory && m_CommandHistory->CanUndo()) {
                m_CommandHistory->Undo();
                BOOM_INFO("[Shortcut] Undo (Ctrl+Z)");
            }
        }

        // Ctrl+Y or Ctrl+Shift+Z: Redo
        if ((ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Y, false)) ||
            (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)))
        {
            if (m_CommandHistory && m_CommandHistory->CanRedo()) {
                m_CommandHistory->Redo();
                BOOM_INFO("[Shortcut] Redo (Ctrl+Y)");
            }
        }

        // ===== File Menu Shortcuts =====

        // Ctrl+N: New Scene
        if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_N, false))
        {
            m_App->NewScene("UntitledScene");
            RefreshSceneList(true);
            BOOM_INFO("[Shortcut] Created new scene (Ctrl+N)");
        }

        // Ctrl+S: Save Scene
        if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            m_ShowSaveDialog = true;
            if (m_App->IsSceneLoaded()) {
                RefreshSceneList(true);
                // Prefill current scene name
                std::string currentPath = m_App->GetCurrentScenePath();
                if (!currentPath.empty()) {
                    size_t lastSlash = currentPath.find_last_of("/\\");
                    size_t lastDot = currentPath.find_last_of(".");
                    if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash) {
                        std::string sceneName = currentPath.substr(lastSlash + 1, lastDot - lastSlash - 1);
                        strncpy_s(m_SceneNameBuffer, sizeof(m_SceneNameBuffer), sceneName.c_str(), _TRUNCATE);
                    }
                }
            }
            BOOM_INFO("[Shortcut] Save scene dialog (Ctrl+S)");
        }

        // Ctrl+Shift+S: Save Scene As (conflicts with Stop play mode, so check if NOT playing)
        if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            if (!m_App->IsPlaying()) {
                m_ShowSaveDialog = true;
                m_SceneNameBuffer[0] = '\0'; // Clear for fresh name
                BOOM_INFO("[Shortcut] Save scene as dialog (Ctrl+Shift+S)");
            } else {
                // In play mode, Ctrl+Shift+S stops play mode (handled below)
                m_App->Stop();
                BOOM_INFO("[Shortcut] Stopped play mode (Ctrl+Shift+S)");
            }
        }

        // Ctrl+O: Load Scene
        if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_O, false))
        {
            m_ShowLoadDialog = true;
            RefreshSceneList(false);
            BOOM_INFO("[Shortcut] Load scene dialog (Ctrl+O)");
        }

        // Alt+F4: Exit Application
        if (alt && ImGui::IsKeyPressed(ImGuiKey_F4, false))
        {
            m_App->Exit();
            BOOM_INFO("[Shortcut] Exiting application (Alt+F4)");
        }

        // ===== Playback Control Shortcuts =====

        // Ctrl+P: Play/Resume
        if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_P, false))
        {
            if (!m_App->IsPlaying()) {
                m_App->Play();
                BOOM_INFO("[Shortcut] Play mode started (Ctrl+P)");
            }
            else if (m_App->IsPaused())
            {
                // Check if the pause was from the Editor OR from the Game.
                if (m_App->IsInGamePauseMenuLoaded())
                {
                    // The game is paused by 'Escape', so 'Ctrl+P' does nothing.
                    // Only the 'R' key (handled by C#) should resume.
                    BOOM_WARN("[Shortcut] Ctrl+P ignored. Game is paused, use 'R' key to resume.");
                }
                else
                {
                    // The game is paused by the Editor (Ctrl+Shift+P), so 'Ctrl+P' can resume.
                    m_App->Resume();
                    BOOM_INFO("[Shortcut] Resumed from editor pause (Ctrl+P)");
                }
            }
        }

        // Ctrl+Shift+P: Pause
        if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_P, false))
        {
            if (m_App->IsPlaying() && !m_App->IsPaused()) {
                m_App->Pause();
                BOOM_INFO("[Shortcut] Paused (Ctrl+Shift+P)");
            }
        }

        // ===== Visualization Shortcuts =====

        // F9: Toggle Physics Debug Visualization
        if (!ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_F9, false))
        {
            if (m_Context && m_Context->physics) {
                // Toggle the state by reading from Application
                bool currentState = m_App->m_PhysDebugViz;
                bool newState = !currentState;

                // Apply to physics system
                m_Context->physics->EnableDebugVisualization(newState, 1.0f);

                // Update the Application's state
                m_App->m_PhysDebugViz = newState;

                BOOM_INFO("[Shortcut] Physics Debug Visualization (F9): {}", newState ? "ON" : "OFF");
            }
        }

        // Note: Ctrl+Shift+S for Stop is handled above in the Save Section
        // to avoid conflicts with "Save As" when not playing
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

        // --- EXPORT GAME ---
        static char exportPath[512] = "Build/Game";
        static int exportConfig = 1; // 0 = Debug, 1 = Release

        if (m_ShowExportDialog) {
            ImGui::OpenPopup("Export Game");
            m_ShowExportDialog = false;
        }

        if (ImGui::BeginPopupModal("Export Game", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Export your game as a standalone application");
            ImGui::Separator();

            ImGui::TextUnformatted("Output Directory:");
            ImGui::InputText("##export_path", exportPath, IM_ARRAYSIZE(exportPath));

            ImGui::Separator();

            ImGui::TextUnformatted("Configuration:");
            ImGui::RadioButton("Debug", &exportConfig, 0); ImGui::SameLine();
            ImGui::RadioButton("Release", &exportConfig, 1);

            ImGui::Separator();
            ImGui::TextWrapped("This will copy the Runtime executable, engine DLLs, assets, scenes, and scripts to the output directory.");
            ImGui::Separator();

            if (ImGui::Button("Export", ImVec2(120, 0)))
            {
                try {
                    const std::string configName = (exportConfig == 0) ? "Debug" : "Release";

                    // Get solution directory (parent of Editor folder)
                    // Current path is typically x64/Debug when running Editor.exe
                    fs::path solutionDir = fs::current_path();
                    // Navigate up to solution root (handle both running from IDE and from exe)
                    if (solutionDir.filename() == "Debug" || solutionDir.filename() == "Release") {
                        solutionDir = solutionDir.parent_path().parent_path(); // x64/Debug -> Gam300
                    } else if (solutionDir.filename() == "Editor") {
                        solutionDir = solutionDir.parent_path(); // Editor -> Gam300
                    }

                    const fs::path buildDir = solutionDir / "x64" / configName;

                    // Make output path relative to solution directory
                    fs::path outputDir = fs::path(exportPath);
                    if (outputDir.is_relative()) {
                        outputDir = solutionDir / outputDir;
                    }

                    BOOM_INFO("[Export] Starting game export to: {}", outputDir.string());
                    BOOM_INFO("[Export] Configuration: {}", configName);
                    BOOM_INFO("[Export] Solution directory: {}", solutionDir.string());
                    BOOM_INFO("[Export] Build directory: {}", buildDir.string());

                    // Create output directory
                    fs::create_directories(outputDir);

                    // Check if build directory exists
                    if (!fs::exists(buildDir)) {
                        BOOM_ERROR("[Export] Build directory not found: {}", buildDir.string());
                        BOOM_ERROR("[Export] Please build Runtime in {} configuration first!", configName);
                        ImGui::CloseCurrentPopup();
                        throw std::runtime_error("Build directory not found. Build Runtime in " + configName + " configuration first.");
                    }

                    // Copy Runtime executable
                    const fs::path runtimeExe = buildDir / "Runtime.exe";
                    if (!fs::exists(runtimeExe)) {
                        BOOM_ERROR("[Export] Runtime.exe not found at: {}", runtimeExe.string());
                        BOOM_ERROR("[Export] Please build the Runtime project in {} configuration!", configName);
                        ImGui::CloseCurrentPopup();
                        throw std::runtime_error("Runtime.exe not found. Build Runtime project first.");
                    }

                    fs::copy_file(runtimeExe, outputDir / "Deathless.exe",
                        fs::copy_options::overwrite_existing);
                    BOOM_INFO("[Export] Copied Runtime.exe -> Deathless.exe");

                    // Copy ALL DLLs from the build directory (BoomEngine + all dependencies)
                    // This includes PhysX, FMOD, GLFW, Mono, etc.
                    int dllCount = 0;
                    if (fs::exists(buildDir) && fs::is_directory(buildDir)) {
                        for (const auto& entry : fs::directory_iterator(buildDir)) {
                            if (entry.path().extension() == ".dll") {
                                try {
                                    fs::copy_file(entry.path(),
                                        outputDir / entry.path().filename(),
                                        fs::copy_options::overwrite_existing);
                                    dllCount++;
                                    BOOM_INFO("[Export] Copied {}", entry.path().filename().string());
                                } catch (const std::exception& e) {
                                    BOOM_WARN("[Export] Failed to copy {}: {}",
                                        entry.path().filename().string(), e.what());
                                }
                            }
                        }
                    }
                    BOOM_INFO("[Export] Copied {} DLL files", dllCount);

                    // Copy Resources directory
                    const fs::path resourcesDir = solutionDir / "Editor" / "Resources";
                    const fs::path outputResources = outputDir / "Resources";
                    if (fs::exists(resourcesDir)) {
                        fs::create_directories(outputResources);
                        fs::copy(resourcesDir, outputResources,
                            fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                        BOOM_INFO("[Export] Copied Resources directory");
                    }

                    // Copy Scenes directory
                    const fs::path scenesDir = solutionDir / "Editor" / "Scenes";
                    const fs::path outputScenes = outputDir / "Scenes";
                    if (fs::exists(scenesDir)) {
                        fs::create_directories(outputScenes);
                        fs::copy(scenesDir, outputScenes,
                            fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                        BOOM_INFO("[Export] Copied Scenes directory");
                    }

                    // Copy assets.yaml (asset registry - CRITICAL for runtime!)
                    const fs::path assetsYaml = solutionDir / "Editor" / "AssetsProp" / "assets.yaml";
                    const fs::path outputAssets = outputDir / "Resources" / "assets.yaml";
                    if (fs::exists(assetsYaml)) {
                        fs::create_directories(outputDir / "Resources");
                        fs::copy_file(assetsYaml, outputAssets, fs::copy_options::overwrite_existing);
                        BOOM_INFO("[Export] Copied assets.yaml (asset registry)");
                    }
                    else {
                        BOOM_ERROR("[Export] WARNING: assets.yaml not found at {}", assetsYaml.string());
                    }

                    // Copy GameScripts.dll
                    const std::string scriptConfig = (exportConfig == 0) ? "Debug" : "Release";
                    const fs::path scriptsDir = solutionDir / "GameScripts" / "bin" / "x64" / scriptConfig;
                    const fs::path scriptsDll = scriptsDir / "GameScripts.dll";
                    const fs::path outputScriptsDir = outputDir / "Scripts";
                    if (fs::exists(scriptsDll)) {
                        fs::create_directories(outputScriptsDir);
                        fs::copy_file(scriptsDll, outputScriptsDir / "GameScripts.dll",
                            fs::copy_options::overwrite_existing);
                        BOOM_INFO("[Export] Copied GameScripts.dll");
                    }

                    // Copy Mono runtime folders (CRITICAL for standalone execution!)
                    // Mono needs etc/ for config and lib/ for base assemblies
                    const fs::path monoRoot = solutionDir.parent_path() / "mono";
                    if (fs::exists(monoRoot)) {
                        // Copy etc/ folder (Mono configuration)
                        const fs::path monoEtc = monoRoot / "etc";
                        const fs::path outputEtc = outputDir / "etc";
                        if (fs::exists(monoEtc)) {
                            fs::copy(monoEtc, outputEtc,
                                fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                            BOOM_INFO("[Export] Copied Mono etc/ folder");
                        }

                        // Copy lib/ folder (Mono assemblies)
                        const fs::path monoLib = monoRoot / "lib";
                        const fs::path outputLib = outputDir / "lib";
                        if (fs::exists(monoLib)) {
                            fs::copy(monoLib, outputLib,
                                fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                            BOOM_INFO("[Export] Copied Mono lib/ folder");
                        }
                    }
                    else {
                        BOOM_ERROR("[Export] WARNING: Mono runtime not found at {}", monoRoot.string());
                    }

                    BOOM_INFO("[Export] Export completed successfully to: {}", outputDir.string());
                    ImGui::CloseCurrentPopup();
                }
                catch (const std::exception& e) {
                    BOOM_ERROR("[Export] Export failed: {}", e.what());
                    std::cerr << "Export failed: " << e.what() << std::endl;
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
