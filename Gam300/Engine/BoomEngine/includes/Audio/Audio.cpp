#include "Core.h"
#include "Audio.hpp"
#include <iostream>
#include <filesystem>
#include <mutex>

static FMOD::ChannelGroup* sMasterGroup = nullptr;
static FMOD::ChannelGroup* sMusicGroup = nullptr;
static FMOD::ChannelGroup* sSFXGroup = nullptr;

static inline bool FMODCheck(FMOD_RESULT result, const char* context) {
    if (result != FMOD_OK) {
        std::cerr << "[FMOD] " << context << " failed: " << result << std::endl;
        return false;
    }
    return true;
}

BOOM_API SoundEngine& SoundEngine::Instance() {
    static SoundEngine instance;
    return instance;
}

BOOM_API bool SoundEngine::Init() {
    FMOD_RESULT result = FMOD::System_Create(&mSystem);
    if (result != FMOD_OK) {
        std::cerr << "FMOD error: " << result << std::endl;
        return false;
    }

    result = mSystem->init(512, FMOD_INIT_NORMAL, nullptr);
    if (result != FMOD_OK) {
        std::cerr << "FMOD init failed: " << result << std::endl;
        return false;
    }

    // doppler scale = 1.0, distance factor = 1.0 (game units per meter), rolloff scale = 1.0
    mSystem->set3DSettings(1.0f, 1.0f, 1.0f);

    result = mSystem->getMasterChannelGroup(&sMasterGroup);
    if (!FMODCheck(result, "getMasterChannelGroup")) return false;

    result = mSystem->createChannelGroup("Music", &sMusicGroup);
    if (!FMODCheck(result, "createChannelGroup Music")) return false;

    result = mSystem->createChannelGroup("SFX", &sSFXGroup);
    if (!FMODCheck(result, "createChannelGroup SFX")) return false;

    if (sMasterGroup && sMusicGroup) sMasterGroup->addGroup(sMusicGroup);
    if (sMasterGroup && sSFXGroup)   sMasterGroup->addGroup(sSFXGroup);

    // Ensure channel groups and master have sane defaults (unmuted, audible)
    if (sMasterGroup) {
        sMasterGroup->setVolume(1.0f);
        sMasterGroup->setMute(false);
        sMasterGroup->setPaused(false);
    }
    if (sMusicGroup) {
        sMusicGroup->setVolume(1.0f);
        sMusicGroup->setMute(false);
        sMusicGroup->setPaused(false);
    }
    if (sSFXGroup) {
        sSFXGroup->setVolume(1.0f);
        sSFXGroup->setMute(false);
        sSFXGroup->setPaused(false);
    }

    {
        std::scoped_lock lock(mMutex);
        mChannelGroups["Master"] = sMasterGroup;
        mChannelGroups["Music"] = sMusicGroup;
        mChannelGroups["SFX"] = sSFXGroup;
    }

    return true;
}

BOOM_API bool SoundEngine::CreateChannelGroup(const std::string& groupName, const std::string& parentGroup)
{
    if (!mSystem) return false;
    if (groupName.empty()) return false;

    std::scoped_lock lock(mMutex);

    if (mChannelGroups.find(groupName) != mChannelGroups.end()) { return false; }

    FMOD::ChannelGroup* newGroup = nullptr;
    FMOD_RESULT result = mSystem->createChannelGroup(groupName.c_str(), &newGroup);
    if (result != FMOD_OK || !newGroup)
    {
        std::cerr << "[SoundEngine] createChannelGroup failed for " << groupName << " error: " << result << std::endl;
        return false;
    }

    FMOD::ChannelGroup* parent = nullptr;
    auto it = mChannelGroups.find(parentGroup);
    if (it != mChannelGroups.end()) parent = it->second;
    if (!parent) parent = sMasterGroup;

    if (parent && newGroup) parent->addGroup(newGroup);

    mChannelGroups[groupName] = newGroup;
    return true;
}

