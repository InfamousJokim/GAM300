#pragma once
#ifndef APPLICATION_H
#define APPLICATION_H
#include "Interface.h"
#include "ECS/ECS.hpp"
#include "Physics/Context.h"
#include "Audio/Audio.hpp"   
#include "Auxiliaries/DataSerializer.h"
#include "Auxiliaries/PrefabUtility.h"
#include "Scripting/ScriptAPI.h"
#include "Scripting/ScriptRuntime.h"
#include "Graphics/Shaders/DebugLines.h"
#include "Input/CameraManager.h"

namespace Boom
{
    /**
     * @enum ApplicationState
     * @brief Defines the current state of the application
     */
    enum class ApplicationState
    {
        RUNNING,
        PAUSED,
        STOPPED
    };

    /**
     * @class Application
     * @brief Core application that owns the context and drives all layers.
     *
     * Inherits from AppInterface to receive the same lifecycle hooks
     * and gain access to the shared AppContext.
     */
    struct Application : AppInterface
    {
        template<typename EntityType, typename... Components, typename Fn>
        BOOM_INLINE void EnttView(Fn&& fn) {
            auto view = m_Context->scene.view<Components...>();
            for (auto e : view) {
                fn(EntityType{ &m_Context->scene, e }, m_Context->scene.get<Components>(e)...);
            }
        }


        // Application state management
        ApplicationState m_AppState = ApplicationState::RUNNING;
        double m_PausedTime = 0.0;  // Track time spent paused
        double m_LastPauseTime = 0.0;  // When the last pause started
        bool m_ShouldExit = false;  // Flag for graceful shutdown
        float m_TestRot = 0.0f;

        // NEW: PhysX debug visualization toggle
        bool m_PhysDebugViz = true;

        // Temporary for showing physics
        double m_SphereTimer = 0.0;
        double m_SphereResetInterval = 5.0;
        glm::vec3 m_SphereInitialPosition = { 2.5f, 1.2f, 0.0f };

        /**
         * @brief Constructs the Application, assigns its unique ID, and allocates the AppContext.
         *
         * BOOM_INLINE hints to the compiler to inline this small constructor
         * to avoid function-call overhead during startup.
         */
        BOOM_INLINE Application()
        {
            m_LayerID = TypeID<Application>();
            m_Context = new AppContext();
            RegisterEventCallbacks();

            AttachCallback<WindowResizeEvent>([this](auto e) {
                m_Context->renderer->Resize(e.width, e.height);
                }
            );

            AttachCallback<WindowTitleRenameEvent>([this](auto e) {
                m_Context->window->SetWindowTitle(e.title);
                }
            );
        }

        /**
         * @brief Destructor that frees the AppContext.
         *
         * BOOM_INLINE here helps keep the size of the vtable cleanup small
         * by inlining the delete call.
         */
        BOOM_INLINE ~Application()
        {
            DestroyPhysicsActors();
            BOOM_DELETE(m_Context);
            //called here in case of the need of multiple windows
            glfwTerminate();
        }

        /**
        * @brief Pauses the application (stops updates but continues rendering)
        */
        BOOM_INLINE void Pause()
        {
            if (m_AppState == ApplicationState::RUNNING) {
                m_AppState = ApplicationState::PAUSED;
                m_LastPauseTime = glfwGetTime();
                BOOM_INFO("[Application] Paused");

                // Pause audio if available
            }
        }

        /**
         * @brief Resumes the application from pause
         */
        BOOM_INLINE void Resume()
        {
            if (m_AppState == ApplicationState::PAUSED) {
                m_AppState = ApplicationState::RUNNING;

                // Add paused time to total paused time
                m_PausedTime += (glfwGetTime() - m_LastPauseTime);

                BOOM_INFO("[Application] Resumed");

                // Resume audio if available
            }
        }

        /**
         * @brief Stops the application gracefully
         */
        BOOM_INLINE void Stop()
        {
            m_AppState = ApplicationState::STOPPED;
            m_ShouldExit = true;
            BOOM_INFO("[Application] Stopping application...");

            // Stop audio if available
        }

