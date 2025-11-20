#pragma once
#ifndef APPLICATION_H
#define APPLICATION_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#undef APIENTRY
#include <Windows.h>

#endif

#include "Interface.h"
#include "ECS/ECS.hpp"
#include "Physics/Context.h"
#include "Audio/Audio.hpp"   
#include "Auxiliaries/DataSerializer.h"
#include "Auxiliaries/PrefabUtility.h"
#include "../Graphics/Utilities/Culling.h"
#include "Input/CameraManager.h"
#include "Graphics/Shaders/DebugLines.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "Scripting/MonoRuntime.h"
#include "Scripting/ScriptingSystem.h"
#include "Scripting/ScriptBinding.h"

#include "AI/GridChaseAI.h"
#include "AI/DetourNavSystem.h"
#include "AI/NavAgent.h"
#include "AI/AISystem.h"


namespace Boom {

    BOOM_INLINE void DecomposeMatrix(const glm::mat4& matrix, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale)
    {
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;

        glm::decompose(matrix, scale, orientation, translation, skew, perspective);
        rotation = glm::degrees(glm::eulerAngles(orientation));
    }

    inline glm::mat4 RecomposeMatrix(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
    {
        glm::mat4 matrix = glm::translate(glm::mat4(1.0f), translation);
        matrix *= glm::mat4_cast(glm::quat(glm::radians(rotation)));
        matrix = glm::scale(matrix, scale);
        return matrix;
    }
   
}

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
                // fn(EntityType{ &m_Context->scene, e }, m_Context->scene.get<Components>(e)...);
                EntityType entity{ &m_Context->scene, e };
                fn(entity, m_Context->scene.get<Components>(e)...);
            }
        }


        // Application state management
        ApplicationState m_AppState = ApplicationState::RUNNING;
        double m_PausedTime = 0.0;  // Track time spent paused
        double m_LastPauseTime = 0.0;  // When the last pause started
        bool m_ShouldExit = false;  // Flag for graceful shutdown
        float m_TestRot = 0.0f;

        bool m_PhysDebugViz = true;

        // --- Mono State ---
        MonoDomain* m_MonoRootDomain = nullptr;
        MonoDomain* m_MonoAppDomain = nullptr;
        MonoAssembly* m_GameAssembly = nullptr;
        MonoImage* m_GameImage = nullptr;
        std::string      m_MonoBase;       // e.g. "<EditorRoot>/Mono"
        std::string      m_AssembliesPath; // e.g. "<EditorRoot>/Scripts/bin/x64/Debug"


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

            m_Context->app = this;

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
            ShutdownMonoRuntime();
            BOOM_DELETE(m_Context);
            //called here in case of the need of multiple windows
            glfwTerminate();
        }

        BOOM_API void RunContext(bool showFrame = false);

        void RenderScene();

        glm::mat4 GetWorldMatrix(Entity& entity);

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
        * 
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
            //const std::string assetsFilePath = scenePath + sceneName + "_assets.yaml";

            BOOM_INFO("[Scene] Saving scene '{}' to '{}'", sceneName, sceneFilePath);

            // Serialize scene and assets
            serializer.Serialize(m_Context->scene, sceneFilePath);
            //serializer.Serialize(*m_Context->assets, assetsFilePath);

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
            //const std::string assetsFilePath = scenePath + sceneName + "_assets.yaml";

            BOOM_INFO("[Scene] Loading scene '{}' from '{}'", sceneName, sceneFilePath);

            // Clean up current scene
            CleanupCurrentScene();

            // Then load scene
            BOOM_INFO("[Scene] Loading scene data...");
            serializer.Deserialize(m_Context->scene, *m_Context->assets, sceneFilePath);

            // Update tracking
            strncpy_s(m_CurrentScenePath, sizeof(m_CurrentScenePath), sceneFilePath.c_str(), _TRUNCATE);
            m_SceneLoaded = true;

            // Reinitialize systems that need it
            ReinitializeSceneSystems();

            InvokeStaticVoid("GameScripts", "Entry", "Start");

            BOOM_INFO("[Scene] Successfully loaded scene '{}'", sceneName);
            return true;
        }


        BOOM_INLINE bool LoadSceneAdditive(const std::string& sceneName, const std::string& scenePath = "Scenes/")
        {
            DataSerializer serializer;
            const std::string sceneFilePath = scenePath + sceneName + ".yaml";
            BOOM_INFO("[Scene] Additively loading scene '{}'", sceneName, sceneFilePath);

            // Deserialize new entities and components into the EXISTING scene
            serializer.Deserialize(m_Context->scene, *m_Context->assets, sceneFilePath);

            // Reinitialize systems, but ONLY for the newly loaded objects.
            BOOM_INFO("[Scene] Initializing physics for additive objects...");
            auto view = m_Context->scene.view<PauseMenuTagComponent, RigidBodyComponent>();
            for (auto e : view) {
                // Check if it's already been added
                auto& rb = view.get<RigidBodyComponent>(e);
                if (!rb.RigidBody.actor) {
                    Boom::Entity entity{ &m_Context->scene, e };
                    m_Context->physics->AddRigidBody(entity, *m_Context->assets);
                }
            }

            InvokeStaticVoid("GameScripts", "Entry", "Start");

            BOOM_INFO("[Scene] Successfully added scene '{}'", sceneName);
            return true;
        }

        template<typename TagComponent>
        BOOM_INLINE void UnloadAdditiveScene()
        {
            BOOM_INFO("[Scene] Unloading additive scene with tag...");
            auto& reg = m_Context->scene;
            auto view = reg.view<TagComponent>();

            for (auto e : view) {
                if (reg.all_of<RigidBodyComponent>(e)) {
                    auto& rb = reg.get<RigidBodyComponent>(e);
                    if (rb.RigidBody.actor) {
                        rb.RigidBody.actor->release();
                        rb.RigidBody.actor = nullptr;
                    }
                }
            }
            reg.destroy(view.begin(), view.end());
            BOOM_INFO("[Scene] Unload complete.");
        }

        BOOM_INLINE void LightsUpdate() {
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
        }

        BOOM_INLINE void RenderShadowScene() {
            //building shadows
            EnttView<Entity, DirectLightComponent, TransformComponent>(
                [this](auto, DirectLightComponent&, TransformComponent& tc)
                {
                    // light direction
                    auto& lightDir = tc.transform.rotate;
                    m_Context->renderer->BeginShadowPass(lightDir);

                    EnttView<Entity, ModelComponent>([this](auto entity, ModelComponent& comp) {
                        //ignore lights and non initialized models
                        if (!entity.Has<ModelComponent>() || comp.modelID == EMPTY_ASSET) return;
                        if (entity.Has<DirectLightComponent>() || entity.Has<PointLightComponent>() || entity.Has<SpotLightComponent>()) return;

                        ModelAsset& model{ m_Context->assets->Get<ModelAsset>(comp.modelID) };
                        std::vector<glm::mat4> joints;
                        if (entity.Has<AnimatorComponent>()) {
                            auto& an = entity.Get<AnimatorComponent>();
                            joints = an.animator->Animate(0); //dont update animation here
                        }
                        else if (model.hasJoints) {
                            static std::vector<glm::mat4> identityPalette(100, glm::mat4(1.0f));
                            joints = identityPalette;
                        }

                        glm::mat4 worldMatrix = GetWorldMatrix(entity);
                        Transform3D worldTransform;
                        DecomposeMatrix(worldMatrix, worldTransform.translate, worldTransform.rotate, worldTransform.scale);

                        m_Context->renderer->DrawShadow(model.data, worldTransform, joints);
                    });
            
                    m_Context->renderer->EndShadowPass();
                });
        }
        

        /**
         * @brief Creates a new empty scene
         * @param sceneName Optional name for the new scene
         */
        BOOM_INLINE void NewScene(const std::string& sceneName = "NewScene")
        {
            BOOM_INFO("[Scene] Creating new scene '{}'", sceneName);

			LoadScene("templateScene");

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

        BOOM_INLINE void UpdateKinematicTransforms()
        {
            EnttView<Entity, RigidBodyComponent>(
                [this](auto entity, RigidBodyComponent& rb)
                {
                    if (rb.RigidBody.type == RigidBody3D::Type::KINEMATIC) {
                        auto* actor = rb.RigidBody.actor;
                        if (!actor) return;

                        // Get the FINAL world matrix of the physics body,
                        // no matter how deep it is in the hierarchy.
                        glm::mat4 worldMatrix = GetWorldMatrix(entity);

                        // Decompose the world matrix to get the final T and R
                        glm::vec3 worldTranslate, worldRotate, worldScale;
                        DecomposeMatrix(worldMatrix, worldTranslate, worldRotate, worldScale);
                        // --- END FIX ---

                        physx::PxTransform currentPose = actor->getGlobalPose();

                        // Convert new world transform to PhysX
                        glm::quat rotQuat = glm::quat(glm::radians(worldRotate));

                        physx::PxVec3 newPos(worldTranslate.x, worldTranslate.y, worldTranslate.z);
                        physx::PxQuat newRot(rotQuat.x, rotQuat.y, rotQuat.z, rotQuat.w);

                        if (currentPose.p != newPos || currentPose.q != newRot)
                        {
                            actor->setGlobalPose(physx::PxTransform(newPos, newRot));
                        }
                    }
                });
        }

        BOOM_INLINE DetourNavSystem* GetNavSystem() override { return m_Nav.get(); }

        BOOM_INLINE const DetourNavSystem* GetNavSystem() const override { return m_Nav.get(); }

        BOOM_INLINE static void AppendLine(std::vector<Boom::LineVert>& out, const glm::vec3& a, const glm::vec3& b, const glm::vec4& cA, const glm::vec4& cB)
        {
            out.push_back(Boom::LineVert{ a, cA });
            out.push_back(Boom::LineVert{ b, cB });
        }

        BOOM_INLINE void SetPreviousScenePath(const std::string& path)
        {
            strncpy_s(m_PreviousScenePath, 512, path.c_str(), _TRUNCATE);
        }

        BOOM_INLINE std::string GetPreviousScenePath() const
        {
            return std::string(m_PreviousScenePath);
        }

        BOOM_INLINE void SetCurrentScenePath(const std::string& path)
        {
            strncpy_s(m_CurrentScenePath, 512, path.c_str(), _TRUNCATE);
        }

    private:
        std::unordered_map<std::string, std::pair<glm::vec3, glm::vec3>> m_SphereInitialStates;

        glm::vec3 pivotPosition{};
		bool m_NavInitialized = false;
        bool m_AIinitialized = false;
        std::unique_ptr<Boom::DebugLinesShader> m_DebugLinesShader;
        std::vector<Boom::PhysicsContext::DebugLine> m_PhysLinesCPU;
        char m_CurrentScenePath[512] = "\0";
        char m_PreviousScenePath[512] = "\0";
        bool m_SceneLoaded = false;
        std::unique_ptr<DetourNavSystem> m_Nav;
       
        Boom::AISystem                         m_AIagents;
        Boom::NavAgentSystem                   m_NavAgents;
        entt::entity                           m_PlayerE = entt::null;
        entt::entity                           m_AgentE = entt::null;

        BOOM_INLINE void EnsureNinjaSeeksSamurai()
        {
            auto& reg = m_Context->scene;

            // Find target (player) named "Samurai"
            entt::entity samurai = Boom::FindEntityByName(reg, "Samurai");
            if (samurai == entt::null) {
                BOOM_WARN("[Nav] 'Samurai' not found in scene; Ninja will idle.");
                return;
            }

            // Find or create the seeker named "Ninja"
            entt::entity ninja = Boom::FindEntityByName(reg, "Ninja");
       

            // Ensure Ninja has a NavAgentComponent and configure it to follow Samurai
            auto& nac = reg.get_or_emplace<Boom::NavAgentComponent>(ninja);

            // NOTE: NavAgentComponent wraps NavAgent as 'agent'
            nac.follow = samurai;
            nac.active = true;
            nac.dirty = true;   // force first path build on the next update
            nac.speed = 2.5f;   // tune as you like
            nac.arrive = 0.15f;

            BOOM_INFO("[Nav] 'Ninja' will seek 'Samurai'.");
        }
        
        BOOM_INLINE void InitNavRuntime()
        {
            if (m_NavInitialized) return;

            //auto& reg = m_Context->scene;

            // 1) Build / load navmesh only once
            if (!m_Nav) {
                const char* kNavPath = "Resources/NavData/level1.bin";
                m_Nav = std::make_unique<Boom::DetourNavSystem>();
                if (!m_Nav || !m_Nav->initFromFile(kNavPath)) {
                    BOOM_ERROR("[Nav] Failed to load navmesh: {}", kNavPath);
                    m_Nav.reset();
                    return;
                }
                BOOM_INFO("[Nav] Loaded navmesh.");
            }

            //// 2) Cache the player (if any)
            //m_PlayerE = Boom::FindEntityByName(reg, "Player");
            //if (m_PlayerE == entt::null) {
            //    BOOM_WARN("[Nav] 'Player' not found; agent will idle until one exists.");
            //}

            //// 3) Find an existing agent FIRST (prefer 'NavAgent', then allow 'Enemy' to be used as agent)
            //if (m_AgentE == entt::null || !reg.valid(m_AgentE)) {
            //    entt::entity byName = Boom::FindEntityByName(reg, "NavAgent");
            //    if (byName == entt::null) {
            //        byName = Boom::FindEntityByName(reg, "Enemy"); // optional reuse
            //    }

            //    if (byName != entt::null) {
            //        m_AgentE = byName;
            //    }
            //    else {
            //        // Create a single agent as a sphere
            //        glm::vec3 spawnPos{ 2.f, 1.f, 2.f }; // default
            //        if (auto ground = Boom::FindEntityByName(reg, "ground");
            //            ground != entt::null && reg.all_of<TransformComponent>(ground)) {
            //            const auto& gt = reg.get<TransformComponent>(ground).transform;
            //            spawnPos.y = gt.translate.y + 1.0f; // float above ground
            //        }

            //        // Uses your helper (speed = 2.0f set inside NavAgentComponent)
            //        m_AgentE = CreateEnemySphere(reg, "NavAgent", spawnPos, /*radius*/0.5f);
            //        RegisterSphereInitialState("NavAgent", spawnPos /*, glm::vec3(0)*/);
            //      
            //        BOOM_INFO("[Nav] Spawned 'NavAgent' sphere at ({}, {}, {}), r = {}",
            //            spawnPos.x, spawnPos.y, spawnPos.z, 0.5f);
            //    }
            //}

            //// 4) Ensure required components exist on the chosen agent
            //if (!reg.all_of<Boom::TransformComponent>(m_AgentE))
            //    reg.emplace<Boom::TransformComponent>(m_AgentE);
            //if (!reg.all_of<Boom::NavAgentComponent>(m_AgentE))
            //    reg.emplace<Boom::NavAgentComponent>(m_AgentE);

            //// 5) Configure follow target once
            //if (m_PlayerE != entt::null) {
            //    auto& ag = reg.get<Boom::NavAgentComponent>(m_AgentE);
            //    ag.follow = m_PlayerE;
            //    ag.dirty = true; // force first path build
            //    BOOM_INFO("[Nav] Agent now follows 'Player'.");
            //}

            m_NavInitialized = true;  // ← prevents re-entering
        }

        BOOM_INLINE void SphereInitialState(const std::string& name, const glm::vec3& pos, const glm::vec3& vel = glm::vec3(0.0f)) {
            m_SphereInitialStates[name] = { pos, vel };
        }

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
            //* m_Context->assets = AssetRegistry();


            // RESTORE PREFABS after registry reset
            for (auto& [uid, asset] : savedPrefabs) {
                m_Context->assets->GetMap<PrefabAsset>()[uid] = std::static_pointer_cast<PrefabAsset>(asset);
            }
#if defined(_DEBUG)
            BOOM_INFO("[Scene] Restored {} prefabs", savedPrefabs.size());
#endif

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
                    // Check if this is a contact event
                    if (e.Event == PxEvent::CONTACT)
                    {
                        // Get both entities from the event payload
                        entt::entity ent1 = (entt::entity)e.Entity1;
                        entt::entity ent2 = (entt::entity)e.Entity2;

                        // Safely get the RigidBodyComponent for entity 1 and set its flag
                        if (m_Context->scene.valid(ent1) && m_Context->scene.all_of<RigidBodyComponent>(ent1))
                        {
                            m_Context->scene.get<RigidBodyComponent>(ent1).RigidBody.isColliding = true;
                        }

                        // Safely get the RigidBodyComponent for entity 2 and set its flag
                        if (m_Context->scene.valid(ent2) && m_Context->scene.all_of<RigidBodyComponent>(ent2))
                        {
                            m_Context->scene.get<RigidBodyComponent>(ent2).RigidBody.isColliding = true;
                        }
                    }
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


        //---------------Physics------------------------
        BOOM_API void DestroyPhysicsActors();

        void RunPhysicsSimulation();

        void DrawRigidBodiesDebugOnly(const glm::mat4& view, const glm::mat4& proj);

        static void AppendCapsuleWire(float radius, float halfHeight, const physx::PxTransform& world, std::vector<Boom::LineVert>& out, const glm::vec4& color);

        BOOM_INLINE static glm::mat4 PxToGlm(const physx::PxTransform& t)
        {
            // GLM expects (w,x,y,z) ctor, PhysX stores (x,y,z,w)
            glm::quat q(t.q.w, t.q.x, t.q.y, t.q.z);
            glm::mat4 m = glm::mat4_cast(q);
            m[3] = glm::vec4(t.p.x, t.p.y, t.p.z, 1.0f);
            return m;
        }

        BOOM_INLINE static void AppendBoxWire(const physx::PxBoxGeometry& g, const physx::PxTransform& world, std::vector<Boom::LineVert>& out, const glm::vec4& color)
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

        BOOM_INLINE static void AppendCircle(const glm::mat4& M, float r, int segments, int axis, float yOffset, std::vector<Boom::LineVert>& out, const glm::vec4& color)
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

        BOOM_INLINE static void AppendSphereWire(float radius, const physx::PxTransform& world, std::vector<Boom::LineVert>& out, const glm::vec4& color)
        {
            const glm::mat4 M = PxToGlm(world);
            const int seg = 24;
            // 3 great circles
            AppendCircle(M, radius, seg, 0, 0.0f, out, color); // YZ plane
            AppendCircle(M, radius, seg, 1, 0.0f, out, color); // XZ plane
            AppendCircle(M, radius, seg, 2, 0.0f, out, color); // XY plane
        }

        BOOM_INLINE static void AppendSemiCircle(const glm::mat4& M, float r, int segments, int axis, bool positiveHalf, std::vector<Boom::LineVert>& out, const glm::vec4& color)
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
            const float offset = positiveHalf ? -glm::half_pi<float>() : glm::half_pi<float>();

            for (int i = 0; i < segments; ++i) {
                glm::vec3 a = P(offset + i * step);
                glm::vec3 b = P(offset + (i + 1) * step);
                AppendLine(out, a, b, color, color);
            }
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

        // -- MONO functions -- 
        void UpdateThirdPersonCameras();

        BOOM_INLINE static std::string GetExeDir()
        {
#ifdef _WIN32
            char buf[MAX_PATH]{};
            DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
            if (n == 0 || n == MAX_PATH) return std::filesystem::current_path().string();
            std::filesystem::path p(buf);
            return p.parent_path().string();
#else
            // Fallback for non-Windows platforms
            return std::filesystem::current_path().string();
#endif
        }

        BOOM_INLINE bool InitMonoRuntime(const std::string& monoBaseDir,
            const std::string& assembliesDir,
            const char* domainName = "BoomDomain")
        {
            // 0) sanity on folders
            if (!std::filesystem::exists(monoBaseDir) ||
                !std::filesystem::exists(monoBaseDir + "/lib") ||
                !std::filesystem::exists(monoBaseDir + "/etc"))
            {
                
#ifdef _DEBUG
                BOOM_ERROR("[Mono] Invalid mono base folder: '{}'", monoBaseDir);

#endif // DEBUG
                return false;
            }
            if (!std::filesystem::exists(assembliesDir))
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] Assemblies folder not found: '{}'", assembliesDir);

#endif // DEBUG
                
                return false;
            }

            m_MonoBase = monoBaseDir;
            m_AssembliesPath = assembliesDir;

            // 1) point Mono at its runtime folders
            //    On Windows, make sure the Mono DLLs (e.g., mono-2.0-sgen.dll) are next to the EXE or in PATH.
            mono_set_dirs((m_MonoBase + "/lib").c_str(),
                (m_MonoBase + "/etc").c_str());

            // 2) config (enables machine.config etc.)
            //mono_config_parse(nullptr);

            // 3) assembly search paths (so "GameScripts.dll" can be found by name)
            mono_set_assemblies_path(m_AssembliesPath.c_str());

            // 4) root domain
            m_MonoRootDomain = mono_jit_init_version(domainName, "v4.0.30319");
            if (!m_MonoRootDomain)
            {
                BOOM_ERROR("[Mono] mono_jit_init_version failed.");
                return false;
            }

            // 5) create a child/app domain (optional but recommended for unload/reload patterns)
            m_MonoAppDomain = mono_domain_create_appdomain(const_cast<char*>("BoomAppDomain"), nullptr);
            if (!m_MonoAppDomain)
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] mono_domain_create_appdomain failed.");