BOOM_API bool SoundEngine::RemoveChannelGroup(const std::string& groupName)
{
    if (groupName.empty() || groupName == "Master") return false;

    std::scoped_lock lock(mMutex);

    auto it = mChannelGroups.find(groupName);
    if (it == mChannelGroups.end()) return false;

    FMOD::ChannelGroup* group = it->second;

    for (auto& [name, ch] : mChannels)
    {
        if (ch)
        {
            FMOD::ChannelGroup* current = nullptr;
            ch->getChannelGroup(&current);
            if (current == group) { ch->stop(); }
        }
    }

    if (group) { group->release(); }

    mChannelGroups.erase(it);
    return true;
}

BOOM_API void SoundEngine::SetGroupVolume(const std::string& groupName, float volume)
{
    std::scoped_lock lock(mMutex);
    auto it = mChannelGroups.find(groupName);
    if (it != mChannelGroups.end() && it->second) { it->second->setVolume(volume); }

    else { std::cerr << "[SoundEngine] Unknown group '" << groupName << "'\n"; }
}

BOOM_API float SoundEngine::GetGroupVolume(const std::string& groupName) const
{
    std::scoped_lock lock(mMutex);
    float vol = 0.0f;
    auto it = mChannelGroups.find(groupName);

    if (it != mChannelGroups.end() && it->second) { it->second->getVolume(&vol); }
    return vol;
}

BOOM_API void SoundEngine::Update() {
    if (mSystem) mSystem->update();

    // Get listener position for debugging
    FMOD_VECTOR listenerPos = { 0,0,0 };
    if (mSystem && mDebug3D) {
        FMOD_VECTOR lpos, lvel, lforward, lup;
        if (mSystem->get3DListenerAttributes(0, &lpos, &lvel, &lforward, &lup) == FMOD_OK) {
            listenerPos = lpos;
            std::cerr << "[SoundEngine] Listener pos(" << lpos.x << "," << lpos.y << "," << lpos.z << ") forward(" << lforward.x << "," << lforward.y << "," << lforward.z << ")\n";
        }
    }

    // Ensure master/music/SFX groups are always unpaused and unmuted to prevent audio dropout
    if (sMasterGroup) {
        bool masterMuted = false;
        if (sMasterGroup->getMute(&masterMuted) == FMOD_OK && masterMuted) {
            sMasterGroup->setMute(false);
        }
        bool masterPaused = false;
        if (sMasterGroup->getPaused(&masterPaused) == FMOD_OK && masterPaused) {
            sMasterGroup->setPaused(false);
        }
    }
    if (sMusicGroup) {
        bool musicMuted = false;
        if (sMusicGroup->getMute(&musicMuted) == FMOD_OK && musicMuted) {
            sMusicGroup->setMute(false);
        }
    }
    if (sSFXGroup) {
        bool sfxMuted = false;
        if (sSFXGroup->getMute(&sfxMuted) == FMOD_OK && sfxMuted) {
            sSFXGroup->setMute(false);
        }
    }

    // create snapshots under lock
    std::vector<std::pair<std::string, FMOD::Channel*>> channelsSnapshot;
    std::unordered_map<std::string, float> baseVolumeSnapshot;
    {
        std::scoped_lock lock(mMutex);
        channelsSnapshot.reserve(mChannels.size());
        for (auto& p : mChannels) channelsSnapshot.emplace_back(p.first, p.second);
        baseVolumeSnapshot = mChannelBaseVolume;
    }

    std::vector<std::string> stoppedNames;

    for (auto& entry : channelsSnapshot) {
        const std::string& name = entry.first;
        FMOD::Channel* ch = entry.second;
        bool isPlaying = false;
        if (ch) ch->isPlaying(&isPlaying);

        // Ensure channels aren't paused unexpectedly
        if (ch) {
            bool paused = false;
            if (ch->getPaused(&paused) == FMOD_OK && paused) {
                ch->setPaused(false);
            }
        }

        // Debug logging if enabled
        if (mDebug3D && ch) {
            FMOD_VECTOR pos = { 0 }, vel = { 0 };
            ch->get3DAttributes(&pos, &vel);
            float vol = 0.0f;
            ch->getVolume(&vol);

            // Get associated sound min/max if available
            FMOD::Sound* curSound = nullptr;
            float minDist = 0.0f, maxDist = 0.0f;
            if (ch->getCurrentSound(&curSound) == FMOD_OK && curSound) {
                if (curSound->get3DMinMaxDistance(&minDist, &maxDist) != FMOD_OK) {
                    minDist = 0.0f; maxDist = 0.0f;
                }
            }

            // Compute simple distance
            float dx = pos.x - listenerPos.x;
            float dy = pos.y - listenerPos.y;
            float dz = pos.z - listenerPos.z;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            // Check if channel is 3D-capable
            FMOD_MODE chMode = 0;
            bool channelIs3D = false;
            if (ch->getMode(&chMode) == FMOD_OK) channelIs3D = (chMode & FMOD_3D) != 0;

            std::cerr << "[SoundEngine] Channel '" << name << "' pos(" << pos.x << "," << pos.y << "," << pos.z << ") dist=" << distance << " min=" << minDist << " max=" << maxDist << " vol=" << vol << " playing=" << isPlaying << " 3D=" << channelIs3D << "\n";
        }

        if (!isPlaying) stoppedNames.push_back(name);
    }

    if (!stoppedNames.empty()) {
        std::scoped_lock lock(mMutex);
        for (auto& n : stoppedNames) {
            mChannels.erase(n);
            mChannelBaseVolume.erase(n);
        }
    }
}