        /**
         * @brief Toggles between pause and resume
         */
        BOOM_INLINE void TogglePause()
        {
            if (m_AppState == ApplicationState::RUNNING) {
                Pause();
            }
            else if (m_AppState == ApplicationState::PAUSED) {
                Resume();
            }
        }

        /**
         * @brief Gets the current application state
         */
        BOOM_INLINE ApplicationState GetState() const { return m_AppState; }

        /**
         * @brief Gets the adjusted time (excluding paused time)
         */
        BOOM_INLINE double GetAdjustedTime() const
        {
            double currentTime = glfwGetTime();
            double adjustedPausedTime = m_PausedTime;

            // If currently paused, add the current pause duration
            if (m_AppState == ApplicationState::PAUSED) {
                adjustedPausedTime += (currentTime - m_LastPauseTime);
            }

            return currentTime - adjustedPausedTime;
        }


        /**
         * @brief Runs the main loop, calling OnUpdate() on every attached layer.
         *
         * BOOM_INLINE suggests inlining this hot-path entry point so
         * the call to RunContext itself adds minimal overhead.
         */
        BOOM_INLINE void RunContext(bool showFrame = false)
        {
            LoadScene("default");
            CameraController camera(
                m_Context->window.get(),
                CameraController::Config{
                    .mouseSensitivityX = 0.25f,
                    .mouseSensitivityY = 0.25f,
                    .multiplierStep = 0.01f,
                    .gateToViewportRect = true,
                    .gateToRMB = true
                }
            );
            ////init skybox
            EnttView<Entity, SkyboxComponent>([this](auto, auto& comp) {
                SkyboxAsset& skybox{ m_Context->assets->Get<SkyboxAsset>(comp.skyboxID) };
                m_Context->renderer->InitSkybox(skybox.data, skybox.envMap, skybox.size);
                });

            CreateScriptInstancesFromScene();



            m_DebugLinesShader = std::make_unique<Boom::DebugLinesShader>("debug_lines.glsl");

            // NEW: turn on PhysX debug visualization once at startup
            m_Context->physics->EnableDebugVisualization(m_PhysDebugViz, 1.0f);

            // temp input for mouse motion
            glm::dvec2 curMP{};
            glm::dvec2 prevMP{};
            while (m_Context->window->PollEvents() && !m_ShouldExit)
            {
                std::shared_ptr<GLFWwindow> engineWindow = m_Context->window->Handle();
                Camera3D* activeCam = nullptr;
                EnttView<Entity, CameraComponent>([&](auto, CameraComponent& comp) {
                    if (!activeCam && comp.camera.cameraType == Camera3D::CameraType::Main)
                        activeCam = &comp.camera;
                    });
                if (!activeCam) { // fallback: first camera
                    EnttView<Entity, CameraComponent>([&](auto, CameraComponent& comp) {
                        if (!activeCam) activeCam = &comp.camera;
                        });
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

                // NEW: toggle debug rigidbodies with F10
                {
                    static bool prevF10 = false;
                    bool f10Pressed = glfwGetKey(engineWindow.get(), GLFW_KEY_F10) == GLFW_PRESS;
                    if (f10Pressed && !prevF10)
                    {
                        m_DebugRigidBodiesOnly = !m_DebugRigidBodiesOnly;
                        // Turn off PhysX global viz when we use RB-only to avoid duplication
                        m_Context->physics->EnableDebugVisualization(!m_DebugRigidBodiesOnly && m_PhysDebugViz, 1.0f);
                        BOOM_INFO("[Debug] Rigidbody-only collider viz: {}", m_DebugRigidBodiesOnly ? "ON" : "OFF");
                    }
                    prevF10 = f10Pressed;
                }

                // ✨ NEW: Add this block to test SetRigidBodyType with F11
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

                m_Context->profiler.BeginFrame();
                m_Context->profiler.Start("Total Frame");
                m_Context->profiler.Start("Renderer Start Frame");
                m_Context->renderer->NewFrame();
                m_Context->profiler.End("Renderer Start Frame");

                // Only update rotation when running
                if (m_AppState == ApplicationState::RUNNING) {
                    script_update_all(static_cast<float>(m_Context->DeltaTime));
                    RunPhysicsSimulation();
                }

                m_SphereTimer += m_Context->DeltaTime;
                if (m_SphereTimer >= m_SphereResetInterval) {
                    ResetSphere();
                    m_SphereTimer = 0.0;
                }
                // When paused, m_TestRot stays at its current value

                // ... (The commented out sphere timer logic remains here) ...

                //lights (always set up)
                // ... (Your light setup loops remain here) ...
                {
                    int points = 0;
                    EnttView<Entity, PointLightComponent, TransformComponent>(
                        [this, &points](auto, PointLightComponent& plc, TransformComponent& tc)
                        {
                            m_Context->renderer->SetLight(plc.light, tc.transform, points++);
                        });
                    m_Context->renderer->SetPointLightCount(points);
                }
                {
                    int directs = 0;
                    EnttView<Entity, DirectLightComponent, TransformComponent>(
                        [this, &directs](auto, DirectLightComponent& dlc, TransformComponent& tc)
                        {
                            m_Context->renderer->SetLight(dlc.light, tc.transform, directs++);
                        });
                    m_Context->renderer->SetDirectionalLightCount(directs);
                }
                {
                    int spots = 0;
                    EnttView<Entity, SpotLightComponent, TransformComponent>(
                        [this, &spots](auto, SpotLightComponent& slc, TransformComponent& tc)
                        {
                            m_Context->renderer->SetLight(slc.light, tc.transform, spots++);
                        });
                    m_Context->renderer->SetSpotLightCount(spots);
                }


                //temp input for mouse motion
                glfwGetCursorPos(m_Context->window->Handle().get(), &curMP.x, &curMP.y);

                camera.update(static_cast<float>(m_Context->DeltaTime));

                // ADD: Per-frame debug view/proj used by debug line renderer
                glm::mat4 dbgView(1.0f);
                glm::mat4 dbgProj(1.0f);
                glm::vec3 dbgCamPos(0.0f);

                //camera (always set up, but rotation freezes when paused)
                EnttView<Entity, CameraComponent>([this, &curMP, &prevMP, &dbgView, &dbgProj, &dbgCamPos](auto entity, CameraComponent& comp) {
                    Transform3D& transform{ entity.template Get<TransformComponent>().transform };

                    //get dir vector of current camera
                    transform.rotate.x += m_Context->window->camRot.x;
                    transform.rotate.y += m_Context->window->camRot.y;
                    glm::quat quat{ glm::radians(transform.rotate) };
                    glm::vec3 dir{ quat * m_Context->window->camMoveDir };
                    transform.translate += dir;

                    //comp.camera.FOV = m_Context->window->camFOV;
                    if (curMP == prevMP) {
                        m_Context->window->camRot = {};
                        if (m_Context->window->isMiddleClickDown)
                            m_Context->window->camMoveDir = {};
                    }

                    m_Context->renderer->SetCamera(comp.camera, transform);

                    // Cache matrices and camera world position
                    dbgView = comp.camera.View(transform);
                    dbgProj = comp.camera.Projection(m_Context->renderer->Aspect());
                    dbgCamPos = transform.translate;
                    });
                prevMP = curMP;

                {
                    EnttView<Entity, TransformComponent, RigidBodyComponent>([this](auto entity, TransformComponent& tc, RigidBodyComponent& rbc) {
                        // Check if the current scale is different from the stored scale
                        if (tc.transform.scale != rbc.RigidBody.previousScale)
                        {
                            // If it changed, update the collider shape
                            m_Context->physics->UpdateColliderShape(entity);

                            // Then, update the stored scale to the new value for the next frame
                            rbc.RigidBody.previousScale = tc.transform.scale;
                        }
                        });
                }

                //pbr ecs (always render)
                EnttView<Entity, ModelComponent>([this](auto entity, ModelComponent& comp) {
                    static int renderCount = 0;
                    static bool debugModelsPrinted = false;

                    if (!debugModelsPrinted && renderCount < 5) {
                        BOOM_INFO("[Render] Rendering model entity, ModelID: {}, MaterialID: {}",
                            comp.modelID, comp.materialID);
                    }

                    ModelAsset& model{ m_Context->assets->Get<ModelAsset>(comp.modelID) };

                    if (!model.data) {
                        BOOM_ERROR("[Render] Model data is null for ModelID: {} ({})",
                            comp.modelID, model.name);
                        return; // Skip rendering this model
                    }

                    //set animator uniform if model has one
                    if (entity.template Has<AnimatorComponent>()) {
                        AnimatorComponent& an{ entity.template Get<AnimatorComponent>() };
                        // Only animate if not paused
                        float deltaTime = (m_AppState == ApplicationState::RUNNING) ? static_cast<float>(m_Context->DeltaTime) : 0.0f;
                        auto& joints{ an.animator->Animate(deltaTime) };
                        m_Context->renderer->SetJoints(joints);
                    }

                    Transform3D& transform{ entity.template Get<TransformComponent>().transform };

                    if (!debugModelsPrinted && renderCount < 5) {
                        BOOM_INFO("[Render] Transform position: ({}, {}, {}), scale: ({}, {}, {})",
                            transform.translate.x, transform.translate.y, transform.translate.z,
                            transform.scale.x, transform.scale.y, transform.scale.z);
                    }

                    //draw model with material if it has one
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

                        m_Context->renderer->Draw(model.data, transform, material.data);
                    }

                    renderCount++;
                    if (renderCount >= 5) debugModelsPrinted = true;
                    });

                // =========================================================================
                // CORRECTED SECTION: Physics debug drawing is now here, outside the model loop
                // =========================================================================
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
                            m_DebugLinesShader->Draw(dbgView, dbgProj, filtered, 1.5f);
                    }
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
                //update layers only when running
                if (m_AppState == ApplicationState::RUNNING)
                {
                    // Run physics simulation only when running
                    // NOTE: I noticed you call RunPhysicsSimulation twice, I've left it as is,
                    // but you might want to ensure it only runs once per frame.
                    RunPhysicsSimulation();
                }

                m_Context->profiler.End("Total Frame");
                m_Context->profiler.EndFrame();
            }
        }
        



