#pragma once

#include <functional>
#include <cstddef>
#include <string>
#include <memory>

// ImGui
#include "Vendors/imgui/imgui.h"

// entt (for entt::entity / entt::null)
#include <entt/entity/entity.hpp>
#include "Application/Interface.h"

namespace Boom {
    struct Application;
    struct AppContext; // your engine context (the one Editor holds)
}

namespace EditorUI {

    class Editor; // forward declaration to accept Editor* in ctor

    // A small config bundle, leaving the panel UI-only.
    // You can wire these from Editor (or the panel's ctor will try to fill a few).
    struct MenuBarConfig {
        // Engine pointers
        Boom::Application* app{ nullptr };
        Boom::AppContext* ctx{ nullptr };

        // View toggles (wired to your editor state)
        bool* showInspector{ nullptr };
        bool* showHierarchy{ nullptr };
        bool* showViewport{ nullptr };
        bool* showPrefabBrowser{ nullptr };
        bool* showPerformance{ nullptr };
        bool* showPlaybackControls{ nullptr };
        bool* showConsole{ nullptr };
        bool* showAudio{ nullptr };
		bool* showResources{ nullptr };
		bool* showDirectory{ nullptr };
        bool* showAnimatorGraph{ nullptr };
        // Dialog flags
        bool* showSaveDialog{ nullptr };
        bool* showLoadDialog{ nullptr };
        bool* showSavePrefabDialog{ nullptr };
        bool* showExportDialog{ nullptr };

        // Selected entity handle (optional; used by Save/Delete Selected)
        entt::entity selectedEntity{ entt::null};

        // Scene name text buffer (for Save/Save As defaults)
        char* sceneNameBuffer{ nullptr };
        size_t sceneNameBufferSize{ 0 };

        // Helpers
        std::function<void(bool)> RefreshSceneList; // pass true to force
    };

    class MenuBarPanel {
    public:
        // Editor.cpp calls: m_MenuBar = std::make_unique<MenuBarPanel>(this);
        explicit MenuBarPanel(Editor* owner);

        // Optional: allow Editor to update bindings later
        void SetConfig(const MenuBarConfig& cfg) { m = cfg; }

        // Render the main menu bar
        void Render();

    private:
        void PrefillSceneNameFromCurrent();

    private:
        MenuBarConfig m{};
        Editor* m_Owner{ nullptr };
        Boom::AppInterface* m_App = nullptr;
    };

} // namespace EditorUI
