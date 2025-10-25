#pragma once
#include "Callback.h"
#include "Utilities.h"
#include "Auxiliaries/Assets.h"
#include <iostream>
#include "PxPhysicsAPI.h"

namespace Boom {
    struct PhysicsContext {

        // Simple line primitive for debug rendering
        struct DebugLine {
            glm::vec3 p0;
            glm::vec3 p1;
            glm::vec4 c0;
            glm::vec4 c1;
        };



        BOOM_INLINE PhysicsContext() {
            // sinitialize physX SDK
            m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_AllocatorCallback, m_ErrorCallback);
            if (!m_Foundation)
            {
                BOOM_ERROR("Error initializing PhysX m_Foundation");
                return;
            }
            // create context instance
            m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION,
                *m_Foundation, PxTolerancesScale());
            if (!m_Physics)
            {
                BOOM_ERROR("Error initializing PhysX m_Physics");
                m_Foundation->release();
                return;
            }

            // create worker threads
            m_Dispatcher = PxDefaultCpuDispatcherCreate(2);

            // create a scene desciption
            PxSceneDesc sceneDesc(m_Physics->getTolerancesScale());
            sceneDesc.simulationEventCallback = &m_EventCallback;
            sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
            sceneDesc.filterShader = CustomFilterShader;
            sceneDesc.cpuDispatcher = m_Dispatcher;

            // create scene instance
            m_Scene = m_Physics->createScene(sceneDesc);


            if (!m_Scene)
            {
                BOOM_ERROR("Error creating PhysX m_Scene");
                m_Physics->release();
                m_Foundation->release();
                return;
            }