        /**
     * @brief Saves the current scene and assets to files
     * @param sceneName The name of the scene (without extension)
     * @param scenePath Optional custom path for scene files (defaults to "Scenes/")
     * @return true if save was successful, false otherwise
     */
        BOOM_INLINE bool SaveScene(const std::string& sceneName, const std::string& scenePath = "Scenes/")
        {
            //Try blocks cause crashed in release mode. Need to find new alternative
            DataSerializer serializer;

            const std::string sceneFilePath = scenePath + sceneName + ".yaml";
            const std::string assetsFilePath = scenePath + sceneName + "_assets.yaml";

            BOOM_INFO("[Scene] Saving scene '{}' to '{}'", sceneName, sceneFilePath);

            // Serialize scene and assets
            serializer.Serialize(m_Context->scene, sceneFilePath);
            serializer.Serialize(*m_Context->assets, assetsFilePath);

            // Update current scene tracking
            strncpy_s(m_CurrentScenePath, sizeof(m_CurrentScenePath), sceneFilePath.c_str(), _TRUNCATE);

            BOOM_INFO("[Scene] Successfully saved scene '{}' and assets", sceneName);
            return true;
        }

        /**
         * @brief Loads a scene from files, replacing the current scene
         * @param sceneName The name of the scene (without extension)
         * @param scenePath Optional custom path for scene files (defaults to "Scenes/")
         * @return true if load was successful, false otherwise
         */
        BOOM_INLINE bool LoadScene(const std::string& sceneName, const std::string& scenePath = "Scenes/")
        {
            DataSerializer serializer;

            const std::string sceneFilePath = scenePath + sceneName + ".yaml";
            const std::string assetsFilePath = scenePath + sceneName + "_assets.yaml";

            BOOM_INFO("[Scene] Loading scene '{}' from '{}'", sceneName, sceneFilePath);

            // Clean up current scene
            CleanupCurrentScene();

            // Load assets first
            BOOM_INFO("[Scene] Loading assets...");
            serializer.Deserialize(*m_Context->assets, assetsFilePath);

            // Then load scene
            BOOM_INFO("[Scene] Loading scene data...");
            serializer.Deserialize(m_Context->scene, *m_Context->assets, sceneFilePath);

            // Update tracking
            strncpy_s(m_CurrentScenePath, sizeof(m_CurrentScenePath), sceneFilePath.c_str(), _TRUNCATE);
            m_SceneLoaded = true;

            // Reinitialize systems that need it
            ReinitializeSceneSystems();

            BOOM_INFO("[Scene] Successfully loaded scene '{}'", sceneName);
            return true;
        }


