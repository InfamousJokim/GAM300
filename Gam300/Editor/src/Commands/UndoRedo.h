#pragma once
#pragma warning(disable : 4834) // Disable [[nodiscard]] warnings for logging
#include <memory>
#include <vector>
#include <string>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

// Forward declarations
namespace Boom {
    struct AppContext;
    struct Transform3D;
    struct InfoComponent;
    struct TransformComponent;
    struct ModelComponent;
    struct CameraComponent;
    struct Camera3D;
    struct SpriteComponent;
    using AssetID = uint64_t;
}

// Include minimal headers needed for implementation
#include "Graphics/Utilities/Data.h"  // For Transform3D
#include "ECS/ECS.hpp"                // For components, helper functions, RandomU64, EMPTY_ASSET

namespace EditorUI {

    // ==================== COMMAND INTERFACE ====================

    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
        virtual std::string GetDescription() const = 0;
    };

    using CommandPtr = std::unique_ptr<ICommand>;

    // ==================== TRANSFORM COMMAND ====================

    class TransformCommand : public ICommand {
    public:
        TransformCommand(entt::registry* registry,
                        entt::entity entity,
                        const Boom::Transform3D& oldTransform,
                        const Boom::Transform3D& newTransform,
                        const std::string& description = "Transform Change")
            : m_Registry(registry)
            , m_Entity(entity)
            , m_OldTransform(oldTransform)
            , m_NewTransform(newTransform)
            , m_Description(description)
        {
            // Store UID for serialization stability
            if (m_Registry && m_Registry->valid(m_Entity) &&
                m_Registry->all_of<Boom::InfoComponent>(m_Entity)) {
                m_EntityUID = m_Registry->get<Boom::InfoComponent>(m_Entity).uid;
            }
        }

        void Execute() override {
            entt::entity entity = FindEntityByUID();
            if (entity == entt::null) return;

            if (m_Registry->all_of<Boom::TransformComponent>(entity)) {
                m_Registry->get<Boom::TransformComponent>(entity).transform = m_NewTransform;
            }
        }

        void Undo() override {
            entt::entity entity = FindEntityByUID();
            if (entity == entt::null) return;

            if (m_Registry->all_of<Boom::TransformComponent>(entity)) {
                m_Registry->get<Boom::TransformComponent>(entity).transform = m_OldTransform;
            }
        }

        std::string GetDescription() const override { return m_Description; }

    private:
        entt::entity FindEntityByUID() const {
            if (!m_Registry) return entt::null;

            // Try cached entity first
            if (m_Registry->valid(m_Entity) &&
                m_Registry->all_of<Boom::InfoComponent>(m_Entity)) {
                if (m_Registry->get<Boom::InfoComponent>(m_Entity).uid == m_EntityUID) {
                    return m_Entity;
                }
            }

            // Search by UID
            auto view = m_Registry->view<Boom::InfoComponent>();
            for (auto e : view) {
                if (view.get<Boom::InfoComponent>(e).uid == m_EntityUID) {
                    return e;
                }
            }

            return entt::null;
        }

        entt::registry* m_Registry;
        entt::entity m_Entity;
        Boom::AssetID m_EntityUID;
        Boom::Transform3D m_OldTransform;
        Boom::Transform3D m_NewTransform;
        std::string m_Description;
    };

    // ==================== CREATE ENTITY COMMAND ====================

    class CreateEntityCommand : public ICommand {
    public:
        CreateEntityCommand(entt::registry* registry,
                           const std::string& entityName,
                           const Boom::Transform3D& transform = Boom::Transform3D{})
            : m_Registry(registry)
            , m_EntityName(entityName)
            , m_InitialTransform(transform)
            , m_CreatedEntity(entt::null)
            , m_EntityUID(Boom::RandomU64())  // Generate UID immediately
            , m_WasExecuted(false)
        {
        }

        void Execute() override {
            if (!m_Registry) return;

            if (m_WasExecuted) {
                // Re-create the entity with the same UID
                m_CreatedEntity = m_Registry->create();

                auto& info = m_Registry->emplace<Boom::InfoComponent>(m_CreatedEntity);
                info.name = m_EntityName;
                info.uid = m_EntityUID;  // Use pre-generated UID
                info.parent = m_ParentUID;

                auto& trans = m_Registry->emplace<Boom::TransformComponent>(m_CreatedEntity);
                trans.transform = m_InitialTransform;

                BOOM_INFO("[Undo/Redo] Re-created entity '{}'", m_EntityName);
            } else {
                // First-time creation
                m_CreatedEntity = m_Registry->create();

                auto& info = m_Registry->emplace<Boom::InfoComponent>(m_CreatedEntity);
                info.name = m_EntityName;
                info.uid = m_EntityUID;  // Use pre-generated UID (not auto-generated)
                m_ParentUID = info.parent;

                auto& trans = m_Registry->emplace<Boom::TransformComponent>(m_CreatedEntity);
                trans.transform = m_InitialTransform;

                m_WasExecuted = true;
                BOOM_INFO("[Undo/Redo] Created entity '{}'", m_EntityName);
            }
        }

        void Undo() override {
            if (!m_Registry || m_CreatedEntity == entt::null) return;

            if (!m_Registry->valid(m_CreatedEntity)) {
                BOOM_WARN("[Undo/Redo] Cannot undo create: entity already destroyed");
                return;
            }

            // Store parent relationship before deletion
            if (m_Registry->all_of<Boom::InfoComponent>(m_CreatedEntity)) {
                m_ParentUID = m_Registry->get<Boom::InfoComponent>(m_CreatedEntity).parent;
            }

            BOOM_INFO("[Undo/Redo] Deleting entity '{}'", m_EntityName);
            m_Registry->destroy(m_CreatedEntity);
            m_CreatedEntity = entt::null;
        }

        std::string GetDescription() const override {
            return "Create Entity '" + m_EntityName + "'";
        }

        entt::entity GetCreatedEntity() const { return m_CreatedEntity; }
        Boom::AssetID GetEntityUID() const { return m_EntityUID; }

    private:
        entt::registry* m_Registry;
        std::string m_EntityName;
        Boom::Transform3D m_InitialTransform;
        entt::entity m_CreatedEntity;
        Boom::AssetID m_EntityUID;
        Boom::AssetID m_ParentUID = 0;
        bool m_WasExecuted;
    };

    // ==================== DELETE ENTITY COMMAND ====================

    struct EntitySnapshot {
        Boom::AssetID uid = 0;
        Boom::AssetID parentUID = 0;
        std::string name;
        Boom::Transform3D transform;

        // Component flags
        bool hasModel = false;
        Boom::AssetID modelID = 0;
        Boom::AssetID materialID = 0;

        bool hasCamera = false;
        Boom::Camera3D camera;

        bool hasSprite = false;
        Boom::AssetID spriteTextureID = 0;
        glm::vec4 spriteColor = glm::vec4(1.0f);
        bool spriteUIOverlay = true;

        // Add more component snapshots as needed
        std::vector<EntitySnapshot> children;
    };

    class DeleteEntityCommand : public ICommand {
    public:
        DeleteEntityCommand(entt::registry* registry, entt::entity entity)
            : m_Registry(registry)
            , m_Entity(entity)
        {
            if (m_Registry && m_Registry->valid(entity)) {
                CaptureEntityState(entity, m_Snapshot);
            }
        }

        void Execute() override {
            entt::entity entity = FindEntityByUID(m_Snapshot.uid);
            if (entity == entt::null) {
                BOOM_WARN("[Undo/Redo] Cannot delete: entity not found");
                return;
            }

            BOOM_INFO("[Undo/Redo] Deleting entity '{}'", m_Snapshot.name);
            Boom::DeleteEntityRecursive(*m_Registry, entity, nullptr);
        }

        void Undo() override {
            if (!m_Registry) return;

            BOOM_INFO("[Undo/Redo] Restoring deleted entity '{}'", m_Snapshot.name);
            RestoreEntityState(m_Snapshot);
        }

        std::string GetDescription() const override {
            return "Delete Entity '" + m_Snapshot.name + "'";
        }

    private:
        void CaptureEntityState(entt::entity entity, EntitySnapshot& snapshot) {
            if (!m_Registry->valid(entity)) return;

            // Capture InfoComponent
            if (m_Registry->all_of<Boom::InfoComponent>(entity)) {
                const auto& info = m_Registry->get<Boom::InfoComponent>(entity);
                snapshot.uid = info.uid;
                snapshot.parentUID = info.parent;
                snapshot.name = info.name;
            }

            // Capture TransformComponent
            if (m_Registry->all_of<Boom::TransformComponent>(entity)) {
                snapshot.transform = m_Registry->get<Boom::TransformComponent>(entity).transform;
            }

            // Capture ModelComponent
            if (m_Registry->all_of<Boom::ModelComponent>(entity)) {
                snapshot.hasModel = true;
                const auto& model = m_Registry->get<Boom::ModelComponent>(entity);
                snapshot.modelID = model.modelID;
                snapshot.materialID = model.materialID;
            }

            // Capture CameraComponent
            if (m_Registry->all_of<Boom::CameraComponent>(entity)) {
                snapshot.hasCamera = true;
                snapshot.camera = m_Registry->get<Boom::CameraComponent>(entity).camera;
            }

            // Capture SpriteComponent
            if (m_Registry->all_of<Boom::SpriteComponent>(entity)) {
                snapshot.hasSprite = true;
                const auto& sprite = m_Registry->get<Boom::SpriteComponent>(entity);
                snapshot.spriteTextureID = sprite.textureID;
                snapshot.spriteColor = sprite.color;
                snapshot.spriteUIOverlay = sprite.uiOverlay;
            }

            // Capture children recursively
            auto children = Boom::GetChildren(*m_Registry, entity);
            for (entt::entity child : children) {
                EntitySnapshot childSnapshot;
                CaptureEntityState(child, childSnapshot);
                snapshot.children.push_back(std::move(childSnapshot));
            }
        }

        entt::entity RestoreEntityState(const EntitySnapshot& snapshot, entt::entity parent = entt::null) {
            if (!m_Registry) return entt::null;

            entt::entity entity = m_Registry->create();

            // Restore InfoComponent
            auto& info = m_Registry->emplace<Boom::InfoComponent>(entity);
            info.uid = snapshot.uid;
            info.name = snapshot.name;
            if (parent != entt::null && m_Registry->valid(parent) &&
                m_Registry->all_of<Boom::InfoComponent>(parent)) {
                info.parent = m_Registry->get<Boom::InfoComponent>(parent).uid;
            } else {
                info.parent = snapshot.parentUID;
            }

            // Restore TransformComponent
            auto& trans = m_Registry->emplace<Boom::TransformComponent>(entity);
            trans.transform = snapshot.transform;

            // Restore ModelComponent
            if (snapshot.hasModel) {
                auto& model = m_Registry->emplace<Boom::ModelComponent>(entity);
                model.modelID = snapshot.modelID;
                model.materialID = snapshot.materialID;
            }

            // Restore CameraComponent
            if (snapshot.hasCamera) {
                auto& camera = m_Registry->emplace<Boom::CameraComponent>(entity);
                camera.camera = snapshot.camera;
            }

            // Restore SpriteComponent
            if (snapshot.hasSprite) {
                auto& sprite = m_Registry->emplace<Boom::SpriteComponent>(entity);
                sprite.textureID = snapshot.spriteTextureID;
                sprite.color = snapshot.spriteColor;
                sprite.uiOverlay = snapshot.spriteUIOverlay;
            }

            // Restore children recursively
            for (const auto& childSnapshot : snapshot.children) {
                RestoreEntityState(childSnapshot, entity);
            }

            return entity;
        }

        entt::entity FindEntityByUID(Boom::AssetID uid) const {
            if (!m_Registry) return entt::null;

            auto view = m_Registry->view<Boom::InfoComponent>();
            for (auto e : view) {
                if (view.get<Boom::InfoComponent>(e).uid == uid) {
                    return e;
                }
            }

            return entt::null;
        }

        entt::registry* m_Registry;
        entt::entity m_Entity;
        EntitySnapshot m_Snapshot;
    };

    // ==================== COMPONENT PROPERTY COMMAND ====================

    template<typename TComponent>
    class ComponentPropertyCommand : public ICommand {
    public:
        ComponentPropertyCommand(entt::registry* registry,
                                entt::entity entity,
                                const TComponent& oldValue,
                                const TComponent& newValue,
                                const std::string& description = "Component Property Change")
            : m_Registry(registry)
            , m_Entity(entity)
            , m_OldValue(oldValue)
            , m_NewValue(newValue)
            , m_Description(description)
        {
            // Store UID for stability
            if (m_Registry && m_Registry->valid(m_Entity) &&
                m_Registry->all_of<Boom::InfoComponent>(m_Entity)) {
                m_EntityUID = m_Registry->get<Boom::InfoComponent>(m_Entity).uid;
            }
        }

        void Execute() override {
            entt::entity entity = FindEntityByUID();
            if (entity == entt::null) {
                BOOM_WARN("[ComponentPropertyCommand] Execute failed: entity UID {} not found", m_EntityUID);
                return;
            }

            if (m_Registry->all_of<TComponent>(entity)) {
                m_Registry->get<TComponent>(entity) = m_NewValue;
                BOOM_INFO("[ComponentPropertyCommand] Execute: Applied new value to entity UID {}", m_EntityUID);
            } else {
                BOOM_WARN("[ComponentPropertyCommand] Execute: entity {} missing component", static_cast<uint32_t>(entity));
            }
        }

        void Undo() override {
            entt::entity entity = FindEntityByUID();
            if (entity == entt::null) {
                BOOM_WARN("[ComponentPropertyCommand] Undo failed: entity UID {} not found", m_EntityUID);
                return;
            }

            if (m_Registry->all_of<TComponent>(entity)) {
                m_Registry->get<TComponent>(entity) = m_OldValue;
                BOOM_INFO("[ComponentPropertyCommand] Undo: Restored old value to entity UID {}", m_EntityUID);
            } else {
                BOOM_WARN("[ComponentPropertyCommand] Undo: entity {} missing component", static_cast<uint32_t>(entity));
            }
        }

        std::string GetDescription() const override { return m_Description; }

    private:
        entt::entity FindEntityByUID() const {
            if (!m_Registry) return entt::null;

            // Try cached entity first
            if (m_Registry->valid(m_Entity) &&
                m_Registry->all_of<Boom::InfoComponent>(m_Entity)) {
                if (m_Registry->get<Boom::InfoComponent>(m_Entity).uid == m_EntityUID) {
                    return m_Entity;
                }
            }

            // Search by UID
            auto view = m_Registry->view<Boom::InfoComponent>();
            for (auto e : view) {
                if (view.get<Boom::InfoComponent>(e).uid == m_EntityUID) {
                    return e;
                }
            }

            return entt::null;
        }

        entt::registry* m_Registry;
        entt::entity m_Entity;
        Boom::AssetID m_EntityUID = 0;
        TComponent m_OldValue;
        TComponent m_NewValue;
        std::string m_Description;
    };

    // ==================== DUPLICATE ENTITY COMMAND ====================

    class DuplicateEntityCommand : public ICommand {
    public:
        DuplicateEntityCommand(entt::registry* registry, entt::entity sourceEntity)
            : m_Registry(registry)
            , m_SourceEntity(sourceEntity)
            , m_DuplicatedEntity(entt::null)
        {
            // Store source UID for stability
            if (m_Registry && m_Registry->valid(sourceEntity) &&
                m_Registry->all_of<Boom::InfoComponent>(sourceEntity)) {
                m_SourceUID = m_Registry->get<Boom::InfoComponent>(sourceEntity).uid;
            }
        }

        void Execute() override {
            if (!m_Registry) return;

            entt::entity source = FindEntityByUID(m_SourceUID);
            if (source == entt::null) {
                BOOM_WARN("[Undo] Cannot duplicate: source entity UID {} not found", m_SourceUID);
                return;
            }

            // Duplicate the entity (including children)
            m_DuplicatedEntity = Boom::DuplicateEntity(*m_Registry, source, true);

            if (m_DuplicatedEntity != entt::null && m_Registry->all_of<Boom::InfoComponent>(m_DuplicatedEntity)) {
                m_DuplicatedUID = m_Registry->get<Boom::InfoComponent>(m_DuplicatedEntity).uid;
                BOOM_INFO("[Undo] Duplicated entity - new UID: {}", m_DuplicatedUID);
            }
        }

        void Undo() override {
            if (!m_Registry || m_DuplicatedEntity == entt::null) return;

            entt::entity dup = FindEntityByUID(m_DuplicatedUID);
            if (dup == entt::null) {
                BOOM_WARN("[Undo] Cannot undo duplicate: duplicated entity UID {} not found", m_DuplicatedUID);
                return;
            }

            // Delete the duplicated entity (including its children)
            BOOM_INFO("[Undo] Deleting duplicated entity UID: {}", m_DuplicatedUID);
            Boom::DeleteEntityRecursive(*m_Registry, dup, nullptr);
            m_DuplicatedEntity = entt::null;
        }

        std::string GetDescription() const override {
            return "Duplicate Entity";
        }

        entt::entity GetDuplicatedEntity() const { return m_DuplicatedEntity; }
        Boom::AssetID GetDuplicatedUID() const { return m_DuplicatedUID; }

    private:
        entt::entity FindEntityByUID(Boom::AssetID uid) const {
            if (!m_Registry || uid == 0) return entt::null;

            auto view = m_Registry->view<Boom::InfoComponent>();
            for (auto e : view) {
                if (view.get<Boom::InfoComponent>(e).uid == uid) {
                    return e;
                }
            }
            return entt::null;
        }

        entt::registry* m_Registry;
        entt::entity m_SourceEntity;
        entt::entity m_DuplicatedEntity;
        Boom::AssetID m_SourceUID = 0;
        Boom::AssetID m_DuplicatedUID = 0;
    };

    // ==================== REPARENT COMMAND ====================

    class ReparentCommand : public ICommand {
    public:
        ReparentCommand(entt::registry* registry,
                       entt::entity entity,
                       entt::entity oldParent,
                       entt::entity newParent,
                       const Boom::Transform3D& oldLocalTransform)
            : m_Registry(registry)
            , m_Entity(entity)
            , m_OldParent(oldParent)
            , m_NewParent(newParent)
            , m_OldLocalTransform(oldLocalTransform)
        {
            // Store UIDs for serialization stability
            if (m_Registry && m_Registry->valid(entity) &&
                m_Registry->all_of<Boom::InfoComponent>(entity)) {
                m_EntityUID = m_Registry->get<Boom::InfoComponent>(entity).uid;
            }

            if (oldParent != entt::null && m_Registry->valid(oldParent) &&
                m_Registry->all_of<Boom::InfoComponent>(oldParent)) {
                m_OldParentUID = m_Registry->get<Boom::InfoComponent>(oldParent).uid;
            }

            if (newParent != entt::null && m_Registry->valid(newParent) &&
                m_Registry->all_of<Boom::InfoComponent>(newParent)) {
                m_NewParentUID = m_Registry->get<Boom::InfoComponent>(newParent).uid;
            }
        }

        void Execute() override {
            entt::entity entity = FindEntityByUID(m_EntityUID);
            entt::entity newParent = (m_NewParentUID != 0) ? FindEntityByUID(m_NewParentUID) : entt::null;

            if (entity == entt::null) return;

            // Capture new local transform after reparenting
            if (m_Registry->all_of<Boom::TransformComponent>(entity)) {
                m_NewLocalTransform = m_Registry->get<Boom::TransformComponent>(entity).transform;
            }

            Boom::SetParent(*m_Registry, entity, newParent, true);
        }

        void Undo() override {
            entt::entity entity = FindEntityByUID(m_EntityUID);
            entt::entity oldParent = (m_OldParentUID != 0) ? FindEntityByUID(m_OldParentUID) : entt::null;

            if (entity == entt::null) return;

            Boom::SetParent(*m_Registry, entity, oldParent, false);

            // Restore old local transform
            if (m_Registry->all_of<Boom::TransformComponent>(entity)) {
                m_Registry->get<Boom::TransformComponent>(entity).transform = m_OldLocalTransform;
            }
        }

        std::string GetDescription() const override {
            return "Reparent Entity";
        }

    private:
        entt::entity FindEntityByUID(Boom::AssetID uid) const {
            if (!m_Registry || uid == 0) return entt::null;

            auto view = m_Registry->view<Boom::InfoComponent>();
            for (auto e : view) {
                if (view.get<Boom::InfoComponent>(e).uid == uid) {
                    return e;
                }
            }

            return entt::null;
        }

        entt::registry* m_Registry;
        entt::entity m_Entity;
        entt::entity m_OldParent;
        entt::entity m_NewParent;
        Boom::AssetID m_EntityUID = 0;
        Boom::AssetID m_OldParentUID = 0;
        Boom::AssetID m_NewParentUID = 0;
        Boom::Transform3D m_OldLocalTransform;
        Boom::Transform3D m_NewLocalTransform;
    };

    // ==================== COMMAND HISTORY ====================

    class CommandHistory {
    public:
        CommandHistory(size_t maxHistory = 100)
            : m_MaxHistory(maxHistory)
            , m_CurrentIndex(-1)
        {
        }

        void ClearRedo()
        {
            const int size = static_cast<int>(m_Commands.size());
            const int firstRedoIndex = m_CurrentIndex + 1;

            BOOM_INFO("[CommandHistory] ClearRedo: idx = {}, size = {}, firstRedo = {}",
                m_CurrentIndex, size, firstRedoIndex);

            if (size == 0) return;

            if (firstRedoIndex >= 0 && firstRedoIndex < size) {
                m_Commands.erase(m_Commands.begin() + firstRedoIndex, m_Commands.end());
            }
        }



        void Execute(CommandPtr command) {
            if (!command) {
                BOOM_WARN("[CommandHistory] Execute called with null command");
                return;
            }

            // 1) Clear redo stack safely
            ClearRedo();

            // 2) Append new command to history and update index
            m_Commands.push_back(std::move(command));
            m_CurrentIndex = static_cast<int>(m_Commands.size()) - 1;

            // 3) Enforce history limit
            if (m_Commands.size() > m_MaxHistory) {
                m_Commands.erase(m_Commands.begin());
                m_CurrentIndex = static_cast<int>(m_Commands.size()) - 1; // stay at last
            }

            // 4) Execute the command from history
            auto& cmd = m_Commands[m_CurrentIndex];
            cmd->Execute();

            BOOM_INFO("[CommandHistory] Executed: {} (index: {})",
                cmd->GetDescription(), m_CurrentIndex);
        }


        bool CanUndo() const {
            return m_CurrentIndex >= 0 && !m_Commands.empty();
        }

        bool CanRedo() const {
            return m_CurrentIndex < static_cast<int>(m_Commands.size()) - 1;
        }

        void Undo() {
            if (!CanUndo()) {
                BOOM_WARN("[CommandHistory] Cannot undo: no commands in history (idx: {}, size: {})",
                    m_CurrentIndex, m_Commands.size());
                return;
            }

            BOOM_ASSERT(m_CurrentIndex >= 0);  // should always hold if CanUndo() is correct

            BOOM_INFO("[CommandHistory] Undoing: {}", m_Commands[m_CurrentIndex]->GetDescription());
            m_Commands[m_CurrentIndex]->Undo();

            m_CurrentIndex--;
            if (m_CurrentIndex < -1) {
                BOOM_WARN("[CommandHistory] m_CurrentIndex went below -1 ({}). Clamping to -1.", m_CurrentIndex);
                m_CurrentIndex = -1;
            }
        }


        void Redo() {
            if (!CanRedo()) {
                BOOM_WARN("[CommandHistory] Cannot redo: no commands to redo");
                return;
            }

            m_CurrentIndex++;
            BOOM_INFO("[CommandHistory] Redoing: {}", m_Commands[m_CurrentIndex]->GetDescription());
            m_Commands[m_CurrentIndex]->Execute();
        }

        void Clear() {
            m_Commands.clear();
            m_CurrentIndex = -1;
            BOOM_INFO("[CommandHistory] Cleared history");
        }

        size_t GetHistorySize() const { return m_Commands.size(); }
        int GetCurrentIndex() const { return m_CurrentIndex; }

        std::string GetCurrentCommandDescription() const {
            if (m_CurrentIndex >= 0 && m_CurrentIndex < static_cast<int>(m_Commands.size())) {
                return m_Commands[m_CurrentIndex]->GetDescription();
            }
            return "";
        }

    private:
        std::vector<CommandPtr> m_Commands;
        int m_CurrentIndex;
        size_t m_MaxHistory;
    };

} // namespace EditorUI
