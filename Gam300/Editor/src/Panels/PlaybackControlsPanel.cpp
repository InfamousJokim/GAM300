#include "Panels/PlaybackControlsPanel.h"
#include "Editor.h"
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "AppWindow.h"           // declares Boom::Application / ApplicationState
#include "Vendors/imgui/imgui.h"

namespace EditorUI {

    PlaybackControlsPanel::PlaybackControlsPanel(Editor* owner, Boom::Application* app)
        : m_Owner(owner), m_App(app)
    {
        DEBUG_DLL_BOUNDARY("PlaybackControlsPanel::Ctor");
        if (!m_Owner) { BOOM_ERROR("PlaybackControls - null owner"); return; }
        m_Ctx = m_Owner->GetContext();
        DEBUG_POINTER(m_Ctx, "AppContext");
    }

    void PlaybackControlsPanel::Render() { OnShow(); }

    void PlaybackControlsPanel::OnShow()
    {
        if (!m_Show) return;

        if (ImGui::Begin("Playback Controls", &m_Show))
        {
            auto* app = m_App;
            if (!app) { ImGui::TextDisabled("Application not wired"); ImGui::End(); return; }

            auto state = app->GetState();
            bool isPlaying = app->IsPlaying();
            bool isPaused = app->IsPaused();

            // Display mode (like Unity's edit/play mode indicator)
            ImGui::Text("Mode: "); ImGui::SameLine();
            if (isPlaying) {
                if (isPaused) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "PLAY MODE (PAUSED)");
                } else {
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "PLAY MODE");
                }
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "EDIT MODE");
            }

            ImGui::Separator();
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

            // Play Button (Green when available, like Unity)
            {
                bool canPlay = !isPlaying || isPaused;

                if (canPlay) {
                    // Green color when play button is active (like Unity)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
                }

                if (!canPlay) ImGui::BeginDisabled();

                const char* playLabel = (!isPlaying) ? "Play" : "Resume";
                if (ImGui::Button(playLabel, ImVec2(100, 30))) {
                    if (!isPlaying) {
                        app->Play();
                    } else if (isPaused) {
                        app->Resume();
                    }
                }

                if (!canPlay) ImGui::EndDisabled();
                if (canPlay) ImGui::PopStyleColor(3);
            }

            ImGui::SameLine();

            // Pause Button
            {
                bool canPause = isPlaying && !isPaused;

                if (!canPause) ImGui::BeginDisabled();
                if (ImGui::Button("Pause", ImVec2(100, 30))) {
                    app->Pause();
                }
                if (!canPause) ImGui::EndDisabled();
            }

            ImGui::SameLine();

            // Stop Button (Red when available, like Unity)
            {
                bool canStop = isPlaying;

                if (canStop) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
                }

                if (!canStop) ImGui::BeginDisabled();
                if (ImGui::Button("Stop", ImVec2(100, 30))) {
                    app->Stop();
                }
                if (!canStop) ImGui::EndDisabled();
                if (canStop) ImGui::PopStyleColor(3);
            }

            ImGui::PopStyleVar();

            ImGui::Separator();
            ImGui::Text("Keyboard Shortcuts:");
            ImGui::BulletText("Ctrl+P: Play/Resume");
            ImGui::BulletText("Ctrl+Shift+P: Pause");
            ImGui::BulletText("Ctrl+Shift+S: Stop");

            if (isPlaying) {
                ImGui::Separator();

                // Warning about changes being lost (like Unity)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
                ImGui::TextWrapped("WARNING: Changes made during play mode will be lost when you stop!");
                ImGui::PopStyleColor();

                ImGui::Separator();
                ImGui::Text("Play Time: %.2f seconds", app->GetAdjustedTime());
                if (isPaused) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Time is paused");
                }
            } else {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Edit Mode: Changes will be saved. Press Play to test.");
            }
        }
        ImGui::End();
    }

} // namespace EditorUI
