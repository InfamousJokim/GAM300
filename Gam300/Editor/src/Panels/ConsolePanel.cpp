#include "Panels/ConsolePanel.h"

#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <memory>
#include <mutex>
#include <deque>

// ImGui
#include "Vendors/imgui/imgui.h"

// Engine/Editor
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "EditorPCH.h"

// spdlog sink glue
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>

#ifndef ICON_FA_TERMINAL
#define ICON_FA_TERMINAL ""
#endif

namespace {
    // -------- thread-safe queue that any sink can push into --------
    struct LogEvent {
        spdlog::level::level_enum lvl;
        std::string text;
    };
    std::mutex              g_queueMutex;
    std::deque<LogEvent>    g_queue;
    size_t                  g_queueCap = 5000;

    inline void Enqueue(spdlog::level::level_enum lvl, std::string msg) {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        if (g_queue.size() >= g_queueCap) g_queue.pop_front();
        g_queue.push_back({ lvl, std::move(msg) });
    }

    inline void DrainTo(std::deque<LogEvent>& out) {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        if (!g_queue.empty()) {
            out.insert(out.end(),
                std::make_move_iterator(g_queue.begin()),
                std::make_move_iterator(g_queue.end()));
            g_queue.clear();
        }
    }

    // -------- sink that forwards spdlog messages into our queue ----
    template <typename Mutex>
    class imgui_console_sink : public spdlog::sinks::base_sink<Mutex> {
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            spdlog::memory_buf_t formatted;
            this->formatter_->format(msg, formatted);
            Enqueue(msg.level, fmt::to_string(formatted));
        }
        void flush_() override {}
    };

    using imgui_console_sink_mt = imgui_console_sink<std::mutex>;
} // anonymous

namespace EditorUI
{
    // --------------------------------
    // ctor
    // --------------------------------
    ConsolePanel::ConsolePanel(AppInterface* c)
        : IWidget(c)
        , m_FilterPtr(new ImGuiTextFilter())
    {
        DEBUG_DLL_BOUNDARY("ConsolePanel::Constructor");
        DEBUG_POINTER(context, "AppInterface");

        if (!context) {
            BOOM_ERROR("ConsolePanel::Constructor - Null context!");
        }
        else {
            BOOM_INFO("ConsolePanel::Constructor - OK");
        }

        m_KeyDownPrev.fill(false);
        m_InputBuf[0] = '\0';

        // Hook spdlog → this console once
        EnsureSpdlogSinkHooked();
    }

    // --------------------------------
    // spdlog sink hookup (idempotent)
    // --------------------------------
    void ConsolePanel::EnsureSpdlogSinkHooked()
    {
        // Attach to your global Boom logger if available; otherwise attach to default logger.
        try {
            std::shared_ptr<spdlog::logger> logger;
#ifdef BOOM_ENABLE_LOG
            logger = Boom::GetLogger();
#endif
            if (!logger) logger = spdlog::default_logger();

            // Check if an imgui sink already exists to avoid duplicates
            bool hasSink = false;
            for (auto& s : logger->sinks()) {
                if (dynamic_cast<imgui_console_sink_mt*>(s.get())) { hasSink = true; break; }
            }
            if (!hasSink) {
                auto sink = std::make_shared<imgui_console_sink_mt>();
                sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
                logger->sinks().push_back(std::move(sink));
                logger->set_level(spdlog::level::trace); // capture everything
            }
        }
        catch (...) {
            // Sink hookup is non-fatal for the editor
        }
    }

    // --------------------------------
    // public logging API
    // --------------------------------
    void ConsolePanel::Clear()
    {
        m_Lines.clear();
    }

    void ConsolePanel::AddLogLevel(spdlog::level::level_enum lvl, const char* fmt, ...)
    {
        if (m_Pause) return;

        char buf[768];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        if ((int)m_Lines.size() >= m_MaxLines)
            m_Lines.pop_front();

        m_Lines.push_back({ lvl, buf });
    }

