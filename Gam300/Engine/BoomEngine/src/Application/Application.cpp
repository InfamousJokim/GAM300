#include "Core.h"
#include "Application/Application.h"

namespace Boom
{

    void Application::RunContext(bool showFrame)
    {
        BOOM_INFO("[Application] RunContext started");
        LoadScene("level");


        // -- LOADING in MONO --
        const std::string exeDir = GetExeDir();


        std::filesystem::path repoRoot = std::filesystem::path(exeDir)
            .parent_path()  // Debug -> x64
            .parent_path()  // x64 -> Gam300
            .parent_path(); // Gam300 -> GAM300

        const std::string monoBase = (repoRoot / "mono").string();
#if defined(_DEBUG)

        const std::string asmDir = (repoRoot / "Gam300" / "GameScripts" / "bin" / "x64" / "Debug").string();
#else
        const std::string asmDir = (repoRoot / "Gam300" / "GameScripts" / "bin" / "x64" / "Release").string();
#endif

        if (!InitMonoRuntime(monoBase, asmDir, "BoomDomain"))
        {
#ifdef _DEBUG
            BOOM_ERROR("[Scripting] Failed to initialize Mono runtime!");
#endif // DEBUG
        }

        else
        {
            RegisterScriptInternalCalls(m_Context);
            if (!LoadGameAssembly("GameScripts.dll"))
            {
#ifdef _DEBUG
                BOOM_ERROR("[Scripting] Failed to load GameScripts.dll");

#endif // DEBUG

            }
            else
            {

                InvokeStaticVoid("GameScripts", "Entry", "Start", nullptr);
#ifdef _DEBUG
                BOOM_INFO("[Scripting] GameScripts entry invoked.");
#endif // DEBUG

            }
        }
        // --- END MONO INITIALIZE ---

        InitNavRuntime();
        //EnsureNinjaSeeksSamurai();
        CameraController camera(
            m_Context->window.get()
        );

        ////init skybox
        EnttView<Entity, SkyboxComponent>([this](auto, auto& comp) {
            SkyboxAsset& skybox{ m_Context->assets->Get<SkyboxAsset>(comp.skyboxID) };
            m_Context->renderer->InitSkybox(skybox.data, skybox.envMap, skybox.size);
            });

        m_DebugLinesShader = std::make_unique<Boom::DebugLinesShader>("debug_lines.glsl");
        m_Context->physics->EnableDebugVisualization(m_PhysDebugViz, 1.0f);

        //temp input for mouse motion
        glm::dvec2 curMP{};
        glm::dvec2 prevMP{};
        while (m_Context->window->PollEvents() && !m_ShouldExit)
        {
            std::shared_ptr<GLFWwindow> engineWindow = m_Context->window->Handle();
            SoundEngine::Instance().Update();
            Camera3D* activeCam = nullptr;
            Transform3D camTransform{};
            EnttView<Entity, CameraComponent>([&](auto en, CameraComponent& comp) {
                if (!activeCam && comp.camera.cameraType == Camera3D::CameraType::Main) {
                    camTransform = en.Get<TransformComponent>().transform;
                    activeCam = &comp.camera;
                }
                });
            if (!activeCam) { // fallback: first camera
            }

            // 2) Attach BEFORE update so scroll/pan use this camera's FOV in this frame
            if (activeCam) camera.attachCamera(activeCam);
            glfwMakeContextCurrent(engineWindow.get());

            // NEW: runtime toggle with F9
            {
                static bool prevF9 = false;
                bool f9Pressed = glfwGetKey(engineWindow.get(), GLFW_KEY_F9) == GLFW_PRESS;
                if (f9Pressed && !prevF9)
                {
                    m_PhysDebugViz = !m_PhysDebugViz;
                    m_Context->physics->EnableDebugVisualization(m_PhysDebugViz, 1.0f);
                    BOOM_INFO("[PhysX] Debug visualization: {}", m_PhysDebugViz ? "ON" : "OFF");
                }
                prevF9 = f9Pressed;
            }

            //   F11 for testing change of rigid body type
            {
                static bool prevF11 = false;
                bool f11Pressed = glfwGetKey(engineWindow.get(), GLFW_KEY_F11) == GLFW_PRESS;
                if (f11Pressed && !prevF11)
                {
                    // Find the "Sphere" entity to toggle its type
                    EnttView<Entity, InfoComponent, RigidBodyComponent>([this](auto entity, InfoComponent& info, RigidBodyComponent& rb) {
                        if (info.name == "Sphere") {
                            // Determine the new type by flipping the current one
                            RigidBody3D::Type currentType = rb.RigidBody.type;
                            RigidBody3D::Type newType = (currentType == RigidBody3D::DYNAMIC)
                                ? RigidBody3D::STATIC
                                : RigidBody3D::DYNAMIC;

                            // Call the function to perform the switch!
                            m_Context->physics->SetRigidBodyType(entity, newType);

                            // Log the change to the console for confirmation
                            BOOM_INFO("[Test F11] Toggled Sphere rigid body to: {}", (newType == RigidBody3D::DYNAMIC) ? "DYNAMIC" : "STATIC");
                        }
                        });
                }
                prevF11 = f11Pressed;
            }


            // Always update delta time, but adjust for pause state
            ComputeFrameDeltaTime();
            InvokeStatic1Float("GameScripts", "Entry", "Update", static_cast<float>(m_Context->DeltaTime));
            m_AIagents.update(m_Context->scene, static_cast<float>(m_Context->DeltaTime));
            if (m_Nav) {
                m_NavAgents.update(m_Context->scene, static_cast<float>(m_Context->DeltaTime), *m_Nav);
            }

            //compute camera position to colliders
            //if (m_AppState == ApplicationState::RUNNING)
               //m_Context->physics->ResolveThirdPersonCameraPosition(, camTransform.translate);

            // ============ END NEW SECTION ============
            m_Context->profiler.BeginFrame();
            m_Context->profiler.Start("Total Frame");
            m_Context->profiler.Start("Renderer Start Frame");
            std::apply(glClearColor, CONSTANTS::DEFAULT_BACKGROUND_COLOR);
            RenderShadowScene();
            m_Context->renderer->NewFrame();
            m_Context->profiler.End("Renderer Start Frame");

            // Only update rotation when running
            if (m_AppState == ApplicationState::RUNNING) {
                EnttView<Entity, RigidBodyComponent>([](auto, RigidBodyComponent& rb) {
                    rb.RigidBody.isColliding = false;
                    });
                UpdateKinematicTransforms();
                RunPhysicsSimulation();
                InitNavRuntime();
                UpdateThirdPersonCameras();
            }


            LightsUpdate();

            //temp input for mouse motion
            glfwGetCursorPos(m_Context->window->Handle().get(), &curMP.x, &curMP.y);
            // ONLY update the flycam controller if the game is PAUSED
            if (m_AppState != ApplicationState::RUNNING) {
                camera.update(static_cast<float>(m_Context->DeltaTime));
            }

            glm::mat4 dbgView(1.0f);
            glm::mat4 dbgProj(1.0f);
            glm::vec3 dbgCamPos(0.0f);

            EnttView<Entity, CameraComponent>([this, &curMP, &prevMP, &dbgView, &dbgProj, &dbgCamPos](auto entity, CameraComponent& comp) {
                Transform3D& transform{ entity.template Get<TransformComponent>().transform };

                // ONLY apply flycam logic if the game is PAUSED
                if (m_AppState != ApplicationState::RUNNING)
                {
                    // This is the flycam logic, only run when not playing
                    transform.rotate.x += m_Context->window->camRot.x;
                    transform.rotate.y += m_Context->window->camRot.y;
                    glm::quat quat{ glm::radians(transform.rotate) };
                    glm::vec3 dir{ quat * m_Context->window->camMoveDir };
                    transform.translate += dir;

                    if (curMP == prevMP) {
                        m_Context->window->camRot = {};
                        if (m_Context->window->isMiddleClickDown)
                            m_Context->window->camMoveDir = {};
                    }
                }

                // This part is needed by BOTH cameras, so leave it outside the 'if'
                m_Context->renderer->SetCamera(comp.camera, transform);
                dbgView = comp.camera.View(transform);
                dbgProj = comp.camera.Projection(m_Context->renderer->Aspect());
                dbgCamPos = transform.translate;
                });
            {
                prevMP = curMP;

                EnttView<Entity, TransformComponent, RigidBodyComponent>([this](auto entity, TransformComponent& tc, RigidBodyComponent& rbc) {
                    // Check if the current scale is different from the stored scale
                    if (tc.transform.scale != rbc.RigidBody.previousScale)
                    {
                        // If it changed, update the collider shape
                        m_Context->physics->UpdateColliderShape(entity, GetAssetRegistry());

                        // Then, update the stored scale to the new value for the next frame
                        rbc.RigidBody.previousScale = tc.transform.scale;
                    }
                    });
            }

            RenderScene();
            //DrawDebugTPC();
            if (m_PhysDebugViz && m_DebugLinesShader)
            {
                m_Context->physics->CollectDebugLines(m_PhysLinesCPU);
                if (!m_PhysLinesCPU.empty())
                {
                    // Build CPU line list
                    std::vector<Boom::LineVert> lineVerts;
                    lineVerts.reserve(m_PhysLinesCPU.size() * 2);
                    for (const auto& l : m_PhysLinesCPU)
                    {
                        lineVerts.push_back(Boom::LineVert{ l.p0, l.c0 });
                        lineVerts.push_back(Boom::LineVert{ l.p1, l.c1 });
                    }

                    // Cull any segments within a small radius of the camera (fix “ball in face”)
                    std::vector<Boom::LineVert> filtered;
                    filtered.reserve(lineVerts.size());
                    const float camCullRadius = 0.6f;

                    for (size_t i = 0; i + 1 < lineVerts.size(); i += 2)
                    {
                        const auto& a = lineVerts[i + 0];
                        const auto& b = lineVerts[i + 1];

                        const float segDist = DistancePointSegment(dbgCamPos, a.pos, b.pos);
                        const float endA = glm::distance(a.pos, dbgCamPos);
                        const float endB = glm::distance(b.pos, dbgCamPos);

                        // Keep only segments not near the camera (endpoints and segment body)
                        if (segDist >= camCullRadius && endA >= camCullRadius && endB >= camCullRadius)
                        {
                            filtered.push_back(a);
                            filtered.push_back(b);
                        }
                    }

                    if (!filtered.empty())
                        m_DebugLinesShader->Draw(dbgView, dbgProj, filtered, 50.5f);
                }
            }
            if (m_PhysDebugViz && m_DebugLinesShader)
            {
                DrawRigidBodiesDebugOnly(dbgView, dbgProj);
            }
            if (m_Context->ShowNavDebug && m_DebugLinesShader && m_Nav) {
                // Draw navmesh edges & centroids near the camera. Tweak radius to taste.
                const float navDrawRadius = 60.0f; // try 40–100 to see more/less
                m_Nav->DrawDetourNavMesh_Query(*m_DebugLinesShader, dbgView, dbgProj, dbgCamPos, navDrawRadius);
            }

            //skybox ecs (should be drawn at the end)
            EnttView<Entity, SkyboxComponent>([this](auto entity, SkyboxComponent& comp) {
                Transform3D& transform{ entity.template Get<TransformComponent>().transform };
                SkyboxAsset& skybox{ m_Context->assets->Get<SkyboxAsset>(comp.skyboxID) };
                m_Context->renderer->DrawSkybox(skybox.data, transform);
                });

            m_Context->profiler.Start("Renderer End Frame");
            m_Context->renderer->EndFrame();
            m_Context->profiler.End("Renderer End Frame");

            //draw the updated frame
            m_Context->renderer->ShowFrame(showFrame);


            for (auto layer : m_Context->layers)
            {
                layer->OnUpdate();
            }

            m_Context->profiler.End("Total Frame");
            m_Context->profiler.EndFrame();
        }
    }