BOOM_API void SoundEngine::PauseAll(bool pause)
{
    std::scoped_lock lock(mMutex);

    // Pause/unpause all channels we track
    for (auto& [name, ch] : mChannels) {
        if (ch) ch->setPaused(pause);
    }

    // Also set pause on master group to be safe
    if (sMasterGroup) sMasterGroup->setPaused(pause);
}

BOOM_API void SoundEngine::StopAll()
{
    std::scoped_lock lock(mMutex);

    // Stop individual channels but do not unload preloaded sounds
    for (auto& [name, ch] : mChannels) {
        if (ch) ch->stop();
    }
    mChannels.clear();
    mChannelBaseVolume.clear();

    // Stop master group
    if (sMasterGroup) sMasterGroup->stop();

    // Do not release mSounds here; keep preloaded sounds available
}

BOOM_API void SoundEngine::Shutdown() {
    for (auto& [name, ch] : mChannels) { if (ch) ch->stop(); }
    mChannels.clear();

    // Release sounds
    for (auto& [name, sound] : mSounds) { if (sound) sound->release(); }
    mSounds.clear();

    // Release custom channel groups (skip Master - owned by system)
    {
        std::scoped_lock lock(mMutex);
        for (auto it = mChannelGroups.begin(); it != mChannelGroups.end(); )
        {
            const std::string& n = it->first;
            FMOD::ChannelGroup* g = it->second;
            if (n != "Master" && g)
            {
                g->release();
                it = mChannelGroups.erase(it);
            }

            else { ++it; }
        }
        mChannelGroups.clear();
    }

    sMusicGroup = nullptr;
    sSFXGroup = nullptr;
    sMasterGroup = nullptr;


    if (mSystem)
    {
        mSystem->close();
        mSystem->release();
        mSystem = nullptr;
    }
}

