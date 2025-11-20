#define IMGUI_DEFINE_MATH_OPERATORS
#include "AnimatorGraphPanel.h"
#include "Editor.h"
#include "Context/Context.h"
#include "BoomEngine.h"
#include "Vendors/imgui/imgui_internal.h"
#include <cstring>
#include <cstdio>

using namespace EditorUI;

AnimatorGraphPanel::AnimatorGraphPanel(Editor* editor)
    : m_Editor(editor)
    , m_App(dynamic_cast<Boom::AppInterface*>(editor))
{
    // Configure options
    m_Options.mBackgroundColor = IM_COL32(30, 30, 30, 255);
    m_Options.mGridColor = IM_COL32(50, 50, 50, 100);
    m_Options.mLineThickness = 3.0f;
    m_Options.mDisplayLinksAsCurves = true;
}

void AnimatorGraphPanel::Render()
{
    bool isOpen = ImGui::Begin("Animator Graph");

    // Critical: Check if window is collapsed, docking, or invalid
    if (!isOpen || ImGui::IsWindowCollapsed()) {
        ImGui::End();
        return;
    }

    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 windowPos = ImGui::GetWindowPos();

    // Skip rendering if window is too small or being dragged/docked
    if (windowSize.x < 100 || windowSize.y < 100) {
        ImGui::End();
        return;
    }

    // Skip if being docked (window has no valid draw area)
    ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    if (contentAvail.x < 10 || contentAvail.y < 10) {
        ImGui::End();
        return;
    }

    if (!m_Editor || !m_App) {
        ImGui::Text("No editor");
        ImGui::End();
        return;
    }

    if (m_App->SelectedEntity() == entt::null) {
        ImGui::Text("Select entity with Animator component");
        ImGui::End();
        return;
    }

    Boom::AppContext* ctx = m_Editor->GetContext();
    if (!ctx) {
        ImGui::Text("No context");
        ImGui::End();
        return;
    }

    auto& reg = ctx->scene;
    auto entity = m_App->SelectedEntity();

    if (!reg.all_of<Boom::AnimatorComponent>(entity)) {
        ImGui::Text("Select entity with Animator component");
        ImGui::End();
        return;
    }

    auto& animComp = reg.get<Boom::AnimatorComponent>(entity);
    m_CurrentAnimator = animComp.animator.get();

    if (!m_CurrentAnimator) {
        ImGui::Text("No animator");
        ImGui::End();
        return;
    }

    // One more safety check before any heavy operations
    ImVec2 availRegion = ImGui::GetContentRegionAvail();
    if (availRegion.x < 200 || availRegion.y < 100) {
        ImGui::TextDisabled("Window too small to display graph");
        ImGui::End();
        return;
    }

    UpdateNodesFromAnimator();

    // Split view: graph on left, parameters on right
    float leftWidth = availRegion.x * 0.7f;
    float rightWidth = availRegion.x - leftWidth - 8;

    if (leftWidth < 150 || rightWidth < 50) {
        ImGui::TextDisabled("Resize window to view graph");
        ImGui::End();
        return;
    }

    // BeginChild with safety - must succeed
    bool childStarted = ImGui::BeginChild("GraphView", ImVec2(leftWidth, availRegion.y), ImGuiChildFlags_Border);

    if (childStarted) {
        // Toolbar
        if (ImGui::Button("Add State")) {
            if (m_CurrentAnimator->GetClipCount() > 0) {
                size_t idx = m_CurrentAnimator->AddState("New State", 0);
                CreateDefaultNodePosition(idx);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Fit View")) {
            m_FitMode = GraphEditor::Fit_AllNodes;
        }

        // Graph editor - MUST have valid space before calling Show
        ImVec2 graphSize = ImGui::GetContentRegionAvail();

        // This is the top-left of the graph canvas in *screen space*
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();

        if (graphSize.x > 100 && graphSize.y > 80) {
            try {
                GraphEditor::Show(*this, m_Options, m_ViewState, true, &m_FitMode);
            }
            catch (...) {
                ImGui::TextDisabled("Graph rendering error");
            }
        }
        else {
            ImGui::TextDisabled("Initializing graph view...");
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImVec2 mouseScreen = ImGui::GetMousePos();

            // Check if mouse is inside the graph canvas rect
            bool insideCanvas =
                mouseScreen.x >= canvasPos.x &&
                mouseScreen.x <= canvasPos.x + graphSize.x &&
                mouseScreen.y >= canvasPos.y &&
                mouseScreen.y <= canvasPos.y + graphSize.y;

            if (insideCanvas) {
                // Mouse position in canvas-local coordinates
                ImVec2 mouseGraph;
                mouseGraph.x = mouseScreen.x - canvasPos.x;
                mouseGraph.y = mouseScreen.y - canvasPos.y;

                // Assume node positions are in the same canvas-local space as in GetNode
                m_ContextNode = (size_t)-1;

                for (size_t i = 0; i < m_CurrentAnimator->GetStateCount(); ++i) {
                    auto it = m_NodePositions.find(i);
                    if (it == m_NodePositions.end())
                        continue;

                    ImVec2 nodePos = it->second;
                    ImRect nodeRect(nodePos, nodePos + ImVec2(200, 100));

                    if (nodeRect.Contains(mouseGraph)) {
                        m_ContextNode = i;
                        break;
                    }
                }

                m_ShowContextMenu = true;
            }
        }


        // Context menu MUST be inside the GraphView child window
        if (m_ShowContextMenu) {
            ImGui::OpenPopup("GraphContextMenu");
            m_ShowContextMenu = false;
        }

        if (ImGui::BeginPopup("GraphContextMenu")) {
            bool clickedOnNode = (m_ContextNode != (size_t)-1) &&
                (m_ContextNode < m_CurrentAnimator->GetStateCount());

            // (you can temporarily debug here:)
            // ImGui::Text("ContextNode = %d", (int)m_ContextNode);

            if (clickedOnNode) {
                auto* state = m_CurrentAnimator->GetState(m_ContextNode);
                if (state) {
                    ImGui::Text("State: %s", state->name.c_str());
                    ImGui::Separator();

                    if (ImGui::MenuItem("Edit State")) {
                        m_EditingStateIndex = m_ContextNode;
                        m_ShowEditStateDialog = true;
                    }
                    if (ImGui::MenuItem("Set as Default")) {
                        m_CurrentAnimator->SetDefaultState(m_ContextNode);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete State")) {
                        // ... your existing delete logic ...
                    }
                }
            } else {
                // Empty space menu
                ImGui::Text("Graph Actions");
                ImGui::Separator();

                if (m_CurrentAnimator->GetClipCount() > 0) {
                    if (ImGui::MenuItem("Add State")) {
                        size_t idx = m_CurrentAnimator->AddState("New State", 0);
                        CreateDefaultNodePosition(idx);
                    }
                    if (ImGui::MenuItem("Add Blend Tree 1D")) {
                        size_t idx = m_CurrentAnimator->AddState("Blend Tree", 0);
                        auto* state = m_CurrentAnimator->GetState(idx);
                        if (state) {
                            state->motionType = Boom::Animator::State::BLEND_TREE_1D;
                            state->blendTree.parameterName = "Speed";
                        }
                        CreateDefaultNodePosition(idx);
                    }
                } else {
                    ImGui::TextDisabled("Add State (no clips loaded)");
                }
            }
            ImGui::EndPopup();
        }
    }

    // ALWAYS end child if we started it
    if (childStarted) {
        ImGui::EndChild();
    }

    // Only add parameter panel if we have space
    ImVec2 paramAvail = ImGui::GetContentRegionAvail();
    if (paramAvail.x >= 50 && paramAvail.y >= 50) {
        ImGui::SameLine();
        DrawParametersPanel();
    }

    // Edit state dialog (modal, shown outside child windows)
    if (m_ShowEditStateDialog) {
        ImGui::OpenPopup("Edit State");
        m_ShowEditStateDialog = false;
    }

    if (ImGui::BeginPopupModal("Edit State", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_EditingStateIndex < m_CurrentAnimator->GetStateCount()) {
            auto* state = m_CurrentAnimator->GetState(m_EditingStateIndex);
            if (state) {
                static char nameBuffer[128];
                #ifdef _MSC_VER
                    strncpy_s(nameBuffer, state->name.c_str(), _TRUNCATE);
                #else
                    std::strncpy(nameBuffer, state->name.c_str(), 127);
                    nameBuffer[127] = '\0';
                #endif

                if (ImGui::InputText("Name", nameBuffer, 128)) {
                    state->name = nameBuffer;
                }

                // Motion type selection
                const char* motionTypes[] = { "Single Clip", "Blend Tree 1D" };
                int motionType = (int)state->motionType;
                if (ImGui::Combo("Motion Type", &motionType, motionTypes, 2)) {
                    state->motionType = (Boom::Animator::State::MotionType)motionType;
                }

                ImGui::Separator();

                if (state->motionType == Boom::Animator::State::SINGLE_CLIP)
                {
                    // Single clip mode
                    int clipIdx = (int)state->clipIndex;
                    if (ImGui::SliderInt("Clip", &clipIdx, 0, std::max(0, (int)m_CurrentAnimator->GetClipCount() - 1))) {
                        state->clipIndex = clipIdx;
                    }

                    ImGui::SliderFloat("Speed", &state->speed, 0.1f, 5.0f);
                    ImGui::Checkbox("Loop", &state->loop);
                }
                else if (state->motionType == Boom::Animator::State::BLEND_TREE_1D)
                {
                    // Blend tree mode
                    ImGui::Text("Blend Tree 1D");

                    static char paramBuffer[64];
                    #ifdef _MSC_VER
                        strncpy_s(paramBuffer, state->blendTree.parameterName.c_str(), _TRUNCATE);
                    #else
                        std::strncpy(paramBuffer, state->blendTree.parameterName.c_str(), 63);
                        paramBuffer[63] = '\0';
                    #endif

                    if (ImGui::InputText("Parameter", paramBuffer, 64)) {
                        state->blendTree.parameterName = paramBuffer;
                    }

                    ImGui::SliderFloat("Speed", &state->speed, 0.1f, 5.0f);
                    ImGui::Checkbox("Loop", &state->loop);

                    ImGui::Separator();
                    ImGui::Text("Motions:");

                    // Display and edit motions
                    for (size_t i = 0; i < state->blendTree.motions.size(); ++i) {
                        ImGui::PushID((int)i);
                        auto& motion = state->blendTree.motions[i];

                        int clipIdx = (int)motion.clipIndex;
                        if (ImGui::SliderInt("Clip", &clipIdx, 0, std::max(0, (int)m_CurrentAnimator->GetClipCount() - 1))) {
                            motion.clipIndex = clipIdx;
                        }

                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(100);
                        ImGui::InputFloat("Threshold", &motion.threshold);

                        ImGui::SameLine();
                        if (ImGui::SmallButton("X")) {
                            state->blendTree.motions.erase(state->blendTree.motions.begin() + i);
                            ImGui::PopID();
                            break;
                        }

                        ImGui::PopID();
                    }

                    if (ImGui::Button("Add Motion") && m_CurrentAnimator->GetClipCount() > 0) {
                        Boom::Animator::BlendTreeMotion newMotion;
                        newMotion.clipIndex = 0;
                        newMotion.threshold = state->blendTree.motions.empty() ? 0.0f :
                            state->blendTree.motions.back().threshold + 1.0f;
                        state->blendTree.motions.push_back(newMotion);
                        state->blendTree.SortMotions();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Sort Motions")) {
                        state->blendTree.SortMotions();
                    }
                }

                // Transitions
                ImGui::Separator();
                ImGui::Text("Transitions:");

                for (size_t i = 0; i < state->transitions.size(); ++i) {
                    ImGui::PushID((int)i);
                    auto& trans = state->transitions[i];

                    auto* targetState = m_CurrentAnimator->GetState(trans.targetStateIndex);
                    bool nodeOpen = false;
                    if (targetState) {
                        nodeOpen = ImGui::TreeNode("Transition", "To: %s", targetState->name.c_str());
                    }

                    if (nodeOpen) {
                        const char* condTypes[] = { "None", "Float >", "Float <", "Bool ==", "Trigger" };
                        int condType = (int)trans.conditionType;
                        if (ImGui::Combo("Condition", &condType, condTypes, 5)) {
                            trans.conditionType = (Boom::Animator::Transition::ConditionType)condType;
                        }

                        if (trans.conditionType != Boom::Animator::Transition::NONE) {
                            static char paramName[64];
                            #ifdef _MSC_VER
                                strncpy_s(paramName, trans.parameterName.c_str(), _TRUNCATE);
                            #else
                                std::strncpy(paramName, trans.parameterName.c_str(), 63);
                                paramName[63] = '\0';
                            #endif
                            if (ImGui::InputText("Parameter", paramName, 64)) {
                                trans.parameterName = paramName;
                            }

                            if (trans.conditionType == Boom::Animator::Transition::FLOAT_GREATER ||
                                trans.conditionType == Boom::Animator::Transition::FLOAT_LESS) {
                                ImGui::InputFloat("Value", &trans.floatValue);
                            } else if (trans.conditionType == Boom::Animator::Transition::BOOL_EQUALS) {
                                ImGui::Checkbox("Value", &trans.boolValue);
                            }
                        }

                        ImGui::SliderFloat("Duration", &trans.transitionDuration, 0.0f, 2.0f);
                        ImGui::Checkbox("Has Exit Time", &trans.hasExitTime);
                        if (trans.hasExitTime) {
                            ImGui::SliderFloat("Exit Time", &trans.exitTime, 0.0f, 1.0f);
                        }

                        bool shouldDelete = ImGui::Button("Delete Transition");

                        ImGui::TreePop();

                        if (shouldDelete) {
                            state->transitions.erase(state->transitions.begin() + i);
                            ImGui::PopID();
                            break;
                        }
                    }
                    ImGui::PopID();
                }
            }
        }

        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

void AnimatorGraphPanel::DrawParametersPanel()
{
    ImVec2 paramSize = ImGui::GetContentRegionAvail();

    // Safety check before BeginChild
    if (paramSize.x < 10 || paramSize.y < 10) {
        return;
    }

    bool childStarted = ImGui::BeginChild("Parameters", paramSize, ImGuiChildFlags_Border);

    if (!childStarted) {
        return;
    }

    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Parameters");
    ImGui::Separator();

    // Float parameters
    if (ImGui::CollapsingHeader("Float", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& floats = m_CurrentAnimator->GetFloatParams();
        std::vector<std::string> toDelete;
        for (auto& [name, value] : floats) {
            ImGui::PushID(name.c_str());
            if (ImGui::Button("X")) toDelete.push_back(name);
            ImGui::SameLine();
            ImGui::SliderFloat(name.c_str(), &value, -10.0f, 10.0f);
            ImGui::PopID();
        }
        for (auto& name : toDelete) floats.erase(name);

        static char newFloat[64] = "";
        ImGui::InputText("##NewFloat", newFloat, sizeof(newFloat));
        ImGui::SameLine();
        if (ImGui::Button("Add Float") && std::strlen(newFloat) > 0) {
            m_CurrentAnimator->SetFloat(newFloat, 0.0f);
            newFloat[0] = '\0';
        }
    }

    // Bool parameters
    if (ImGui::CollapsingHeader("Bool", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& bools = m_CurrentAnimator->GetBoolParams();
        std::vector<std::string> toDelete;
        for (auto& [name, value] : bools) {
            ImGui::PushID(name.c_str());
            if (ImGui::Button("X")) toDelete.push_back(name);
            ImGui::SameLine();
            ImGui::Checkbox(name.c_str(), &value);
            ImGui::PopID();
        }
        for (auto& name : toDelete) bools.erase(name);

        static char newBool[64] = "";
        ImGui::InputText("##NewBool", newBool, sizeof(newBool));
        ImGui::SameLine();
        if (ImGui::Button("Add Bool") && std::strlen(newBool) > 0) {
            m_CurrentAnimator->SetBool(newBool, false);
            newBool[0] = '\0';
        }
    }

    ImGui::EndChild();
}

void AnimatorGraphPanel::UpdateNodesFromAnimator()
{
    if (!m_CurrentAnimator) return;

    size_t stateCount = m_CurrentAnimator->GetStateCount();
    m_NodeSelected.resize(stateCount, false);

    // Create default positions for new states
    for (size_t i = 0; i < stateCount; ++i) {
        if (m_NodePositions.find(i) == m_NodePositions.end()) {
            CreateDefaultNodePosition(i);
        }
    }
}

void AnimatorGraphPanel::CreateDefaultNodePosition(size_t stateIndex)
{
    float x = 100.0f + (stateIndex % 3) * 300.0f;
    float y = 100.0f + (stateIndex / 3) * 200.0f;
    m_NodePositions[stateIndex] = ImVec2(x, y);
}

// Delegate implementations
bool AnimatorGraphPanel::AllowedLink(GraphEditor::NodeIndex from, GraphEditor::NodeIndex to)
{
    return from != to; // No self-loops
}

void AnimatorGraphPanel::SelectNode(GraphEditor::NodeIndex nodeIndex, bool selected)
{
    if (!m_CurrentAnimator) return;

    // Ensure vector is sized correctly
    size_t stateCount = m_CurrentAnimator->GetStateCount();
    if (m_NodeSelected.size() != stateCount) {
        m_NodeSelected.resize(stateCount, false);
    }

    if (nodeIndex < m_NodeSelected.size()) {
        m_NodeSelected[nodeIndex] = selected;
    }
}

void AnimatorGraphPanel::MoveSelectedNodes(const ImVec2 delta)
{
    if (!m_CurrentAnimator) return;

    size_t stateCount = m_CurrentAnimator->GetStateCount();
    for (size_t i = 0; i < m_NodeSelected.size() && i < stateCount; ++i) {
        if (m_NodeSelected[i] && m_NodePositions.find(i) != m_NodePositions.end()) {
            m_NodePositions[i] = m_NodePositions[i] + delta;
        }
    }
}

void AnimatorGraphPanel::AddLink(GraphEditor::NodeIndex inputNodeIndex, GraphEditor::SlotIndex inputSlotIndex,
    GraphEditor::NodeIndex outputNodeIndex, GraphEditor::SlotIndex outputSlotIndex)
{
    if (!m_CurrentAnimator) return;

    // Add transition from inputNode to outputNode
    m_CurrentAnimator->AddTransition(inputNodeIndex, outputNodeIndex);
}

void AnimatorGraphPanel::DelLink(GraphEditor::LinkIndex linkIndex)
{
    if (!m_CurrentAnimator) return;

    // Find and remove the transition
    size_t currentLink = 0;
    for (size_t stateIdx = 0; stateIdx < m_CurrentAnimator->GetStateCount(); ++stateIdx) {
        auto* state = m_CurrentAnimator->GetState(stateIdx);
        if (!state) continue;

        for (size_t transIdx = 0; transIdx < state->transitions.size(); ++transIdx) {
            if (currentLink == linkIndex) {
                state->transitions.erase(state->transitions.begin() + transIdx);
                return;
            }
            currentLink++;
        }
    }
}

void AnimatorGraphPanel::CustomDraw(ImDrawList* drawList, ImRect rectangle, GraphEditor::NodeIndex nodeIndex)
{
    if (!m_CurrentAnimator) return;

    auto* state = m_CurrentAnimator->GetState(nodeIndex);
    if (!state) return;

    ImVec2 textPos = rectangle.Min + ImVec2(5, 5);

    // Show motion type
    if (state->motionType == Boom::Animator::State::BLEND_TREE_1D)
    {
        drawList->AddText(textPos, IM_COL32(100, 200, 255, 255), "Blend Tree 1D");
        textPos.y += 18;

        char param[64];
        std::snprintf(param, sizeof(param), "Param: %s", state->blendTree.parameterName.c_str());
        drawList->AddText(textPos, IM_COL32(180, 180, 180, 255), param);
        textPos.y += 16;

        char motions[64];
        std::snprintf(motions, sizeof(motions), "Motions: %zu", state->blendTree.motions.size());
        drawList->AddText(textPos, IM_COL32(150, 150, 150, 255), motions);
    }
    else
    {
        // Single clip
        auto* clip = m_CurrentAnimator->GetClip(state->clipIndex);
        if (clip) {
            drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), clip->name.c_str());
            textPos.y += 20;
        }

        char info[128];
        std::snprintf(info, sizeof(info), "Speed: %.2f | Loop: %s", state->speed, state->loop ? "Yes" : "No");
        drawList->AddText(textPos, IM_COL32(150, 150, 150, 255), info);
    }

    // Highlight current state
    if (m_CurrentAnimator->GetCurrentStateIndex() == nodeIndex) {
        drawList->AddRect(rectangle.Min, rectangle.Max, IM_COL32(255, 255, 0, 255), 3.0f, 0, 3.0f);
    }
}

void AnimatorGraphPanel::RightClick(GraphEditor::NodeIndex nodeIndex, GraphEditor::SlotIndex slotIndexInput,
    GraphEditor::SlotIndex slotIndexOutput)
{
    //// nodeIndex is -1 when clicking on empty space
    //if (nodeIndex == (GraphEditor::NodeIndex)-1) {
    //    m_ContextNode = (size_t)-1;
    //} else {
    //    m_ContextNode = nodeIndex;
    //}
    //m_ShowContextMenu = true;
}

const size_t AnimatorGraphPanel::GetTemplateCount()
{
    return 1; // Single template for state nodes
}

const GraphEditor::Template AnimatorGraphPanel::GetTemplate(GraphEditor::TemplateIndex index)
{
    static const char* inputs[] = { "In" };
    static const char* outputs[] = { "Out" };
    static ImU32 inputColors[] = { IM_COL32(150, 150, 255, 255) };
    static ImU32 outputColors[] = { IM_COL32(255, 150, 150, 255) };

    GraphEditor::Template tmpl;
    tmpl.mBackgroundColor = IM_COL32(60, 60, 70, 255);
    tmpl.mBackgroundColorOver = IM_COL32(75, 75, 85, 255);
    tmpl.mHeaderColor = IM_COL32(100, 100, 180, 255);
    tmpl.mInputCount = 1;
    tmpl.mInputNames = inputs;
    tmpl.mInputColors = inputColors;
    tmpl.mOutputCount = 1;
    tmpl.mOutputNames = outputs;
    tmpl.mOutputColors = outputColors;

    return tmpl;
}

const size_t AnimatorGraphPanel::GetNodeCount()
{
    return m_CurrentAnimator ? m_CurrentAnimator->GetStateCount() : 0;
}

const GraphEditor::Node AnimatorGraphPanel::GetNode(GraphEditor::NodeIndex index)
{
    GraphEditor::Node node;

    if (!m_CurrentAnimator || index >= m_CurrentAnimator->GetStateCount()) {
        node.mName = "Invalid";
        node.mTemplateIndex = 0;
        node.mRect = ImRect(ImVec2(0, 0), ImVec2(200, 100));
        node.mSelected = false;
        return node;
    }

    auto* state = m_CurrentAnimator->GetState(index);
    if (!state) {
        node.mName = "Invalid";
        node.mTemplateIndex = 0;
        node.mRect = ImRect(ImVec2(0, 0), ImVec2(200, 100));
        node.mSelected = false;
        return node;
    }

    node.mName = state->name.c_str();
    node.mTemplateIndex = 0;

    // Get position, create default if missing
    if (m_NodePositions.find(index) == m_NodePositions.end()) {
        CreateDefaultNodePosition(index);
    }
    ImVec2 pos = m_NodePositions[index];
    node.mRect = ImRect(pos, pos + ImVec2(200, 100));

    // Ensure selection vector is sized correctly
    if (m_NodeSelected.size() != m_CurrentAnimator->GetStateCount()) {
        m_NodeSelected.resize(m_CurrentAnimator->GetStateCount(), false);
    }
    node.mSelected = index < m_NodeSelected.size() ? m_NodeSelected[index] : false;

    return node;
}

const size_t AnimatorGraphPanel::GetLinkCount()
{
    if (!m_CurrentAnimator) return 0;

    size_t count = 0;
    for (size_t i = 0; i < m_CurrentAnimator->GetStateCount(); ++i) {
        auto* state = m_CurrentAnimator->GetState(i);
        if (state) {
            count += state->transitions.size();
        }
    }
    return count;
}

const GraphEditor::Link AnimatorGraphPanel::GetLink(GraphEditor::LinkIndex index)
{
    GraphEditor::Link link;
    link.mInputNodeIndex = 0;
    link.mInputSlotIndex = 0;
    link.mOutputNodeIndex = 0;
    link.mOutputSlotIndex = 0;

    if (!m_CurrentAnimator) return link;

    size_t currentLink = 0;
    for (size_t stateIdx = 0; stateIdx < m_CurrentAnimator->GetStateCount(); ++stateIdx) {
        auto* state = m_CurrentAnimator->GetState(stateIdx);
        if (!state) continue;

        for (auto& trans : state->transitions) {
            if (currentLink == index) {
                link.mInputNodeIndex = stateIdx;
                link.mInputSlotIndex = 0;
                link.mOutputNodeIndex = trans.targetStateIndex;
                link.mOutputSlotIndex = 0;
                return link;
            }
            currentLink++;
        }
    }

    return link;
}