    void Application::RenderScene()
    {
        std::vector<std::pair<SpriteComponent, Transform2D>> guiList;
        //pbr ecs (always render)
        EnttView<Entity, TransformComponent>([this, &guiList](auto entity, TransformComponent& t) {
            if (entity.Has<ModelComponent>()) {
                ModelComponent& comp{ entity.Get<ModelComponent>() };
                if (comp.modelID == EMPTY_ASSET) return;
                ModelAsset& model{ m_Context->assets->Get<ModelAsset>(comp.modelID) };
                if (entity.Has<AnimatorComponent>()) {
                    auto& an = entity.Get<AnimatorComponent>();
                    float dt = (m_AppState == ApplicationState::RUNNING) ? (float)m_Context->DeltaTime : 0.0f;
                    auto& joints = an.animator->Animate(dt);
                    m_Context->renderer->SetJoints(joints);           // existing
                }
                else {
                    // NEW: ensure no stale palette leaks into this draw
                    if (model.hasJoints)
                    {
                        static std::vector<glm::mat4> identityPalette(100, glm::mat4(1.0f));
                        m_Context->renderer->SetJoints(identityPalette);
                    }
                }

                glm::mat4 worldMatrix = GetWorldMatrix(entity);
                Transform3D worldTransform;
                DecomposeMatrix(worldMatrix, worldTransform.translate, worldTransform.rotate, worldTransform.scale);

                //draw model with material if it has one otherwise draw default material
                if (comp.materialID != EMPTY_ASSET) {
                    auto& material{ m_Context->assets->Get<MaterialAsset>(comp.materialID) };

                    // Only assign textures if they exist and are valid
                    if (material.albedoMapID != EMPTY_ASSET) {
                        auto& albedoTex = m_Context->assets->Get<TextureAsset>(material.albedoMapID);
                        if (albedoTex.data) {
                            material.data.albedoMap = albedoTex.data;
                        }
                    }
                    if (material.normalMapID != EMPTY_ASSET) {
                        auto& normalTex = m_Context->assets->Get<TextureAsset>(material.normalMapID);
                        if (normalTex.data) {
                            material.data.normalMap = normalTex.data;
                        }
                    }
                    if (material.roughnessMapID != EMPTY_ASSET) {
                        auto& roughnessTex = m_Context->assets->Get<TextureAsset>(material.roughnessMapID);
                        if (roughnessTex.data) {
                            material.data.roughnessMap = roughnessTex.data;
                        }
                    }

                    m_Context->renderer->Draw(model.data, worldTransform, material.data);
                }
                else {
                    m_Context->renderer->Draw(model.data, worldTransform);
                }
            }
            else if (entity.Has<SpriteComponent>()) {
                SpriteComponent& comp{ entity.Get<SpriteComponent>() };
                if (comp.textureID == EMPTY_ASSET) return;

                if (!comp.uiOverlay) {
                    TextureAsset& texture{ m_Context->assets->Get<TextureAsset>(comp.textureID) };
                    m_Context->renderer->DrawQuad(texture.data, t.transform, comp.color);
                }
                else guiList.push_back({ comp, t.transform });
            }
            });

        //sort guiList based on z-axis from negative to positive(opengl z-axis towards camera)
        std::sort(guiList.begin(), guiList.end(), [](const auto& a, const auto& b) {
            return a.second.translate.z > b.second.translate.z;  // descending Z order
            });

        //render gui overlays at the end
        for (auto const& gui : guiList) {
            TextureAsset& texture{ m_Context->assets->Get<TextureAsset>(gui.first.textureID) };
            m_Context->renderer->DrawQuad(texture.data, gui.second, gui.first.color);
        }
    }

