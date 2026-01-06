#pragma once

#include <fmod.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <glm/vec3.hpp>
#include "common/Core.h"

class SoundEngine {
public:
    static BOOM_API SoundEngine& Instance();

    BOOM_API bool  Init();
    BOOM_API void  Update();
    BOOM_API void  Shutdown();

    BOOM_API void  PlaySound(const std::string& name, const std::string& filePath, bool loop);
    BOOM_API void  PlaySound(const std::string& name, const std::string& filePath, bool loop, const std::string& groupName);
    BOOM_API void  StopSound(const std::string& name);
    BOOM_API void  SetVolume(const std::string& name, float volume);

    BOOM_API void  PlaySoundAt(const std::string& name, const std::string& filePath, const glm::vec3& position, bool loop);

    BOOM_API std::vector<std::string> GetChannelGroupNames() const;

    BOOM_API bool  IsPlaying(const std::string& name) const;
    BOOM_API void  Pause(const std::string& name, bool pause);
    BOOM_API void  SetLooping(const std::string& name, bool loop);
    BOOM_API void  StopAllExcept(const std::string& keepName);

    BOOM_API void  PauseAll(bool pause);
    BOOM_API void  StopAll();

    BOOM_API bool  PreloadSound(const std::string& name, const std::string& filePath, bool stream = false, bool loop = false);
    BOOM_API void  UnloadSound(const std::string& name);

    BOOM_API bool  CreateChannelGroup(const std::string& groupName, const std::string& parentGroup = "Master");
    BOOM_API bool  RemoveChannelGroup(const std::string& groupName);
    BOOM_API bool  HasChannelGroup(const std::string& groupName) const;

    BOOM_API void  SetGroupVolume(const std::string& groupName, float volume);
    BOOM_API float GetGroupVolume(const std::string& groupName) const;

    // Set3D position of a currently playing channel by name (entity-attached sounds use this)
    BOOM_API void  SetSoundPosition(const std::string& name, const glm::vec3& position);

    // Set listener attributes (call from camera update)
    BOOM_API void  SetListenerAttributes(const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& forward, const glm::vec3& up);

    // Set 3D audio parameters for a sound
    BOOM_API void  Set3DMinMaxDistance(const std::string& name, float minDist, float maxDist);

    // Get listener position (for debugging)
    BOOM_API glm::vec3 GetListenerPosition() const;

    // Enable runtime 3D debug logging (prints listener + channel positions and audibility)
    BOOM_API void  SetDebug3D(bool enabled) { mDebug3D = enabled; }
    BOOM_API bool  GetDebug3D() const { return mDebug3D; }

private:
    FMOD::System* mSystem = nullptr;
    std::unordered_map<std::string, FMOD::Sound*>   mSounds;
    // Track a single channel instance per sound name for positional updates
    std::unordered_map<std::string, FMOD::Channel*> mChannels;
    std::unordered_map<std::string, FMOD::ChannelGroup*> mChannelGroups;
    // Store the base (un-attenuated) volume for each channel name so Update() can apply distance attenuation
    std::unordered_map<std::string, float> mChannelBaseVolume;

    std::unordered_map<std::string, int> mSoundRefCount;
    mutable std::mutex mMutex;

    // Listener state for debugging
    glm::vec3 mLastListenerPos = glm::vec3(0.0f);
    glm::vec3 mLastListenerForward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 mLastListenerUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // Debugging
    bool mDebug3D = false;
};