            // Debug visualization disabled by default
            m_DebugVisEnabled = false;
        }

        BOOM_INLINE ~PhysicsContext() {
            if (m_Scene) { m_Scene->release(); }
            if (m_Physics) { m_Physics->release(); }
            if (m_Dispatcher) { m_Dispatcher->release(); }
            if (m_Foundation) { m_Foundation->release(); }
        }

        // Enable/disable PhysX scene debug visualization and which primitives to emit
        BOOM_INLINE void EnableDebugVisualization(bool enable, float scale = 1.0f)
        {
            m_DebugVisEnabled = enable;

            // Global scale. Set to 0.0f to turn off all visualization.
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eSCALE, enable ? scale : 0.0f);

            if (!enable) return;

            // Keep shapes only; turn off extra clutter that can block view.
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eACTOR_AXES, 1.0f);
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_POINT, 1.0f);
            m_Scene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_NORMAL, 1.0f);
            // Common extras, enable as needed:
            // m_Scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_AABBS, 1.0f);
            // m_Scene->setVisualizationParameter(PxVisualizationParameter::eBODY_MASS_AXES, 1.0f);
            // m_Scene->setVisualizationParameter(PxVisualizationParameter::eJOINT_LOCAL_FRAMES, 1.0f);
            // m_Scene->setVisualizationParameter(PxVisualizationParameter::eJOINT_LIMITS, 1.0f);
        }

        // Convert PhysX' color (ARGB packed) to glm::vec4 RGBA
        BOOM_INLINE static glm::vec4 UnpackPxColor(PxU32 c)
        {
            float a = ((c >> 24) & 0xFF) / 255.0f;
            float r = ((c >> 16) & 0xFF) / 255.0f;
            float g = ((c >> 8) & 0xFF) / 255.0f;
            float b = ((c >> 0) & 0xFF) / 255.0f;
            return { r, g, b, a };
        }

        // Gather current PhysX debug buffer as line segments to feed your renderer
        BOOM_INLINE void CollectDebugLines(std::vector<DebugLine>& outLines) const
        {
            outLines.clear();
            if (!m_DebugVisEnabled || !m_Scene) return;

            const PxRenderBuffer& rb = m_Scene->getRenderBuffer();

            // Lines
            const PxU32 nLines = rb.getNbLines();
            const PxDebugLine* lines = rb.getLines();
            for (PxU32 i = 0; i < nLines; ++i) {
                DebugLine dl;
                dl.p0 = { lines[i].pos0.x, lines[i].pos0.y, lines[i].pos0.z };
                dl.p1 = { lines[i].pos1.x, lines[i].pos1.y, lines[i].pos1.z };
                dl.c0 = UnpackPxColor(lines[i].color0);
                dl.c1 = UnpackPxColor(lines[i].color1);
                outLines.push_back(dl);
            }

            // Triangles (emit 3 edges)
            const PxU32 nTris = rb.getNbTriangles();
            const PxDebugTriangle* tris = rb.getTriangles();
            for (PxU32 i = 0; i < nTris; ++i) {
                glm::vec3 a{ tris[i].pos0.x, tris[i].pos0.y, tris[i].pos0.z };
                glm::vec3 b{ tris[i].pos1.x, tris[i].pos1.y, tris[i].pos1.z };
                glm::vec3 c{ tris[i].pos2.x, tris[i].pos2.y, tris[i].pos2.z };
                glm::vec4 ca = UnpackPxColor(tris[i].color0);
                glm::vec4 cb = UnpackPxColor(tris[i].color1);
                glm::vec4 cc = UnpackPxColor(tris[i].color2);

                outLines.push_back(DebugLine{ a, b, ca, cb });
                outLines.push_back(DebugLine{ b, c, cb, cc });
                outLines.push_back(DebugLine{ c, a, cc, ca });
            }

            // Points (draw as tiny axis-cross lines)
            const PxU32 nPts = rb.getNbPoints();
            const PxDebugPoint* pts = rb.getPoints();
            const float s = 0.02f; // point cross half-size
            for (PxU32 i = 0; i < nPts; ++i) {
                glm::vec3 p{ pts[i].pos.x, pts[i].pos.y, pts[i].pos.z };
                glm::vec4 c = UnpackPxColor(pts[i].color);

                outLines.push_back(DebugLine{ p + glm::vec3(-s, 0, 0), p + glm::vec3(+s, 0, 0), c, c });
                outLines.push_back(DebugLine{ p + glm::vec3(0, -s, 0), p + glm::vec3(0, +s, 0), c, c });
                outLines.push_back(DebugLine{ p + glm::vec3(0, 0, -s), p + glm::vec3(0, 0, +s), c, c });
            }
        }

        BOOM_INLINE void UpdateColliderShape(Entity& entity)
        {
            if (!entity.Has<RigidBodyComponent>() || !entity.Has<ColliderComponent>()) {
                return;
            }

            auto& transform = entity.Get<TransformComponent>().transform;
            auto& body = entity.Get<RigidBodyComponent>().RigidBody;
            auto& collider = entity.Get<ColliderComponent>().Collider;

            if (!body.actor) return;

            // 1. --- Destroy the old shape ---
            if (collider.Shape) {
                body.actor->detachShape(*collider.Shape);
                collider.Shape->release();
                collider.Shape = nullptr;
            }

            // 2. --- Re-create the shape with the new scale (logic moved from AddRigidBody) ---
            if (collider.type == Collider3D::BOX) {
                PxBoxGeometry box(ToPxVec3(transform.scale / 2.0f));
                collider.Shape = m_Physics->createShape(box, *collider.material);
            }
            else if (collider.type == Collider3D::SPHERE) {
                PxSphereGeometry sphere(transform.scale.x / 2.0f);
                collider.Shape = m_Physics->createShape(sphere, *collider.material);
            }
            else if (collider.type == Collider3D::CAPSULE) {
                // (Your existing, correct capsule creation logic goes here)
                const glm::vec3 s = glm::abs(transform.scale);
                enum Axis { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2 };
                Axis majorAxis = AXIS_X;
                if (s.y > s.x && s.y > s.z) majorAxis = AXIS_Y;
                else if (s.z > s.x && s.z > s.y) majorAxis = AXIS_Z;
                float radius, halfHeight;
                if (majorAxis == AXIS_Y) { radius = 0.5f * std::max(s.x, s.z); halfHeight = 0.5f * s.y; }
                else if (majorAxis == AXIS_Z) { radius = 0.5f * std::max(s.x, s.y); halfHeight = 0.5f * s.z; }
                else { radius = 0.5f * std::max(s.y, s.z); halfHeight = 0.5f * s.x; }
                halfHeight = halfHeight - radius;
                const float kMin = 0.01f;
                if (radius <= 0.0f) radius = kMin;
                if (halfHeight <= 0.0f) halfHeight = kMin;
                PxCapsuleGeometry capsule(radius, halfHeight);
                collider.Shape = m_Physics->createShape(capsule, *collider.material);
                PxQuat localQ = PxQuat(PxIdentity);
                if (majorAxis == AXIS_Y) localQ = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
                else if (majorAxis == AXIS_Z) localQ = PxQuat(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));
                collider.Shape->setLocalPose(PxTransform(PxVec3(0.0f), localQ));
            }
            // (Add your MESH logic here if it needs to be rescaled too)

            // 3. --- Attach the new shape and update physics properties ---
            if (collider.Shape) {
                collider.Shape->setFlag(PxShapeFlag::eVISUALIZATION, true);
                body.actor->attachShape(*collider.Shape);
                if (body.type == RigidBody3D::DYNAMIC) {
                    // This is CRITICAL for stability after a shape change!
                    PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidBody*>(body.actor), body.density);
                }
            }
        }

        // Rigid Body
        BOOM_INLINE void AddRigidBody(Entity& entity, AssetRegistry& assetRegistry) {
            auto& transform = entity.Get<TransformComponent>().transform;
            auto& body = entity.Get<RigidBodyComponent>().RigidBody;
            //bool hasCollider = entity.Has<ColliderComponent>();



            // create rigidbody transformation
            PxTransform pose(ToPxVec3(transform.translate));

            glm::vec3 eulerRadians = glm::radians(transform.rotate);
            glm::quat rot = glm::quat(eulerRadians);

            // Normalize the quaternion to ensure it's valid
            rot = glm::normalize(rot);

            pose.q = PxQuat(rot.x, rot.y, rot.z, rot.w);

            body.previousScale = transform.scale;

            // create a rigid body actor
            if (entity.template Has<ColliderComponent>())
            {
                // create collider shape

                auto& collider = entity.Get<ColliderComponent>().Collider;

                // create collider material
                collider.material = m_Physics->createMaterial(collider.staticFriction,
                    collider.dynamicFriction,
                    collider.restitution);

                if (collider.type == Collider3D::BOX)
                {
                    PxBoxGeometry
                        box(ToPxVec3(transform.scale / 2.0f));
                    collider.Shape = m_Physics->createShape(box, *collider.material);
                }
                else if (collider.type == Collider3D::SPHERE) {
                    PxSphereGeometry
                        sphere(transform.scale.x / 2.0f);
                    collider.Shape = m_Physics->createShape(sphere, *collider.material);
                    PxTransform relativePose(PxQuat(0, PxVec3(0, 0, 1)));
                    collider.Shape->setLocalPose(relativePose);
                }
                else if (collider.type == Collider3D::CAPSULE) {
                    if (!m_Physics || !collider.material) {
                        return;
                    }

                    // Decide which axis the capsule should align to based on the largest scale component.
                    const glm::vec3 s = glm::abs(transform.scale);
                    enum Axis { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2 };
                    Axis majorAxis = AXIS_X;
                    if (s.y > s.x && s.y > s.z) {
                        majorAxis = AXIS_Y;
                    }
                    else if (s.z > s.x && s.z > s.y) {
                        majorAxis = AXIS_Z;
                    }

                    float radius, halfHeight;

                    // Correctly calculate radius and half-height based on the major axis.
                    if (majorAxis == AXIS_Y) { // Y is the longest axis
                        radius = 0.5f * std::max(s.x, s.z);
                        halfHeight = 0.5f * s.y;
                    }
                    else if (majorAxis == AXIS_Z) { // Z is the longest axis
                        radius = 0.5f * std::max(s.x, s.y);
                        halfHeight = 0.5f * s.z;
                    }
                    else { // X is the longest axis (or it's a uniform scale)
                        radius = 0.5f * std::max(s.y, s.z);
                        halfHeight = 0.5f * s.x;
                    }

                    // The halfHeight parameter for PhysX is for the CYLINDRICAL part only.
                    // We must subtract the radius from the half-length of the major axis.
                    halfHeight = halfHeight - radius;

                    // Enforce positive dimensions to prevent invalid geometry.
                    const float kMin = 0.01f;
                    if (radius <= 0.0f)     radius = kMin;
                    if (halfHeight <= 0.0f) halfHeight = kMin; // For a sphere, this will be kMin.

                    PxCapsuleGeometry capsule(radius, halfHeight);
                    PX_ASSERT(capsule.isValid());

                    collider.Shape = m_Physics->createShape(capsule, *collider.material);
                    if (!collider.Shape) {
                        BOOM_ERROR("PxPhysics::createShape failed for capsule");
                        return;
                    }

                    // Rotate the capsule from PhysX's default +X axis to our chosen major axis.
                    PxQuat localQ = PxQuat(PxIdentity); // Default rotation for X-axis alignment
                    if (majorAxis == AXIS_Y) {
                        // Rotate +90 degrees around Z to map X -> Y
                        localQ = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
                    }
                    else if (majorAxis == AXIS_Z) {
                        // Rotate -90 degrees around Y to map X -> Z
                        localQ = PxQuat(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));
                    }

                    collider.Shape->setLocalPose(PxTransform(PxVec3(0.0f), localQ));
                }

                else if (collider.type == Collider3D::MESH) {
                    std::cout << "Creating MESH collider from actual model geometry..." << std::endl;

                    if (entity.template Has<ModelComponent>()) {
                        auto& modelComp = entity.Get<ModelComponent>();
                        if (modelComp.modelID) {

                            auto& modelAsset = assetRegistry.Get<ModelAsset>(modelComp.modelID);
                            Model3D modelPtr = modelAsset.data;

                            // Only support StaticModel for mesh colliders
                            auto staticModel = std::dynamic_pointer_cast<StaticModel>(modelPtr);
                            if (staticModel) {
                                auto physicsMeshData = staticModel->GetMeshData();

                                if (!physicsMeshData.empty()) {
                                    // Use the first mesh (you could combine multiple meshes)
                                    auto& meshData = physicsMeshData[0];

                                    // Scale vertices by transform
                                    MeshData<ShadedVert> scaledMeshData = meshData;
                                    for (auto& vertex : scaledMeshData.vtx) {
                                        vertex.pos *= transform.scale;
                                    }

                                    std::cout << "Using actual model geometry: " << scaledMeshData.vtx.size() << " vertices" << std::endl;

                                    // Cook the actual mesh geometry
                                    collider.mesh = CookMesh(scaledMeshData);
                                    collider.Shape = m_Physics->createShape(collider.mesh, *collider.material);

                                    if (!collider.Shape) {
                                        BOOM_ERROR("Failed to create mesh collider shape");
                                        return;
                                    }

                                    std::cout << "REAL MESH collider created successfully!" << std::endl;
                                }
                                else {
                                    BOOM_ERROR("StaticModel has no physics mesh data");
                                    return;
                                }
                            }
                            else {
                                BOOM_ERROR("Mesh colliders currently only support StaticModel");
                                return;
                            }
                        }
                        else {
                            BOOM_ERROR("Entity has ModelComponent but no model loaded");
                            return;
                        }
                    }
                    else {
                        BOOM_ERROR("Mesh collider requires ModelComponent");
                        return;
                    }
                }
                else
                {
                    BOOM_ERROR("Error creating collider invalid type provided");
                    return;
                }

                // Ensure shape is included in debug viz
                if (collider.Shape) {
                    collider.Shape->setFlag(PxShapeFlag::eVISUALIZATION, true);
                }

                // create actor instanace
                if (body.type == RigidBody3D::DYNAMIC)
                {
                    body.actor = PxCreateDynamic(*m_Physics, pose, *collider.Shape, body.density);

                    // --- SOLUTION: Add this line ---
                    // Recalculate the mass and inertia tensor based on the final, rotated capsule shape.
                    PxRigidBodyExt::updateMassAndInertia(*static_cast<PxRigidBody*>(body.actor), body.density);

                    body.actor->setActorFlag(PxActorFlag::eSEND_SLEEP_NOTIFIES, true);

                    PxRigidDynamic* dyn = static_cast<PxRigidDynamic*>(body.actor);
                    if (dyn) {
                        // You can remove dyn->setMass(body.mass); as updateMassAndInertia handles it.
                        dyn->setLinearVelocity(PxVec3(body.initialVelocity.x, body.initialVelocity.y, body.initialVelocity.z));
                    }
                }
                else if (body.type == RigidBody3D::STATIC)
                {
                    body.actor = PxCreateStatic(*m_Physics,
                        pose, *collider.Shape);
                }
            }
            else
            {
                if (body.type == RigidBody3D::DYNAMIC)
                {
                    body.actor = m_Physics->createRigidDynamic(pose);
                }
                else if (body.type == RigidBody3D::STATIC)
                {
                    body.actor = m_Physics->createRigidStatic(pose);


                }
            }

            // check actor
            if (!body.actor)
            {
                BOOM_ERROR("Error creating dynamic actor");
                return;
            }

            // Opt-in the actor to debug visualization
            body.actor->setActorFlag(PxActorFlag::eVISUALIZATION, true);

            // set user data to entt id
            body.actor->userData = new EntityID(entity.ID());

            // add actor to the m_Scene
            m_Scene->addActor(*body.actor);
        }

        BOOM_INLINE void SetRigidBodyType(Entity& entity, RigidBody3D::Type newType)
        {
            if (!entity.Has<RigidBodyComponent>()) return;

            auto& body = entity.Get<RigidBodyComponent>().RigidBody;
            auto* oldActor = body.actor;

            // 1. --- Guard Clause: If the type isn't changing, do nothing ---
            if (!oldActor || body.type == newType) {
                return;
            }

            // 2. --- Preserve all essential properties from the old actor ---
            PxTransform transform = oldActor->getGlobalPose();
            EntityID* userData = static_cast<EntityID*>(oldActor->userData);

            // Get all shapes from the old actor. An actor can have multiple shapes.
            const PxU32 numShapes = oldActor->getNbShapes();
            std::vector<PxShape*> shapes(numShapes);
            oldActor->getShapes(shapes.data(), numShapes);

            // 3. --- Remove and release the old actor ---
            m_Scene->removeActor(*oldActor);
            oldActor->release();

            // 4. --- Create the new actor of the desired type ---
            PxRigidActor* newActor = nullptr;
            if (newType == RigidBody3D::DYNAMIC)
            {
                PxRigidDynamic* dyn = m_Physics->createRigidDynamic(transform);
                // CRITICAL: Update mass and inertia for the new dynamic body
                PxRigidBodyExt::updateMassAndInertia(*dyn, body.density);
                newActor = dyn;
            }
            else // newType is STATIC
            {
                newActor = m_Physics->createRigidStatic(transform);
            }

            // 5. --- Re-attach all shapes and restore user data ---
            if (newActor) {
                for (PxShape* shape : shapes) {
                    newActor->attachShape(*shape);
                }
                newActor->userData = userData;
                m_Scene->addActor(*newActor);
            }

            // 6. --- IMPORTANT: Update our component to point to the new actor and type ---
            body.actor = newActor;
            body.type = newType;
        }



        // Mesh Colliders
        BOOM_INLINE PxConvexMeshGeometry CookMesh(const MeshData<ShadedVert>& data) {
            // px vertex container
            std::vector<PxVec3> vertices;



            // convert position attributes
            for (auto& vertex : data.vtx)
            {
                vertices.push_back(ToPxVec3(vertex.pos));
            }

            PxConvexMeshDesc meshDesc;
            // vertices
            meshDesc.points.data = vertices.data();
            meshDesc.points.stride = sizeof(PxVec3);
            meshDesc.points.count = static_cast<PxU32>(vertices.size());
            // indices
            meshDesc.indices.data = data.idx.data();
            meshDesc.indices.count = static_cast<PxU32>(data.idx.size());
            // flags
            meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

            // cooking the mesh
            PxCookingParams cookingParams =
                PxCookingParams(PxTolerancesScale());
            PxCooking* cooking = PxCreateCooking(PX_PHYSICS_VERSION,
                *m_Foundation, cookingParams);
            PxConvexMeshCookingResult::Enum result;
            PxConvexMesh* convexMesh = cooking->createConvexMesh(meshDesc,
                m_Physics->getPhysicsInsertionCallback(), &result);
            PxConvexMeshGeometry convexMeshGeometry(convexMesh);

            cooking->release();
            return convexMeshGeometry;

        }

        BOOM_INLINE void UpdatePhysicsMaterial(Entity& ent) {
            // Ensure the entity has the required components
            if (!ent.Has<RigidBodyComponent>() || !ent.Has<ColliderComponent>()) {
                return;
            }

            auto& collider = ent.Get<ColliderComponent>().Collider;
            auto* actor = ent.Get<RigidBodyComponent>().RigidBody.actor;

            if (!actor || !collider.Shape) {
                BOOM_WARN("Attempted to update physics material on an entity with no actor or shape.");
                return;
            }

            // A shape can have multiple materials, but we are using just one.
            // We retrieve the material that is currently attached to the shape.
            PxMaterial* material;
            collider.Shape->getMaterials(&material, 1);

            if (material) {
                // Apply the values from our component to the live PxMaterial.
                material->setDynamicFriction(collider.dynamicFriction);
                material->setStaticFriction(collider.staticFriction);
                material->setRestitution(collider.restitution);
            }
        }


        BOOM_INLINE void Simulate(uint32_t step, float dt)
        {
            for (uint32_t i = 0; i < step; ++i)
            {
                // simulate m_Physics for a time step
                m_Scene->simulate(dt);

                // block until simulation is complete


                m_Scene->fetchResults(true);
            }
        }

        BOOM_INLINE void SetEventCallback(PxCallbackFunction&& callback)
        {
            m_EventCallback.m_Callback = callback;
        }



    private:
        // custom collision filter shader callback
        static PxFilterFlags CustomFilterShader
        (
            [[maybe_unused]] PxFilterObjectAttributes attributes0,
            [[maybe_unused]] PxFilterData filterData0,
            [[maybe_unused]] PxFilterObjectAttributes attributes1,
            [[maybe_unused]] PxFilterData filterData1,
            [[maybe_unused]] PxPairFlags& pairFlags,
            [[maybe_unused]] const void* constantBlock,
            [[maybe_unused]] PxU32 constantBlockSize
        )
        {
            (void)constantBlock;
            (void)constantBlockSize;
            // generate contacts and triggers for actors
            pairFlags |= PxPairFlag::eCONTACT_DEFAULT |
                PxPairFlag::eTRIGGER_DEFAULT;

            return PxFilterFlag::eDEFAULT;
        }



    private:
        PxDefaultErrorCallback m_ErrorCallback;
        PxDefaultAllocator m_AllocatorCallback;
        PxDefaultCpuDispatcher* m_Dispatcher;
        PxEventCallback m_EventCallback;
        PxFoundation* m_Foundation;
        PxPhysics* m_Physics;
        PxScene* m_Scene;

        bool m_DebugVisEnabled; // new
    };
}