    void ConsolePanel::AddLog(const char* fmt, ...)
    {
        if (m_Pause) return;

        char buf[768];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        if ((int)m_Lines.size() >= m_MaxLines)
            m_Lines.pop_front();

        m_Lines.push_back({ spdlog::level::info, buf });
    }

    // ---------------------------------------------
    // Track the last ImGui item as a "viewport"
    // ---------------------------------------------
    void ConsolePanel::TrackLastItemAsViewport(const char* label)
    {
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImVec2 size{ max.x - min.x, max.y - min.y };

        const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup |
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        ImVec2 mouseGlobal = ImGui::GetMousePos();
        ImVec2 mouseLocal{ mouseGlobal.x - min.x, mouseGlobal.y - min.y };

        const bool inside = hovered &&
            mouseLocal.x >= 0 && mouseLocal.y >= 0 &&
            mouseLocal.x <= size.x && mouseLocal.y <= size.y;

        double now = ImGui::GetTime();
        float dx = mouseLocal.x - m_LastMouse.x;
        float dy = mouseLocal.y - m_LastMouse.y;
        float deltaDist = (m_LastMouse.x == -FLT_MAX) ? 1e9f : std::sqrt(dx * dx + dy * dy);

        if (inside && m_LogMouseMoves && (now - m_LastLogTime) >= m_LogEverySeconds && deltaDist >= 0.5f) {
            AddLog("[%s] Mouse local(%.1f, %.1f)  global(%.1f, %.1f)  size(%.0f x %.0f)",
                label ? label : "Viewport",
                mouseLocal.x, mouseLocal.y, mouseGlobal.x, mouseGlobal.y, size.x, size.y);
            m_LastMouse = mouseLocal;
            m_LastLogTime = now;
        }

        if (inside && m_LogMouseClicks) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                AddLog("[%s] Click: LMB @ local(%.1f, %.1f)", label ? label : "Viewport", mouseLocal.x, mouseLocal.y);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                AddLog("[%s] Click: RMB @ local(%.1f, %.1f)", label ? label : "Viewport", mouseLocal.x, mouseLocal.y);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
                AddLog("[%s] Click: MMB @ local(%.1f, %.1f)", label ? label : "Viewport", mouseLocal.x, mouseLocal.y);
        }
    }

    // small helper for level colors
    static ImVec4 ColorFor(spdlog::level::level_enum lvl) {
        switch (lvl) {
        case spdlog::level::trace:    return ImVec4(0.65f, 0.65f, 0.65f, 1.f);
        case spdlog::level::debug:    return ImVec4(0.70f, 0.75f, 0.95f, 1.f);
        case spdlog::level::info:     return ImVec4(0.90f, 0.90f, 0.90f, 1.f);
        case spdlog::level::warn:     return ImVec4(1.00f, 0.85f, 0.40f, 1.f);
        case spdlog::level::err:      return ImVec4(1.00f, 0.50f, 0.50f, 1.f);
        case spdlog::level::critical: return ImVec4(1.00f, 0.25f, 0.25f, 1.f);
        default:                      return ImVec4(0.90f, 0.90f, 0.90f, 1.f);
        }
    }

    // ---------------------------
    // IWidget overrides
    // ---------------------------
    void ConsolePanel::Render()
    {
        if (!context) {
            BOOM_ERROR("ConsolePanel::OnShow - Null context!");
            return;
        }

        // 1) Drain spdlog → queue into this panel’s lines
        {
            std::deque<LogEvent> drained;
            DrainTo(drained);
            for (auto& e : drained) {
                if ((int)m_Lines.size() >= m_MaxLines) m_Lines.pop_front();
                m_Lines.push_back({ e.lvl, std::move(e.text) });
            }
        }

        // 2) Your existing input echo (keys/chars)
        ImGuiIO& io = ImGui::GetIO();
        for (int k = (int)ImGuiKey_NamedKey_BEGIN; k < (int)ImGuiKey_NamedKey_END; ++k) {
            ImGuiKey key = (ImGuiKey)k;
            bool down = ImGui::IsKeyDown(key);
            if (down && !m_KeyDownPrev[k]) {
                const char* name = ImGui::GetKeyName(key);
                AddLog("[KeyDown] %s", (name && *name) ? name : "(Unknown)");
            }
            m_KeyDownPrev[k] = down;
        }
        if (!io.InputQueueCharacters.empty()) {
            for (ImWchar c : io.InputQueueCharacters) {
                if (c >= 0x20 && c != 0x7F)
                    AddLog("[Char] '%c' (U+%04X)", (char)c, (unsigned)c);
                else
                    AddLog("[Char] U+%04X", (unsigned)c);
            }
        }

        if (ImGui::Begin(ICON_FA_TERMINAL "\tDebug Console", &m_Open))
        {
            // Toolbar
            if (ImGui::Button("Clear")) Clear();
            ImGui::SameLine(); ImGui::Checkbox("Auto-scroll", &m_AutoScroll);
            ImGui::SameLine(); ImGui::Checkbox("Pause", &m_Pause);
            ImGui::SameLine(); ImGui::Checkbox("Log mouse moves", &m_LogMouseMoves);
            ImGui::SameLine(); ImGui::Checkbox("Log clicks", &m_LogMouseClicks);
            ImGui::SameLine(); ImGui::SetNextItemWidth(180.f);

            ImGuiTextFilter& filter = *static_cast<ImGuiTextFilter*>(m_FilterPtr);
            filter.Draw("Filter");

            ImGui::Separator();

            const float inputRowHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;
            ImGui::BeginChild("ConsoleScroll", ImVec2(0, -inputRowHeight), false, ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto& line : m_Lines) {
                if (!filter.PassFilter(line.text.c_str())) continue;
                ImGui::PushStyleColor(ImGuiCol_Text, ColorFor(line.level));
                ImGui::TextUnformatted(line.text.c_str());
                ImGui::PopStyleColor();
            }
            if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            // Command input
            ImGui::Separator();
            ImGui::SetNextItemWidth(-1.0f);
            ImGuiInputTextFlags itf = ImGuiInputTextFlags_EnterReturnsTrue;
            if (m_FocusInput) { ImGui::SetKeyboardFocusHere(); m_FocusInput = false; }
            if (ImGui::InputText("##ConsoleInput", m_InputBuf, IM_ARRAYSIZE(m_InputBuf), itf)) {
                if (m_InputBuf[0]) {
                    AddLog("> %s", m_InputBuf);    // echo
                    // TODO: parse/execute commands here
                    m_InputBuf[0] = '\0';
                }
                m_FocusInput = true; // keep focus
            }
        }
        ImGui::End();
    }

    void ConsolePanel::OnSelect(Entity entity)
    {
        DEBUG_DLL_BOUNDARY("ConsolePanel::OnSelect");
        BOOM_INFO("ConsolePanel::OnSelect - Entity selected: {}", (uint32_t)entity);
    }

    void ConsolePanel::DebugConsoleState() const
    {
#ifdef DEBUG
        BOOM_INFO("=== ConsolePanel Debug State ===");
        BOOM_INFO("Lines: {}", (int)m_Lines.size());
        BOOM_INFO("MaxLines: {}", m_MaxLines);
        BOOM_INFO("AutoScroll: {}", m_AutoScroll);
        BOOM_INFO("Pause: {}", m_Pause);
        BOOM_INFO("LogMouseMoves: {}", m_LogMouseMoves);
        BOOM_INFO("LogMouseClicks: {}", m_LogMouseClicks);
        BOOM_INFO("=== End Debug State ===");
#endif
    }
} // namespace EditorUI