        /**
         * @brief Creates a new empty scene
         * @param sceneName Optional name for the new scene
         */
        BOOM_INLINE void NewScene(const std::string& sceneName = "NewScene")
        {
            BOOM_INFO("[Scene] Creating new scene '{}'", sceneName);

            // Clean up current scene
            CleanupCurrentScene();

            // Create basic scene with camera
            CreateDefaultScene();

            m_CurrentScenePath[0] = '\0'; // Clear the path
            m_SceneLoaded = false;

            BOOM_INFO("[Scene] New scene '{}' created", sceneName);
        }

        /**
         * @brief Gets the current scene file path
         */
        BOOM_INLINE std::string GetCurrentScenePath() const { return std::string(m_CurrentScenePath); }

        /**
         * @brief Checks if a scene is currently loaded
         */
        BOOM_INLINE bool IsSceneLoaded() const { return m_SceneLoaded; }

    private:

        std::unique_ptr<Boom::DebugLinesShader> m_DebugLinesShader;
        std::vector<Boom::PhysicsContext::DebugLine> m_PhysLinesCPU;
        char m_CurrentScenePath[512] = "\0";
        bool m_SceneLoaded = false;

        // Toggle: draw only entities that have RigidBodyComponent (+ColliderComponent)
        bool m_DebugRigidBodiesOnly = true; // F10 toggles this at runtime