BOOM_API void SoundEngine::PlaySound(const std::string& name, const std::string& filePath, bool loop)
{
    if (!mSystem)
    {
        std::cerr << "FMOD system not initialized!\n";
        return;
    }

    std::cout << "[SoundEngine] Current working directory: "
        << std::filesystem::current_path().string() << std::endl;

    if (!std::filesystem::exists(filePath)) {
        std::cerr << "[SoundEngine] File does NOT exist: " << filePath << std::endl;
    }
    else {
        std::cout << "[SoundEngine] File found: " << filePath << std::endl;
    }

    if (mSounds.find(name) == mSounds.end()) {
        // Make explicit mode composition so 2D / looping flags are correct
        FMOD_MODE mode = static_cast<FMOD_MODE>(FMOD_DEFAULT | FMOD_2D);
        mode = static_cast<FMOD_MODE>(mode | (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF));
        FMOD::Sound* sound = nullptr;

        FMOD_RESULT result = mSystem->createSound(filePath.c_str(), mode, nullptr, &sound);
        if (result != FMOD_OK) {
            std::cerr << "FMOD failed to load " << filePath << " error: " << result << std::endl;
            return;
        }

        mSounds[name] = sound;
    }

    FMOD::Channel* channel = nullptr;
    FMOD_RESULT result = mSystem->playSound(mSounds[name], nullptr, false, &channel);
    if (result != FMOD_OK) {
        std::cerr << "FMOD failed to play " << name << " error: " << result << std::endl;
        return;
    }

    // Route looped audio to Music group, otherwise to SFX. (Heuristic)
    FMOD::ChannelGroup* targetGroup = loop ? sMusicGroup : sSFXGroup;

    if (channel && targetGroup) { channel->setChannelGroup(targetGroup); }

    // Set defaults and unpause
    if (channel)
    {
        channel->setVolume(1.0f);
        channel->setPaused(false);
        {
            std::scoped_lock lock(mMutex);
            mChannels[name] = channel;
            // remember base volume
            mChannelBaseVolume[name] = 1.0f;
        }
    }
}


BOOM_API void SoundEngine::PlaySound(const std::string& name, const std::string& filePath, bool loop, const std::string& groupName)
{
    if (!mSystem)
    {
        std::cerr << "FMOD system not initialized!\n";
        return;
    }

    if (mSounds.find(name) == mSounds.end()) {
        FMOD_MODE mode = static_cast<FMOD_MODE>(FMOD_DEFAULT | FMOD_2D);
        mode = static_cast<FMOD_MODE>(mode | (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF));
        FMOD::Sound* sound = nullptr;

        FMOD_RESULT result = mSystem->createSound(filePath.c_str(), mode, nullptr, &sound);
        if (result != FMOD_OK) {
            std::cerr << "FMOD failed to load " << filePath << " error: " << result << std::endl;
            return;
        }

        mSounds[name] = sound;
    }

    FMOD::Channel* channel = nullptr;
    FMOD_RESULT result = mSystem->playSound(mSounds[name], nullptr, false, &channel);
    if (result != FMOD_OK) {
        std::cerr << "FMOD failed to play " << name << " error: " << result << std::endl;
        return;
    }

    FMOD::ChannelGroup* targetGroup = nullptr;
    {
        std::scoped_lock lock(mMutex);
        auto it = mChannelGroups.find(groupName);
        if (it != mChannelGroups.end()) targetGroup = it->second;
    }

    if (!targetGroup) targetGroup = loop ? sMusicGroup : sSFXGroup;

    if (channel && targetGroup) { channel->setChannelGroup(targetGroup); }

    if (channel)
    {
        channel->setVolume(1.0f);
        channel->setPaused(false);
        {
            std::scoped_lock lock(mMutex);
            mChannels[name] = channel;
            // remember base volume
            mChannelBaseVolume[name] = 1.0f;
        }
    }
}

BOOM_API void SoundEngine::StopSound(const std::string& name) {
    auto it = mChannels.find(name);

    if (it != mChannels.end())
    {
        if (it->second) it->second->stop();
        mChannels.erase(it);
    }
    std::scoped_lock lock(mMutex);
    mChannelBaseVolume.erase(name);
}

BOOM_API void SoundEngine::SetVolume(const std::string& name, float volume) {
    auto it = mChannels.find(name);

    if (it != mChannels.end() && it->second) { it->second->setVolume(volume); }
    // remember base volume
    std::scoped_lock lock(mMutex);
    mChannelBaseVolume[name] = volume;
}

