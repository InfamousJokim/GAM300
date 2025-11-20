#pragma once
#include <memory>
#include <unordered_map>

#include "Vendors/imgui/imgui.h"
#include "Vendors/imGuizmo/GraphEditor.h"

// Forward declarations
namespace Boom {
    struct Entity;
    class AppContext;
    struct Animator;
	class AppInterface;
}


namespace EditorUI {

    class Editor;

    class AnimatorGraphPanel : public GraphEditor::Delegate {
    public:
        AnimatorGraphPanel(Editor* editor);
        void Render();

        // GraphEditor::Delegate interface
        bool AllowedLink(GraphEditor::NodeIndex from, GraphEditor::NodeIndex to) override;
        void SelectNode(GraphEditor::NodeIndex nodeIndex, bool selected) override;
        void MoveSelectedNodes(const ImVec2 delta) override;
        void AddLink(GraphEditor::NodeIndex inputNodeIndex, GraphEditor::SlotIndex inputSlotIndex,
            GraphEditor::NodeIndex outputNodeIndex, GraphEditor::SlotIndex outputSlotIndex) override;
        void DelLink(GraphEditor::LinkIndex linkIndex) override;
        void CustomDraw(ImDrawList* drawList, ImRect rectangle, GraphEditor::NodeIndex nodeIndex) override;
        void RightClick(GraphEditor::NodeIndex nodeIndex, GraphEditor::SlotIndex slotIndexInput,
            GraphEditor::SlotIndex slotIndexOutput) override;

        const size_t GetTemplateCount() override;
        const GraphEditor::Template GetTemplate(GraphEditor::TemplateIndex index) override;
        const size_t GetNodeCount() override;
        const GraphEditor::Node GetNode(GraphEditor::NodeIndex index) override;
        const size_t GetLinkCount() override;
        const GraphEditor::Link GetLink(GraphEditor::LinkIndex index) override;

    private:
        void DrawParametersPanel();
        void UpdateNodesFromAnimator();
        void CreateDefaultNodePosition(size_t stateIndex);

        Editor* m_Editor = nullptr;

        Boom::AppInterface* m_App = nullptr;
        Boom::Animator* m_CurrentAnimator = nullptr;

        // Node positions (persisted per state)
        std::unordered_map<size_t, ImVec2> m_NodePositions;
        std::vector<bool> m_NodeSelected;

        GraphEditor::Options m_Options;
        GraphEditor::ViewState m_ViewState;
        GraphEditor::FitOnScreen m_FitMode = GraphEditor::Fit_None;

        // UI state
        bool m_ShowContextMenu = false;
        ImVec2 m_ContextMenuPos;
        GraphEditor::NodeIndex m_ContextNode = -1;

        // Edit dialogs
        bool m_ShowEditStateDialog = false;
        bool m_ShowEditTransitionDialog = false;
        size_t m_EditingStateIndex = 0;
        size_t m_EditingTransitionParent = 0;
        size_t m_EditingTransitionIndex = 0;
    };
}