        /**
        * @brief Cleans up the current scene and physics actors
        */
        BOOM_INLINE void CleanupCurrentScene()
        {
            BOOM_INFO("[Scene] Cleaning up current scene...");

            // Destroy physics actors before clearing scene
            DestroyPhysicsActors();

            // Clear the ECS scene
            m_Context->scene.clear();

            // PRESERVE PREFABS - but only those that exist on disk
            std::unordered_map<AssetID, std::shared_ptr<Asset>> savedPrefabs;
            auto& prefabMap = m_Context->assets->GetMap<PrefabAsset>();
            for (auto& [uid, asset] : prefabMap) {
                if (uid != EMPTY_ASSET) {
                    // Check if the prefab file exists on disk
                    std::string filepath = "Prefabs/" + asset->name + ".prefab";
                    if (std::filesystem::exists(filepath)) {
                        savedPrefabs[uid] = asset;
                    }
                    else {
                        BOOM_INFO("[Scene] Skipping prefab '{}' - file not found on disk", asset->name);
                    }
                }
            }
#if defined(_DEBUG)
            BOOM_INFO("[Scene] Preserved {} prefabs", savedPrefabs.size());
#endif

            // Reset asset registry (keeping EMPTY_ASSET sentinels)
            * m_Context->assets = AssetRegistry();

            // RESTORE PREFABS after registry reset
            for (auto& [uid, asset] : savedPrefabs) {
                m_Context->assets->GetMap<PrefabAsset>()[uid] = std::static_pointer_cast<PrefabAsset>(asset);
            }
#if defined(_DEBUG)
            BOOM_INFO("[Scene] Restored {} prefabs", savedPrefabs.size());
#endif
            // Destroy script instances first
            DestroyScriptInstancesFromScene();

            // Reset any scene-specific state



            BOOM_INFO("[Scene] Scene cleanup complete");
        }