BOOM_API bool SoundEngine::IsPlaying(const std::string& name) const {
    auto it = mChannels.find(name);
    if (it == mChannels.end() || !it->second) return false;
    bool playing = false;
    it->second->isPlaying(&playing);
    return playing;
}

BOOM_API void SoundEngine::Pause(const std::string& name, bool pause) {
    auto it = mChannels.find(name);

    if (it != mChannels.end() && it->second) { it->second->setPaused(pause); }
}

BOOM_API void SoundEngine::SetLooping(const std::string& name, bool loop) {
    auto it = mChannels.find(name);

    if (it != mChannels.end() && it->second) { it->second->setMode(loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF); }
}

BOOM_API void SoundEngine::StopAllExcept(const std::string& keepName) {
    for (auto it = mChannels.begin(); it != mChannels.end(); )
    {
        const std::string& n = it->first;
        FMOD::Channel* ch = it->second;
        if (n != keepName && ch) ch->stop();

        if (n != keepName)
            it = mChannels.erase(it);
        else
            ++it;
    }
}

BOOM_API bool SoundEngine::PreloadSound(const std::string& name, const std::string& filePath, bool stream, bool loop)
{
    if (!mSystem) return false;

    std::scoped_lock lock(mMutex);

    auto it = mSounds.find(name);
    if (it != mSounds.end())
    {
        mSoundRefCount[name] += 1;
        return true;
    }

    FMOD_MODE mode = FMOD_DEFAULT | FMOD_2D;
    if (stream) mode = static_cast<FMOD_MODE>(mode | FMOD_CREATESTREAM);
    mode = static_cast<FMOD_MODE>(mode | (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF));

    FMOD::Sound* sound = nullptr;
    FMOD_RESULT result = mSystem->createSound(filePath.c_str(), mode, nullptr, &sound);

    if (result != FMOD_OK)
    {
        std::cerr << "FMOD failed to preload " << filePath << " error: " << result << std::endl;
        return false;
    }

    mSounds[name] = sound;
    mSoundRefCount[name] = 1;
    return true;
}

BOOM_API void SoundEngine::UnloadSound(const std::string& name)
{
    std::scoped_lock lock(mMutex);

    auto rcIt = mSoundRefCount.find(name);
    if (rcIt == mSoundRefCount.end()) { return; }

    rcIt->second -= 1;
    if (rcIt->second > 0) { return; }

    mSoundRefCount.erase(rcIt);

    auto sIt = mSounds.find(name);
    if (sIt != mSounds.end())
    {
        if (sIt->second) { sIt->second->release(); }

        mSounds.erase(sIt);
    }
}

