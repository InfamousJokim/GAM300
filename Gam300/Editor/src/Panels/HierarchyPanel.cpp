#include "Panels/HierarchyPanel.h"

// keep header light; pull real types here
#include "Editor.h"
#include "Application/Interface.h"   // AppInterface (GetContext, etc.)
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "BoomEngine.h"              // InfoComponent (adjust include if needed)

#include <entt/entt.hpp>

namespace {
    glm::vec3 EulerToDirection(glm::vec3 eulerDegrees)
    {
        // Create rotation matrix from Euler angles (yaw-pitch-roll order = Y * X * Z)
        glm::mat4 rotation = glm::eulerAngleYXZ(
            glm::radians(eulerDegrees.y),  // yaw   (around Y)
            glm::radians(eulerDegrees.x),  // pitch (around X)
            glm::radians(eulerDegrees.z)   // roll  (around Z)
        );

        // Forward direction in GLM is +Z by default, but most games use -Z or +X
        // Here we assume typical FPS/game convention: forward = +X axis in local space
        glm::vec3 forward = glm::vec3(rotation[0]); // X axis of the matrix = right
        glm::vec3 up = glm::vec3(rotation[1]); // Y axis = up
        glm::vec3 right = glm::vec3(rotation[2]); // Z axis = forward? Wait!

        // Better: define local +X as forward (common in games)
        glm::vec3 localForward(0.0f, 0.0f, -1.0f);
        return glm::normalize(glm::vec3(rotation * glm::vec4(localForward, 0.0f)));
    }
}

namespace EditorUI {

    HierarchyPanel::HierarchyPanel(Editor* owner)
        : m_Owner(owner)
        , m_ShowDeletePopup(false)
        , m_EntityToDelete(entt::null)
    {
        DEBUG_DLL_BOUNDARY("HierarchyPanel::Constructor");

        if (!m_Owner) { BOOM_ERROR("HierarchyPanel - Null owner!"); return; }

        // FIXED: Since Editor now inherits from AppInterface, cast works
        m_App = static_cast<Boom::AppInterface*>(m_Owner);
        DEBUG_POINTER(m_App, "AppInterface");

        // Get context through the AppInterface
        if (m_App) {
            m_Ctx = owner->GetContext();
            DEBUG_POINTER(m_Ctx, "AppContext");
        }



    }

    //initialize function:
    // to be call once to start the transition process for camera 
    // (optimally called right when double click is gotten)
    TransformComponent* StartTransitionCam(Boom::AppInterface* m_App, Boom::AppContext* m_Ctx, float& curTime, bool& isTransitionCam, glm::vec3& startingCamPos, glm::vec3& targetPos) {
        if (!ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) return nullptr;

        //set starting, target position and begin transition boolean
        TransformComponent* camTPtr{nullptr};
        auto entView = m_App->GetEntityRegistry().view<CameraComponent, TransformComponent>();
        CameraComponent* camPtr{};
        for (auto ent : entView) {
            camPtr = &entView.get<CameraComponent>(ent);
            if (camPtr) {
                camTPtr = &entView.get<TransformComponent>(ent);
                startingCamPos = camTPtr->transform.translate;
            }
        }

        Transform3D const& t{ Boom::Entity{ &m_Ctx->scene, m_App->SelectedEntity() }.Get<TransformComponent>().transform };
        targetPos = t.translate;

        //offset camera to encompass whole object in view
        targetPos.y += .25f;
        targetPos -= EulerToDirection(camTPtr->transform.rotate) * glm::max(t.scale.x, t.scale.y, t.scale.z);

        curTime = 0.f;
        isTransitionCam = true;

        return camTPtr;
    }

    //update function:
    // to be called every frame regardless of panel being active or hidden
    // * should not be called within ImGui::Begin("Hierarchy", p_open){ ... } to allow updates even if hiearchy is not shown
    void TransitionCam(glm::vec3& curCamPos, Boom::AppInterface* m_App, float& curTime, bool& isTransitionCam, glm::vec3& startingCamPos, glm::vec3& targetPos) {
        //init constants
        const float transtitionTime{ 0.2f };
        const float dt{ (float)m_App->GetDeltaTime() };

        //call glm::slerp here to transistion camera nicely
        curTime += dt;
        float t = glm::smoothstep(0.f, 1.f, curTime / transtitionTime);
        if (t >= 1.f) {
            t = 1.f;
            isTransitionCam = false;
        }

        curCamPos = glm::lerp(startingCamPos, targetPos, t);
    }    