        /**
         * @brief Reinitializes systems after loading a scene
         */
        BOOM_INLINE void ReinitializeSceneSystems()
        {
            BOOM_INFO("[Scene] Reinitializing scene systems...");

            EnttView<Entity, SkyboxComponent>([this](auto, auto& comp) {
                SkyboxAsset& skybox{ m_Context->assets->Get<SkyboxAsset>(comp.skyboxID) };
                m_Context->renderer->InitSkybox(skybox.data, skybox.envMap, skybox.size);
                BOOM_INFO("[Scene] Reinitialized skybox");
                });

            // Only reinitialize physics
            EnttView<Entity, RigidBodyComponent>([this](auto entity, auto&) {
                m_Context->physics->AddRigidBody(entity, *m_Context->assets);
                });

            // Recreate script instances for newly loaded entities
            CreateScriptInstancesFromScene();

            BOOM_INFO("[Scene] Scene systems reinitialization complete");
        }

        /**
         * @brief Creates a minimal default scene with camera
         */
        BOOM_INLINE void CreateDefaultScene()
        {
            BOOM_INFO("[Scene] Creating default scene...");

            // Create basic camera entity
            Entity camera{ &m_Context->scene };
            camera.Attach<InfoComponent>();
            camera.Attach<TransformComponent>();
            camera.Attach<CameraComponent>();

            BOOM_INFO("[Scene] Default scene created with camera");
        }

        BOOM_INLINE void RegisterEventCallbacks()
        {
            // Set physics event callback (mark unused param to avoid warnings)
            m_Context->physics->SetEventCallback([this](auto e)
                {
                    (void)e;
                    // Scripting/event logic can be added here
                });

            // Attach window resize event callback
            AttachCallback<WindowResizeEvent>([this](auto e)
                {
                    m_Context->renderer->Resize(e.width, e.height);
                });
        }
        BOOM_INLINE void ComputeFrameDeltaTime()
        {
            static double sLastTime = glfwGetTime();
            double currentTime = glfwGetTime();

            // Calculate raw delta time
            double rawDelta = (currentTime - sLastTime);

            // Only update delta time if not paused
            if (m_AppState == ApplicationState::RUNNING) {
                m_Context->DeltaTime = rawDelta;
            }
            else {
                m_Context->DeltaTime = 0.0; // No time progression when paused
            }

            sLastTime = currentTime;
        }

        BOOM_INLINE void ResetSphere()
        {
            EnttView<Entity, InfoComponent, TransformComponent, RigidBodyComponent>(
                [this](auto entity, InfoComponent& info, TransformComponent& transform, RigidBodyComponent& rb)
                {
                    (void)entity;
                    if (info.name != "Sphere") return;

                    auto* dyn = rb.RigidBody.actor->is<physx::PxRigidDynamic>();
                    if (!dyn) return;

                    const physx::PxVec3 p(
                        m_SphereInitialPosition.x,
                        m_SphereInitialPosition.y,
                        m_SphereInitialPosition.z
                    );

                    const physx::PxQuat q(0.f, 0.f, 0.f, 1.f); // identity quaternion

                    const physx::PxTransform pose(p, q);
                    dyn->setGlobalPose(pose);
                    dyn->setLinearVelocity(physx::PxVec3(2.f, 0.f, 0.f));
                    dyn->setAngularVelocity(physx::PxVec3(0.f, 0.f, 0.f));

                    // Mirror to ECS transform immediately (prevents 1-frame hitch)
                    transform.transform.translate = m_SphereInitialPosition;
                    transform.transform.rotate = glm::vec3(0.0f);
                });
        }