    glm::mat4 Application::GetWorldMatrix(Entity& entity)
    {
        // 1. Get this entity's local matrix (e.g., the visual model's transform)
        glm::mat4 localMatrix(1.0f);
        if (entity.Has<TransformComponent>()) {
            localMatrix = entity.Get<TransformComponent>().transform.Matrix();
        }

        // 2. Get the parent's world matrix (e.g., the physics body's transform)
        glm::mat4 pMatrix(1.0f);
        if (entity.Has<InfoComponent>()) {
            uint64_t parentUID = entity.Get<InfoComponent>().parent;

            if (parentUID != 0) // 0 means root/no parent
            {
                entt::entity parentEnttID = entt::null;
                auto view = m_Context->scene.view<InfoComponent>();
                for (auto e : view) {
                    if (view.get<InfoComponent>(e).uid == parentUID) {
                        parentEnttID = e;
                        break;
                    }
                }

                if (parentEnttID != entt::null) {
                    Entity parentEntity{ &m_Context->scene, parentEnttID };
                    pMatrix = GetWorldMatrix(parentEntity); // Recurse
                }
            }
        }

        // 3. Decompose the parent's matrix into T, R, and S
        glm::vec3 pTranslate, pRotate, pScale;
        DecomposeMatrix(pMatrix, pTranslate, pRotate, pScale);


        // 4. Recompose the parent's matrix *without* its scale
        glm::mat4 pMatrix_NoScale;
        pMatrix_NoScale = RecomposeMatrix(pTranslate, pRotate, glm::vec3(1.0f));

        // 5. Return the parent's (T*R) multiplied by the child's (T*R*S)
        return pMatrix_NoScale * localMatrix;
    }

