#pragma once
#include "common/Core.h"
#include "Callback.h"
#include "Utilities.h"
#include "Auxiliaries/Assets.h"
#include <iostream>
#include "PxPhysicsAPI.h"
#include <foundation/PxMath.h>

namespace Boom {
    struct PhysicsContext {

        // Simple line primitive for debug rendering
        struct DebugLine {
            glm::vec3 p0;
            glm::vec3 p1;
            glm::vec4 c0;
            glm::vec4 c1;
        };

        struct PhysXRayResult {
            bool hitFound = false;
            glm::vec3 position = glm::vec3(0.0f);
            glm::vec3 normal = glm::vec3(0.0f);
            float distance = 0.0f;
            entt::entity hitEntity = entt::null;
        };

        // ====================================================================================
        // REGION: LIFECYCLE & CORE
        // ====================================================================================
        #pragma region Lifecycle
        BOOM_INLINE PhysicsContext()
            : m_Foundation(nullptr), m_Physics(nullptr), m_Dispatcher(nullptr), m_Scene(nullptr), m_DebugVisEnabled(false)
        {
            // Initialize PhysX SDK
            m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_AllocatorCallback, m_ErrorCallback);
            if (!m_Foundation) { BOOM_ERROR("Error initializing PhysX m_Foundation"); return; }

            // Create Physics Instance
            m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_Foundation, PxTolerancesScale());
            if (!m_Physics) { BOOM_ERROR("Error initializing PhysX m_Physics"); m_Foundation->release(); return; }

            // Create Worker Threads
            m_Dispatcher = PxDefaultCpuDispatcherCreate(2);

            // Create Scene Description
            PxSceneDesc sceneDesc(m_Physics->getTolerancesScale());
            sceneDesc.simulationEventCallback = &m_EventCallback;
            sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
            sceneDesc.filterShader = CustomFilterShader;
            sceneDesc.cpuDispatcher = m_Dispatcher;

            // Create Scene
            m_Scene = m_Physics->createScene(sceneDesc);
            if (!m_Scene) { BOOM_ERROR("Error creating PhysX m_Scene"); m_Physics->release(); m_Foundation->release(); return; }