        BOOM_INLINE void DestroyPhysicsActors()
        {
            EnttView<Entity, RigidBodyComponent>([this](auto entity, auto& comp)
                {
                    if (entity.template Has<ColliderComponent>())
                    {
                        auto& collider = entity.template Get<ColliderComponent>().Collider;
                        collider.material->release();
                        collider.Shape->release();
                    }
                    // Destroy actor user data
                    EntityID* owner = static_cast<EntityID*>(comp.RigidBody.actor->userData);
                    BOOM_DELETE(owner);
                    // Destroy actor instance
                    comp.RigidBody.actor->release();
                });
        }

        BOOM_INLINE void RunPhysicsSimulation()
        {
            // Only simulate physics if running
            if (m_AppState == ApplicationState::RUNNING)
            {

                m_Context->physics->Simulate(1, static_cast<float>(m_Context->DeltaTime));
                EnttView<Entity, RigidBodyComponent>([this](auto entity, auto& comp)
                    {
                        auto& transform = entity.template Get<TransformComponent>().transform;
                        auto pose = comp.RigidBody.actor->getGlobalPose();
                        if (comp.RigidBody.actor->is<physx::PxRigidDynamic>()) {
                            glm::quat rot(pose.q.w, pose.q.x, pose.q.y, pose.q.z); // This is CORRECT
                            transform.rotate = glm::degrees(glm::eulerAngles(rot));
                            transform.translate = PxToVec3(pose.p);
                        }
                    });
            }
        }


        BOOM_INLINE void CreateScriptInstancesFromScene()
        {
            EnttView<Entity, ScriptComponent>([this](auto entity, ScriptComponent& sc) {
                if (sc.TypeName.empty()) return;
                // Create a managed instance and remember its ID on the component
                sc.InstanceId = script_create_instance(sc.TypeName.c_str(), static_cast<ScriptEntityId>(entity.ID()));
                BOOM_INFO("[Scripting] Created instance '{}' -> entt {}",
                    sc.TypeName,
                    static_cast<uint32_t>(entity.ID()));
                });
        }

        BOOM_INLINE void DestroyScriptInstancesFromScene()
        {
            EnttView<Entity, ScriptComponent>([this](auto, ScriptComponent& sc) {
                if (sc.InstanceId) {
                    script_destroy_instance(sc.InstanceId);
                    sc.InstanceId = 0;
                }
                });
        }

        BOOM_INLINE static glm::mat4 PxToGlm(const physx::PxTransform& t)
        {
            // GLM expects (w,x,y,z) ctor, PhysX stores (x,y,z,w)
            glm::quat q(t.q.w, t.q.x, t.q.y, t.q.z);
            glm::mat4 m = glm::mat4_cast(q);
            m[3] = glm::vec4(t.p.x, t.p.y, t.p.z, 1.0f);
            return m;
        }

        BOOM_INLINE static void AppendLine(std::vector<Boom::LineVert>& out,
            const glm::vec3& a, const glm::vec3& b,
            const glm::vec4& cA, const glm::vec4& cB)
        {
            out.push_back(Boom::LineVert{ a, cA });
            out.push_back(Boom::LineVert{ b, cB });
        }

        BOOM_INLINE static void AppendBoxWire(const physx::PxBoxGeometry& g,
            const physx::PxTransform& world,
            std::vector<Boom::LineVert>& out,
            const glm::vec4& color)
        {
            const glm::vec3 he(g.halfExtents.x, g.halfExtents.y, g.halfExtents.z);
            const glm::mat4 M = PxToGlm(world);

            const glm::vec3 c[8] = {
                {-he.x, -he.y, -he.z}, { he.x, -he.y, -he.z},
                { he.x,  he.y, -he.z}, {-he.x,  he.y, -he.z},
                {-he.x, -he.y,  he.z}, { he.x, -he.y,  he.z},
                { he.x,  he.y,  he.z}, {-he.x,  he.y,  he.z}
            };
            auto X = [&](glm::vec3 p) { return glm::vec3(M * glm::vec4(p, 1)); };

            const int e[12][2] = {
                {0,1},{1,2},{2,3},{3,0},
                {4,5},{5,6},{6,7},{7,4},
                {0,4},{1,5},{2,6},{3,7}
            };
            for (auto& pair : e)
                AppendLine(out, X(c[pair[0]]), X(c[pair[1]]), color, color);
        }