    void Application::UpdateThirdPersonCameras()

    {
        // 1. Get input
        glm::vec2 mouseDelta = m_Context->window->input.mouseDeltaLast();
        glm::vec2 scrollDelta = m_Context->window->input.scrollDelta();

        // 2. Iterate over all third-person cameras
        EnttView<Entity, ThirdPersonCameraComponent, TransformComponent>(
            [this, &mouseDelta, &scrollDelta](Entity, ThirdPersonCameraComponent& cam, TransformComponent& tc)
            {
                // 3. Find the target entity by its UID
                if (cam.targetUID == 0) return; // No target UID set

                entt::entity targetEnttID = entt::null;
                auto infoView = m_Context->scene.view<InfoComponent>();
                for (auto e : infoView) {
                    if (infoView.get<InfoComponent>(e).uid == cam.targetUID) {
                        targetEnttID = e;
                        break;
                    }
                }

                if (targetEnttID == entt::null) return; // Target not found

                Entity target{ &m_Context->scene, targetEnttID };
                if (!target.Has<TransformComponent>()) return; // Target has no position

                //
                // === NEW LOGIC STARTS HERE ===
                // I am following Dark Souls k&m control scheme: mouse is camera only, does not change player's direction
                //

                // the target
                Transform3D& targetTransform = target.Get<TransformComponent>().transform;
                glm::vec3 targetPosition = targetTransform.translate;

                //camera movement
                cam.currentYaw -= mouseDelta.x * cam.mouseSensitivity;
                cam.currentPitch += mouseDelta.y * cam.mouseSensitivity;
                cam.currentPitch = glm::clamp(cam.currentPitch, -85.f, 85.f);

                // zoom
                cam.currentDistance -= scrollDelta.y * cam.scrollSensitivity;
                cam.currentDistance = glm::clamp(cam.currentDistance, cam.minDistance, cam.maxDistance);

                // 9. Calculate the camera's final orientation
                glm::quat orientation = glm::quat(
                    glm::vec3(glm::radians(cam.currentPitch),
                        glm::radians(cam.currentYaw),
                        0.0f));

                // camera target point
                pivotPosition = targetPosition + cam.offset;

                // calculate final new camera translate according to spherical movement
                glm::vec3 offsetVector = glm::vec3(0.0f, 0.0f, -cam.currentDistance);
                glm::vec3 rotatedOffset = orientation * offsetVector;
                glm::vec3 desiredPosition = pivotPosition + rotatedOffset;

                // 13. Make the camera look at the pivot point
                tc.transform.rotate = glm::degrees(glm::eulerAngles(
                    glm::quatLookAt(glm::normalize(pivotPosition - desiredPosition), glm::vec3(0, 1, 0))
                ));

                tc.transform.translate = m_Context->physics->ResolveThirdPersonCameraPosition(pivotPosition, desiredPosition);
            }
        );
    }


