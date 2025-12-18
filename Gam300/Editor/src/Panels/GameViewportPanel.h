#pragma once
#include <cstdint>
#include "Vendors/imgui/imgui.h"

namespace Boom {
    struct AppContext;
    struct AppInterface;
}

namespace EditorUI {
    class Editor;

    class GameViewportPanel {
    public:
        explicit GameViewportPanel(Editor* owner);
        ~GameViewportPanel();

        void Render();
        void Show(bool v) { m_ShowViewport = v; }
        bool IsVisible() const { return m_ShowViewport; }

    private:
        std::uint32_t QuerySceneFrame() const;

        Editor* m_Owner = nullptr;
        Boom::AppInterface* m_App = nullptr;
        Boom::AppContext* m_Ctx = nullptr;
        bool m_ShowViewport = true;
    };
}