    // Helper to render entity tree recursively
    void HierarchyPanel::RenderEntityNode(entt::registry& registry, entt::entity entity, Boom::AppInterface* app, Boom::AppContext* ctx,
                          bool& showDeletePopup, entt::entity& entityToDelete)
    {
        const auto& info = registry.get<Boom::InfoComponent>(entity);
        const bool isSelected = (app->SelectedEntity() == entity);

        // Get children to determine if this is a leaf node
        auto children = Boom::GetChildren(registry, entity);
        bool hasChildren = !children.empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;
        if (isSelected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!hasChildren) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

	    static TransformComponent* camTPtr{ nullptr };
        static glm::vec3 startingCamPos{};
        static glm::vec3 targetPos{};

        ImGui::PushID(static_cast<int>(entt::to_integral(entity)));

        // Tree node or leaf
        bool nodeOpen = false;
        if (hasChildren) {
            nodeOpen = ImGui::TreeNodeEx(info.name.c_str(), flags);
        } else {
            ImGui::TreeNodeEx(info.name.c_str(), flags);
        }

        // Handle selection
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            app->SelectedEntity(true) = entity;

            // Double-click to focus camera
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                StartTransitionCam(m_App, m_Ctx, curTime, isTransitionCam, startingCamPos, targetPos);
            }
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            entt::entity currentParent = Boom::GetParentEntity(registry, entity);

            if (currentParent != entt::null) {
                if (ImGui::MenuItem("Unparent (Clear Parent)")) {
                    if (Boom::SetParent(registry, entity, entt::null)) {
                        BOOM_INFO("[Hierarchy] Unparented '{}'", info.name);
                    }
                }
                ImGui::Separator();
            }

            if (ImGui::MenuItem("Delete")) {
                // Mark for deletion (will show confirmation popup)
                entityToDelete = entity;
                showDeletePopup = true;
            }

            if (ImGui::MenuItem("Duplicate")) {
                entt::entity duplicated = Boom::DuplicateEntity(registry, entity, true);
                if (duplicated != entt::null) {
                    app->SelectedEntity(true) = duplicated;
                    BOOM_INFO("[Hierarchy] Duplicated '{}' (including {} children)",
                             info.name, Boom::GetChildren(registry, entity).size());
                }
            }

            ImGui::Separator();

            // Debug: Show parent info
            if (currentParent != entt::null && registry.all_of<Boom::InfoComponent>(currentParent)) {
                ImGui::TextDisabled("Parent: %s", registry.get<Boom::InfoComponent>(currentParent).name.c_str());
            } else {
                ImGui::TextDisabled("Parent: None (Root)");
            }

            auto& infoComp = registry.get<Boom::InfoComponent>(entity);
            ImGui::TextDisabled("UID: %llu", infoComp.uid);
            ImGui::TextDisabled("Parent UID: %llu", infoComp.parent);