        BOOM_INLINE static void AppendCircle(const glm::mat4& M, float r,
            int segments, int axis, // 0=X,1=Y,2=Z
            float yOffset,
            std::vector<Boom::LineVert>& out,
            const glm::vec4& color)
        {
            auto P = [&](float a)->glm::vec3 {
                float s = sinf(a), c = cosf(a);
                glm::vec3 p;
                if (axis == 0)      p = glm::vec3(0, r * c, r * s);
                else if (axis == 1) p = glm::vec3(r * c, 0, r * s);
                else                p = glm::vec3(r * c, r * s, 0);
                p.y += (axis == 1 ? 0.0f : yOffset);
                return glm::vec3(M * glm::vec4(p, 1));
                };
            const float step = glm::two_pi<float>() / (float)segments;
            for (int i = 0; i < segments; ++i) {
                glm::vec3 a = P(i * step);
                glm::vec3 b = P((i + 1) * step);
                AppendLine(out, a, b, color, color);
            }
        }

        BOOM_INLINE static void AppendSphereWire(float radius,
            const physx::PxTransform& world,
            std::vector<Boom::LineVert>& out,
            const glm::vec4& color)
        {
            const glm::mat4 M = PxToGlm(world);
            const int seg = 24;
            // 3 great circles
            AppendCircle(M, radius, seg, 0, 0.0f, out, color); // YZ plane
            AppendCircle(M, radius, seg, 1, 0.0f, out, color); // XZ plane
            AppendCircle(M, radius, seg, 2, 0.0f, out, color); // XY plane
        }

        BOOM_INLINE static void AppendSemiCircle(const glm::mat4& M, float r,
            int segments, int axis, // 0=X,1=Y,2=Z
            bool positiveHalf,      // which half to draw
            std::vector<Boom::LineVert>& out,
            const glm::vec4& color)
        {
            auto P = [&](float a)->glm::vec3 {
                float s = sinf(a), c = cosf(a);
                glm::vec3 p;
                if (axis == 0)      p = glm::vec3(0, r * c, r * s); // YZ plane circle
                else if (axis == 1) p = glm::vec3(r * c, 0, r * s); // XZ plane circle
                else                p = glm::vec3(r * c, r * s, 0); // XY plane circle
                return glm::vec3(M * glm::vec4(p, 1));
                };

            const float step = glm::pi<float>() / (float)segments; // Step over 180 degrees
            const float offset = positiveHalf ? 0.0f : glm::pi<float>();

            for (int i = 0; i < segments; ++i) {
                glm::vec3 a = P(offset + i * step);
                glm::vec3 b = P(offset + (i + 1) * step);
                AppendLine(out, a, b, color, color);
            }
        }

        BOOM_INLINE static void AppendCapsuleWire(float radius, float halfHeight,
            const physx::PxTransform& world,
            std::vector<Boom::LineVert>& out,
            const glm::vec4& color)
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

        BOOM_INLINE void DrawRigidBodiesDebugOnly(const glm::mat4& view, const glm::mat4& proj)
        {
            if (!m_DebugLinesShader) return;

            std::vector<Boom::LineVert> verts;
            verts.reserve(1024);
            const glm::vec4 color(0.1f, 1.0f, 0.1f, 1.0f);

            EnttView<Entity, RigidBodyComponent>([&](auto entity, RigidBodyComponent& rb) {
                if (!entity.template Has<ColliderComponent>()) return;

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
                m_DebugLinesShader->Draw(view, proj, verts, 1.5f);
        }

        BOOM_INLINE static float DistancePointSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
        {
            const glm::vec3 ab = b - a;
            const float ab2 = glm::dot(ab, ab);
            if (ab2 <= 1e-6f) return glm::distance(p, a);
            const float t = glm::clamp(glm::dot(p - a, ab) / ab2, 0.0f, 1.0f);
            const glm::vec3 closest = a + t * ab;
            return glm::distance(p, closest);
        }
    };

}

#endif // !APPLICATION_H