            m_DebugVisEnabled = false;
        }

        BOOM_INLINE ~PhysicsContext() {
            if (m_Scene) { m_Scene->release(); }
            if (m_Physics) { m_Physics->release(); }
            if (m_Dispatcher) { m_Dispatcher->release(); }
            if (m_Foundation) { m_Foundation->release(); }
        }

        BOOM_INLINE void Simulate(uint32_t step, float dt)
        {
            for (uint32_t i = 0; i < step; ++i) {
                m_Scene->simulate(dt);
                m_Scene->fetchResults(true);
            }
        }

        BOOM_INLINE PxScene* GetPxScene() const { return m_Scene; }

        BOOM_INLINE void SetEventCallback(PxCallbackFunction&& callback) {
            m_EventCallback.m_Callback = callback;
        }
        #pragma endregion

        // ====================================================================================
        // REGION: ACTOR MANAGEMENT (ADD / REMOVE)
        // ====================================================================================
        #pragma region ActorManagement

        // Adds a RigidBody (Dynamic/Static/Kinematic) to the entity
        BOOM_INLINE void AddRigidBody(Entity& entity, AssetRegistry& assetRegistry) {
            auto& transform = entity.Get<TransformComponent>().transform;
            auto& body = entity.Get<RigidBodyComponent>().RigidBody;

            // 1. Cleanup existing actors to prevent leaks/duplicates
            RemoveRigidBody(entity);

            // 2. Prepare Transform
            PxTransform pose = ToPxTransform(transform);

            // 3. Create Collider Shape (if available)
            PxShape* shape = nullptr;
            if (entity.Has<ColliderComponent>()) {
                shape = CreatePxShape(entity.Get<ColliderComponent>().Collider, transform, assetRegistry);
            }

            // 4. Create Actor
            if (body.type == RigidBody3D::DYNAMIC || body.type == RigidBody3D::KINEMATIC) {
                // Must have a shape for dynamic simulation usually, but we handle nulls safely
                if (shape) {
                    body.actor = PxCreateDynamic(*m_Physics, pose, *shape, body.density);
                    PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidBody*>(body.actor), body.density);
                }
                else {
                    body.actor = m_Physics->createRigidDynamic(pose);
                }

                PxRigidDynamic* dyn = static_cast<PxRigidDynamic*>(body.actor);
                if (dyn) {
                    dyn->setLinearVelocity(ToPxVec3(body.initialVelocity));
                    dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, body.type == RigidBody3D::KINEMATIC);
                    dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, body.freezeRotationX);
                    dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, body.freezeRotationY);
                    dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, body.freezeRotationZ);
                    dyn->setActorFlag(PxActorFlag::eSEND_SLEEP_NOTIFIES, true);
                }
            }
            else { // STATIC
                if (shape) body.actor = PxCreateStatic(*m_Physics, pose, *shape);
                else       body.actor = m_Physics->createRigidStatic(pose);
            }

            if (!body.actor) { BOOM_ERROR("Error creating actor"); if (shape) shape->release(); return; }

            // 5. Final Setup
            body.actor->setActorFlag(PxActorFlag::eVISUALIZATION, true);
            body.actor->userData = new EntityID(entity.ID());
            m_Scene->addActor(*body.actor);

            // Shape ref count is incremented by attach, we can release our local pointer if we don't need it, 
            // but PhysX createShape returns a ref count of 1. PxCreateDynamic attaches it (inc ref). 
            // The helper 'CreatePxShape' returns an unattached shape.
            if (shape) shape->release();
        }

        // Adds a Static Actor for an entity that ONLY has a collider (Triggers, Environment)
        BOOM_INLINE void AddColliderOnly(Entity& entity, AssetRegistry& assetRegistry) {
            if (entity.Has<RigidBodyComponent>()) return; // RB handles it
            if (!entity.Has<ColliderComponent>()) { BOOM_WARN("AddColliderOnly called without Collider"); return; }

            auto& transform = entity.Get<TransformComponent>().transform;
            auto& collider = entity.Get<ColliderComponent>().Collider;

            // 1. Cleanup
            RemoveColliderActor(entity);

            // 2. Create Shape
            PxShape* shape = CreatePxShape(collider, transform, assetRegistry);
            if (!shape) { BOOM_ERROR("Failed to create shape for ColliderOnly"); return; }

            // 3. Create Static Actor
            PxTransform pose = ToPxTransform(transform);
            PxRigidStatic* staticActor = m_Physics->createRigidStatic(pose);

            staticActor->attachShape(*shape);
            staticActor->setActorFlag(PxActorFlag::eVISUALIZATION, true);
            staticActor->userData = new EntityID(entity.ID());

            m_Scene->addActor(*staticActor);

            // Store reference
            collider.actor = staticActor;

            // Release local shape ref (actor holds one now)
            shape->release();
            collider.Shape = shape; // Keep a raw pointer if needed for updates, but rely on actor

            BOOM_INFO("Created collider-only entity");
        }

        BOOM_INLINE void RemoveRigidBody(Entity& entity) {
            PxRigidActor* actor = nullptr;

            if (entity.Has<RigidBodyComponent>()) {
                auto& rb = entity.Get<RigidBodyComponent>();
                actor = rb.RigidBody.actor;
                rb.RigidBody.actor = nullptr;
            }
            else if (entity.Has<ColliderComponent>()) {
                // Check if we stored it in the collider component
                auto& col = entity.Get<ColliderComponent>().Collider;
                actor = col.actor;
                col.actor = nullptr;
                // Fallback: If not stored, check shape
                if (!actor && col.Shape) actor = col.Shape->getActor();
            }

            if (!actor) return;

            // Clean User Data
            if (actor->userData) {
                delete static_cast<EntityID*>(actor->userData);
                actor->userData = nullptr;
            }

            m_Scene->removeActor(*actor);
            actor->release();
        }

        // Helper for entities that strictly only have ColliderComponent
        BOOM_INLINE void RemoveColliderActor(Entity& entity) {
            if (!entity.Has<ColliderComponent>()) return;
            // Re-use generic logic
            RemoveRigidBody(entity);
        }

        BOOM_INLINE void ForceRemoveActor(uint32_t entityID) {
            if (!m_Scene) return;
            PxU32 nbActors = m_Scene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC);
            std::vector<PxActor*> actors(nbActors);
            m_Scene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC, actors.data(), nbActors);

            for (PxActor* actor : actors) {
                if (actor->userData && *static_cast<EntityID*>(actor->userData) == static_cast<EntityID>(entityID)) {
                    m_Scene->removeActor(*actor);
                    delete static_cast<EntityID*>(actor->userData);
                    actor->release();
                    BOOM_INFO("[Physics] Force removed actor for Entity {}", entityID);
                }
            }
        }
        #pragma endregion

        // ====================================================================================
        // REGION: RUNTIME UPDATES
        // ====================================================================================
        #pragma region RuntimeUpdates

        BOOM_INLINE void UpdateColliderShape(Entity& entity, AssetRegistry& assetRegistry) {
            if (!entity.Has<RigidBodyComponent>() || !entity.Has<ColliderComponent>()) return;

            auto& body = entity.Get<RigidBodyComponent>().RigidBody;
            auto& collider = entity.Get<ColliderComponent>().Collider;
            auto& transform = entity.Get<TransformComponent>().transform;

            if (!body.actor) return;

            // 1. Detach and Release Old Shape
            if (collider.Shape) {
                body.actor->detachShape(*collider.Shape);
                collider.Shape->release(); // Release should decrease ref count to 0 if detached
                collider.Shape = nullptr;
            }

            // 2. Create New Shape
            collider.Shape = CreatePxShape(collider, transform, assetRegistry);

            // 3. Attach New Shape
            if (collider.Shape) {
                body.actor->attachShape(*collider.Shape);
                collider.Shape->release(); // Actor owns it now

                // Recalculate mass for dynamic objects
                if (body.type == RigidBody3D::DYNAMIC) {
                    PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidBody*>(body.actor), body.density);
                }
            }
        }

        BOOM_INLINE void UpdateRigidBodyTransform(Entity entity, const Transform3D& transform) {
            if (!entity.Has<Boom::RigidBodyComponent>()) return;
            auto& rc = entity.Get<Boom::RigidBodyComponent>();
            if (!rc.RigidBody.actor) return;

            PxTransform pose = ToPxTransform(transform);
            rc.RigidBody.actor->setGlobalPose(pose);

            if (rc.RigidBody.type == RigidBody3D::Type::DYNAMIC) {
                static_cast<PxRigidDynamic*>(rc.RigidBody.actor)->wakeUp();
            }
        }

        BOOM_INLINE void SetRigidBodyType(Entity& entity, RigidBody3D::Type newType) {
            if (!entity.Has<RigidBodyComponent>()) return;
            auto& body = entity.Get<RigidBodyComponent>().RigidBody;
            if (!body.actor || body.type == newType) return;

            // Cache properties
            PxTransform pose = body.actor->getGlobalPose();
            EntityID* userData = static_cast<EntityID*>(body.actor->userData);

            // Get Shapes
            const PxU32 numShapes = body.actor->getNbShapes();
            std::vector<PxShape*> shapes(numShapes);
            body.actor->getShapes(shapes.data(), numShapes);

            // Remove old actor
            m_Scene->removeActor(*body.actor);
            body.actor->release();

            // Create new actor
            PxRigidActor* newActor = nullptr;
            if (newType == RigidBody3D::STATIC) {
                newActor = m_Physics->createRigidStatic(pose);
            }
            else {
                PxRigidDynamic* dyn = m_Physics->createRigidDynamic(pose);
                PxRigidBodyExt::updateMassAndInertia(*dyn, body.density);
                dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, newType == RigidBody3D::KINEMATIC);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, body.freezeRotationX);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, body.freezeRotationY);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, body.freezeRotationZ);
                newActor = dyn;
            }

            // Re-attach shapes & Add to scene
            if (newActor) {
                for (PxShape* shape : shapes) newActor->attachShape(*shape);
                newActor->userData = userData;
                m_Scene->addActor(*newActor);
            }

            body.actor = newActor;
            body.type = newType;
        }

        BOOM_INLINE void SetColliderType(Entity& entity, Collider3D::Type newType, AssetRegistry& assetRegistry) {
            if (!entity.Has<ColliderComponent>()) return;
            auto& collider = entity.Get<ColliderComponent>().Collider;
            if (collider.type == newType) return;

            collider.type = newType;
            UpdateColliderShape(entity, assetRegistry);
        }

        BOOM_INLINE void UpdatePhysicsMaterial(Entity& ent) {
            if (!ent.Has<ColliderComponent>()) return;
            auto& collider = ent.Get<ColliderComponent>().Collider;
            if (!collider.Shape) return;

            PxMaterial* material;
            if (collider.Shape->getMaterials(&material, 1) > 0) {
                material->setDynamicFriction(collider.dynamicFriction);
                material->setStaticFriction(collider.staticFriction);
                material->setRestitution(collider.restitution);
            }
        }

        BOOM_INLINE void SetRotationLock(Boom::Entity entity, bool lockX, bool lockY, bool lockZ) {
            if (!entity.Has<Boom::RigidBodyComponent>()) return;
            auto& rc = entity.Get<Boom::RigidBodyComponent>();
            if (auto* dyn = rc.RigidBody.actor->is<physx::PxRigidDynamic>()) {
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, lockX);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, lockY);
                dyn->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, lockZ);
            }
        }
        #pragma endregion

        // ====================================================================================
        // REGION: QUERIES (RAYCAST)
        // ====================================================================================
        #pragma region Queries

        BOOM_INLINE PhysXRayResult Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDist) {
            PhysXRayResult result;
            if (!m_Scene) return result;

            PxRaycastBuffer hit;
            if (m_Scene->raycast(ToPxVec3(origin), ToPxVec3(glm::normalize(direction)), maxDist, hit)) {
                result.hitFound = true;
                result.position = ToGLMVec3(hit.block.position);
                result.normal = ToGLMVec3(hit.block.normal);
                result.distance = hit.block.distance;
                if (hit.block.actor && hit.block.actor->userData) {
                    result.hitEntity = (entt::entity)(*static_cast<EntityID*>(hit.block.actor->userData));
                }
            }
            return result;
        }

        BOOM_INLINE glm::vec3 ResolveThirdPersonCameraPosition(glm::vec3 const& playerEye, glm::vec3 const& idealCamPosition, float minDist = 0.5f) {
            PxVec3 targetPos = ToPxVec3(playerEye);
            PxVec3 idealCamPos = ToPxVec3(idealCamPosition);
            PxVec3 dir = (idealCamPos - targetPos).getNormalized();
            PxReal maxDist = (idealCamPos - targetPos).magnitude();

            PxRaycastBuffer hit;
            if (m_Scene->raycast(targetPos, dir, maxDist, hit))
                return ToGLMVec3(targetPos + dir * PxMax(hit.block.distance - 0.05f, minDist));
            return ToGLMVec3(idealCamPos);
        }

        #pragma endregion

        // ====================================================================================
        // REGION: MESH COOKING & IO
        // ====================================================================================
        #pragma region Cooking

        BOOM_INLINE bool CompileAndSavePhysicsMesh(ModelAsset& modelAsset, const std::string& savePath) {
            return GenericCookMesh(modelAsset, savePath, false);
        }

        BOOM_INLINE bool CompileAndSaveTriangleMesh(ModelAsset& modelAsset, const std::string& savePath) {
            return GenericCookMesh(modelAsset, savePath, true);
        }

        // Generic Mesh Loader (Convex)
        BOOM_INLINE physx::PxConvexMesh* LoadCookedMesh(const std::string& path) {
            auto buffer = ReadFileToBuffer(path);
            if (!buffer.data) return nullptr;
            PxDefaultMemoryInputData input(reinterpret_cast<PxU8*>(buffer.data), static_cast<PxU32>(buffer.size));
            PxConvexMesh* mesh = m_Physics->createConvexMesh(input);
            delete[] buffer.data;
            return mesh;
        }

        // Generic Mesh Loader (Triangle)
        BOOM_INLINE physx::PxTriangleMesh* LoadCookedTriangleMesh(const std::string& path) {
            auto buffer = ReadFileToBuffer(path);
            if (!buffer.data) return nullptr;
            PxDefaultMemoryInputData input(reinterpret_cast<PxU8*>(buffer.data), static_cast<PxU32>(buffer.size));
            PxTriangleMesh* mesh = m_Physics->createTriangleMesh(input);
            delete[] buffer.data;
            return mesh;
        }

        BOOM_INLINE PxConvexMeshGeometry CookMesh(const MeshData<ShadedVert>& data) {
            std::vector<PxVec3> vertices;
            for (auto& vertex : data.vtx) vertices.push_back(ToPxVec3(vertex.pos));

            PxConvexMeshDesc meshDesc;
            meshDesc.points.data = vertices.data();
            meshDesc.points.stride = sizeof(PxVec3);
            meshDesc.points.count = static_cast<PxU32>(vertices.size());
            meshDesc.indices.data = data.idx.data();
            meshDesc.indices.count = static_cast<PxU32>(data.idx.size());
            meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

            return CookConvexGeometryFromDesc(meshDesc);
        }
        #pragma endregion

        // ====================================================================================
        // REGION: DEBUG VISUALIZATION
        // ====================================================================================
        #pragma region DebugViz

        BOOM_INLINE void EnableDebugVisualization(bool enable, float scale = 1.0f) {
            m_DebugVisEnabled = enable;
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eSCALE, enable ? scale : 0.0f);
            if (!enable) return;
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eACTOR_AXES, 1.0f);
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_POINT, 1.0f);
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_NORMAL, 1.0f);
        }

        BOOM_INLINE void CollectDebugLines(std::vector<DebugLine>& outLines) const {
            outLines.clear();
            if (!m_DebugVisEnabled || !m_Scene) return;
            const PxRenderBuffer& rb = m_Scene->getRenderBuffer();

            // Lines
            for (PxU32 i = 0; i < rb.getNbLines(); ++i) {
                const auto& l = rb.getLines()[i];
                outLines.push_back({ {l.pos0.x, l.pos0.y, l.pos0.z}, {l.pos1.x, l.pos1.y, l.pos1.z}, UnpackPxColor(l.color0), UnpackPxColor(l.color1) });
            }
            // Triangles
            for (PxU32 i = 0; i < rb.getNbTriangles(); ++i) {
                const auto& t = rb.getTriangles()[i];
                glm::vec3 a{ t.pos0.x, t.pos0.y, t.pos0.z }, b{ t.pos1.x, t.pos1.y, t.pos1.z }, c{ t.pos2.x, t.pos2.y, t.pos2.z };
                glm::vec4 ca = UnpackPxColor(t.color0), cb = UnpackPxColor(t.color1), cc = UnpackPxColor(t.color2);
                outLines.push_back({ a, b, ca, cb }); outLines.push_back({ b, c, cb, cc }); outLines.push_back({ c, a, cc, ca });
            }
        }
        #pragma endregion


    private:
        // ====================================================================================
        // REGION: PRIVATE HELPERS
        // ====================================================================================

        // Centralized Shape Creation to avoid code duplication in AddRigidBody/AddColliderOnly
        BOOM_INLINE PxShape* CreatePxShape(Collider3D& collider, Transform3D& transform, AssetRegistry& assetRegistry)
        {
            if (!m_Physics) return nullptr;

            // 1. Create Material if needed
            if (!collider.material) {
                collider.material = m_Physics->createMaterial(collider.staticFriction, collider.dynamicFriction, collider.restitution);
            }

            PxTransform userLocalPose(ToPxVec3(collider.localPosition), ToPxQuat(collider.localRotation));
            PxShape* shape = nullptr;
            glm::vec3 s = glm::abs(transform.scale * collider.localScale);

            // 2. Geometry Switch
            if (collider.type == Collider3D::BOX) {
                glm::vec3 halfExtents = s / 2.0f;
                halfExtents = glm::max(halfExtents, glm::vec3(0.01f)); // Safe min size
                shape = m_Physics->createShape(PxBoxGeometry(ToPxVec3(halfExtents)), *collider.material);
                shape->setLocalPose(userLocalPose);
            }
            else if (collider.type == Collider3D::SPHERE) {
                float radius = std::max(s.x * 0.5f, 0.01f);
                shape = m_Physics->createShape(PxSphereGeometry(radius), *collider.material);
                shape->setLocalPose(userLocalPose);
            }
            else if (collider.type == Collider3D::CAPSULE) {
                // Determine Axis
                int axis = 0; // 0=X, 1=Y, 2=Z
                if (s.y > s.x && s.y > s.z) axis = 1;
                else if (s.z > s.x && s.z > s.y) axis = 2;

                float radius, halfHeight;
                if (axis == 1) { radius = 0.5f * std::max(s.x, s.z); halfHeight = 0.5f * s.y; }
                else if (axis == 2) { radius = 0.5f * std::max(s.x, s.y); halfHeight = 0.5f * s.z; }
                else { radius = 0.5f * std::max(s.y, s.z); halfHeight = 0.5f * s.x; }

                halfHeight = std::max(halfHeight - radius, 0.01f);
                radius = std::max(radius, 0.01f);

                shape = m_Physics->createShape(PxCapsuleGeometry(radius, halfHeight), *collider.material);

                // Align rotation
                PxQuat rot = PxQuat(PxIdentity);
                if (axis == 1) rot = PxQuat(PxHalfPi, PxVec3(0, 0, 1));
                else if (axis == 2) rot = PxQuat(-PxHalfPi, PxVec3(0, 1, 0));

                shape->setLocalPose(userLocalPose * PxTransform(PxVec3(0), rot));
            }
            else if (collider.type == Collider3D::PLANE) {
                shape = m_Physics->createShape(PxPlaneGeometry(), *collider.material);
                PxQuat rot = PxQuat(PxIdentity);
                if (s.y < s.x && s.y < s.z) rot = PxQuat(PxHalfPi, PxVec3(0, 0, 1)); // Floor
                else if (s.z < s.x && s.z < s.y) rot = PxQuat(-PxHalfPi, PxVec3(0, 1, 0)); // Wall
                shape->setLocalPose(userLocalPose * PxTransform(PxVec3(0), rot));
            }
            else if (collider.type == Collider3D::CONVEX_MESH) {
                if (collider.physicsMeshID != EMPTY_ASSET) {
                    auto& asset = assetRegistry.Get<PhysicsMeshAsset>(collider.physicsMeshID);
                    if (!asset.mesh) asset.mesh = LoadCookedMesh(asset.cookedMeshPath);
                    if (asset.mesh) {
                        shape = m_Physics->createShape(PxConvexMeshGeometry(asset.mesh, PxMeshScale(ToPxVec3(s))), *collider.material);
                        shape->setLocalPose(userLocalPose);
                    }
                }
            }
            else if (collider.type == Collider3D::TRIANGLE_MESH) {
                if (collider.physicsMeshID != EMPTY_ASSET) {
                    auto& asset = assetRegistry.Get<PhysicsMeshAsset>(collider.physicsMeshID);
                    if (!asset.triangleMesh) asset.triangleMesh = LoadCookedTriangleMesh(asset.cookedMeshPath);
                    if (asset.triangleMesh) {
                        shape = m_Physics->createShape(PxTriangleMeshGeometry(asset.triangleMesh, PxMeshScale(ToPxVec3(s))), *collider.material);
                        shape->setLocalPose(userLocalPose);
                    }
                }
            }
            // Simple Primitive Meshes (Cylinder/Tri)
            else if (collider.type == Collider3D::CYLINDER) {
                PxConvexMesh* mesh = CreateCylinderMesh(1.0f, 1.0f);
                if (mesh) {
                    float r = 0.5f * std::max(s.x, s.z);
                    shape = m_Physics->createShape(PxConvexMeshGeometry(mesh, PxMeshScale(PxVec3(r, 0.5f * s.y, r))), *collider.material);
                    shape->setLocalPose(userLocalPose);
                }
            }

            // 3. Flags
            if (shape) {
                shape->setFlag(PxShapeFlag::eVISUALIZATION, true);
                if (collider.isTrigger) {
                    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                }
                else {
                    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
                    shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
                }
            }

            return shape;
        }

        // Helpers
        BOOM_INLINE static glm::vec4 UnpackPxColor(PxU32 c) {
            return { ((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, ((c >> 0) & 0xFF) / 255.0f, ((c >> 24) & 0xFF) / 255.0f };
        }

        BOOM_INLINE PxTransform ToPxTransform(const Transform3D& t) {
            PxTransform p;
            p.p = ToPxVec3(t.translate);
            glm::quat q = glm::normalize(glm::quat(glm::radians(t.rotate)));
            p.q = PxQuat(q.x, q.y, q.z, q.w);
            return p;
        }

        struct FileBuffer { char* data; std::streamsize size; };
        BOOM_INLINE FileBuffer ReadFileToBuffer(const std::string& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file.is_open()) return { nullptr, 0 };
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            char* buf = new char[size];
            file.read(buf, size);
            return { buf, size };
        }

        BOOM_INLINE PxConvexMeshGeometry CookConvexGeometryFromDesc(PxConvexMeshDesc& desc) {
            PxCookingParams params(m_Physics->getTolerancesScale());
            PxCooking* cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_Foundation, params);
            PxDefaultMemoryOutputStream buf;
            PxConvexMeshCookingResult::Enum res;
            PxConvexMesh* mesh = cooking->createConvexMesh(desc, m_Physics->getPhysicsInsertionCallback(), &res);
            cooking->release();
            return PxConvexMeshGeometry(mesh);
        }

        BOOM_INLINE PxConvexMesh* CreateCylinderMesh(float radius, float halfHeight, int segments = 16) {
            // (Previous implementation of cylinder generation logic)
            std::vector<PxVec3> vertices;
            for (int i = 0; i < segments; ++i) {
                float a = (float)i / segments * PxTwoPi;
                vertices.push_back(PxVec3(radius * cosf(a), halfHeight, radius * sinf(a)));
                vertices.push_back(PxVec3(radius * cosf(a), -halfHeight, radius * sinf(a)));
            }
            vertices.push_back(PxVec3(0, halfHeight, 0)); vertices.push_back(PxVec3(0, -halfHeight, 0));
            PxConvexMeshDesc md; md.points.data = vertices.data(); md.points.stride = sizeof(PxVec3); md.points.count = (PxU32)vertices.size(); md.flags = PxConvexFlag::eCOMPUTE_CONVEX;
            return CookConvexGeometryFromDesc(md).convexMesh;
        }

        BOOM_INLINE bool GenericCookMesh(ModelAsset& modelAsset, const std::string& savePath, bool isTriangle) {
            auto staticModel = std::dynamic_pointer_cast<StaticModel>(modelAsset.data);
            if (!staticModel || staticModel->GetMeshData().empty()) return false;
            auto& meshData = staticModel->GetMeshData()[0];

            std::vector<PxVec3> vertices;
            for (const auto& v : meshData.vtx) vertices.push_back(ToPxVec3(v.pos));

            PxCookingParams params(m_Physics->getTolerancesScale());
            PxCooking* cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_Foundation, params);
            PxDefaultMemoryOutputStream buf;
            bool status = false;

            if (isTriangle) {
                PxTriangleMeshDesc md;
                md.points.data = vertices.data(); md.points.stride = sizeof(PxVec3); md.points.count = (PxU32)vertices.size();
                md.triangles.data = meshData.idx.data(); md.triangles.stride = 3 * sizeof(uint32_t); md.triangles.count = (PxU32)meshData.idx.size() / 3;
                status = cooking->cookTriangleMesh(md, buf);
            }
            else {
                PxConvexMeshDesc md;
                md.points.data = vertices.data(); md.points.stride = sizeof(PxVec3); md.points.count = (PxU32)vertices.size();
                md.flags = PxConvexFlag::eCOMPUTE_CONVEX;
                status = cooking->cookConvexMesh(md, buf);
            }
            cooking->release();

            if (status) {
                std::ofstream outFile(savePath, std::ios::binary);
                outFile.write(reinterpret_cast<const char*>(buf.getData()), buf.getSize());
                return true;
            }
            return false;
        }

        static PxFilterFlags CustomFilterShader(
            PxFilterObjectAttributes attr0, [[maybe_unused]] PxFilterData fd0,
            PxFilterObjectAttributes attr1, [[maybe_unused]] PxFilterData fd1,
            PxPairFlags& pairFlags, [[maybe_unused]] const void* constantBlock,
            [[maybe_unused]] PxU32 constantBlockSize)
        {
            (void)fd0;
            (void)fd1;
            (void)constantBlock;
            (void)constantBlockSize;

            if (PxFilterObjectIsTrigger(attr0) || PxFilterObjectIsTrigger(attr1)) {
                pairFlags = PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_LOST | PxPairFlag::eDETECT_DISCRETE_CONTACT;
            }
            else {
                pairFlags = PxPairFlag::eCONTACT_DEFAULT | PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_PERSISTS | PxPairFlag::eNOTIFY_TOUCH_LOST;
            }
            return PxFilterFlag::eDEFAULT;
        }

        PxDefaultErrorCallback m_ErrorCallback;
        PxDefaultAllocator m_AllocatorCallback;
        PxDefaultCpuDispatcher* m_Dispatcher;
        PxEventCallback m_EventCallback;
        PxFoundation* m_Foundation;
        PxPhysics* m_Physics;
        PxScene* m_Scene;
        bool m_DebugVisEnabled;
    };
}