            ImGui::EndPopup();
        }

        // Drag source (for reparenting)
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("ENTITY_HIERARCHY", &entity, sizeof(entt::entity));
            ImGui::Text("Move: %s", info.name.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target (for reparenting)
       // Drop target (for reparenting)
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY")) {
                entt::entity draggedEntity = *(const entt::entity*)payload->Data;
                // Preserve WORLD transform; local will adjust
                if (Boom::SetParent(registry, draggedEntity, entity /* preserveWorldTransform = default true */)) {
                    BOOM_INFO("[Hierarchy] Reparented '{}' to '{}'",
                        registry.get<Boom::InfoComponent>(draggedEntity).name, info.name);
                }
                else {
                    BOOM_WARN("[Hierarchy] Failed to reparent (circular reference prevented)");
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Render children recursively
        if (hasChildren && nodeOpen) {
            for (entt::entity child : children) {
                RenderEntityNode(registry, child, app, ctx, showDeletePopup, entityToDelete);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void HierarchyPanel::Render()
    {
        if (!m_Ctx) return;

        bool open_local = true;
        bool* p_open = m_ShowHierarchy ? m_ShowHierarchy : &open_local;
        //if (isTransitionCam) TransitionCam(cur)

        if (ImGui::Begin("Hierarchy", p_open))
        {
            ImGui::TextUnformatted("Scene Hierarchy");
            ImGui::Separator();

            auto& registry = m_Ctx->scene;

            // Keyboard shortcuts (only when popup is NOT open)
            if (!m_ShowDeletePopup && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                entt::entity selected = m_App->SelectedEntity();

                // Ctrl+D: Duplicate selected entity
                if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
                    if (selected != entt::null && registry.valid(selected)) {
                        entt::entity duplicated = Boom::DuplicateEntity(registry, selected, true);
                        if (duplicated != entt::null) {
                            m_App->SelectedEntity(true) = duplicated;
                            if (registry.all_of<Boom::InfoComponent>(selected)) {
                                BOOM_INFO("[Hierarchy] Duplicated '{}' with Ctrl+D",
                                         registry.get<Boom::InfoComponent>(selected).name);
                            }
                        }
                    }
                }

                // Delete: Delete selected entity
                if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                    if (selected != entt::null && registry.valid(selected)) {
                        m_EntityToDelete = selected;
                        m_ShowDeletePopup = true;
                    }
                }
            }

            // Drop target for root (to unparent entities)
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY")) {
                    entt::entity draggedEntity = *(const entt::entity*)payload->Data;
                    // Set dragged entity's parent to null (make it root)
                    if (Boom::SetParent(registry, draggedEntity, entt::null)) {
                        BOOM_INFO("[Hierarchy] Unparented '{}'",
                                 registry.get<Boom::InfoComponent>(draggedEntity).name);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Render only root entities (those with no parent)
            auto view = registry.view<Boom::InfoComponent>();
            for (entt::entity e : view)
            {
                const auto& info = view.get<Boom::InfoComponent>(e);

                // Only render if this entity has no parent (is a root)
                if (info.parent == EMPTY_ASSET) {
                    RenderEntityNode(registry, e, m_App, m_Ctx, m_ShowDeletePopup, m_EntityToDelete);
                }
            }

            // Delete confirmation popup
            if (m_ShowDeletePopup) {
                ImGui::OpenPopup("Delete Entity?");
            }

            // Center the popup before opening
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing); // Minimum width

            if (ImGui::BeginPopupModal("Delete Entity?", &m_ShowDeletePopup,
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
                if (registry.valid(m_EntityToDelete) && registry.all_of<Boom::InfoComponent>(m_EntityToDelete)) {
                    const auto& info = registry.get<Boom::InfoComponent>(m_EntityToDelete);
                    auto children = Boom::GetChildren(registry, m_EntityToDelete);

                    ImGui::Text("Are you sure you want to delete:");
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "  %s", info.name.c_str());

                    if (!children.empty()) {
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "This will also delete %zu child entities:", children.size());
                        int displayed = 0;
                        for (entt::entity child : children) {
                            if (registry.all_of<Boom::InfoComponent>(child)) {
                                ImGui::BulletText("%s", registry.get<Boom::InfoComponent>(child).name.c_str());
                                if (++displayed >= 5 && children.size() > 5) {
                                    ImGui::BulletText("... and %zu more", children.size() - 5);
                                    break;
                                }
                            }
                        }
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                        // Delete entity and all children
                        Boom::DeleteEntityRecursive(registry, m_EntityToDelete, nullptr);

                        // Clear selection if deleted entity was selected
                        if (m_App->SelectedEntity() == m_EntityToDelete) {
                            m_App->SelectedEntity(true) = entt::null;
                        }

                        m_ShowDeletePopup = false;
                        m_EntityToDelete = entt::null;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                        m_ShowDeletePopup = false;
                        m_EntityToDelete = entt::null;
                        ImGui::CloseCurrentPopup();
                    }
                } else {
                    // Entity became invalid
                    m_ShowDeletePopup = false;
                    m_EntityToDelete = entt::null;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            // If popup was closed by clicking X button, reset state
            if (!m_ShowDeletePopup && m_EntityToDelete != entt::null) {
                m_EntityToDelete = entt::null;
            }
        }
        ImGui::End();
    }

} // namespace EditorUI
