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
#include "Scripting/FileWatcher.h"
#include "AI/GridChaseAI.h"
#include "AI/DetourNavSystem.h"
#include "AI/NavAgent.h"
#include "AI/AISystem.h"
#include "Input/RayCast.h"

namespace std {
	template<>
	struct hash<std::pair<uint32_t, uint32_t>> {
		size_t operator()(const std::pair<uint32_t, uint32_t>& p) const {
			auto h1 = std::hash<uint32_t>{}(p.first);
			auto h2 = std::hash<uint32_t>{}(p.second);
			return h1 ^ (h2 << 1); // Simple hash combination
		}
	};
}

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
		ApplicationState m_AppState = ApplicationState::STOPPED;  // Start in edit mode (like Unity)
		bool m_IsGameLogicPaused = false;
		double m_PausedTime = 0.0;  // Track time spent paused
		double m_LastPauseTime = 0.0;  // When the last pause started
		bool m_ShouldExit = false;  // Flag for graceful shutdown
		float m_TestRot = 0.0f;

		bool m_PhysDebugViz = false;

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
			if (m_Context) {
				for (AppInterface*& layer : m_Context->layers)
				{
					BOOM_DELETE(layer);
				}
				// Clear the vector to prevent the AppContext destructor
				// from trying to delete them a second time.
				m_Context->layers.clear();
			}

			DestroyPhysicsActors();
			BOOM_DELETE(m_Context);
			//called here in case of the need of multiple windows
			glfwTerminate();
		}

		BOOM_API void RunContext(bool showFrame = false);

		void RenderScene(bool isPicking = false);

		/**
		* @brief Enters play mode (like Unity's Play button)
		* Saves the current scene state so it can be restored when Stop is called
		*/
		BOOM_INLINE void Play()
		{
			if (m_IsInPlayMode) {
				BOOM_WARN("[Application] Already in play mode");
				return;
			}

			m_PrePlayScene_OriginalPath = m_CurrentScenePath;

			// Save current scene state to temporary file
			m_PrePlayScenePath = "Scenes/__temp_preplay_state__.yaml";
			DataSerializer serializer;
			serializer.Serialize(m_Context->scene, m_PrePlayScenePath);
			BOOM_INFO("[Application] Saved pre-play scene state");

			// Initialize physics actors for all rigid bodies
			EnttView<Entity, ColliderComponent>([this](auto entity, auto&) {
				// Only add if it DOESN'T have a RigidBodyComponent
				if (!entity.Has<RigidBodyComponent>()) {
					m_Context->physics->AddColliderOnly(entity, *m_Context->assets);
				}
				else {
					// Has both collider and rigidbody - use existing logic
					m_Context->physics->AddRigidBody(entity, *m_Context->assets);
				}
			});

			// Reset time tracking
			m_PausedTime = 0.0;
			m_LastPauseTime = 0.0;

			// Enter play mode
			m_IsInPlayMode = true;
			m_AppState = ApplicationState::RUNNING;
			m_ShouldExit = false;

			if (m_Context->scriptingSystem && m_Context->scriptingSystem->IsAlive()) {
				// Call Entry.Start() first
				m_Context->scriptingSystem->CallStart();

				// Recreate instances for all entities with ScriptComponent
				int scriptsCreated = 0;
				auto& registry = m_Context->scene;
				auto scriptView = registry.view<ScriptComponent>();

				for (auto entity : scriptView) {
					auto& sc = scriptView.get<ScriptComponent>(entity);

					// Reset InstanceId to force recreation
					if (sc.InstanceId == 0 && sc.Enabled && !sc.TypeName.empty()) {
						if (m_Context->scriptingSystem->RecreateForEntity(entity, sc)) {
							scriptsCreated++;
							BOOM_INFO("[Play] Recreated script instance for entity {} (type: {})",
								static_cast<uint32_t>(entity), sc.TypeName);
						}
					}
				}

				if (scriptsCreated > 0) {
					BOOM_INFO("[Play] Recreated {} script instances", scriptsCreated);
				}
			}

			BOOM_INFO("[Application] Entered play mode");
		}

		/**
		* @brief Pauses the application (stops updates but continues rendering)
		*/
		BOOM_INLINE void Pause()
		{
			if (!m_IsInPlayMode) {
				BOOM_WARN("[Application] Cannot pause - not in play mode");
				return;
			}

			if (m_AppState == ApplicationState::RUNNING) {
				m_AppState = ApplicationState::PAUSED;
				m_LastPauseTime = glfwGetTime();
				BOOM_INFO("[Application] Paused");

				// Pause audio if available
				SoundEngine::Instance().PauseAll(true);
			}
		}

		/**
		* @brief Resumes the application from pause
		*/
		BOOM_INLINE void Resume()
		{
			if (!m_IsInPlayMode) {
				BOOM_WARN("[Application] Cannot resume - not in play mode");
				return;
			}

			if (m_AppState == ApplicationState::PAUSED) {
				m_AppState = ApplicationState::RUNNING;

				// Add paused time to total paused time
				m_PausedTime += (glfwGetTime() - m_LastPauseTime);

				BOOM_INFO("[Application] Resumed");

				// Resume audio if available
				SoundEngine::Instance().PauseAll(false);
			}
		}

		/**
		* @brief Stops play mode and restores the scene to pre-play state (like Unity's Stop button)
		*/
		BOOM_INLINE void Stop()
		{
			if (!m_IsInPlayMode) {
				BOOM_WARN("[Application] Not in play mode, nothing to stop");
				return;
			}

			BOOM_INFO("[Application] Stopping play mode...");

			if (m_Context->scriptingSystem && m_Context->scriptingSystem->IsAlive()) {
				auto& registry = m_Context->scene;
				auto scriptView = registry.view<ScriptComponent>();

				int scriptsDestroyed = 0;
				for (auto entity : scriptView) {
					auto& sc = scriptView.get<ScriptComponent>(entity);

					if (sc.InstanceId != 0) {
						m_Context->scriptingSystem->DestroyForEntity(entity, sc);
						scriptsDestroyed++;
					}
				}

				if (scriptsDestroyed > 0) {
					BOOM_INFO("[Stop] Destroyed {} script instances", scriptsDestroyed);
				}
			}

			// Destroy all physics actors before restoring scene
			DestroyPhysicsActors();

			// Restore pre-play scene state
			if (!m_PrePlayScenePath.empty() && std::filesystem::exists(m_PrePlayScenePath)) {
				DataSerializer serializer;

				// Clear the current scene
				m_Context->scene.clear();

				// Deserialize the saved state
				serializer.Deserialize(m_Context->scene, *m_Context->assets, m_PrePlayScenePath);

				// Restore the original scene path
				strncpy_s(m_CurrentScenePath, sizeof(m_CurrentScenePath), m_PrePlayScene_OriginalPath.c_str(), _TRUNCATE);
				m_PrePlayScene_OriginalPath.clear();
				m_SceneLoaded = (m_CurrentScenePath[0] != '\0'); // Update scene loaded status

				// Delete the temporary file
				std::filesystem::remove(m_PrePlayScenePath);
				m_PrePlayScenePath.clear();

				BOOM_INFO("[Application] Restored pre-play scene state");
			}

			// Reinitialize systems (skybox, etc.) but NOT physics
			EnttView<Entity, SkyboxComponent>([this](auto, auto& comp) {
				SkyboxAsset& skybox{ m_Context->assets->Get<SkyboxAsset>(comp.skyboxID) };
				m_Context->renderer->InitSkybox(skybox.data, skybox.envMap, skybox.size);
				});

			// Reset time tracking
			m_IsGameLogicPaused = false;
			m_PausedTime = 0.0;
			m_LastPauseTime = 0.0;

			// Exit play mode but keep application running
			m_IsInPlayMode = false;
			m_AppState = ApplicationState::STOPPED;
			// NOTE: m_ShouldExit stays false - we don't exit the app!

			// Stop audio
			SoundEngine::Instance().StopAll();

			BOOM_INFO("[Application] Exited play mode");
		}

		/**
		* @brief Exits the application completely
		*/
		BOOM_INLINE void Exit()
		{
			m_ShouldExit = true;
			BOOM_INFO("[Application] Exiting application...");
		}

		/**
		* @brief Toggles between pause and resume (only works in play mode)
		*/
		BOOM_INLINE void TogglePause()
		{
			if (!m_IsInPlayMode) {
				BOOM_WARN("[Application] Cannot toggle pause - not in play mode");
				return;
			}

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

		// Set application state to pause
		BOOM_INLINE void SetGameLogicPaused(bool paused) { m_IsGameLogicPaused = paused; }

		/**
		* @brief Checks if the application is currently in play mode
		*/
		BOOM_INLINE bool IsPlaying() const { return m_IsInPlayMode; }

		/**
		* @brief Checks if the application is paused (in play mode but not updating)
		*/
		BOOM_INLINE bool IsPaused() const { return m_IsInPlayMode && m_AppState == ApplicationState::PAUSED; }

		BOOM_INLINE bool IsInGamePauseMenuLoaded() const
		{
			if (!m_Context) return false;
			auto view = m_Context->scene.view<PauseMenuTagComponent>();
			return !view.empty();
		}

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

			BOOM_INFO("[Scene] Successfully saved scene '{}' and assets", sceneName);
			return true;
		}

		BOOM_INLINE void ApplySceneNavmeshFromScene()
		{
			if (!m_Context)
				return;

			auto& reg = m_Context->scene;

			// Find the settings entity
			entt::entity settings = Boom::TryGetSceneSettings(reg);
			if (settings == entt::null)
			{
				BOOM_INFO("[Nav] No SceneNavmeshComponent in this scene; skipping navmesh load");
				m_NavInitialized = false;
				return;
			}

			auto& sn = reg.get<SceneNavmeshComponent>(settings);

			if (sn.navmeshFile.empty())
			{
				BOOM_INFO("[Nav] SceneNavmeshComponent.navmeshFile is empty; skipping navmesh load");
				m_NavInitialized = false;
				return;
			}

			// Ensure nav system exists
			if (!m_Nav)
				m_Nav = std::make_unique<DetourNavSystem>();

			const std::string& path = sn.navmeshFile;
			bool ok = m_Nav->reloadFromFile(path.c_str());
			if (ok)
			{
				BOOM_INFO("[Nav] Scene navmesh loaded from '{}'", path);
				m_NavInitialized = true;
			}
			else
			{
				BOOM_ERROR("[Nav] Failed to load scene navmesh from '{}'", path);
				m_NavInitialized = false;
			}
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

			BOOM_INFO("[Scene] Loading scene '{}' from '{}'", sceneName, sceneFilePath);

			// Clean up current scene
			CleanupCurrentScene();

			// CRITICAL: Load all assets from assets.yaml BEFORE loading the scene
			// This ensures textures, models, etc. are available when scene references them
			//BOOM_INFO("[Scene] Loading assets from Resources/assets.yaml");
			//serializer.DeserializeAsync(*m_Context->assets, "Resources/assets.yaml", m_Context->window->Handle().get());
			serializer.Deserialize(m_Context->scene, *m_Context->assets, sceneFilePath);

			// *** ADD THIS LINE - Clear all trigger callbacks before loading new scene ***
			Boom::ClearAllTriggerCallbacks();

			//serializer.Deserialize(m_Context->scene, *m_Context->assets, sceneFilePath);

			// Update tracking
			strncpy_s(m_CurrentScenePath, sizeof(m_CurrentScenePath), sceneFilePath.c_str(), _TRUNCATE);
			m_SceneLoaded = true;

			// Reinitialize systems that need it
			ReinitializeSceneSystems();
			m_NavInitialized = false;
			ApplySceneNavmeshFromScene();
			BOOM_INFO("[Scene] Successfully loaded scene '{}'", sceneName);
			return true;
		}


		BOOM_INLINE bool LoadSceneAdditive(const std::string& sceneName, const std::string& scenePath = "Scenes/")
		{
			// --- MODIFIED ---
			// This function now *only* loads from file if objects aren't already in the scene.
			// It *always* leaves the objects in a deactivated state.

			BOOM_INFO("[Scene] Checking for existing objects for: {}", sceneName.c_str());
			auto& reg = m_Context->scene;

			// 1. Check if objects *already exist* (activated or deactivated)
			// We assume PauseMenuTagComponent as that's what we're caching
			auto existingView = reg.view<PauseMenuTagComponent>();
			if (!existingView.empty())
			{
				BOOM_INFO("[Scene] Objects for '{}' already exist in scene. Load unnecessary.", sceneName.c_str());
				// Ensure they are deactivated, just in case
				for (auto e : existingView) {
					if (!reg.all_of<DeactivatedComponent>(e)) {
						reg.emplace_or_replace<DeactivatedComponent>(e);
					}
				}
				return true; // Already "loaded"
			}

			// --- If we are here, no objects were found at all. ---
			// --- Proceed with the ORIGINAL slow load (first time only). ---

			BOOM_INFO("[Scene] No existing objects found. Performing full additive load for '{}'", sceneName.c_str());

			DataSerializer serializer;
			const std::string sceneFilePath = scenePath + sceneName + ".yaml";
			BOOM_INFO("[Scene] Additively loading scene '{}'", sceneName, sceneFilePath);

			serializer.Deserialize(m_Context->scene, *m_Context->assets, sceneFilePath);

			// --- !!! THIS IS THE FIX !!! ---
			// After deserializing, we must *DEACTIVATE* all newly loaded objects
			// so they are hidden by default.
			BOOM_INFO("[Scene] Deactivating newly deserialized objects...");
			auto newlyLoadedView = m_Context->scene.view<PauseMenuTagComponent>();
			int deactivatedCount = 0;
			for (auto e : newlyLoadedView)
			{
				// Add the tag to hide it
				reg.emplace_or_replace<DeactivatedComponent>(e);
				deactivatedCount++;
			}
			if (deactivatedCount > 0) {
				BOOM_INFO("[Scene] Added 'DeactivatedComponent' to %d newly loaded objects.", deactivatedCount);
			}
			// --- !!! END OF FIX !!! ---


			BOOM_INFO("[Scene] Initializing physics for additive objects...");
			auto view = m_Context->scene.view<PauseMenuTagComponent, RigidBodyComponent>();
			for (auto e : view) {
				auto& rb = view.get<RigidBodyComponent>(e);
				if (!rb.RigidBody.actor) {
					Boom::Entity entity{ &m_Context->scene, e };
					m_Context->physics->AddRigidBody(entity, *m_Context->assets);
				}
			}

			// --- NEW: Also initialize and deactivate collider-only objects ---
			auto colliderView = reg.view<PauseMenuTagComponent, ColliderComponent>();
			for (auto e : colliderView)
			{
				if (reg.all_of<RigidBodyComponent>(e)) continue; // Handled above

				auto& col = colliderView.get<ColliderComponent>(e);
				if (!col.Collider.actor) {
					Boom::Entity entity{ &m_Context->scene, e };
					m_Context->physics->AddColliderOnly(entity, *m_Context->assets);
				}
				if (col.Collider.actor) {
					col.Collider.actor->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, true);
				}
			}

			BOOM_INFO("[Scene] Initializing C# scripts for additive objects...");
			if (m_Context->scriptingSystem)
			{
				auto scriptView = m_Context->scene.view<PauseMenuTagComponent, ScriptComponent>();
				for (auto e : scriptView)
				{
					auto& sc = scriptView.get<ScriptComponent>(e);
					m_Context->scriptingSystem->RecreateForEntity(e, sc);
				}
			}

			BOOM_INFO("[Scene] Successfully added and deactivated scene '{}'", sceneName);

			return true;
		}

		template<typename TagComponent>
		BOOM_INLINE void UnloadAdditiveScene()
		{
			BOOM_INFO("[Scene] Unloading additive scene with tag...");
			auto& reg = m_Context->scene;
			auto view = reg.view<TagComponent>();

			for (auto e : view) {
				reg.emplace_or_replace<DeactivatedComponent>(e);

				if (reg.all_of<RigidBodyComponent>(e)) {
					auto& rb = reg.get<RigidBodyComponent>(e);
					if (rb.RigidBody.actor) {
						rb.RigidBody.actor->release();
						rb.RigidBody.actor = nullptr;
					}
				}
			}
			//reg.destroy(view.begin(), view.end());
			BOOM_INFO("[Scene] Unload complete.");
		}

		template<typename TagComponent>
		BOOM_INLINE void ShowAdditiveScene()
		{
			BOOM_INFO("[Scene] Reactivating additive scene with tag...");
			auto& reg = m_Context->scene;
			int reactivatedCount = 0;

			// Find all entities that are part of the tag AND are deactivated
			auto reactiveView = reg.view<TagComponent, DeactivatedComponent>();
			for (auto e : reactiveView)
			{
				// Reactivate it! Remove the tag.
				reg.remove<DeactivatedComponent>(e);

				// Enable physics actor

				if (reg.all_of<ColliderComponent>(e)) {
					auto& col = reg.get<ColliderComponent>(e);
					if (col.Collider.actor) {
						col.Collider.actor->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, false);
					}
				}
				reactivatedCount++;
			}

			if (reactivatedCount > 0) {
				BOOM_INFO("[Scene] Reactivated %d cached objects.", reactivatedCount);
			}
			else {
				BOOM_WARN("[Scene] ShowAdditiveScene called, but no cached objects found to reactivate.");
			}
		}



		BOOM_INLINE void LightsUpdate() {

			// Point lights
			std::vector<GPUPointLight> gpuPoints;
			gpuPoints.reserve(MAX_POINT_LIGHTS);
			int points = 0;

			EnttView<Entity, PointLightComponent, TransformComponent>(
				[&](auto, PointLightComponent& plc, TransformComponent& tc)
				{
					if (points >= MAX_POINT_LIGHTS) return;

					GPUPointLight g{};
					g.position_range = glm::vec4(tc.transform.translate, plc.light.range);
					g.radiance_intensity = glm::vec4(plc.light.radiance, plc.light.intensity);
					gpuPoints.push_back(g);
					++points;
				});

			m_Context->renderer->UploadPointLights(gpuPoints, points);

			// Directional lights
			std::vector<GPUDirLight> gpuDirs;
			gpuDirs.reserve(MAX_DIR_LIGHTS);
			int directs = 0;

			EnttView<Entity, DirectLightComponent, TransformComponent>(
				[&](auto, DirectLightComponent& dlc, TransformComponent& tc)
				{
					if (directs >= MAX_DIR_LIGHTS) return;

					GPUDirLight g{};
					g.dir_intensity = glm::vec4(glm::normalize(tc.transform.rotate), dlc.light.intensity);
					g.radiance = glm::vec4(dlc.light.radiance, 0.0f);
					gpuDirs.push_back(g);
					++directs;
				});

			m_Context->renderer->UploadDirLights(gpuDirs, directs);

			// Spot lights
			std::vector<GPUSpotLight> gpuSpots;
			gpuSpots.reserve(MAX_SPOT_LIGHTS);
			int spots = 0;

			EnttView<Entity, SpotLightComponent, TransformComponent>(
				[&](auto, SpotLightComponent& slc, TransformComponent& tc)
				{
					if (spots >= MAX_SPOT_LIGHTS) return;

					GPUSpotLight g{};
					g.position_falloff = glm::vec4(tc.transform.translate, slc.light.fallOff);
					g.dir_cutoff = glm::vec4(glm::normalize(tc.transform.rotate), slc.light.cutOff);
					g.radiance_intensity = glm::vec4(slc.light.radiance, slc.light.intensity);
					gpuSpots.push_back(g);
					++spots;
				});

			m_Context->renderer->UploadSpotLights(gpuSpots, spots);

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

						ModelAsset* model{ m_Context->assets->TryGet<ModelAsset>(comp.modelID) };
						if (!model) return;

						std::vector<glm::mat4> joints;
						if (entity.Has<AnimatorComponent>()) {
							auto& an = entity.Get<AnimatorComponent>();
							joints = an.animator->Animate(0); //dont update animation here
						}
						else if (model->hasJoints) {
							static std::vector<glm::mat4> identityPalette(100, glm::mat4(1.0f));
							joints = identityPalette;
						}

						glm::mat4 worldMatrix = Boom::GetWorldMatrix(m_Context->scene, entity.ID());
						Transform3D worldTransform;
						DecomposeMatrix(worldMatrix, worldTransform.translate, worldTransform.rotate, worldTransform.scale);

						m_Context->renderer->DrawShadow(model->data, worldTransform, joints);
						});

					m_Context->renderer->EndShadowPass();
				});
		}

		void SnapEntity(Entity entity, const glm::vec3& direction, float maxDistance = 100.0f);
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
						glm::mat4 worldMatrix = Boom::GetWorldMatrix(m_Context->scene, entity.ID());

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

		BOOM_INLINE DetourNavSystem* GetNavSystem() override
		{
			// Lazily allocate the nav system so Editor panels can always use it
			if (!m_Nav)
			{
				m_Nav = std::make_unique<DetourNavSystem>();
				m_NavInitialized = false;
				BOOM_INFO("[Nav] GetNavSystem(): created DetourNavSystem");
			}
			return m_Nav.get();
		}


		BOOM_INLINE const DetourNavSystem* GetNavSystem() const override { return m_Nav.get(); }

		// Accessor for DebugLinesShader (used by Editor panels like ModelPreviewPanel)
		BOOM_INLINE Boom::DebugLinesShader* GetDebugLinesShader() { return m_DebugLinesShader.get(); }
		BOOM_INLINE const Boom::DebugLinesShader* GetDebugLinesShader() const { return m_DebugLinesShader.get(); }

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

		// Helper to cast a ray specifically from the GAMEPLAY camera
		BOOM_INLINE uint32_t CastRayFromGameCamera()
		{
			double mx, my;
			glfwGetCursorPos(m_Context->window->Handle().get(), &mx, &my);
			auto* win = m_Context->window.get();

			// 1. Get Viewport Values
			float vX = win->GetViewportX();
			float vY = win->GetViewportY();
			float vW = win->GetViewportW();
			float vH = win->GetViewportH();

			// 2. Get Real Window Size
			float windowW = (float)m_Context->window->getWidth();
			float windowH = (float)m_Context->window->getHeight();

			// --- THE FIX FOR STANDALONE BUILDS ---
			// If viewport width/height are invalid (0), it means we are NOT in the editor.
			// So we default to the full window dimensions.
			if (vW <= 1.0f || vH <= 1.0f) {
				vX = 0.0f;
				vY = 0.0f;
				vW = windowW;
				vH = windowH;
			}
			// -------------------------------------

			// 3. Calculate Local Coordinates
			float localX = (float)mx - vX;
			float localY = (float)my - vY;

			// Safety: Ensure we don't divide by zero if window is minimized
			if (vW <= 0.0f || vH <= 0.0f) return (uint32_t)entt::null;

			// Check bounds
			if (localX < 0 || localY < 0 || localX > vW || localY > vH) {
				return (uint32_t)entt::null;
			}

			// ... (Camera finding logic is same as before) ...
			Camera3D* mainCam = nullptr;
			Transform3D* mainCamTrans = nullptr;
			EnttView<Entity, CameraComponent>([&](Entity e, CameraComponent& cc) {
				if (cc.camera.cameraType == Camera3D::CameraType::Main) {
					mainCam = &cc.camera;
					mainCamTrans = &e.Get<TransformComponent>().transform;
				}
				});

			if (!mainCam || !mainCamTrans) return (uint32_t)entt::null;

			// 4. Use the Correct Aspect Ratio (vW / vH)
			float aspect = vW / vH;
			glm::mat4 view = mainCam->View(*mainCamTrans);
			glm::mat4 proj = mainCam->Projection(aspect);
			glm::vec2 viewportSize = { vW, vH };

			Boom::RayCast raycaster(m_Context);
			entt::entity hit = raycaster.CastRayFromScreen(
				localX, localY,
				view, proj,
				mainCamTrans->translate,
				viewportSize
			);

			return (uint32_t)hit;
		}

		BOOM_INLINE bool GetMousePosInViewport(float& x, float& y)
		{
			if (!m_Context || !m_Context->window) return false;

			double mx, my;
			glfwGetCursorPos(m_Context->window->Handle().get(), &mx, &my);
			auto* win = m_Context->window.get();

			// 1. Get Viewport Values
			float vX = win->GetViewportX();
			float vY = win->GetViewportY();
			float vW = win->GetViewportW();
			float vH = win->GetViewportH();

			// 2. Get Real Window Size
			float windowW = (float)m_Context->window->getWidth();
			float windowH = (float)m_Context->window->getHeight();

			// --- THE FIX FOR STANDALONE BUILDS ---
			if (vW <= 1.0f || vH <= 1.0f) {
				vX = 0.0f; vY = 0.0f; vW = windowW; vH = windowH;
			}
			// -------------------------------------

			// 3. Calculate Local Coordinates
			float localX = (float)mx - vX;
			float localY = (float)my - vY;

			// 4. Check bounds
			if (localX < 0 || localY < 0 || localX > vW || localY > vH) {
				return false; // Mouse is outside the viewport
			}

			x = localX;
			y = localY;
			return true;
		}

	private:
		std::unordered_map<std::string, std::pair<glm::vec3, glm::vec3>> m_SphereInitialStates;

		std::unordered_set<std::pair<uint32_t, uint32_t>> m_ActiveTriggerPairs;

		glm::vec3 pivotPosition{};
		bool m_NavInitialized = false;
		bool m_AIinitialized = false;
		std::unique_ptr<Boom::DebugLinesShader> m_DebugLinesShader;
		std::vector<Boom::PhysicsContext::DebugLine> m_PhysLinesCPU;
		char m_CurrentScenePath[512] = "\0";
		char m_PreviousScenePath[512] = "\0";
		bool m_SceneLoaded = false;
		std::unique_ptr<DetourNavSystem> m_Nav;

		// Pre-play state storage for Unity-like play/stop behavior
		std::string m_PrePlayScenePath = "";
		std::string m_PrePlayScene_OriginalPath = "";
		bool m_IsInPlayMode = true;

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

			// Clear trigger tracking
			m_ActiveTriggerPairs.clear();

			// *** ADD THIS - Clear trigger callbacks to prevent stale delegates ***
			Boom::ClearAllTriggerCallbacks();

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
			if (m_Nav) {
				BOOM_INFO("[Nav] Releasing navmesh for previous scene");
				m_Nav.reset();
				m_NavInitialized = false;
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
				SkyboxAsset* skybox{ m_Context->assets->TryGet<SkyboxAsset>(comp.skyboxID) };
				if (skybox) {
					m_Context->renderer->InitSkybox(skybox->data, skybox->envMap, skybox->size);
					BOOM_INFO("[Scene] Reinitialized skybox");
				}
				});

			// Reinitialize physics - BOTH RigidBodies AND Collider-Only (Triggers)
			EnttView<Entity, RigidBodyComponent>([this](auto entity, auto&) {
				m_Context->physics->AddRigidBody(entity, *m_Context->assets);
				});

			// *** ADD THIS - Reinitialize collider-only entities (triggers) ***
			EnttView<Entity, ColliderComponent>([this](auto entity, auto&) {
				// Skip if it has a RigidBodyComponent (already handled above)
				if (entity.Has<RigidBodyComponent>()) return;

				m_Context->physics->AddColliderOnly(entity, *m_Context->assets);
				BOOM_INFO("[Scene] Reinitialized collider-only actor for entity {}",
					static_cast<uint32_t>(entity.ID()));
				});

			// Creating script instances 
			int scriptsCreated = 0;
			EnttView<Entity, ScriptComponent>([this, &scriptsCreated](auto entity, ScriptComponent& sc) {
				if (m_Context->scriptingSystem->RecreateForEntity(entity, sc)) {
					scriptsCreated++;
				}
				});

			if (scriptsCreated > 0) {
				BOOM_INFO("[Scene] Created {} script instances", scriptsCreated);
			}

			m_Context->scriptingSystem->CallStart();

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
			// Set physics event callback
			m_Context->physics->SetEventCallback([this](auto e)
				{
					// Handle TRIGGER events
					if (e.Event == PxEvent::TRIGGER)
					{
						BOOM_INFO("[Trigger] Trigger event detected");

						// Based on Callback.h, Entity1 is otherActor, Entity2 is triggerActor
						entt::entity triggerEntity = (entt::entity)e.Entity2;  // triggerActor
						entt::entity enteringEntity = (entt::entity)e.Entity1; // otherActor

						// *** ADD DETAILED LOGGING ***
						BOOM_INFO("[Trigger] Raw entity IDs - Trigger: {}, Entering: {}",
							static_cast<uint32_t>(triggerEntity), static_cast<uint32_t>(enteringEntity));

						// *** CRITICAL FIX: Validate entities EXIST in the scene ***
						// This prevents processing stale events from old scene loads
						if (!m_Context->scene.valid(triggerEntity)) {
							BOOM_WARN("[Trigger] Ignoring stale event - Trigger entity {} no longer valid",
								static_cast<uint32_t>(triggerEntity));
							return; // ← Silently ignore stale events
						}
						if (!m_Context->scene.valid(enteringEntity)) {
							BOOM_WARN("[Trigger] Ignoring stale event - Entering entity {} no longer valid",
								static_cast<uint32_t>(enteringEntity));
							return; // ← Silently ignore stale events
						}

						// Log entity names for debugging
						std::string triggerName = "Unknown", enteringName = "Unknown";
						if (m_Context->scene.all_of<InfoComponent>(triggerEntity))
							triggerName = m_Context->scene.get<InfoComponent>(triggerEntity).name;
						if (m_Context->scene.all_of<InfoComponent>(enteringEntity))
							enteringName = m_Context->scene.get<InfoComponent>(enteringEntity).name;

						BOOM_INFO("[Trigger] Trigger='{}' ({}), Entering='{}' ({})",
							triggerName, static_cast<uint32_t>(triggerEntity),
							enteringName, static_cast<uint32_t>(enteringEntity));

						// Convert to handles for script callbacks
						uint64_t triggerHandle = static_cast<uint64_t>(static_cast<uint32_t>(triggerEntity));
						uint64_t enteringHandle = static_cast<uint64_t>(static_cast<uint32_t>(enteringEntity));

						// Create pair for tracking enter/exit state
						std::pair<uint32_t, uint32_t> triggerPair = {
							static_cast<uint32_t>(triggerEntity),
							static_cast<uint32_t>(enteringEntity)
						};

						// Determine if this is enter or exit based on our tracking
						bool wasAlreadyActive = (m_ActiveTriggerPairs.find(triggerPair) != m_ActiveTriggerPairs.end());

						BOOM_INFO("[Trigger] Pair tracking - wasAlreadyActive: {}", wasAlreadyActive);

						if (!wasAlreadyActive) {
							// This is an ENTER event
							m_ActiveTriggerPairs.insert(triggerPair);

							BOOM_INFO("[Trigger] '{}' ENTERED trigger '{}' - invoking callbacks", enteringName, triggerName);

							// Call enter callbacks with safety
							try {
								CallTriggerEnterCallbacks(triggerHandle, enteringHandle);
							}
							catch (...) {
								BOOM_ERROR("[Trigger] Exception in enter callback for trigger {} and entity {}",
									triggerHandle, enteringHandle);
							}
						}
						else {
							// This is an EXIT event - the pair was already active
							m_ActiveTriggerPairs.erase(triggerPair);

							BOOM_INFO("[Trigger] '{}' EXITED trigger '{}' - invoking callbacks", enteringName, triggerName);

							// Call exit callbacks with safety
							try {
								CallTriggerExitCallbacks(triggerHandle, enteringHandle);
							}
							catch (...) {
								BOOM_ERROR("[Trigger] Exception in exit callback for trigger {} and entity {}",
									triggerHandle, enteringHandle);
							}
						}
					}

					// Handle CONTACT events
					else if (e.Event == PxEvent::CONTACT)
					{
						entt::entity ent1 = (entt::entity)e.Entity1;
						entt::entity ent2 = (entt::entity)e.Entity2;

						if (m_Context->scene.valid(ent1) && m_Context->scene.all_of<RigidBodyComponent>(ent1))
						{
							m_Context->scene.get<RigidBodyComponent>(ent1).RigidBody.isColliding = true;
						}

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

			// Delta time behavior:
			// - Edit mode: Always update (for smooth camera movement)
			// - Play mode running: Update normally
			// - Play mode paused: No time progression (0)
			if (!m_IsInPlayMode) {
				// Edit mode - camera needs delta time
				m_Context->DeltaTime = rawDelta;
			}
			else if (m_AppState == ApplicationState::RUNNING) {
				// Play mode running - normal time
				m_Context->DeltaTime = rawDelta;
			}
			else {
				// Play mode paused - freeze time
				m_Context->DeltaTime = 0.0;
			}

			sLastTime = currentTime;
		}


		//---------------Physics------------------------
		BOOM_API void DestroyPhysicsActors();

		void RunPhysicsSimulation();

		void SnapEntityToSurface(entt::entity entity, glm::vec3 direction);

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


		static void AppendCylinderWire(float radius, float halfHeight, const physx::PxTransform& world, std::vector<Boom::LineVert>& out, const glm::vec4& color);

		void AppendConvexMeshWire(const physx::PxConvexMeshGeometry& geom, const physx::PxTransform& world, std::vector<Boom::LineVert>& out, const glm::vec4& color);
		void AppendTriangleMeshWire(const physx::PxTriangleMeshGeometry& geom, const physx::PxTransform& world, std::vector<Boom::LineVert>& out, const glm::vec4& color);
		static void AppendTriangleWire(const glm::vec3& scale, const physx::PxTransform& world, std::vector<Boom::LineVert>& out, const glm::vec4& color);

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

		BOOM_INLINE void RecreateScriptForEntity(entt::entity entity)
		{
			if (!m_Context->scene.valid(entity)) return;

			auto* sc = m_Context->scene.try_get<ScriptComponent>(entity);
			if (!sc) return;

			m_Context->scriptingSystem->RecreateForEntity(entity, *sc);
		}
		// Add this as a private method
		BOOM_INLINE void CleanupTriggerPairs()
		{
			// Clean up any invalid trigger pairs (entities that no longer exist)
			auto it = m_ActiveTriggerPairs.begin();
			while (it != m_ActiveTriggerPairs.end()) {
				entt::entity triggerEntity = static_cast<entt::entity>(it->first);
				entt::entity enteringEntity = static_cast<entt::entity>(it->second);

				if (!m_Context->scene.valid(triggerEntity) || !m_Context->scene.valid(enteringEntity)) {
					it = m_ActiveTriggerPairs.erase(it);
				}
				else {
					++it;
				}
			}
		}

	};


}

#endif // !APPLICATION_H