BOOM_API void SoundEngine::PlaySoundAt(const std::string& name, const std::string& filePath, const glm::vec3& position, bool loop)
{
    if (!mSystem) {
        std::cerr << "FMOD system not initialized!\n";
        return;
    }

    std::cout << "[SoundEngine] PlaySoundAt: " << name << " -> " << filePath << " pos(" << position.x << "," << position.y << "," << position.z << ")\n";

    // For looped audio (BGM/ambient), play as 2D so it's always audible
    // For one-shot audio (SFX), play as 3D with positional attenuation
    if (loop) {
        // BGM: use regular 2D PlaySound but track the entity position for consistency
        if (mSounds.find(name) == mSounds.end()) {
            FMOD_MODE mode = static_cast<FMOD_MODE>(FMOD_DEFAULT | FMOD_2D);
            mode = static_cast<FMOD_MODE>(mode | FMOD_LOOP_NORMAL);
            FMOD::Sound* sound = nullptr;

            FMOD_RESULT result = mSystem->createSound(filePath.c_str(), mode, nullptr, &sound);
            if (result != FMOD_OK || !sound) {
                std::cerr << "FMOD failed to load (BGM) " << filePath << " error: " << result << std::endl;
                return;
            }

            mSounds[name] = sound;
        }

        FMOD::Channel* channel = nullptr;
        FMOD_RESULT result = mSystem->playSound(mSounds[name], nullptr, false, &channel);
        if (result != FMOD_OK || !channel) {
            std::cerr << "FMOD failed to play (BGM) " << name << " error: " << result << std::endl;
            return;
        }

        // Route to Music group
        FMOD::ChannelGroup* targetGroup = nullptr;
        {
            std::scoped_lock lock(mMutex);
            auto it = mChannelGroups.find("Music");
            if (it != mChannelGroups.end()) targetGroup = it->second;
        }
        if (channel && targetGroup) channel->setChannelGroup(targetGroup);

        if (channel) {
            channel->setVolume(1.0f);
            channel->setPaused(false);
            {
                std::scoped_lock lock(mMutex);
                mChannels[name] = channel;
                mChannelBaseVolume[name] = 1.0f;
            }
        }
        return;
    }

    // Use a distinct key for 3D sounds so 2D and 3D variants don't collide.
    // SFX: Use 3D positional audio with distance attenuation
    const std::string soundKey = name + "|3D";

    // Ensure sound loaded (3D mode)
    {
        std::scoped_lock lock(mMutex);
        auto it = mSounds.find(soundKey);
        bool needCreate = (it == mSounds.end());

        if (!needCreate) {
            // Verify it's actually a 3D sound
            FMOD_MODE currentMode = FMOD_DEFAULT;
            if (it->second) {
                if (it->second->getMode(&currentMode) == FMOD_OK) {
                    if (!(currentMode & FMOD_3D)) {
                        // release and mark for re-create
                        it->second->release();
                        mSounds.erase(it);
                        needCreate = true;
                    }
                }
            }
        }

        if (needCreate) {
            // Use linear rolloff so attenuation follows min/max distances predictably
            FMOD_MODE mode = static_cast<FMOD_MODE>(FMOD_DEFAULT | FMOD_3D | FMOD_3D_LINEARROLLOFF);
            mode = static_cast<FMOD_MODE>(mode | FMOD_LOOP_OFF);

            FMOD::Sound* sound = nullptr;
            FMOD_RESULT result = mSystem->createSound(filePath.c_str(), mode, nullptr, &sound);
            if (result != FMOD_OK || !sound) {
                std::cerr << "FMOD failed to load (3D) " << filePath << " error: " << result << std::endl;
                return;
            }

            // Set reasonable 3D min/max distances for one-shot SFX
            // Min distance: full volume within this radius
            // Max distance: silent beyond this radius
            // Using more realistic values: 1.0 = full volume within 1 unit, fade to silent by 50 units
            sound->set3DMinMaxDistance(1.0f, 50.0f);

            mSounds[soundKey] = sound;

            // Debug: verify mode
            FMOD_MODE verifyMode = FMOD_DEFAULT;
            if (sound->getMode(&verifyMode) == FMOD_OK) {
                if (verifyMode & FMOD_3D) std::cerr << "[SoundEngine] Loaded sound as 3D SFX: " << soundKey << "\n";
                else std::cerr << "[SoundEngine] WARNING: sound not 3D after create: " << soundKey << "\n";
            }
        }
    }

    FMOD::Channel* channel = nullptr;
    FMOD_RESULT result = mSystem->playSound(mSounds[soundKey], nullptr, true, &channel); // start paused to set 3D attrs
    if (result != FMOD_OK || !channel) {
        std::cerr << "FMOD failed to play (3D) " << name << " error: " << result << std::endl;
        return;
    }

    // Ensure channel mode includes 3D. Some drivers may ignore sound flags; force on channel if needed.
    {
        FMOD_MODE chMode = 0;
        if (channel->getMode(&chMode) == FMOD_OK) {
            if (!(chMode & FMOD_3D)) {
                FMOD_MODE newMode = static_cast<FMOD_MODE>(chMode | FMOD_3D);
                // Also ensure linear rolloff is enabled on the channel so distance attenuation uses the expected curve
                newMode = static_cast<FMOD_MODE>(newMode | FMOD_3D_LINEARROLLOFF);
                if (channel->setMode(newMode) != FMOD_OK) {
                    std::cerr << "[SoundEngine] Warning: failed to set channel 3D mode for " << name << "\n";
                }
                else {
                    std::cerr << "[SoundEngine] Forcing channel to 3D mode for " << name << "\n";
                }
            }
        }
        // Ensure 3D level enabled
        channel->set3DLevel(1.0f);
    }

    // route to group: fallback heuristic (loop->Music, else SFX) but prefer named groups if present
    // Route to SFX group
    FMOD::ChannelGroup* targetGroup = nullptr;
    {
        std::scoped_lock lock(mMutex);
        auto it = mChannelGroups.find(loop ? "Music" : "SFX");
        if (it != mChannelGroups.end()) targetGroup = it->second;
    }
    if (channel && targetGroup) channel->setChannelGroup(targetGroup);

    // Convert position to FMOD_VECTOR and set 3D attributes (velocity zero)
    FMOD_VECTOR fpos = { position.x, position.y, position.z };
    FMOD_VECTOR fvel = { 0.0f, 0.0f, 0.0f };
    if (channel) {
        FMOD_RESULT r = channel->set3DAttributes(&fpos, &fvel);
        if (r != FMOD_OK) {
            std::cerr << "[SoundEngine] set3DAttributes failed for " << name << " result=" << r << std::endl;
        }

        // Debug: check channel mode to ensure it's 3D-capable
        FMOD_MODE chMode = FMOD_DEFAULT;
        if (channel->getMode(&chMode) == FMOD_OK) {
            if (!(chMode & FMOD_3D)) {
                std::cerr << "[SoundEngine] WARNING: Channel not 3D for " << name << "\n";
            }
        }

        channel->setVolume(1.0f);
        channel->setPaused(false);
        {
            std::scoped_lock lock(mMutex);
            mChannels[name] = channel;
            // Initialize base volume and let FMOD handle 3D attenuation
            mChannelBaseVolume[name] = 1.0f;
        }
    }
}