    //Physics stuff

    void Application::DestroyPhysicsActors()
    {
        // Get the scene from the physics context *once* outside the loop
        auto* pxScene = m_Context->physics->GetPxScene();
        if (!pxScene) {
            BOOM_ERROR("DestroyPhysicsActors failed: No PxScene available.");
            return;
        }

        // Iterate over all entities with a RigidBodyComponent
        EnttView<Entity, RigidBodyComponent>([this, pxScene](auto entity, auto& comp)
            {
                auto* actor = comp.RigidBody.actor;
                if (!actor) return; // Skip if no actor

                // 1. Clean up Collider Pointers (if they exist)
                if (entity.template Has<ColliderComponent>())
                {
                    auto& collider = entity.template Get<ColliderComponent>().Collider;
                    if (collider.material) {
                        collider.material->release();
                        collider.material = nullptr;
                    }
                    if (collider.Shape) {
                        collider.Shape->release();
                        collider.Shape = nullptr;
                    }
                }

                // 2. Destroy actor user data
                if (actor->userData) {
                    EntityID* owner = static_cast<EntityID*>(actor->userData);
                    BOOM_DELETE(owner);
                    actor->userData = nullptr;
                }

                // 3. (THE FIX) Remove from scene, THEN release memory
                pxScene->removeActor(*actor);
                actor->release();
                comp.RigidBody.actor = nullptr;
            });
    }