#endif // DEBUG
                return false;
            }
            mono_domain_set(m_MonoAppDomain, /* force */ false);
#ifdef _DEBUG
            BOOM_INFO("[Mono] Initialized. Base='{}', Assemblies='{}'", m_MonoBase, m_AssembliesPath);
#endif // DEBUG
            return true;
        }

        BOOM_INLINE void ShutdownMonoRuntime()
        {
            if (m_MonoAppDomain)
            {
                // Switch back to root to safely unload app domain
                mono_domain_set(m_MonoRootDomain, false);
                mono_domain_unload(m_MonoAppDomain);
                m_MonoAppDomain = nullptr;
            }
            if (m_MonoRootDomain)
            {
                mono_jit_cleanup(m_MonoRootDomain);
                m_MonoRootDomain = nullptr;
            }
            m_GameAssembly = nullptr;
            m_GameImage = nullptr;

#ifdef _DEBUG
            BOOM_INFO("[Mono] Shutdown complete.");
#endif // DEBUG

        }

        BOOM_INLINE bool LoadGameAssembly(const std::string& dllName /* e.g., "GameScripts.dll" */)
        {
            if (!m_MonoAppDomain)
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] App domain not initialized.");
#endif // DEBUG

                return false;
            }

            const std::string full = (std::filesystem::path(m_AssembliesPath) / dllName).string();
            if (!std::filesystem::exists(full))
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] Assembly not found: {}", full);

