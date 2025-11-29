#pragma once

#include <deque>
#include <string>
#include <array>
#include "Context/Widgets.h"    // IWidget, Entity

// Forward decls to keep header light
struct ImGuiTextFilter;
struct ImVec2;

namespace spdlog { namespace level { enum level_enum : int; } }

namespace EditorUI
{
    struct ConsoleLine {
        spdlog::level::level_enum level;
        std::string text;
    };

    struct ConsolePanel : IWidget
    {
        explicit ConsolePanel(AppInterface* c);

        void Clear();
        void AddLog(const char* fmt, ...) IM_FMTARGS(2); // defaults to info
        void AddLogLevel(spdlog::level::level_enum lvl, const char* fmt, ...) IM_FMTARGS(3);
        void TrackLastItemAsViewport(const char* label = "Viewport");

        // IWidget overrides
        void Render();
        void OnSelect(Entity entity) override;

        // Debug helpers
        void DebugConsoleState() const;

    private:
        // Circular buffer of lines (message + level)
        std::deque<ConsoleLine> m_Lines;
        ImGuiTextFilter* m_FilterPtr = nullptr;   // owned

        bool  m_Open = true;
        bool  m_AutoScroll = true;
        bool  m_Pause = false;
        int   m_MaxLines = 2000;

        bool   m_LogMouseMoves = true;
        bool   m_LogMouseClicks = true;
        float  m_LogEverySeconds = 0.05f;
        ImVec2 m_LastMouse{ -FLT_MAX, -FLT_MAX };
        double m_LastLogTime = 0.0;

        std::array<bool, ImGuiKey_NamedKey_END> m_KeyDownPrev{};
        char  m_InputBuf[256]{};
        bool  m_FocusInput = false;

        // Register a spdlog sink once (idempotent) to forward to this console
        void EnsureSpdlogSinkHooked();
    };
} // namespace EditorUI