    void Application::RunPhysicsSimulation()
    {
        // Only simulate physics if running
        if (m_AppState == ApplicationState::RUNNING)
        {
            // Apply navigation velocities BEFORE physics simulation
            EnttView<Entity, NavAgentComponent, RigidBodyComponent>([this](auto /*entity*/, auto& navAgent, auto& rb)
                {
                    if (!navAgent.active || !rb.RigidBody.actor) return;

                    auto* dyn = rb.RigidBody.actor->is<physx::PxRigidDynamic>();
                    if (!dyn) return;

                    // Convert glm velocity to PhysX and apply directly
                    physx::PxVec3 pxVel(navAgent.velocity.x, navAgent.velocity.y, navAgent.velocity.z);
                    dyn->setLinearVelocity(pxVel);

                    // OPTIONAL: Lock Y rotation so the agent doesn't tip over
                    dyn->setAngularVelocity(physx::PxVec3(0, 0, 0));
                });

            m_Context->physics->Simulate(1, static_cast<float>(m_Context->DeltaTime));

            EnttView<Entity, RigidBodyComponent>([this](auto entity, auto& comp)
                {
                    auto& transform = entity.template Get<TransformComponent>().transform;

                    // --- guard / lazy create ---
                    if (!comp.RigidBody.actor) {
                        // optional: try to create now
                        if (m_Context->physics)
                            m_Context->physics->AddRigidBody(entity, *m_Context->assets);

                        if (!comp.RigidBody.actor) {
                            // still null -> skip this entity this frame
                            return;
                        }
                    }

                    const auto pose = comp.RigidBody.actor->getGlobalPose();
                    if (comp.RigidBody.actor->is<physx::PxRigidDynamic>()) {
                        glm::quat rot(pose.q.w, pose.q.x, pose.q.y, pose.q.z);
                        transform.rotate = glm::degrees(glm::eulerAngles(rot));
                        transform.translate = PxToVec3(pose.p);
                    }
                });

        }
    }

