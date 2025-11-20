#include "Panels/Inspector/InspectorPanel.h"
#include "Editor.h"          // for Editor::GetContext()
#include "Context/Context.h"        // for Boom::AppContext + scene access
#include "Vendors/imgui/imgui.h"
#include "Auxiliaries/Assets.h"
#include "Context/DebugHelpers.h"
#include <filesystem>


using namespace EditorUI;

namespace EditorUI {

    void InspectorPanel::AnimatorComponentUI(Boom::Entity& selected)
    {
        Boom::AppContext* ctx = GetContext();
        if (!ctx) return;
        if (!selected.Has<Boom::AnimatorComponent>()) return;

        ImGui::PushID("Animator");

        bool isOpen = ImGui::CollapsingHeader(
            "Animator",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap
        );

        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        const float  lineH = ImGui::GetFrameHeight();
        const float  y = headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f;

        ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, y));
        if (ImGui::Button("...", ImVec2(lineH, lineH)))
            ImGui::OpenPopup("AnimatorSettings");

        bool removed = false;
        if (ImGui::BeginPopup("AnimatorSettings")) {
            if (ImGui::MenuItem("Remove Component")) {
                removed = true;
            }
            ImGui::EndPopup();
        }

        ImGui::SetCursorScreenPos(
            ImVec2(headerMin.x, headerMax.y + ImGui::GetStyle().ItemSpacing.y)
        );

        if (isOpen) {
            ImGui::Indent(12.0f);
            ImGui::Spacing();

            auto& animComp = selected.Get<Boom::AnimatorComponent>();
            auto& animator = animComp.animator;

            if (animator) {
                size_t clipCount = animator->GetClipCount();
                size_t stateCount = animator->GetStateCount();

                ImGui::Text("Clips: %zu", clipCount);
                ImGui::Text("States: %zu", stateCount);

                // Debug: Show current runtime values
                ImGui::TextDisabled("Runtime Values:");
                for (auto& [name, value] : animator->GetFloatParams()) {
                    ImGui::TextDisabled("  %s = %.2f", name.c_str(), value);
                }
                for (auto& [name, value] : animator->GetBoolParams()) {
                    ImGui::TextDisabled("  %s = %s", name.c_str(), value ? "true" : "false");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // === Clips Management ===
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Animation Clips");
                ImGui::Spacing();

                // Display loaded clips
                std::vector<size_t> clipsToRemove;
                for (size_t i = 0; i < clipCount; ++i) {
                    const auto* clip = animator->GetClip(i);
                    if (!clip) continue;

                    ImGui::PushID(static_cast<int>(i));
                    ImGui::BulletText("%s (%.2fs)", clip->name.c_str(), clip->duration);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove")) {
                        clipsToRemove.push_back(i);
                    }
                    if (!clip->filePath.empty()) {
                        ImGui::SameLine();
                        ImGui::TextDisabled("- %s", clip->filePath.c_str());
                    }
                    ImGui::PopID();
                }

                // Remove clips (reverse order to preserve indices)
                for (auto it = clipsToRemove.rbegin(); it != clipsToRemove.rend(); ++it) {
                    animator->RemoveClip(*it);
                }

                // Load clip via drag & drop
                ImGui::Spacing();
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Load Clip:");
                ImGui::SameLine();

                ImVec2 dropZoneSize(ImGui::GetContentRegionAvail().x, 40);
                ImVec2 cursorPos = ImGui::GetCursorScreenPos();

                ImGui::InvisibleButton("##AnimDropZone", dropZoneSize);

                // Draw drop zone
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImU32 borderCol = ImGui::IsItemHovered() ? IM_COL32(100, 200, 100, 255) : IM_COL32(80, 80, 80, 255);
                drawList->AddRect(cursorPos, ImVec2(cursorPos.x + dropZoneSize.x, cursorPos.y + dropZoneSize.y), borderCol, 4.0f, 0, 2.0f);

                ImVec2 textSize = ImGui::CalcTextSize("Drag animation file here (.fbx, .gltf)");
                ImVec2 textPos(cursorPos.x + (dropZoneSize.x - textSize.x) * 0.5f, cursorPos.y + (dropZoneSize.y - textSize.y) * 0.5f);
                drawList->AddText(textPos, IM_COL32(150, 150, 150, 255), "Drag animation file here (.fbx, .gltf)");

                // Accept drop
                if (ImGui::BeginDragDropTarget()) {
                    // Accept animation file path
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONSTANTS::DND_PAYLOAD_ANIM_FILE.data())) {
                        std::string filePath((const char*)payload->Data);

                        // Extract filename without extension as default name
                        std::filesystem::path p(filePath);
                        std::string defaultName = p.stem().string();

                        animator->LoadAnimationFromFile(filePath, defaultName);
                        BOOM_INFO("Loaded animation clip from file: {}", filePath);
                    }
                    // Also accept model asset (from resource panel)
                    else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONSTANTS::DND_PAYLOAD_MODEL.data())) {
                        Boom::AssetID assetID = *(Boom::AssetID*)payload->Data;
                        auto& assetReg = m_App->GetAssetRegistry();
                        auto* modelAsset = assetReg.TryGet<Boom::ModelAsset>(assetID);
                        if (modelAsset && modelAsset->uid != EMPTY_ASSET) {
                            std::filesystem::path p(modelAsset->source);
                            std::string defaultName = p.stem().string();
                            animator->LoadAnimationFromFile(modelAsset->source, defaultName);
                            BOOM_INFO("Loaded animation clip from asset: {}", modelAsset->source);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // --- Add State button ---
                if (clipCount > 0) {
                    if (ImGui::Button("+ Add State", ImVec2(-1, 0))) {
                        std::string stateName = "State " + std::to_string(stateCount);
                        animator->AddState(stateName, 0);
                        BOOM_INFO("Added state '{}'", stateName);
                    }
                }
                else {
                    ImGui::BeginDisabled();
                    ImGui::Button("+ Add State (No clips loaded)", ImVec2(-1, 0));
                    ImGui::EndDisabled();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // --- States list ---
                size_t currentStateIdx = animator->GetCurrentStateIndex();
                if (stateCount > 0) {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "States");
                    ImGui::Spacing();
                }

                for (size_t i = 0; i < stateCount; ++i) {
                    const auto* state = animator->GetState(i);
                    if (!state) continue;

                    ImGui::PushID(static_cast<int>(i));

                    bool isCurrent = (i == currentStateIdx);
                    if (isCurrent) {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.3f, 0.2f, 0.3f));
                    }

                    ImGui::BeginChild("StateItem", ImVec2(0, 100), true);

                    if (isCurrent) {
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", state->name.c_str());
                    }
                    else {
                        ImGui::Text("%s", state->name.c_str());
                    }

                    const char* clipName = "None";
                    if (state->clipIndex < clipCount) {
                        const auto* clip = animator->GetClip(state->clipIndex);
                        if (clip) clipName = clip->name.c_str();
                    }
                    ImGui::TextDisabled("Clip: %s", clipName);

                    ImGui::Text(
                        "Speed: %.2f | Loop: %s",
                        state->speed,
                        state->loop ? "Yes" : "No"
                    );

                    ImGui::TextDisabled("Transitions: %zu", state->transitions.size());

                    // Show transitions (clickable)
                    for (size_t t = 0; t < state->transitions.size(); ++t) {
                        const auto& trans = state->transitions[t];
                        const char* targetName = "???";
                        if (trans.targetStateIndex < stateCount) {
                            const auto* targetState = animator->GetState(trans.targetStateIndex);
                            if (targetState) targetName = targetState->name.c_str();
                        }

                        const char* condType = "None";
                        switch (trans.conditionType) {
                            case Boom::Animator::Transition::FLOAT_GREATER: condType = "Float>"; break;
                            case Boom::Animator::Transition::FLOAT_LESS: condType = "Float<"; break;
                            case Boom::Animator::Transition::BOOL_EQUALS: condType = "Bool=="; break;
                            case Boom::Animator::Transition::TRIGGER: condType = "Trigger"; break;
                        }

                        ImGui::Bullet();
                        ImGui::SameLine();
                        ImGui::PushID(static_cast<int>(t));
                        if (ImGui::SmallButton("Edit")) {
                            m_EditingTransitionStateIndex = static_cast<int>(i);
                            m_EditingTransitionIndex = static_cast<int>(t);
                            // Load existing transition data
                            m_TempTransition = state->transitions[t];
                            strncpy_s(m_TransitionParamNameBuffer, m_TempTransition.parameterName.c_str(), sizeof(m_TransitionParamNameBuffer) - 1);
                            m_TransitionParamNameBuffer[sizeof(m_TransitionParamNameBuffer) - 1] = '\0';
                            m_OpenEditTransitionPopup = true;
                        }
                        ImGui::PopID();
                        ImGui::SameLine();
                        ImGui::Text("-> %s (%s)", targetName, condType);
                    }

                    ImGui::Spacing();

                    if (ImGui::Button("Edit", ImVec2(60, 0))) {
                        m_EditingStateIndex = static_cast<int>(i);
                        strncpy_s(m_StateNameBuffer, state->name.c_str(), sizeof(m_StateNameBuffer) - 1);
                        m_StateNameBuffer[sizeof(m_StateNameBuffer) - 1] = '\0';
                        m_OpenEditStatePopup = true;        // <-- just set a flag
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Remove", ImVec2(60, 0))) {
                        animator->RemoveState(i);
                        BOOM_INFO("Removed state at index {}", i);
                        ImGui::EndChild();
                        if (isCurrent) ImGui::PopStyleColor();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::SameLine();

                    if (!isCurrent) {
                        if (ImGui::Button("Set Default", ImVec2(80, 0))) {
                            animator->SetDefaultState(i);
                            BOOM_INFO("Set '%s' as default state", state->name.c_str());
                        }
                    }
                    else {
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[DEFAULT]");
                    }

                    // Add transition button
                    if (ImGui::Button("+ Transition", ImVec2(-1, 0))) {
                        m_EditingTransitionStateIndex = static_cast<int>(i);
                        m_EditingTransitionIndex = -1; // -1 means new transition
                        m_TempTransition = {}; // Reset
                        m_TransitionParamNameBuffer[0] = '\0';
                        m_OpenEditTransitionPopup = true;
                    }

                    ImGui::EndChild();

                    if (isCurrent) {
                        ImGui::PopStyleColor();
                    }

                    ImGui::PopID();
                    ImGui::Spacing();
                }

                // === Parameters Section ===
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Parameters");
                ImGui::Spacing();

                // Display existing parameters
                auto& floatParams = animator->GetFloatParams();
                auto& boolParams = animator->GetBoolParams();
                auto& triggers = animator->GetTriggers();

                // Float parameters
                std::vector<std::string> floatsToRemove;
                for (auto& [name, value] : floatParams) {
                    ImGui::PushID(name.c_str());
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("[F] %s", name.c_str());
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(-60);
                    float newVal = value;
                    if (ImGui::DragFloat("##value", &newVal, 0.01f)) {
                        animator->SetFloat(name, newVal);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("X", ImVec2(20, 0))) {
                        floatsToRemove.push_back(name);
                    }
                    ImGui::PopID();
                }
                for (auto& name : floatsToRemove) {
                    animator->GetFloatParams().erase(name);
                }

                // Bool parameters
                std::vector<std::string> boolsToRemove;
                for (auto& [name, value] : boolParams) {
                    ImGui::PushID(name.c_str());
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("[B] %s", name.c_str());
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(-60);
                    bool newVal = value;
                    if (ImGui::Checkbox("##value", &newVal)) {
                        animator->SetBool(name, newVal);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("X", ImVec2(20, 0))) {
                        boolsToRemove.push_back(name);
                    }
                    ImGui::PopID();
                }
                for (auto& name : boolsToRemove) {
                    animator->GetBoolParams().erase(name);
                }

                // Triggers (display only, triggers are auto-cleared each frame)
                if (!triggers.empty()) {
                    for (auto& name : triggers) {
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "[T] %s (active)", name.c_str());
                    }
                }

                ImGui::Spacing();

                // Add new parameter
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Add Parameter:");
                ImGui::SetNextItemWidth(100);
                const char* paramTypes[] = { "Float", "Bool", "Trigger" };
                ImGui::Combo("##ParamType", &m_NewParamType, paramTypes, 3);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150);
                ImGui::InputText("##ParamName", m_NewParamNameBuffer, sizeof(m_NewParamNameBuffer));
                ImGui::SameLine();
                if (ImGui::Button("Add", ImVec2(50, 0))) {
                    std::string paramName(m_NewParamNameBuffer);
                    if (!paramName.empty()) {
                        if (m_NewParamType == 0) {
                            animator->SetFloat(paramName, 0.0f);
                        } else if (m_NewParamType == 1) {
                            animator->SetBool(paramName, false);
                        } else if (m_NewParamType == 2) {
                            // Triggers don't need to be added upfront, just document them
                            BOOM_INFO("Trigger '{}' can be set via SetTrigger()", paramName);
                        }
                        m_NewParamNameBuffer[0] = '\0';
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }
            else {
                ImGui::TextDisabled("No animator available.");
                ImGui::Spacing();
            }

            ImGui::Unindent(12.0f);
        }

        if (removed) {
            ctx->scene.remove<Boom::AnimatorComponent>(m_App->SelectedEntity());
            ImGui::PopID();
            ImGui::Spacing();
            return;
        }

        ImGui::PopID();
        ImGui::Spacing();

        // === State Edit Popup (at function level) ===
        if (selected.Has<Boom::AnimatorComponent>()) {
            auto& animComp = selected.Get<Boom::AnimatorComponent>();
            auto& animator = animComp.animator;

            // Open the popup on the same ID stack as BeginPopupModal
            if (m_OpenEditStatePopup) {
                ImGui::OpenPopup("EditStatePopup");
                m_OpenEditStatePopup = false;
            }

            if (animator &&
                ImGui::BeginPopupModal("EditStatePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                size_t stateCount = animator->GetStateCount();
                size_t clipCount = animator->GetClipCount();

                if (m_EditingStateIndex >= 0 &&
                    m_EditingStateIndex < static_cast<int>(stateCount))
                {
                    auto* editState = animator->GetState(m_EditingStateIndex);

                    ImGui::Text("Edit State");
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Name
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Name");
                    ImGui::SameLine(100);
                    ImGui::SetNextItemWidth(200);
                    ImGui::InputText("##StateName", m_StateNameBuffer, sizeof(m_StateNameBuffer));

                    // Clip selection
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Clip");
                    ImGui::SameLine(100);
                    ImGui::SetNextItemWidth(200);

                    const char* currentClipName = "None";
                    if (editState->clipIndex < clipCount) {
                        const auto* clip = animator->GetClip(editState->clipIndex);
                        if (clip) currentClipName = clip->name.c_str();
                    }

                    if (ImGui::BeginCombo("##ClipSelect", currentClipName)) {
                        for (size_t c = 0; c < clipCount; ++c) {
                            const auto* clip = animator->GetClip(c);
                            if (!clip) continue;
                            bool isSelected = (editState->clipIndex == c);
                            if (ImGui::Selectable(clip->name.c_str(), isSelected)) {
                                editState->clipIndex = c;
                            }
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    // Speed
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Speed");
                    ImGui::SameLine(100);
                    ImGui::SetNextItemWidth(200);
                    ImGui::DragFloat("##Speed", &editState->speed, 0.01f, 0.0f, 10.0f);

                    // Loop
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Loop");
                    ImGui::SameLine(100);
                    ImGui::Checkbox("##Loop", &editState->loop);

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Buttons
                    if (ImGui::Button("Save", ImVec2(120, 0))) {
                        editState->name = std::string(m_StateNameBuffer);
                        m_EditingStateIndex = -1;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                        m_EditingStateIndex = -1;
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::EndPopup();
            }
        }

        // === Transition Edit Popup ===
        if (selected.Has<Boom::AnimatorComponent>()) {
            auto& animComp = selected.Get<Boom::AnimatorComponent>();
            auto& animator = animComp.animator;

            if (m_OpenEditTransitionPopup) {
                ImGui::OpenPopup("EditTransitionPopup");
                m_OpenEditTransitionPopup = false;
            }

            if (animator && ImGui::BeginPopupModal("EditTransitionPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                size_t stateCount = animator->GetStateCount();

                if (m_EditingTransitionStateIndex >= 0 && m_EditingTransitionStateIndex < static_cast<int>(stateCount)) {
                    auto* fromState = animator->GetState(m_EditingTransitionStateIndex);
                    bool isNewTransition = (m_EditingTransitionIndex == -1);

                    if (isNewTransition) {
                        ImGui::Text("Add Transition from '%s'", fromState->name.c_str());
                    } else {
                        ImGui::Text("Edit Transition from '%s'", fromState->name.c_str());
                    }

                    ImGui::Separator();
                    ImGui::Spacing();

                    // Target state
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Target State");
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(200);

                    const char* currentTargetName = "Select...";
                    if (m_TempTransition.targetStateIndex < stateCount) {
                        const auto* target = animator->GetState(m_TempTransition.targetStateIndex);
                        if (target) currentTargetName = target->name.c_str();
                    }

                    if (ImGui::BeginCombo("##TargetState", currentTargetName)) {
                        for (size_t s = 0; s < stateCount; ++s) {
                            const auto* state = animator->GetState(s);
                            if (!state) continue;
                            bool isSelected = (m_TempTransition.targetStateIndex == s);
                            if (ImGui::Selectable(state->name.c_str(), isSelected)) {
                                m_TempTransition.targetStateIndex = s;
                            }
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    // Condition type
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Condition");
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(200);

                    const char* condTypes[] = { "None", "Float >", "Float <", "Bool ==", "Trigger" };
                    int condTypeIdx = static_cast<int>(m_TempTransition.conditionType);
                    if (ImGui::Combo("##CondType", &condTypeIdx, condTypes, 5)) {
                        m_TempTransition.conditionType = static_cast<Boom::Animator::Transition::ConditionType>(condTypeIdx);
                    }

                    // Show parameter name input if condition needs it
                    if (m_TempTransition.conditionType != Boom::Animator::Transition::NONE) {
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("Parameter");
                        ImGui::SameLine(150);
                        ImGui::SetNextItemWidth(200);
                        if (ImGui::InputText("##ParamName", m_TransitionParamNameBuffer, sizeof(m_TransitionParamNameBuffer))) {
                            m_TempTransition.parameterName = std::string(m_TransitionParamNameBuffer);
                        }

                        // Value input for float/bool
                        if (m_TempTransition.conditionType == Boom::Animator::Transition::FLOAT_GREATER ||
                            m_TempTransition.conditionType == Boom::Animator::Transition::FLOAT_LESS) {
                            ImGui::AlignTextToFramePadding();
                            ImGui::Text("Value");
                            ImGui::SameLine(150);
                            ImGui::SetNextItemWidth(200);
                            ImGui::DragFloat("##FloatValue", &m_TempTransition.floatValue, 0.1f);
                        } else if (m_TempTransition.conditionType == Boom::Animator::Transition::BOOL_EQUALS) {
                            ImGui::AlignTextToFramePadding();
                            ImGui::Text("Value");
                            ImGui::SameLine(150);
                            ImGui::Checkbox("##BoolValue", &m_TempTransition.boolValue);
                        }
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Transition settings
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Duration");
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(200);
                    ImGui::DragFloat("##Duration", &m_TempTransition.transitionDuration, 0.01f, 0.0f, 5.0f);

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Has Exit Time");
                    ImGui::SameLine(150);
                    ImGui::Checkbox("##HasExitTime", &m_TempTransition.hasExitTime);

                    if (m_TempTransition.hasExitTime) {
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("Exit Time");
                        ImGui::SameLine(150);
                        ImGui::SetNextItemWidth(200);
                        ImGui::SliderFloat("##ExitTime", &m_TempTransition.exitTime, 0.0f, 1.0f);
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Buttons
                    if (ImGui::Button("Save", ImVec2(120, 0))) {
                        if (isNewTransition) {
                            fromState->transitions.push_back(m_TempTransition);
                            BOOM_INFO("Added transition to '{}'", animator->GetState(m_TempTransition.targetStateIndex)->name);
                        } else {
                            fromState->transitions[m_EditingTransitionIndex] = m_TempTransition;
                            BOOM_INFO("Updated transition");
                        }
                        m_EditingTransitionStateIndex = -1;
                        m_EditingTransitionIndex = -1;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                        m_EditingTransitionStateIndex = -1;
                        m_EditingTransitionIndex = -1;
                        ImGui::CloseCurrentPopup();
                    }
                    if (!isNewTransition) {
                        ImGui::SameLine();
                        if (ImGui::Button("Delete", ImVec2(120, 0))) {
                            fromState->transitions.erase(fromState->transitions.begin() + m_EditingTransitionIndex);
                            m_EditingTransitionStateIndex = -1;
                            m_EditingTransitionIndex = -1;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }

                ImGui::EndPopup();
            }
        }
    }



} // namespace EditorUI
