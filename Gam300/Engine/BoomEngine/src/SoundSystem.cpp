#include "Core.h"
#include "../includes/Audio/SoundSystem.hpp"
#include "../includes/Audio/Audio.hpp"
#include "../includes/ECS/ECS.hpp"
#include <iostream>
#include <GLFW/glfw3.h>
#include <random>

using namespace Boom;

static std::mt19937 s_Rng(std::random_device{}());

void SoundSystem::Shutdown()
{
	for (auto& [eid, names] : s_activeInstances) {
		for (auto& name : names) {
			SoundEngine::Instance().StopSound(name);
			SoundEngine::Instance().UnloadSound(name);
		}
	}
	s_activeInstances.clear();
	s_lastPos.clear();
	s_lastTriggerTime.clear();
}

void SoundSystem::Update(Boom::EntityRegistry& registry, float dt)
{
	(void)dt;
	auto view = registry.view<TransformComponent, SoundComponent>();

	double now = glfwGetTime();

	for (auto entity : view)
	{
		auto& tf = view.get<TransformComponent>(entity);
		auto& sc = view.get<SoundComponent>(entity);

		uint64_t uid = static_cast<uint64_t>(static_cast<uint32_t>(entity));

		// Ensure container exists
		auto& instances = s_activeInstances[uid];
		auto& lastTimes = s_lastTriggerTime[uid];

		// Iterate through entries in component and ensure each sound is active as requested
		for (size_t i =0; i < sc.entries.size(); ++i)
		{
			const auto& entry = sc.entries[i];
			// Construct unique instance name per entity + index + logical name to avoid collisions
			std::string instanceName = "ent_" + std::to_string(uid) + "_" + std::to_string(i) + "_" + entry.name;

			// If playOnStart and not yet active -> preload and play
			bool alreadyActive = (std::find(instances.begin(), instances.end(), instanceName) != instances.end());

			if (entry.playOnStart && !alreadyActive)
			{
				// choose a file to preload/play (first available)
				std::string chosen = entry.filePath;
				if (!entry.filePaths.empty()) chosen = entry.filePaths.front();

				SoundEngine::Instance().PreloadSound(instanceName, chosen, false, entry.loop);
				SoundEngine::Instance().PlaySoundAt(instanceName, chosen, tf.transform.translate, entry.loop);
				// Apply configured volume
				SoundEngine::Instance().SetVolume(instanceName, entry.volume);
				// Apply 3D audio distance settings
				SoundEngine::Instance().Set3DMinMaxDistance(instanceName, entry.minDistance, entry.maxDistance);

				instances.push_back(instanceName);
				s_lastPos[uid] = tf.transform.translate;
				lastTimes[instanceName] = now;
				continue;
			}

			// ANIMATION TRIGGER
			bool triggered = false;
			if (!entry.animTrigger.empty()) {
				if (registry.any_of<AnimatorComponent>(entity)) {
					auto& animComp = registry.get<AnimatorComponent>(entity);
					Animator* animator = animComp.animator.get();
					if (animator) {
						if (animator->GetTrigger(entry.animTrigger)) {
							double last =0.0;
							auto it = lastTimes.find(instanceName);
							if (it != lastTimes.end()) last = it->second;
							if (now - last >= entry.repeatInterval) {
								triggered = true;
								lastTimes[instanceName] = now;
							}
						}
					}
				}
			}

			// If triggered by animation, play one random file from filePaths (or filePath single)
			if (triggered) {
				// Build list of candidates
				std::vector<std::string> candidates;
				if (!entry.filePaths.empty()) candidates = entry.filePaths;
				else if (!entry.filePath.empty()) candidates.push_back(entry.filePath);

				if (!candidates.empty()) {
					std::uniform_int_distribution<size_t> dist(0, candidates.size() -1);
					size_t idx = dist(s_Rng);
					std::string chosen = candidates[idx];

					// Use a unique instance name for one-shot footstep to avoid stomping looping bgm
					std::string playName = instanceName + "_play_" + std::to_string((uint64_t)now);
					SoundEngine::Instance().PreloadSound(playName, chosen, false, false);
					SoundEngine::Instance().PlaySoundAt(playName, chosen, tf.transform.translate, false);
					// Apply configured volume for this transient instance
					SoundEngine::Instance().SetVolume(playName, entry.volume);
					// Apply 3D audio distance settings for this transient instance
					SoundEngine::Instance().Set3DMinMaxDistance(playName, entry.minDistance, entry.maxDistance);
					// track this playing temporary instance so it can be cleaned up later
					instances.push_back(playName);
					lastTimes[playName] = now;
				}
			}

			// If there's an active instance for this entry, update its position
			for (const auto& n : instances)
			{
				// only update position for instances that belong to this logical entry
				if (n.rfind(instanceName,0) ==0) // starts_with(instanceName)
				{
					glm::vec3 pos = tf.transform.translate;
					SoundEngine::Instance().SetSoundPosition(n, pos);
					s_lastPos[uid] = pos;

					// Also ensure volume follows component value (allow realtime changes from inspector)
					SoundEngine::Instance().SetVolume(n, entry.volume);

					// If filePath cleared, stop and unload this instance
					// Note: we treat legacy filePath empty as no-op
				}
			}
		}

		// Remove instances whose corresponding entry no longer exists or whose filePath was cleared
		// Build a set of desired instance name prefixes from current component
		std::vector<std::string> desiredPrefixes;
		desiredPrefixes.reserve(sc.entries.size());
		for (size_t i =0; i < sc.entries.size(); ++i) {
			const auto& entry = sc.entries[i];
			desiredPrefixes.push_back("ent_" + std::to_string(uid) + "_" + std::to_string(i) + "_" + entry.name);
		}

		// Erase any instances not matching desired prefixes AND older than some grace (5s)
		auto& vec = instances;
		for (auto it = vec.begin(); it != vec.end(); )
		{
			const std::string& name = *it;
			bool keep = false;
			for (const auto& pref : desiredPrefixes) {
				if (name.rfind(pref,0) ==0) { keep = true; break; }
			}
			// also keep very recent transient sounds (<10s)
			double last =0.0;
			auto ltit = s_lastTriggerTime[uid].find(name);
			if (ltit != s_lastTriggerTime[uid].end()) last = ltit->second;

			if (!keep && (now - last) >10.0) {
				SoundEngine::Instance().StopSound(name);
				SoundEngine::Instance().UnloadSound(name);
				s_lastTriggerTime[uid].erase(name);
				it = vec.erase(it);
			}
			else ++it;
		}

		// If component has no entries left, clean up pos map
		if (sc.entries.empty()) {
			s_lastPos.erase(uid);
			s_lastTriggerTime.erase(uid);
		}
	}

	// Remove entries for destroyed entities
	for (auto it = s_activeInstances.begin(); it != s_activeInstances.end(); )
	{
		auto eid = static_cast<Boom::EntityID>(static_cast<uint32_t>(it->first));
		if (!registry.valid(eid))
		{
			for (auto& name : it->second) {
				SoundEngine::Instance().StopSound(name);
				SoundEngine::Instance().UnloadSound(name);
			}
			s_lastPos.erase(it->first);
			s_lastTriggerTime.erase(it->first);
			it = s_activeInstances.erase(it);
		}
		else ++it;
	}
}
