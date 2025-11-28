#include "Panels/AudioPanel.h"
#include "Audio/Audio.hpp"         // SoundEngine
#include "Context/DebugHelpers.h"          // optional logging
#include "Audio/TrackLibrary.h" // fallback built-in tracks
#include "Auxiliaries/Assets.h" // Asset types (for EMPTY_ASSET)
#include <algorithm>

namespace EditorUI
{

    AudioPanel::AudioPanel(AppInterface* ctx)
        : IWidget(ctx)
    {
        m_App = dynamic_cast<Boom::AppInterface*>(ctx);
        DEBUG_POINTER(m_App, "AppInterface");

        // Get context through the AppInterface
        if (m_App) {
            m_Ctx = ctx->GetContext();
            DEBUG_POINTER(m_Ctx, "AppContext");
        }

        EnsureVolumeKeys();
        RefreshAudioAssets();

        /*if (m_Tracks.empty()) m_Selected = -1;

        m_App = ctx;*/
    }

    void AudioPanel::Render()
    {
        OnShow();
    }

    void AudioPanel::OnShow()
    {
        if (!m_Show) return;

        auto& audio = SoundEngine::Instance();

        // Use accumulated delta-time to decide when to refresh (AppContext doesn't expose absolute time)
        double dt = m_App ? m_App->GetDeltaTime() : (m_Ctx ? m_Ctx->DeltaTime :0.0);
        m_LastRefreshTime += dt;
        if (m_LastRefreshTime > AUTO_REFRESH_INTERVAL) {
            m_NeedsRefresh = true;
            m_LastRefreshTime =0.0;
        }

        if (m_NeedsRefresh) {
            RefreshAudioAssets();
            m_NeedsRefresh = false;
        }

        EnsureVolumeKeys();

        if (ImGui::Begin("Music", &m_Show))
        {
            // Early out if you have no tracks
            if (m_Tracks.empty())
            {
                ImGui::TextUnformatted("No tracks configured.");
                ImGui::End();
                return;
            }

            // Clamp selection so static analyzers don’t warn
            if (m_Selected <0 || m_Selected >= static_cast<int>(m_Tracks.size()))
                m_Selected =0;

            // ----- Track picker -----
            {
                const char* current = m_Tracks[static_cast<size_t>(m_Selected)].name.c_str();
                if (ImGui::BeginCombo("Track", current))
                {
                    for (int i =0; i < static_cast<int>(m_Tracks.size()); ++i)
                    {
                        const bool isSel = (i == m_Selected);
                        if (ImGui::Selectable(m_Tracks[static_cast<size_t>(i)].name.c_str(), isSel))
                            m_Selected = i;
                        if (isSel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }

            const std::string& name = m_Tracks[static_cast<size_t>(m_Selected)].name;
            const std::string& path = m_Tracks[static_cast<size_t>(m_Selected)].path;

            // ----- Loop + Restart -----
            if (ImGui::Checkbox("Loop", &m_Loop)) {
                audio.SetLooping(name, m_Loop);
            }
            ImGui::SameLine();
            if (ImGui::Button("Restart")) {
                audio.StopAllExcept(std::string{}); // avoid implicit conv warnings
                audio.PlaySound(name, path, m_Loop);
                audio.SetVolume(name, m_Volume[name]);
                m_Paused = false;
            }

            // ----- Volume -----
            {
                float vol = m_Volume[name];
                if (ImGui::SliderFloat("Volume", &vol,0.0f,1.0f, "%.2f")) {
                    m_Volume[name] = vol;
                    audio.SetVolume(name, vol);
                }
            }

            // ----- Play / Stop / Pause -----
            {
                const bool playing = audio.IsPlaying(name);
                if (!playing) {
                    if (ImGui::Button("Play")) {
                        // Prefer the registry-provided audio paths instead of hardcoded lines
                        audio.StopAllExcept(std::string{});
                        audio.PlaySound(name, path, m_Loop);
                        audio.SetVolume(name, m_Volume[name]);
                        m_Paused = false;
                    }
                }
                else {
                    if (ImGui::Button("Stop")) {
                        audio.StopSound(name);
                        m_Paused = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::Checkbox("Paused", &m_Paused)) {
                        audio.Pause(name, m_Paused);
                    }
                }
            }

            // ----- Quick Switch -----
            ImGui::SeparatorText("Quick Switch");
            for (int i =0; i < static_cast<int>(m_Tracks.size()); ++i) {
                ImGui::PushID(i);
                if (ImGui::Button(m_Tracks[static_cast<size_t>(i)].name.c_str())) {
                    m_Selected = i;
                    const auto& n = m_Tracks[static_cast<size_t>(i)].name;
                    const auto& p = m_Tracks[static_cast<size_t>(i)].path;

                    audio.StopAllExcept(std::string{});
                    audio.PlaySound(n, p, m_Loop);
                    audio.SetVolume(n, m_Volume[n]);
                    m_Paused = false;
                }
                ImGui::PopID();
                if ((i %3) !=2) ImGui::SameLine();
            }
        }
        ImGui::End();
    }

    void AudioPanel::EnsureVolumeKeys()
    {
        for (const auto& t : m_Tracks)
        {
            if (!m_Volume.count(t.name))
                m_Volume[t.name] =1.0f;
        }
    }

    void AudioPanel::RefreshAudioAssets()
    {
        m_Tracks.clear();

        // If we have an AppInterface use the asset registry to populate audio tracks
        if (m_App) {
            m_App->AssetView([&](auto asset) {
                if (!asset) return;
                if (asset->uid == Boom::EMPTY_ASSET) return;
                const std::string& src = asset->source;
                if (src.empty()) return;

                // simple filter: only consider .wav files as audio tracks
                auto pos = src.find_last_of('.');
                if (pos == std::string::npos) return;
                std::string ext = src.substr(pos);
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (ext != ".wav") return;

                m_Tracks.push_back({ asset->name, src });
            });
        }

        // If registry produced no tracks, fall back to built-in list
        if (m_Tracks.empty()) {
            for (const auto& t : Boom::GetBuiltinTracks()) {
                m_Tracks.push_back({ t.name, t.path });
            }
        }

        // Ensure volumes exist for any new tracks
        EnsureVolumeKeys();

        // Keep selected index valid
        if (m_Selected <0 || m_Selected >= static_cast<int>(m_Tracks.size())) m_Selected =0;
    }

    void AudioPanel::PlaySelectedTrack()
    {
        if (m_Tracks.empty()) return;
        const auto& tr = m_Tracks[static_cast<size_t>(m_Selected)];
        auto& audio = SoundEngine::Instance();
        audio.StopAllExcept(std::string{});
        audio.PlaySound(tr.name, tr.path, m_Loop);
        audio.SetVolume(tr.name, m_Volume[tr.name]);
        m_Paused = false;
    }

} // namespace EditorUI
