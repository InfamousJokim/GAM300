// Panels/InspectorPanel.cpp
#include "Panels/Inspector/InspectorPanel.h"
#include "Editor.h"          // for Editor::GetContext()
#include "Context/Context.h"        // for Boom::AppContext + scene access
#include "Vendors/imgui/imgui.h"
#include "Auxiliaries/Assets.h"
#include "Context/DebugHelpers.h"
#include "Panels/PropertiesImgui.h"
#include"Physics/Context.h"
//#include "BoomProperties.h"
using namespace EditorUI;

Boom::AppContext* InspectorPanel::GetContext() const {
    return m_Owner ? m_Owner->GetContext() : nullptr;
}

namespace EditorUI {

    InspectorPanel::InspectorPanel(Editor* owner, bool* showFlag)
        : m_Owner(owner)
        , m_ShowInspector(showFlag)
        , m_App(dynamic_cast<Boom::AppInterface*>(owner))
    {
        DEBUG_POINTER(m_App, "AppInterface");
    }

    // ---- templated section drawer ----
    template<typename TComponent, typename GetPropsFn>
    void InspectorPanel::DrawComponentSection(const char* title,
        TComponent* comp,
        GetPropsFn getProps,
        bool removable,
        const std::function<void()>& onRemove)
    {
        if (!comp) return;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen
            | ImGuiTreeNodeFlags_Framed
            | ImGuiTreeNodeFlags_SpanAvailWidth
            | ImGuiTreeNodeFlags_AllowItemOverlap;

        bool open = ImGui::TreeNodeEx((void*)comp, flags, "%s", title);

        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        const float  lineH = ImGui::GetFrameHeight();

        // right-align the "..." inside the header, unique popup per component
        ImGui::PushID(comp);
        if (removable) {
            const float y = headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, y));
            if (ImGui::Button("...", ImVec2(lineH, lineH)))
                ImGui::OpenPopup("ComponentSettings");