    void Application::AppendCapsuleWire(float radius, float halfHeight, const physx::PxTransform& world, std::vector<Boom::LineVert>& out, const glm::vec4& color)
    {
        const glm::mat4 M = PxToGlm(world);
        const int seg = 12; // Segments for a semi-circle

        // --- Create transforms for the two hemisphere centers by translating in LOCAL space ---
        // A PhysX capsule's length is ALWAYS along its local X-axis.
        glm::mat4 M_end_pos_x = M * glm::translate(glm::mat4(1.0f), glm::vec3(halfHeight, 0, 0));
        glm::mat4 M_end_neg_x = M * glm::translate(glm::mat4(1.0f), glm::vec3(-halfHeight, 0, 0));

        // --- Draw the two hemispheres ---
        // For an X-aligned capsule, the "equator" is a circle in the YZ plane.
        // The other two semi-circles provide the 3D volume.

        // Positive X Hemisphere
        AppendCircle(M_end_pos_x, radius, seg * 2, 0, 0.0f, out, color);     // Equator circle (YZ plane)
        AppendSemiCircle(M_end_pos_x, radius, seg, 1, true, out, color);  // Top arc (XZ plane)
        AppendSemiCircle(M_end_pos_x, radius, seg, 2, true, out, color);  // Side arc (XY plane)

        // Negative X Hemisphere
        AppendCircle(M_end_neg_x, radius, seg * 2, 0, 0.0f, out, color);     // Equator circle (YZ plane)
        AppendSemiCircle(M_end_neg_x, radius, seg, 1, false, out, color); // Bottom arc (XZ plane)
        AppendSemiCircle(M_end_neg_x, radius, seg, 2, false, out, color); // Side arc (XY plane)

        // --- Draw four side rails connecting the equators ---
        for (int i = 0; i < 4; ++i) {
            float angle = glm::half_pi<float>() * i;

            // Points on the circumference of the capsule's equator (in the YZ plane)
            glm::vec3 p_on_circ(0, radius * cos(angle), radius * sin(angle));

            // Define the local start and end points of the rail along the X-axis
            glm::vec3 rail_start_local = glm::vec3(-halfHeight, 0, 0) + p_on_circ;
            glm::vec3 rail_end_local = glm::vec3(halfHeight, 0, 0) + p_on_circ;

            // Transform these local points to the final world space using the main transform M
            glm::vec3 A = glm::vec3(M * glm::vec4(rail_start_local, 1.0f));
            glm::vec3 B = glm::vec3(M * glm::vec4(rail_end_local, 1.0f));

            AppendLine(out, A, B, color, color);
        }
    }

    void Application::DrawRigidBodiesDebugOnly(const glm::mat4& view, const glm::mat4& proj)
    {
        if (!m_DebugLinesShader) return;

        std::vector<Boom::LineVert> verts;
        verts.reserve(1024);
        //const glm::vec4 color(0.1f, 1.0f, 0.1f, 1.0f);

        EnttView<Entity, RigidBodyComponent>([&](auto entity, RigidBodyComponent& rb) {
            if (!entity.template Has<ColliderComponent>()) return;

            const glm::vec4 color = rb.RigidBody.isColliding
                ? glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) // Bright Red when colliding
                : glm::vec4(1.1f, 0.0f, 1.0f, 1.0f);

            physx::PxRigidActor* actor = rb.RigidBody.actor;
            if (!actor) return;

            PxU32 n = actor->getNbShapes();
            if (!n) return;
            std::vector<physx::PxShape*> shapes(n);
            actor->getShapes(shapes.data(), n);

            const physx::PxTransform actorPose = actor->getGlobalPose();
            for (auto* shape : shapes)
            {
                const physx::PxTransform world = actorPose * shape->getLocalPose();
                physx::PxGeometryHolder gh = shape->getGeometry();
                switch (gh.getType())
                {
                case physx::PxGeometryType::eBOX:
                    AppendBoxWire(gh.box(), world, verts, color);
                    break;
                case physx::PxGeometryType::eSPHERE:
                    AppendSphereWire(gh.sphere().radius, world, verts, color);
                    break;
                case physx::PxGeometryType::eCAPSULE:
                    AppendCapsuleWire(gh.capsule().radius, gh.capsule().halfHeight, world, verts, color);
                    break;
                default:
                    // For mesh/convex/etc. you can add more if needed; skip for now.
                    break;
                }
            }
            });

        if (!verts.empty())
            m_DebugLinesShader->Draw(view, proj, verts, 10.5f);
    }

}