#endif // DEBUG
                return false;
            }

            m_GameAssembly = mono_domain_assembly_open(m_MonoAppDomain, full.c_str());
            if (!m_GameAssembly)
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] Failed to load assembly: {}", full);
#endif // DEBUG
                return false;
            }

            m_GameImage = mono_assembly_get_image(m_GameAssembly);
            if (!m_GameImage)
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] mono_assembly_get_image failed.");
#endif // DEBUG
                return false;
            }
#ifdef _DEBUG
            BOOM_INFO("[Mono] Loaded assembly: {}", full);
#endif // DEBUG
            return true;
        }

        BOOM_INLINE bool InvokeStaticVoid(const char* nsName,
            const char* className,
            const char* methodName,
            void** args = nullptr)
        {
            if (!m_GameImage) { BOOM_ERROR("[Mono] No assembly image loaded."); return false; }

            MonoClass* klass = mono_class_from_name(m_GameImage, nsName, className);
            if (!klass) { BOOM_ERROR("[Mono] Class not found: {}.{}", nsName, className); return false; }

            MonoMethod* method = mono_class_get_method_from_name(klass, methodName, /*param_count*/ 0);
            if (!method) { BOOM_ERROR("[Mono] Method not found: {}.{}", className, methodName); return false; }

            MonoObject* exc = nullptr;
            mono_runtime_invoke(method, /*this*/ nullptr, args, &exc);
            if (exc)
            {
                // print exception
                MonoString* s = mono_object_to_string(exc, nullptr);
                char* utf8 = mono_string_to_utf8(s);
#ifdef _DEBUG
                BOOM_ERROR("[Mono] Exception: {}", utf8 ? utf8 : "(null)");
#endif // DEBUG
                if (utf8) mono_free(utf8);
                return false;
            }
            return true;
        }

        BOOM_INLINE bool InvokeStatic1Float(const char* nsName,
            const char* className,
            const char* methodName,
            float value)
        {
            if (!m_GameImage) { BOOM_ERROR("[Mono] No assembly image loaded."); return false; }

            MonoClass* klass = mono_class_from_name(m_GameImage, nsName, className);
            if (!klass) { BOOM_ERROR("[Mono] Class not found: {}.{}", nsName, className); return false; }

            // method with 1 parameter
            MonoMethod* method = mono_class_get_method_from_name(klass, methodName, 1);
            if (!method) { BOOM_ERROR("[Mono] Method not found: {}.{}", className, methodName); return false; }

            void* args[1];
            args[0] = &value;

            MonoObject* exc = nullptr;
            mono_runtime_invoke(method, nullptr, args, &exc);
            if (exc)
            {
                MonoString* s = mono_object_to_string(exc, nullptr);
                char* utf8 = mono_string_to_utf8(s);
                BOOM_ERROR("[Mono] Exception: {}", utf8 ? utf8 : "(null)");
                if (utf8) mono_free(utf8);
                return false;
            }
            return true;
        }

        BOOM_INLINE void DrawDebugTPC() {
            ModelAsset const* mdl{ m_Context->assets->TryGet<ModelAsset>("Cube.FBX") };
            m_Context->renderer->Draw(mdl->data, Transform3D{ pivotPosition, glm::vec3(0.f), glm::vec3(.2f) });
        }
    };



}

#endif // !APPLICATION_H