            if (ImGui::BeginPopup("ComponentSettings")) {
                if (ImGui::MenuItem("Remove Component")) {
                    if (onRemove) onRemove();
                    ImGui::EndPopup();
                    if (open) ImGui::TreePop();
                    ImGui::PopID();
                    return;
                }
                ImGui::EndPopup();
            }
        }
        ImGui::PopID();

        ImGui::SetCursorScreenPos(ImVec2(headerMin.x, headerMax.y + ImGui::GetStyle().ItemSpacing.y));

        if (open) {
            using SchemaT = const xproperty::type::object*;
            SchemaT schema = nullptr;

            // Try all reasonable call forms; your macro declares: SchemaT GetXxx(void*)
            if constexpr (std::is_invocable_r_v<SchemaT, GetPropsFn, void*>) {
                schema = getProps(static_cast<void*>(comp));
            }
            else if constexpr (std::is_invocable_r_v<SchemaT, GetPropsFn, TComponent*>) {
                schema = getProps(comp);
            }
            else if constexpr (std::is_invocable_r_v<SchemaT, GetPropsFn, const TComponent*>) {
                schema = getProps(static_cast<const TComponent*>(comp));
            }
            else if constexpr (std::is_invocable_r_v<SchemaT, GetPropsFn, TComponent&>) {
                schema = getProps(*comp);
            }
            else if constexpr (std::is_invocable_r_v<SchemaT, GetPropsFn, const TComponent&>) {
                schema = getProps(static_cast<const TComponent&>(*comp));
            }
            else {
                // (Fallback: if someone passes a void-returning drawer)
                if constexpr (std::is_invocable_v<GetPropsFn, void*>)               getProps(static_cast<void*>(comp));
                else if constexpr (std::is_invocable_v<GetPropsFn, TComponent*>)    getProps(comp);
                else if constexpr (std::is_invocable_v<GetPropsFn, const TComponent*>) getProps(static_cast<const TComponent*>(comp));
                else if constexpr (std::is_invocable_v<GetPropsFn, TComponent&>)    getProps(*comp);
                else if constexpr (std::is_invocable_v<GetPropsFn, const TComponent&>) getProps(static_cast<const TComponent&>(*comp));
            }

            // If we got a schema, render it with your UI bridge
            if (schema) {
                DrawPropertiesUI(schema, static_cast<void*>(comp));
            }

            ImGui::TreePop();
        }
    }
    void InspectorPanel::Render()
    {
        if (m_ShowInspector && !*m_ShowInspector) return;

        Boom::AppContext* ctx = GetContext();
        if (!ctx) return;

        ImGui::Begin("Inspector", m_ShowInspector);

        DeleteUpdate();
        if (m_App->SelectedEntity() != entt::null) {
            EntityUpdate();
        }
        else if (m_App->SelectedAsset().id != 0u) {
            AssetUpdate();
        }
        else {
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.5f - 20);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::TextWrapped("Select an entity in the hierarchy or an asset in resources to view its properties");
            ImGui::PopStyleColor();
        }

        ImGui::End();
    }

    void InspectorPanel::EntityUpdate() {
        Boom::AppContext* ctx = GetContext();
        // NOTE: adjust Entity wrapper to your real type/ctor signature
            // Assuming: Entity(Boom::Scene*, entt::entity)
        Boom::Entity selected{ &ctx->scene, m_App->SelectedEntity() };

        // ===== ENTITY NAME =====
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));

        if (selected.Has<Boom::InfoComponent>()) {
            auto& info = selected.Get<Boom::InfoComponent>();
            ImGui::TextUnformatted("Entity");
            ImGui::SameLine();
            ImGui::PushItemWidth(-1);
#ifdef _MSC_VER
            strncpy_s(m_NameBuffer, sizeof(m_NameBuffer), info.name.c_str(), sizeof(m_NameBuffer) - 1);
#else
            std::snprintf(m_NameBuffer, sizeof(m_NameBuffer), "%s", info.name.c_str());
#endif
            if (ImGui::InputText("##EntityName", m_NameBuffer, sizeof(m_NameBuffer))) {
                info.name = std::string(m_NameBuffer);
            }
            ImGui::PopItemWidth();

            // ===== PARENT FIELD =====
            ImGui::Spacing();
            ImGui::TextUnformatted("Parent");
            ImGui::SameLine();
            ImGui::PushItemWidth(-1);

            entt::entity currentParent = Boom::GetParentEntity(ctx->scene, m_App->SelectedEntity());
            std::string parentName = "None";
            if (currentParent != entt::null && ctx->scene.all_of<Boom::InfoComponent>(currentParent)) {
                parentName = ctx->scene.get<Boom::InfoComponent>(currentParent).name;
            }

            ImGui::Button(parentName.c_str(), ImVec2(-1, 0));

            // Drag-drop to set parent
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY")) {
                    entt::entity draggedEntity = *(const entt::entity*)payload->Data;
                    if (Boom::SetParent(ctx->scene, m_App->SelectedEntity(), draggedEntity)) {
                        BOOM_INFO("[Inspector] Set parent to '{}'",
                                 ctx->scene.get<Boom::InfoComponent>(draggedEntity).name);
                    } else {
                        BOOM_WARN("[Inspector] Failed to set parent (circular reference prevented)");
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Right-click to clear parent
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && currentParent != entt::null) {
                if (Boom::SetParent(ctx->scene, m_App->SelectedEntity(), entt::null)) {
                    BOOM_INFO("[Inspector] Cleared parent");
                }
            }

            ImGui::PopItemWidth();

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Drag an entity from Hierarchy to set parent\nRight-click to clear parent");
            }
        }

        ImGui::PopStyleVar();
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // ===== COMPONENTS =====
        if (selected.Has<Boom::TransformComponent>()) {
            auto& tc = selected.Get<Boom::TransformComponent>();
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                //modified as dragging speed should vary between variables
                ImGui::DragFloat3("Translate", &tc.transform.translate[0], 0.01f);
                ImGui::DragFloat3("Rotation", &tc.transform.rotate[0], .3142f);
                ImGui::DragFloat3("Scale", &tc.transform.scale[0], 0.01f);
                tc.transform.scale = glm::max(glm::vec3(0.01f), tc.transform.scale); //limit scale to positive
            }
        }

        if (selected.Has<Boom::CameraComponent>()) {
            auto& cc = selected.Get<Boom::CameraComponent>();
            DrawComponentSection("Camera", &cc, GetCameraComponentProperties, true,
                [&]() { ctx->scene.remove<Boom::CameraComponent>(m_App->SelectedEntity()); });
        }

        if (selected.Has<Boom::ThirdPersonCameraComponent>())
        {
            bool component_open = ImGui::CollapsingHeader("Third Person Camera", ImGuiTreeNodeFlags_DefaultOpen);

            // Add a "..." button to remove the component (optional but good to have)
            ComponentSettings<Boom::ThirdPersonCameraComponent>(ctx);

            if (component_open)
            {
                auto& tpc = selected.Get<Boom::ThirdPersonCameraComponent>();

                // --- BEGIN CUSTOM UI WIDGET ---
                ImGui::Text("Target Entity");
                ImGui::SameLine();

                // Find the name of the currently targeted entity
                const char* currentTargetName = "None";
                if (tpc.targetUID != 0) {
                    auto infoView = ctx->scene.view<Boom::InfoComponent>();
                    for (auto e : infoView) {
                        const auto& info = infoView.get<Boom::InfoComponent>(e);
                        if (info.uid == tpc.targetUID) {
                            currentTargetName = info.name.c_str();
                            break;
                        }
                    }
                }

                // Draw the dropdown menu
                if (ImGui::BeginCombo("##TargetEntity", currentTargetName))
                {
                    // Add a "None" option
                    if (ImGui::Selectable("None", tpc.targetUID == 0)) {
                        tpc.targetUID = 0;
                    }

                    // Loop through all entities with an InfoComponent to populate the list
                    auto infoView = ctx->scene.view<Boom::InfoComponent>();
                    for (auto e : infoView) {
                        const auto& info = infoView.get<Boom::InfoComponent>(e);
                        const bool isSelected = (tpc.targetUID == info.uid);
                        if (ImGui::Selectable(info.name.c_str(), isSelected)) {
                            tpc.targetUID = info.uid; // Set the UID when selected
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                // --- END CUSTOM UI WIDGET ---

                // Now draw the rest of the properties automatically using xproperty
                DrawPropertiesUI(Boom::GetThirdPersonCameraComponentProperties(&tpc), &tpc);
            }
        }

        // Model Component
        if (selected.Has<Boom::ModelComponent>()) {
            ImGui::PushID("Model Renderer");
            auto& mc = selected.Get<Boom::ModelComponent>();

            // Use CollapsingHeader to match the style
            if (ImGui::CollapsingHeader("Model Renderer", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap)) {
                if (ComponentSettings<Boom::ModelComponent>(ctx)) {
                    ImGui::PopID();
                    return; // Component was removed, exit early
                }

                // Track previous model per-entity to detect change
                static std::unordered_map<entt::entity, AssetID> previousModelIDs;
                AssetID& previousModelID = previousModelIDs[m_App->SelectedEntity()];
                const bool modelChanged = (mc.modelID != previousModelID);

                // --- UI for assigning model and material ---
                ImGui::BeginTable("##maps", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV);
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
                InputAssetWidget<CONSTANTS::DND_PAYLOAD_MODEL>("Model", mc.modelID);
                InputAssetWidget<CONSTANTS::DND_PAYLOAD_MATERIAL>("Material", mc.materialID);
                ImGui::EndTable();

                // ---- Handle animator add/remove on model change (Unity-like) ----
                if (modelChanged) {
                    previousModelID = mc.modelID;

                    if (mc.modelID != EMPTY_ASSET) {
                        // Resolve the chosen model
                        auto& modelAsset = m_App->GetAssetRegistry().Get<ModelAsset>(mc.modelID);

                        // If this model is skeletal, ensure we (re)provision an AnimatorComponent
                        if (modelAsset.hasJoints && modelAsset.data) {
                            // Try to fetch a skeletal interface / animator from the model
                            // (Your SkeletalModel should expose GetAnimator() or similar.)
                            auto skeletalModel = std::dynamic_pointer_cast<Boom::SkeletalModel>(modelAsset.data);
                            if (skeletalModel && skeletalModel->GetAnimator()) {
                                // Only auto-add if it doesn't exist (don't overwrite existing animator setup!)
                                if (!selected.Has<Boom::AnimatorComponent>()) {
                                    auto& animComp = selected.Attach<Boom::AnimatorComponent>();
                                    animComp.animator = skeletalModel->GetAnimator()->Clone();
                                    BOOM_INFO("Auto-added AnimatorComponent for skeletal model.");
                                }
                            }
                        }
                        else {
                            // Non-skeletal model: remove AnimatorComponent if present (Unity behavior)
                            if (selected.Has<Boom::AnimatorComponent>()) {
                                ctx->scene.remove<Boom::AnimatorComponent>(m_App->SelectedEntity());
                                BOOM_INFO("Removed AnimatorComponent (model is non-skeletal).");
                            }
                        }
                    }
                    else {
                        // Cleared the model asset entirely; remove animator if present
                        if (selected.Has<Boom::AnimatorComponent>()) {
                            ctx->scene.remove<Boom::AnimatorComponent>(m_App->SelectedEntity());
                            BOOM_INFO("Removed AnimatorComponent (no model assigned).");
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::SeparatorText("Physics");

                // --- UI for cooking the mesh collider ---
                if (mc.modelID != EMPTY_ASSET) {
                    ModelAsset& modelAsset = m_App->GetAssetRegistry().Get<ModelAsset>(mc.modelID);

                    if (modelAsset.data) {
                        // 1. Construct the intended path based on the model name
                        std::string saveDir = "Resources/Physics/";
                        std::string savePath = saveDir + modelAsset.name + ".pxm";

                        // 2. Check if this asset already exists in the Registry
                        auto* existingAsset = m_App->GetAssetRegistry().FindPhysicsMeshByPath(savePath);

                        // 3. Check if the file already exists on Disk
                        bool fileExists = std::filesystem::exists(savePath);

                        // 4. Logic: Only allow compilation if NEITHER exists
                        if (existingAsset || fileExists) {
                            ImGui::BeginDisabled();
                            ImGui::Button("Physics Mesh Already Compiled", ImVec2(-1, 0));
                            ImGui::EndDisabled();

                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                                if (existingAsset)
                                    ImGui::SetTooltip("Asset already registered (UID: %llu)", existingAsset->uid);
                                else
                                    ImGui::SetTooltip("File already exists at: %s", savePath.c_str());
                            }
                        }
                        else {
                            if (ImGui::Button("Compile Mesh Collider from this Model", ImVec2(-1, 0))) {
                                if (!std::filesystem::exists(saveDir)) {
                                    std::filesystem::create_directories(saveDir);
                                }

                                bool success = m_App->GetPhysicsContext().CompileAndSavePhysicsMesh(modelAsset, savePath);

                                if (success) {
                                    AssetID newID = RandomU64();
                                    m_App->GetAssetRegistry().AddPhysicsMesh(newID, savePath)->name = modelAsset.name;
                                    BOOM_INFO("Successfully cooked and created PhysicsMeshAsset '{}'", modelAsset.name);
                                    m_App->SaveAssets();
                                }
                                else {
                                    BOOM_ERROR("Failed to cook physics mesh for '{}'. Check model data.", modelAsset.name);
                                }
                            }
                        }
                    }
                    else {
                        ImGui::TextDisabled("Model data not yet loaded.");
                    }
                }
                else {
                    ImGui::TextDisabled("Assign a model to enable mesh cooking.");
                }
            }
            ImGui::PopID();
        }

        if (selected.Has<Boom::SpriteComponent>()) {
            ImGui::PushID("Sprite");
            if (ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap)) {
                if (ComponentSettings<Boom::SpriteComponent>(ctx)) {
                    ImGui::PopID();
                    return; // Component was removed, exit early
                }

                auto& q = selected.Get<Boom::SpriteComponent>();

                ImGui::Checkbox("GUI", &q.uiOverlay);
                ImGui::BeginTable("##maps", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV);
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
                InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("texture", q.textureID);
                ImGui::EndTable();
                ImGui::ColorEdit3("color", &q.color[0]);
            }
            ImGui::PopID();
        }

        if (selected.Has<Boom::AnimatorComponent>()) {
            AnimatorComponentUI(selected);
        }

        if (selected.Has<Boom::RigidBodyComponent>()) {
            ImGui::PushID("Rigid Body");

            // 1. Draw Header
            bool isOpen = ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);

            // 2. Draw "..." Button (to match photo)
            const ImVec2 headerMin = ImGui::GetItemRectMin();
            const ImVec2 headerMax = ImGui::GetItemRectMax();
            const float  lineH = ImGui::GetFrameHeight();
            const float y = headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, y));
            if (ImGui::Button("...", ImVec2(lineH, lineH)))
                ImGui::OpenPopup("RigidBodySettings");

            bool removed = false;
            if (ImGui::BeginPopup("RigidBodySettings")) {
                if (ImGui::MenuItem("Remove Component")) {
                    removed = true; // Set flag to remove later
                }
                ImGui::EndPopup();
            }

            // 3. Reset cursor
            ImGui::SetCursorScreenPos(ImVec2(headerMin.x, headerMax.y + ImGui::GetStyle().ItemSpacing.y));

            // 4. Draw Contents
            if (isOpen) {
                ImGui::Indent(12.0f);
                ImGui::Spacing();

                auto& rc = selected.Get<Boom::RigidBodyComponent>();

                RigidBody3D::Type currentType = rc.RigidBody.type;
                const char* currentTypeName;
                switch (currentType)
                {
                case RigidBody3D::Type::STATIC:  currentTypeName = "Static";  break;
                case RigidBody3D::Type::DYNAMIC: currentTypeName = "Dynamic"; break;
                default:                         currentTypeName = "Unknown"; break;
                }

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Body Type");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);

                if (ImGui::BeginCombo("##BodyType", currentTypeName))
                {
                    bool isStaticSelected = (currentType == RigidBody3D::Type::STATIC);
                    if (ImGui::Selectable("Static", isStaticSelected))
                    {
                        m_App->GetPhysicsContext().SetRigidBodyType(selected, RigidBody3D::Type::STATIC);
                    }
                    if (isStaticSelected) ImGui::SetItemDefaultFocus();

                    bool isDynamicSelected = (currentType == RigidBody3D::Type::DYNAMIC);
                    if (ImGui::Selectable("Dynamic", isDynamicSelected))
                    {
                        m_App->GetPhysicsContext().SetRigidBodyType(selected, RigidBody3D::Type::DYNAMIC);
                    }
                    if (isDynamicSelected) ImGui::SetItemDefaultFocus();

                    ImGui::EndCombo();
                }

                auto* rigidBody = &rc.RigidBody;

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Density");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##Density", &rigidBody->density, 0.01f, 0.0f, 1000.0f);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Mass");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##Mass", &rigidBody->mass, 0.1f, 0.0f, 1000.0f);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Initial Velocity");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##InitialVelocity", &rigidBody->initialVelocity.x, 0.01f);

                // --- ADD THIS NEW SECTION ---
                ImGui::Spacing();
                ImGui::SeparatorText("Constraints"); // Uses a nice separator
                ImGui::Spacing();

                // Store old values to detect changes
                bool oldFreezeX = rigidBody->freezeRotationX;
                bool oldFreezeY = rigidBody->freezeRotationY;
                bool oldFreezeZ = rigidBody->freezeRotationZ;

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Freeze Rotation");
                ImGui::SameLine(150);

                // We use Push/PopID to make the labels unique for ImGui
                ImGui::PushID("FreezeRot");
                ImGui::Checkbox("X", &rigidBody->freezeRotationX);
                ImGui::SameLine();
                ImGui::Checkbox("Y", &rigidBody->freezeRotationY);
                ImGui::SameLine();
                ImGui::Checkbox("Z", &rigidBody->freezeRotationZ);
                ImGui::PopID();

                // If any value changed, notify the physics context
                if (rigidBody->freezeRotationX != oldFreezeX ||
                    rigidBody->freezeRotationY != oldFreezeY ||
                    rigidBody->freezeRotationZ != oldFreezeZ)
                {
                    // This is a new function we will need to create in PhysicsContext 
                    m_App->GetPhysicsContext().SetRotationLock(
                        selected,
                        rigidBody->freezeRotationX,
                        rigidBody->freezeRotationY,
                        rigidBody->freezeRotationZ
                    );
                }

                ImGui::Spacing();
                ImGui::Unindent(12.0f);
            }

            ImGui::PopID();

            if (removed) {
                // Clean up physics before removing component
                m_App->GetPhysicsContext().RemoveRigidBody(selected);
                ctx->scene.remove<Boom::RigidBodyComponent>(m_App->SelectedEntity());
                return; // Exit EntityUpdate early
            }
            ImGui::Spacing();
        }
        // ----- AI (Behaviour Tree) -----
        if (selected.Has<Boom::AIComponent>()) {
            auto& ai = selected.Get<Boom::AIComponent>();
            DrawComponentSection(
                "AI (Behaviour Tree)", &ai,
                [&](void* p) -> const xproperty::type::object* {
                    auto* a = static_cast<Boom::AIComponent*>(p);
                    auto& reg = GetContext()->scene;

                    // --- MODE (this is what you’re missing) --------------------------------
                    ImGui::SeparatorText("Mode");
                    {
                        static const char* kModes[] = { "Auto", "Idle", "Patrol", "Seek" };
                        int idx = static_cast<int>(a->mode);
                        if (ImGui::Combo("Mode", &idx, kModes, IM_ARRAYSIZE(kModes))) {
                            auto newMode = static_cast<Boom::AIComponent::AIMode>(idx);
                            if (a->mode != newMode) {
                                a->mode = newMode;
                                // No more a->root.reset(); AISystem will see mode change and rebuild.
                            }
                        }
                    }

                    // --- PLAYER PICKER ------------------------------------------------------
                    const char* cur = a->playerName.empty() ? "None" : a->playerName.c_str();
                    if (ImGui::BeginCombo("Player (by name)", cur)) {
                        bool isNone = a->playerName.empty();
                        if (ImGui::Selectable("None", isNone)) { a->playerName.clear(); a->player = entt::null; }
                        if (isNone) ImGui::SetItemDefaultFocus();

                        auto view = reg.view<Boom::InfoComponent>();
                        for (auto e : view) {
                            const auto& info = view.get<Boom::InfoComponent>(e);
                            bool sel = (a->playerName == info.name);
                            if (ImGui::Selectable(info.name.c_str(), sel)) {
                                a->playerName = info.name; a->player = entt::null;
                            }
                            if (sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    // --- TUNING -------------------------------------------------------------
                    ImGui::SeparatorText("Tuning");
                    ImGui::DragFloat("Detect Radius", &a->detectRadius, 0.05f, 0.0f, 200.0f);
                    ImGui::DragFloat("Lose Radius", &a->loseRadius, 0.05f, 0.0f, 200.0f);
                    ImGui::DragFloat("Idle Wait (s)", &a->idleWait, 0.01f, 0.0f, 10.0f);
                    ImGui::InputFloat("Idle Timer (runtime)", &a->idleTimer, 0, 0, "%.3f",
                        ImGuiInputTextFlags_ReadOnly);

                    // --- PATROL -------------------------------------------------------------
                    ImGui::SeparatorText("Patrol");
                    if (selected.Has<Boom::TransformComponent>()) {
                        if (ImGui::Button("Add Point From Entity Pos", ImVec2(-1, 0))) {
                            auto& tc = selected.Get<Boom::TransformComponent>();
                            a->patrolPoints.push_back(tc.transform.translate);
                        }
                    }
                    ImGui::Text("Points: %zu", a->patrolPoints.size());
                    if (ImGui::BeginListBox("##patrol_pts", ImVec2(-1, 160))) {
                        for (int i = 0; i < (int)a->patrolPoints.size(); ++i) {
                            const auto& p3 = a->patrolPoints[i];
                            char lbl[96]; std::snprintf(lbl, sizeof(lbl), "%02d: (%.2f, %.2f, %.2f)", i, p3.x, p3.y, p3.z);
                            bool sel = (a->patrolIndex == i);
                            if (ImGui::Selectable(lbl, sel)) a->patrolIndex = i;
                            if (sel) ImGui::SetItemDefaultFocus();

                            if (ImGui::BeginPopupContextItem(lbl)) {
                                if (ImGui::MenuItem("Remove")) {
                                    a->patrolPoints.erase(a->patrolPoints.begin() + i);
                                    if (a->patrolIndex >= (int)a->patrolPoints.size())
                                        a->patrolIndex = std::max(0, (int)a->patrolPoints.size() - 1);
                                    ImGui::EndPopup(); break;
                                }
                                if (ImGui::MenuItem("Insert After (Here)")) {
                                    glm::vec3 p2 = p3;
                                    if (selected.Has<Boom::TransformComponent>())
                                        p2 = selected.Get<Boom::TransformComponent>().transform.translate;
                                    a->patrolPoints.insert(a->patrolPoints.begin() + i + 1, p2);
                                    ImGui::EndPopup(); break;
                                }
                                ImGui::EndPopup();
                            }
                        }
                        ImGui::EndListBox();
                    }
                    if (a->patrolIndex >= 0 && a->patrolIndex < (int)a->patrolPoints.size()) {
                        auto edit = a->patrolPoints[a->patrolIndex];
                        if (ImGui::DragFloat3("Edit Selected Point", &edit.x, 0.01f))
                            a->patrolPoints[a->patrolIndex] = edit;
                    }

                 
                    return nullptr;

               
                },
                /*removable=*/true,
                [&]() { GetContext()->scene.remove<Boom::AIComponent>(m_App->SelectedEntity()); }
            );
        }



        // ----- Nav Agent -----
        if (selected.Has<Boom::NavAgentComponent>()) {
            auto& ag = selected.Get<Boom::NavAgentComponent>();
            DrawComponentSection(
                "Nav Agent", &ag,
                [&](void* p) -> const xproperty::type::object* {
                    auto* a = static_cast<Boom::NavAgentComponent*>(p);
                    auto& reg = GetContext()->scene;

                    // --- UTILITIES TABLE -----------------------------------------------------
                    ImGui::BeginTable("##navtools", 2, ImGuiTableFlags_SizingStretchProp);
                    ImGui::TableSetupColumn("l", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("r", ImGuiTableColumnFlags_WidthFixed, 140.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Utilities");
                    ImGui::TableSetColumnIndex(1);

                    if (ImGui::Button("Target = Player##btn", ImVec2(-1, 0))) {
                        if (selected.Has<Boom::AIComponent>()) {
                            auto& ai = selected.Get<Boom::AIComponent>();
                            if (ai.player != entt::null && reg.all_of<Boom::TransformComponent>(ai.player)) {
                                a->target = reg.get<Boom::TransformComponent>(ai.player).transform.translate;
                                a->dirty = true; a->repathTimer = 0.f;
                            }
                        }
                    }
                    if (ImGui::Button("Target = Here##btn", ImVec2(-1, 0))) {
                        if (selected.Has<Boom::TransformComponent>()) {
                            a->target = selected.Get<Boom::TransformComponent>().transform.translate;
                            a->dirty = true; a->repathTimer = 0.f;
                        }
                    }
                    ImGui::EndTable();
                    ImGui::Separator();

                    // --- BASIC PROPERTIES ----------------------------------------------------
                    bool changed = false;

                    // Target
                    {
                        glm::vec3 t = a->target;
                        if (ImGui::DragFloat3("Target", &t.x, 0.01f)) { a->target = t; changed = true; }
                        if (ImGui::IsItemDeactivatedAfterEdit()) { a->dirty = true; a->repathTimer = 0.f; }
                    }

                    // Speed
                    {
                        float sp = a->speed;
                        if (ImGui::DragFloat("Speed (m/s)", &sp, 0.05f, 0.0f, 100.0f)) { a->speed = sp; }
                    }

                    // Arrive radius
                    {
                        float ar = a->arrive;
                        if (ImGui::DragFloat("Arrive Radius (m)", &ar, 0.01f, 0.01f, 5.0f)) { a->arrive = ar; }
                    }

                    // Active
                    ImGui::Checkbox("Active", &a->active);

                    // Repath tuning
                    {
                        float cd = a->repathCooldown;
                        float rd = a->retargetDist;
                        bool c1 = ImGui::DragFloat("Repath Cooldown (s)", &cd, 0.01f, 0.01f, 10.0f);
                        bool c2 = ImGui::DragFloat("Retarget Distance (m)", &rd, 0.01f, 0.0f, 10.0f);
                        if (c1) a->repathCooldown = cd;
                        if (c2) a->retargetDist = rd;
                        if (c1 || c2) { a->dirty = true; a->repathTimer = 0.f; }
                    }

                    // --- FOLLOW ENTITY PICKER -----------------------------------------------
                    ImGui::SeparatorText("Follow");
                    {
                        // Current label
                        std::string current = "None";
                        if (a->follow != entt::null && reg.all_of<Boom::InfoComponent>(a->follow)) {
                            current = reg.get<Boom::InfoComponent>(a->follow).name;
                        }

                        if (ImGui::BeginCombo("Follow Entity", current.c_str())) {
                            // None option
                            bool isNone = (a->follow == entt::null);
                            if (ImGui::Selectable("None", isNone)) { a->follow = entt::null; a->dirty = true; a->repathTimer = 0.f; }
                            if (isNone) ImGui::SetItemDefaultFocus();

                            // List all entities with InfoComponent
                            auto view = reg.view<Boom::InfoComponent>();
                            for (auto e : view) {
                                const auto& info = view.get<Boom::InfoComponent>(e);
                                bool sel = (a->follow == e);
                                if (ImGui::Selectable(info.name.c_str(), sel)) {
                                    a->follow = e; a->dirty = true; a->repathTimer = 0.f;  a->followName = info.name;
                                }
                                if (sel) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        // Quick actions
                        ImGui::SameLine();
                        if (ImGui::Button("Rebuild Path")) { a->dirty = true; a->repathTimer = 0.f; }
                        ImGui::SameLine();
                        if (ImGui::Button("Clear Follow")) {
                            a->follow = entt::null; a->dirty = true; a->repathTimer = 0.f; a->followName.clear();
                        }
                    }

                    // --- PATH / WAYPOINT TOOLS ----------------------------------------------
                    ImGui::SeparatorText("Path");
                    ImGui::Text("Waypoints: %d / %zu", a->waypoint, a->path.size());
                    ImGui::SameLine();
                    if (ImGui::Button("Clear Path")) { a->path.clear(); a->waypoint = 0; } // view-only for path unless edited below

                    if (!a->path.empty()) {
                        // Select current waypoint
                        if (ImGui::BeginListBox("##pathbox", ImVec2(-1, 140))) {
                            for (int i = 0; i < static_cast<int>(a->path.size()); ++i) {
                                char label[64];
                                std::snprintf(label, sizeof(label), "%02d: (%.2f, %.2f, %.2f)", i, a->path[i].x, a->path[i].y, a->path[i].z);
                                bool selectedRow = (a->waypoint == i);
                                if (ImGui::Selectable(label, selectedRow)) { a->waypoint = i; }
                                if (selectedRow) ImGui::SetItemDefaultFocus();

                                // Context menu per waypoint
                                if (ImGui::BeginPopupContextItem(label)) {
                                    if (ImGui::MenuItem("Remove")) {
                                        a->path.erase(a->path.begin() + i);
                                        if (a->waypoint >= static_cast<int>(a->path.size()))
                                            a->waypoint = (int)std::max<size_t>(0, a->path.size() ? a->path.size() - 1 : 0);
                                        ImGui::EndPopup();
                                        break;
                                    }
                                    if (ImGui::MenuItem("Insert After (use Selected Transform if any)")) {
                                        glm::vec3 insertPos = a->path[i];
                                        if (selected.Has<Boom::TransformComponent>())
                                            insertPos = selected.Get<Boom::TransformComponent>().transform.translate;
                                        a->path.insert(a->path.begin() + i + 1, insertPos);
                                        ImGui::EndPopup();
                                        break;
                                    }
                                    ImGui::EndPopup();
                                }
                            }
                            ImGui::EndListBox();
                        }

                        // Edit currently selected waypoint
                        if (a->waypoint >= 0 && a->waypoint < (int)a->path.size()) {
                            glm::vec3 wp = a->path[a->waypoint];
                            if (ImGui::DragFloat3("Edit Selected Waypoint", &wp.x, 0.01f)) {
                                a->path[a->waypoint] = wp;
                                // editing path does not need immediate rebuild unless you want:
                                // a->dirty = true; a->repathTimer = 0.f;
                            }
                            if (ImGui::Button("Snap Selected to This Entity")) {
                                if (selected.Has<Boom::TransformComponent>()) {
                                    a->path[a->waypoint] = selected.Get<Boom::TransformComponent>().transform.translate;
                                }
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Reverse Path")) {
                                std::reverse(a->path.begin(), a->path.end());
                                a->waypoint = (int)a->path.size() - 1 - a->waypoint;
                            }
                        }
                    }
                    else {
                        ImGui::TextDisabled("No path computed.");
                    }

                    // --- RUNTIME / DEBUG -----------------------------------------------------
                    ImGui::SeparatorText("Runtime");
                    {
                        float frac = 0.f;
                        if (a->repathCooldown > 0.f) frac = std::clamp(a->repathTimer / a->repathCooldown, 0.f, 1.f);
                        ImGui::ProgressBar(frac, ImVec2(-1, 0), "Repath Timer");

                        bool dirty = a->dirty;
                        if (ImGui::Checkbox("Dirty (force rebuild)", &dirty)) {
                            a->dirty = dirty;
                            if (dirty) a->repathTimer = 0.f;
                        }
                        int wp = a->waypoint;
                        if (ImGui::DragInt("Current Waypoint Index", &wp, 1, 0, (int)std::max<size_t>(1, a->path.size()) - 1)) {
                            a->waypoint = std::clamp(wp, 0, (int)std::max<size_t>(0, a->path.size() ? a->path.size() - 1 : 0));
                        }
                    }

                    // If you want your xproperty-driven editor to also show (for the subset you declared),
                    // return its meta-object here instead of nullptr. If your XPROPERTY_DEF exposes a getter like `xmeta()`,
                    // do: `return &NavAgentComponent::xmeta();`. Otherwise, keep nullptr and rely on the manual UI above.
                    return nullptr;
                },
                /*removable=*/true,
                [&]() { GetContext()->scene.remove<Boom::NavAgentComponent>(m_App->SelectedEntity()); }
            );
        }

        if (selected.Has<Boom::ColliderComponent>()) {
            ImGui::PushID("Collider");

            // 1. Draw Header
            bool isOpen = ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);

            // 2. Draw "..." Button
            const ImVec2 headerMin = ImGui::GetItemRectMin();
            const ImVec2 headerMax = ImGui::GetItemRectMax();
            const float  lineH = ImGui::GetFrameHeight();
            const float y = headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, y));
            if (ImGui::Button("...", ImVec2(lineH, lineH)))
                ImGui::OpenPopup("ColliderSettings");

            bool removed = false;
            if (ImGui::BeginPopup("ColliderSettings")) {
                if (ImGui::MenuItem("Remove Component")) {
                    removed = true;
                }
                ImGui::EndPopup();
            }

            // 3. Reset cursor
            ImGui::SetCursorScreenPos(ImVec2(headerMin.x, headerMax.y + ImGui::GetStyle().ItemSpacing.y));

            // 4. Draw Contents
            if (isOpen) {
                ImGui::Indent(12.0f);
                ImGui::Spacing();

                auto& col = selected.Get<Boom::ColliderComponent>();
                auto* collider = &col.Collider;

                // Store old values for change detection
                float oldDynamicFriction = col.Collider.dynamicFriction;
                float oldStaticFriction = col.Collider.staticFriction;
                float oldRestitution = col.Collider.restitution;
                glm::vec3 oldPos = collider->localPosition;
                glm::vec3 oldRot = collider->localRotation;
                glm::vec3 oldScale = collider->localScale;
                //bool oldIsTrigger = collider->isTrigger; // NEW

                // --- NEW: IS TRIGGER CHECKBOX ---
                ImGui::Spacing();
                ImGui::SeparatorText("Behavior");
                ImGui::Spacing();

                bool isTrigger = collider->isTrigger;
                if (ImGui::Checkbox("Is Trigger", &isTrigger)) {
                    collider->isTrigger = isTrigger;

                    // Update the physics shape immediately
                    if (collider->Shape) {
                        if (isTrigger) {
                            collider->Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                            collider->Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                        }
                        else {
                            collider->Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
                            collider->Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                        }
                    }
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Triggers do not produce collision response.\nThey only fire collision events.");
                }

                ImGui::Spacing();
                ImGui::SeparatorText("Shape");
                ImGui::Spacing();
                // --- END NEW SECTION ---

                Collider3D::Type currentType = col.Collider.type;
                const char* currentTypeName = "Unknown";
                switch (currentType)
                {
                case Collider3D::Type::BOX:     currentTypeName = "Box";     break;
                case Collider3D::Type::SPHERE:  currentTypeName = "Sphere";  break;
                case Collider3D::Type::CAPSULE: currentTypeName = "Capsule"; break;
                case Collider3D::Type::MESH:    currentTypeName = "Mesh";    break;
				case Collider3D::Type::CYLINDER:currentTypeName = "Cylinder";break;
				case Collider3D::Type::TRIANGLE:currentTypeName = "Triangle";break;
                case Collider3D::Type::PLANE:   currentTypeName = "Plane";   break;
                }

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Shape Type");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);

                if (ImGui::BeginCombo("##ColliderType", currentTypeName))
                {
                    const char* types[] = { "Box", "Sphere", "Capsule", "Mesh", "Plane", "Cylinder", "Triangle"};
                    for (int i = 0; i < IM_ARRAYSIZE(types); ++i) {
                        bool isSelected = (currentType == static_cast<Collider3D::Type>(i));
                        if (ImGui::Selectable(types[i], isSelected)) {
                            m_App->GetPhysicsContext().SetColliderType(selected, static_cast<Collider3D::Type>(i), m_App->GetAssetRegistry());
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                // Mesh asset picker (only for MESH type)
                if (currentType == Collider3D::Type::MESH)
                {
                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Physics Mesh");
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(-1);

                    auto& assetRegistry = m_App->GetAssetRegistry();
                    auto& currentAsset = assetRegistry.Get<PhysicsMeshAsset>(col.Collider.physicsMeshID);
                    const char* currentName = (currentAsset.uid != EMPTY_ASSET) ? currentAsset.name.c_str() : "Select a mesh...";

                    if (ImGui::BeginCombo("##PhysicsMesh", currentName))
                    {
                        auto& map = assetRegistry.GetMap<PhysicsMeshAsset>();

                        bool isNoneSelected = (col.Collider.physicsMeshID == EMPTY_ASSET);
                        if (ImGui::Selectable("None", isNoneSelected))
                        {
                            col.Collider.physicsMeshID = EMPTY_ASSET;
                            m_App->GetPhysicsContext().UpdateColliderShape(selected, m_App->GetAssetRegistry());
                        }
                        if (isNoneSelected) ImGui::SetItemDefaultFocus();

                        // --- FIX START: Create a set to track displayed names ---
                        std::unordered_set<std::string> displayedNames;

                        for (auto& [uid, asset] : map)
                        {
                            if (uid == EMPTY_ASSET) continue;

                            // 1. Check if we have already displayed a mesh with this name
                            if (displayedNames.find(asset->name) != displayedNames.end()) {
                                continue; // Skip duplicates
                            }

                            // 2. Mark this name as displayed
                            displayedNames.insert(asset->name);

                            // 3. PushID ensures ImGui differentiates items even if names were identical 
                            // (though the set check above prevents that, this is good safety)
                            ImGui::PushID(static_cast<int>(uid));

                            bool isSelected = (col.Collider.physicsMeshID == uid);
                            if (ImGui::Selectable(asset->name.c_str(), isSelected))
                            {
                                col.Collider.physicsMeshID = uid;
                                m_App->GetPhysicsContext().UpdateColliderShape(selected, m_App->GetAssetRegistry());
                            }
                            if (isSelected) ImGui::SetItemDefaultFocus();

                            ImGui::PopID();
                        }
                        // --- FIX END ---

                        ImGui::EndCombo();
                    }
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                // Transform section
                ImGui::SeparatorText("Transform");
                ImGui::Spacing();

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Local Position");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##LocalPosition", &collider->localPosition.x, 0.01f);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Local Rotation");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##LocalRotation", &collider->localRotation.x, 0.1f);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Local Scale");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##LocalScale", &collider->localScale.x, 0.01f);
                collider->localScale = glm::max(collider->localScale, glm::vec3(0.01f));

                // Material section (only if NOT a trigger)
                if (!collider->isTrigger) {
                    ImGui::Spacing();
                    ImGui::SeparatorText("Material");
                    ImGui::Spacing();

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Dynamic Friction");
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(-1);
                    ImGui::DragFloat("##DynamicFriction", &collider->dynamicFriction, 0.01f, 0.0f, 100.0f);

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Static Friction");
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(-1);
                    ImGui::DragFloat("##StaticFriction", &collider->staticFriction, 0.01f, 0.0f, 100.0f);

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Restitution");
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(-1);
                    ImGui::DragFloat("##Restitution", &collider->restitution, 0.01f, 0.0f, 100.0f);
                }
                else {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Material properties not used for triggers");
                    ImGui::Spacing();
                }

                // Apply changes if transform changed
                if (collider->localPosition != oldPos ||
                    collider->localRotation != oldRot ||
                    collider->localScale != oldScale)
                {
                    m_App->GetPhysicsContext().UpdateColliderShape(selected, m_App->GetAssetRegistry());
                }

                // Apply material changes if not a trigger
                if (!collider->isTrigger &&
                    (col.Collider.dynamicFriction != oldDynamicFriction ||
                        col.Collider.staticFriction != oldStaticFriction ||
                        col.Collider.restitution != oldRestitution))
                {
                    m_App->GetPhysicsContext().UpdatePhysicsMaterial(selected);
                }

                // Fit to Transform button
                ImGui::Spacing();
                if (ImGui::Button("Fit to Transform")) {
                    //auto& transform = selected.Get<TransformComponent>().transform;
                    auto& colli = selected.Get<ColliderComponent>().Collider;
                    
                    // Reset to match transform exactly
                    colli.localScale = glm::vec3(1.0f, 1.0f, 1.0f);
                    
                    // Update the physics shape
                    m_App->GetPhysicsContext().UpdateColliderShape(selected, 
                                                                     m_App->GetAssetRegistry());
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Reset collider to match the entity's transform scale");
                }

                ImGui::Spacing();
                ImGui::Unindent(12.0f);
            }
            ImGui::PopID();

            if (removed) {
                // Clean up physics before removing component
                // Only remove rigid body if the entity has one
                if (selected.Has<Boom::RigidBodyComponent>()) {
                    m_App->GetPhysicsContext().RemoveRigidBody(selected);
                }
                ctx->scene.remove<Boom::ColliderComponent>(m_App->SelectedEntity());
                return;
            }
            ImGui::Spacing();
        }

        if (selected.Has<DirectLightComponent>()) {
            auto& dl = selected.Get<DirectLightComponent>();
            DrawComponentSection(
                "Point Light",
                &dl,
                [&](void* p) -> const xproperty::type::object*
                {
                    auto* comp = static_cast<DirectLightComponent*>(p);
                    ImGui::ColorEdit3("Irradiance", &comp->light.radiance[0]);

                    return GetDirectLightComponentProperties(p);
                },
                true,
                [&]() { ctx->scene.remove<DirectLightComponent>(m_App->SelectedEntity()); }
            );
        }

        if (selected.Has<PointLightComponent>()) {
            auto& pl = selected.Get<PointLightComponent>();

            DrawComponentSection(
                "Point Light",
                &pl,
                [&](void* p) -> const xproperty::type::object*
                {
                    auto* comp = static_cast<PointLightComponent*>(p);
                    ImGui::ColorEdit3("Irradiance", &comp->light.radiance[0]);

                    return GetPointLightComponentProperties(p);
                },
                true,
                [&]() { ctx->scene.remove<PointLightComponent>(m_App->SelectedEntity()); }
            );
        }

        if (selected.Has<SpotLightComponent>()) {
            auto& sl = selected.Get<SpotLightComponent>();
            DrawComponentSection(
                "Point Light",
                &sl,
                [&](void* p) -> const xproperty::type::object*
                {
                    auto* comp = static_cast<SpotLightComponent*>(p);
                    ImGui::ColorEdit3("Irradiance", &comp->light.radiance[0]);

                    return GetSpotLightComponentProperties(p);
                },
                true,
                [&]() { ctx->scene.remove<SpotLightComponent>(m_App->SelectedEntity()); }
            );
        }

        if (selected.Has<SkyboxComponent>()) {
            ImGui::PushID("Skybox");
            if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap)) {
                if (ComponentSettings<Boom::SkyboxComponent>(ctx)) {
                    ImGui::PopID();
                    return; // Component was removed, exit early
                }
                auto& sky = selected.Get<SkyboxComponent>();

                ImGui::BeginTable("##maps", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV);
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
                InputAssetWidget<CONSTANTS::DND_PAYLOAD_SKYBOX>("skybox", sky.skyboxID);
                ImGui::EndTable();
            }
            ImGui::PopID();
        }

        if (selected.Has<Boom::PauseMenuTagComponent>()) {
            static Boom::PauseMenuTagComponent fakeTagInstance;

            DrawComponentSection("Pause Menu Tag", &fakeTagInstance, [](void*) { return nullptr; }, true,
                [&]() { ctx->scene.remove<Boom::PauseMenuTagComponent>(m_App->SelectedEntity()); });
        }

        if (selected.Has<Boom::ScriptComponent>()) {
            ImGui::PushID("Script");
            auto& sc = selected.Get<Boom::ScriptComponent>();

            // Collapsing header + settings ("..." to remove)
            bool isOpen = ImGui::CollapsingHeader("Script",
                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);

            // Settings button (remove)
            const ImVec2 headerMin = ImGui::GetItemRectMin();
            const ImVec2 headerMax = ImGui::GetItemRectMax();
            const float  lineH = ImGui::GetFrameHeight();
            const float  y = headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, y));
            if (ImGui::Button("...", ImVec2(lineH, lineH)))
                ImGui::OpenPopup("ScriptSettings");

            bool removed = false;
            if (ImGui::BeginPopup("ScriptSettings")) {
                if (ImGui::MenuItem("Remove Component"))
                    removed = true;
                ImGui::EndPopup();
            }

            // Reset cursor for contents
            ImGui::SetCursorScreenPos(ImVec2(headerMin.x,
                headerMax.y + ImGui::GetStyle().ItemSpacing.y));

            if (isOpen) {
                ImGui::Indent(12.0f);

                // ===== Context / scripting system pointer =====
                auto* appCtx = m_Owner ? m_Owner->GetContext() : nullptr;

                // Get raw pointer from unique_ptr (or nullptr if missing)
                auto* scripting = (appCtx && appCtx->scriptingSystem)
                    ? appCtx->scriptingSystem.get()
                    : nullptr;
                entt::entity currentEntity = m_App->SelectedEntity();

                // Track if we need to recreate the instance this frame
                bool enabledChanged = false;
                bool typeNameChanged = false;

                // ----- Enabled toggle -----
                if (ImGui::Checkbox("Enabled", &sc.Enabled)) {
                    enabledChanged = true;
                }

                // ----- Type name DROPDOWN -----
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Type");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);

                std::vector<std::string> availableTypes;
                if (scripting && scripting->IsAlive()) {
                    availableTypes = scripting->GetAvailableScriptTypes();
                }

                // Current selection or "None"
                const char* currentPreview = sc.TypeName.empty()
                    ? "None"
                    : sc.TypeName.c_str();

                // If scripting system isn't ready, we still show the combo but empty
                if (ImGui::BeginCombo("##ScriptTypeDropdown", currentPreview)) {
                    // "None" option
                    bool isNoneSelected = sc.TypeName.empty();
                    if (ImGui::Selectable("None", isNoneSelected)) {
                        if (!sc.TypeName.empty()) {
                            sc.TypeName.clear();
                            typeNameChanged = true;
                        }
                    }
                    if (isNoneSelected)
                        ImGui::SetItemDefaultFocus();

                    // All available script types
                    for (const auto& typeName : availableTypes) {
                        bool isSelected = (sc.TypeName == typeName);
                        if (ImGui::Selectable(typeName.c_str(), isSelected)) {
                            if (sc.TypeName != typeName) {
                                sc.TypeName = typeName;
                                typeNameChanged = true;
                            }
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }

                // Auto-recreate instance if TypeName or Enabled changed
                if ((typeNameChanged || enabledChanged) && scripting) {
                    scripting->RecreateForEntity(currentEntity, sc);
                    BOOM_INFO("[Inspector] Auto-reloaded script due to changes");
                }

                // ----- Params (JSON) -----
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Params (JSON)");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);

                static char paramsBuf[2048];
                static entt::entity lastJsonEntity = entt::null;

                if (currentEntity != lastJsonEntity) {
                    std::string initial = sc.Params.dump(2); // pretty JSON
#ifdef _MSC_VER
                    strncpy_s(paramsBuf, sizeof(paramsBuf), initial.c_str(), sizeof(paramsBuf) - 1);
#else
                    std::snprintf(paramsBuf, sizeof(paramsBuf), "%s", initial.c_str());
#endif
                    lastJsonEntity = currentEntity;
                }

                if (ImGui::InputTextMultiline(
                    "##ScriptParams",
                    paramsBuf,
                    IM_ARRAYSIZE(paramsBuf),
                    ImVec2(-1, 120),
                    ImGuiInputTextFlags_AllowTabInput))
                {
                    // Try parse back into JSON whenever text changes
                    try {
                        sc.Params = nlohmann::json::parse(paramsBuf);
                    }
                    catch (...) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Invalid JSON");
                    }
                }

                // ----- Runtime info -----
                ImGui::Separator();
                ImGui::TextDisabled("Runtime Info");
                ImGui::Text("Instance ID: %llu", (unsigned long long)sc.InstanceId);

                if (sc.InstanceId != 0 && sc.Enabled) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Active");
                }
                else if (sc.InstanceId == 0 && sc.Enabled) {
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Waiting for creation");
                }
                else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Disabled");
                }

                // ----- Reload buttons -----
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Reload This Script", ImVec2(-1, 0))) {
                    if (scripting) {
                        scripting->RecreateForEntity(currentEntity, sc);
                        BOOM_INFO("[Inspector] Manually reloaded script instance");
                    }
                }

                if (ImGui::Button("Hot Reload All Scripts (DLL)", ImVec2(-1, 0))) {
                    if (scripting) {
                        scripting->ReloadScripts();
                        BOOM_INFO("[Inspector] Hot reloaded all scripts from DLL!");
                    }
                }

                ImGui::Unindent(12.0f);
            }

            ImGui::PopID();

            if (removed) {
                // Destroy the script instance before removing component
                auto* appCtx = m_Owner ? m_Owner->GetContext() : nullptr;

                // Get raw pointer from unique_ptr (or nullptr if missing)
                auto* scripting = (appCtx && appCtx->scriptingSystem)
                    ? appCtx->scriptingSystem.get()
                    : nullptr;
                if (scripting) {
                    scripting->DestroyForEntity(m_App->SelectedEntity(), sc);
                }
                ctx->scene.remove<Boom::ScriptComponent>(m_App->SelectedEntity());
                return;
            }

            ImGui::Spacing();
        }

        // ===== Add Component =====
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        if (ImGui::Button("Add Component", ImVec2(-1, 30))) {
            ImGui::OpenPopup("AddComponentPopup");
        }
        ComponentSelector(selected);
    }

    void InspectorPanel::AssetUpdate() {
        m_App->ModifyAsset([&](auto* asset) {
            ImGui::Text("Modifying: %s (%d)", asset->name.c_str(), asset->uid);
            if (asset->type == AssetType::MATERIAL) {
                MaterialAsset* mat{ dynamic_cast<MaterialAsset*>(asset) };

                //TODO: showcase material as textured sphere
                //data variables (showcase texture name instead of map id)
                // toggle between mapID and standard slider (vec3/float)

                if (ImGui::CollapsingHeader("Maps", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::BeginTable("##maps", 6, ImGuiTableFlags_SizingFixedFit);
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("albedo map", mat->albedoMapID);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("normal map", mat->normalMapID);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("roughness map", mat->roughnessMapID);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("metallic map", mat->metallicMapID);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("occlusion map", mat->occlusionMapID);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("emissive map", mat->emissiveMapID);
                    ImGui::EndTable();
                }

                if (ImGui::CollapsingHeader("Variables", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::DragFloat3("albedo", &mat->data.albedo[0], 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat3("emissive", &mat->data.emissive[0], 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("roughness", &mat->data.roughness, 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("metallic", &mat->data.metallic, 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("occlusion", &mat->data.occlusion, 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                }
            }
            else if (asset->type == AssetType::TEXTURE) {
                TextureAsset* tex{ dynamic_cast<TextureAsset*>(asset) };
                ImGui::Image((ImTextureID)(*tex->data.get()), { 256, 256 });

                //data variables (these are settings for compression)
                if (ImGui::CollapsingHeader("Compression Settings:", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Will Compress?", &tex->data->isCompileAsCompressed);
                    if (tex->data->isCompileAsCompressed) {
                        ImGui::SliderFloat("Quality", &tex->data->quality, 0.f, 1.f);
                        ImGui::SliderInt("Alpha Threshold", &tex->data->alphaThreshold, 0, 255);
                        ImGui::SliderInt("Mip Level", &tex->data->mipLevel, 1, 24);
                        ImGui::Checkbox("Gamma", &tex->data->isGamma);
                    }
                }
            }
            else if (asset->type == AssetType::MODEL) {
                ModelAsset* m{ dynamic_cast<ModelAsset*>(asset) };
                //TODO: showcase model without texture

                if (ImGui::CollapsingHeader("Model Offset", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::DragFloat3("Translate", &m->data->modelTransform.translate[0], 0.01f);
                    ImGui::DragFloat3("Rotation", &m->data->modelTransform.rotate[0], 1.f, 0.f, 360.f);
                    ImGui::DragFloat3("Scale", &m->data->modelTransform.scale[0], 0.01f, 0.01f);
                }
            }
            else {
                ImGui::Button("nothing here!");
            }
            });
    }

    void InspectorPanel::DeleteUpdate()
    {
        // 1. Only care if something is actually selected
        const bool hasSelection =
            (m_App->SelectedEntity() != entt::null) ||
            (m_App->SelectedAsset().id != 0u);

        if (!hasSelection)
            return;

        // 2. When Delete is pressed this frame, open the popup
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            ImGui::OpenPopup("Confirm Delete");
        }

        // 3. Center the popup when it *appears*
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_Appearing,
            ImVec2(0.5f, 0.5f)
        );

        // 4. Draw the popup if it's open
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            AppInterface::AssetInfo info{};

            if (m_App->SelectedEntity() != entt::null)
            {
                Boom::Entity selectedEntity{ &m_App->GetEntityRegistry(), m_App->SelectedEntity() };
                info.name = selectedEntity.Get<Boom::InfoComponent>().name;
                info.id = selectedEntity.Get<Boom::InfoComponent>().uid;
            }
            else if (m_App->SelectedAsset().id != 0u)
            {
                info = m_App->SelectedAsset();
            }

            ImGui::Text("Are you sure you want to delete:\n%s?", info.name.c_str());
            ImGui::Separator();

            // Yes / Enter
            if (ImGui::Button("Yes", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter, false))
            {
                if (m_App->SelectedEntity() != entt::null)
                {
                    // === PHYSICS CLEANUP ===
                    Boom::Entity entity{ &m_App->GetEntityRegistry(), m_App->SelectedEntity() };
                    if (auto* physicsCtx = &m_App->GetPhysicsContext())
                    {
                        physicsCtx->RemoveRigidBody(entity);
                    }

                    m_App->GetEntityRegistry().destroy(m_App->SelectedEntity());
                    m_App->ResetAllSelected();
                }
                else if (m_App->SelectedAsset().id != 0u)
                {
                    m_App->DeleteAsset(info.id, info.type);
                    m_App->ResetAllSelected();
                }

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            // No / Escape
            if (ImGui::Button("No", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }


    // === Animator-specific UpdateComponent specialization ===
    template<>
    void InspectorPanel::UpdateComponent<Boom::AnimatorComponent>(
        Boom::ComponentID id,
        Boom::Entity& selected
    )
    {
        if (!selected.Has<Boom::AnimatorComponent>() &&
            selected.Has<Boom::ModelComponent>())
        {
            auto& modelComp = selected.Get<Boom::ModelComponent>();
            auto& assets = m_App->GetAssetRegistry();

            if (modelComp.modelID != 0) {
                auto& modelAsset = assets.Get<Boom::ModelAsset>(modelComp.modelID);

                if (modelAsset.hasJoints) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID(static_cast<int>(id));

                    if (ImGui::Selectable(COMPONENT_NAMES[static_cast<size_t>(id)].data())) {
                        auto skeletalModel =
                            std::dynamic_pointer_cast<Boom::SkeletalModel>(modelAsset.data);
                        if (skeletalModel && skeletalModel->GetAnimator()) {
                            auto& animComp = selected.Attach<Boom::AnimatorComponent>();
                            animComp.animator = skeletalModel->GetAnimator()->Clone();
                            BOOM_INFO("Added AnimatorComponent");
                        }
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::PopID();
                }
            }
        }
    }

    template <>
    void InspectorPanel::UpdateComponent<Boom::ThirdPersonCameraComponent>(Boom::ComponentID id, Boom::Entity& selected) {

        // --- OUR CUSTOM LOGIC ---
        // Only show this component in the list if the entity
        // 1. Has a CameraComponent
        // 2. Does NOT already have a ThirdPersonCameraComponent
        //
        if (selected.Has<Boom::CameraComponent>() && !selected.Has<Boom::ThirdPersonCameraComponent>()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(static_cast<int>(id));
            if (ImGui::Selectable(COMPONENT_NAMES[static_cast<size_t>(id)].data())) {
                selected.Attach<Boom::ThirdPersonCameraComponent>();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
    }

    void InspectorPanel::ComponentSelector(Boom::Entity& selected) {
        if (ImGui::BeginPopup("AddComponentPopup")) {
            ImGui::SetNextWindowSizeConstraints(ImVec2(300, 200), ImVec2(500, 600));

            ImGui::Text("Select component to add:");
            ImGui::Separator();
            if (ImGui::BeginChild("ComponentScrollArea", ImVec2(0, 250), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                if (ImGui::BeginTable("Component Table", 1, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
                    //commented out code are components that are incomplete (will crash when trying to add them/nothing to show in inspector)
                    UpdateComponent<Boom::InfoComponent>(Boom::ComponentID::INFO, selected);
                    UpdateComponent<Boom::TransformComponent>(Boom::ComponentID::TRANSFORM, selected);
                    UpdateComponent<Boom::CameraComponent>(Boom::ComponentID::CAMERA, selected);
                    UpdateComponent<Boom::RigidBodyComponent>(Boom::ComponentID::RIGIDBODY, selected);
                    UpdateComponent<Boom::ColliderComponent>(Boom::ComponentID::COLLIDER, selected);
                    UpdateComponent<Boom::ModelComponent>(Boom::ComponentID::MODEL, selected);
                    UpdateComponent<Boom::AnimatorComponent>(Boom::ComponentID::ANIMATOR, selected);
                    UpdateComponent<Boom::DirectLightComponent>(Boom::ComponentID::DIRECT_LIGHT, selected);
                    UpdateComponent<Boom::PointLightComponent>(Boom::ComponentID::POINT_LIGHT, selected);
                    UpdateComponent<Boom::SpotLightComponent>(Boom::ComponentID::SPOT_LIGHT, selected);
                    UpdateComponent<Boom::SoundComponent>(Boom::ComponentID::SOUND, selected);
                    UpdateComponent<Boom::ScriptComponent>(Boom::ComponentID::SCRIPT, selected);
                    UpdateComponent<Boom::NavAgentComponent>(Boom::ComponentID::NAV_AGENT_COMPONENT, selected);
                    UpdateComponent<Boom::AIComponent>(Boom::ComponentID::AI_COMPONENT, selected);
                    UpdateComponent<Boom::ThirdPersonCameraComponent>(Boom::ComponentID::THIRD_PERSON_CAMERA, selected);
					UpdateComponent<Boom::SpriteComponent>(Boom::ComponentID::SPRITE, selected);
                    UpdateComponent<Boom::PauseMenuTagComponent>(Boom::ComponentID::PAUSE_MENU_TAG, selected);
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }
    }

    template <class Type>
    void InspectorPanel::UpdateComponent(Boom::ComponentID id, Boom::Entity& selected) {
        if (!selected.Has<Type>()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(static_cast<int>(id));
            if (ImGui::Selectable(COMPONENT_NAMES[static_cast<size_t>(id)].data())) {

                if constexpr (std::is_same_v<Type, Boom::ColliderComponent>) {
                    // ADD THE COLLIDER (works with or without rigidbody now)
                    selected.Attach<Type>();

                    // If there's a rigidbody, use the existing path
                    if (selected.Has<Boom::RigidBodyComponent>()) {
                        m_App->GetPhysicsContext().AddRigidBody(selected, m_App->GetAssetRegistry());
                    }
                    else {
                        // No rigidbody - create a collider-only entity
                        m_App->GetPhysicsContext().AddColliderOnly(selected, m_App->GetAssetRegistry());
                    }

                    ImGui::CloseCurrentPopup();
                }
                else {
                    // This is not a collider, add it normally
                    selected.Attach<Type>();
                    if constexpr (std::is_same_v<Type, Boom::RigidBodyComponent>) {
                        m_App->GetPhysicsContext().AddRigidBody(selected, m_App->GetAssetRegistry());
                    }
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::PopID();
        }
    }

    template <class CType>
    bool InspectorPanel::ComponentSettings(Boom::AppContext* ctx) {
        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        const float  lineH = ImGui::GetFrameHeight();
        ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f));
        if (ImGui::Button("...", ImVec2(lineH, lineH)))
            ImGui::OpenPopup("ComponentSettings");
        bool removed = false;
        if (ImGui::BeginPopup("ComponentSettings")) {
            if (ImGui::MenuItem("Remove Component")) {
                ctx->scene.remove<CType>(m_App->SelectedEntity());
                removed = true;
            }
            ImGui::EndPopup();
        }
        return removed;
    }

    void InspectorPanel::AcceptIDDrop(uint64_t& data, char const* payloadType) {
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType))
            {
                IM_ASSERT(payload->DataSize == sizeof(AssetID));
                data = *(AssetID const*)payload->Data;
                ImGui::Text("Dropped ID: %llu", data);
            }
            ImGui::EndDragDropTarget();
        }
    }

    template <std::string_view const& Payload>
    void InspectorPanel::InputAssetWidget(char const* label, uint64_t& data) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        ImGui::TableSetColumnIndex(1);
        ImVec2 const fieldSize{ ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() };
        ImGui::PushID(label);

        using AssetType = typename PayloadToType<Payload>::Type;
        if (ImGui::Button(m_App->GetAssetName<AssetType>(data).data(), fieldSize)) {
            //TODO: clicking button opens asset picker window
        }
        AcceptIDDrop(data, Payload.data());
        ImGui::PopID();
    }

} // namespace EditorUI