BOOM_API void SoundEngine::SetSoundPosition(const std::string& name, const glm::vec3& position)
{
    std::scoped_lock lock(mMutex);
    auto it = mChannels.find(name);
    if (it == mChannels.end() || !it->second) return;

    FMOD::Channel* channel = it->second;
    FMOD_VECTOR fpos = { position.x, position.y, position.z };
    FMOD_VECTOR fvel = { 0.0f, 0.0f, 0.0f };
    channel->set3DAttributes(&fpos, &fvel);
}

BOOM_API void SoundEngine::SetListenerAttributes(const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& forward, const glm::vec3& up)
{
    if (!mSystem) return;
    FMOD_VECTOR p = { pos.x, pos.y, pos.z };
    FMOD_VECTOR v = { vel.x, vel.y, vel.z };
    FMOD_VECTOR f = { forward.x, forward.y, forward.z };
    FMOD_VECTOR u = { up.x, up.y, up.z };
    mSystem->set3DListenerAttributes(0, &p, &v, &f, &u);

    // Store for debugging
    mLastListenerPos = pos;
    mLastListenerForward = forward;
    mLastListenerUp = up;
}

BOOM_API void SoundEngine::Set3DMinMaxDistance(const std::string& name, float minDist, float maxDist)
{
    std::scoped_lock lock(mMutex);

    // Try to set on the sound itself
    auto soundIt = mSounds.find(name);
    if (soundIt != mSounds.end() && soundIt->second) {
        soundIt->second->set3DMinMaxDistance(minDist, maxDist);
    }

    // Also try the 3D variant key
    const std::string soundKey3D = name + "|3D";
    auto sound3DIt = mSounds.find(soundKey3D);
    if (sound3DIt != mSounds.end() && sound3DIt->second) {
        sound3DIt->second->set3DMinMaxDistance(minDist, maxDist);
    }
}

BOOM_API glm::vec3 SoundEngine::GetListenerPosition() const
{
    return mLastListenerPos;
}
