#include "Core.h"
#include"BoomProperties.h"
#include "Auxiliaries\SerializationRegistry.h"
#include "ECS/ECS.hpp"

namespace Boom
{
    /**
     * @brief Generic component serializer using xproperty
     * Works for ANY component that has XPROPERTY_DEF defined
     */
    template<typename T>
    void RegisterPropertyComponent(const char* componentName)
    {
        auto& registry = SerializationRegistry::Instance();
        std::string name(componentName);

        registry.RegisterComponentSerializer(
            name,
            // ===== SERIALIZE =====
            [name](YAML::Emitter& e, EntityRegistry& scene, EntityID entity) {
                if (scene.all_of<T>(entity)) {
                    auto& comp = scene.get<T>(entity);

                    BOOM_INFO("[PropertySerializer] Serializing {} for entity {}", name, (uint32_t)entity);  // ADD THIS

                    e << YAML::Key << name << YAML::Value << YAML::BeginMap;

                    xproperty::settings::context ctx;
                    if (auto* pObj = xproperty::getObject(comp)) {
                        SerializeObjectToYAML(e, pObj, (void*)&comp, ctx);
                    }
                    else {
                        BOOM_ERROR("[PropertySerializer] Failed to get object info for {}", name);
                    }

                    e << YAML::EndMap;
                }
            },
            // ===== DESERIALIZE =====
            [name](const YAML::Node& node, EntityRegistry& scene, EntityID entity, AssetRegistry& /*assets*/) {
                BOOM_INFO("[PropertySerializer] Deserializing {} for entity {}", name, (uint32_t)entity);  // ADD THIS

                auto& comp = scene.get_or_emplace<T>(entity);

                if (node.IsMap()) {
                    xproperty::settings::context ctx;
                    if (auto* pObj = xproperty::getObject(comp)) {
                        DeserializeObjectFromYAML(node, pObj, (void*)&comp, ctx);
                        BOOM_INFO("[PropertySerializer] Successfully deserialized {}", name);  // ADD THIS
                    }
                    else {
                        BOOM_ERROR("[PropertySerializer] Failed to get object info for {} during deserialize", name);
                    }
                }
                else {
                    BOOM_WARN("[PropertySerializer] Node is not a map for {}", name);  // ADD THIS
                }
            }
        );
    }


    void RegisterAllComponentSerializers()
    {
        auto& registry = SerializationRegistry::Instance();

        // === INFO COMPONENT ===
        RegisterPropertyComponent<InfoComponent>("InfoComponent");

        // === TRANSFORM COMPONENT ===
        RegisterPropertyComponent<TransformComponent>("TransformComponent");

        // === CAMERA COMPONENT ===
        RegisterPropertyComponent<CameraComponent>("CameraComponent");

        // === RIGID BODY COMPONENT ===
		RegisterPropertyComponent<RigidBodyComponent>("RigidBodyComponent");

        // === COLLIDER COMPONENT ===
		RegisterPropertyComponent<ColliderComponent>("ColliderComponent");

        // === MODEL COMPONENT ===
		RegisterPropertyComponent<ModelComponent>("ModelComponent");

        // === ANIMATOR COMPONENT ===
        registry.RegisterComponentSerializer(
            "AnimatorComponent",
            // Serialize
            [](YAML::Emitter& e, EntityRegistry& reg, EntityID ent) {
                if (reg.all_of<AnimatorComponent>(ent)) {
                    auto& animatorComp = reg.get<AnimatorComponent>(ent);
                    auto& animator = animatorComp.animator;
                    if (!animator) return;

                    e << YAML::Key << "AnimatorComponent" << YAML::Value << YAML::BeginMap;

                    // Serialize clip file paths
                    e << YAML::Key << "Clips" << YAML::Value << YAML::BeginSeq;
                    for (size_t i = 0; i < animator->GetClipCount(); ++i) {
                        const auto* clip = animator->GetClip(i);
                        if (clip) {
                            e << YAML::BeginMap;
                            e << YAML::Key << "name" << YAML::Value << clip->name;
                            e << YAML::Key << "filePath" << YAML::Value << clip->filePath;
                            e << YAML::EndMap;
                        }
                    }
                    e << YAML::EndSeq;

                    // Serialize states
                    e << YAML::Key << "States" << YAML::Value << YAML::BeginSeq;
                    for (const auto& state : animator->GetStates()) {
                        e << YAML::BeginMap;
                        e << YAML::Key << "name" << YAML::Value << state.name;
                        e << YAML::Key << "clipIndex" << YAML::Value << state.clipIndex;
                        e << YAML::Key << "speed" << YAML::Value << state.speed;
                        e << YAML::Key << "loop" << YAML::Value << state.loop;

                        // Serialize transitions
                        e << YAML::Key << "transitions" << YAML::Value << YAML::BeginSeq;
                        for (const auto& trans : state.transitions) {
                            e << YAML::BeginMap;
                            e << YAML::Key << "targetStateIndex" << YAML::Value << trans.targetStateIndex;
                            e << YAML::Key << "conditionType" << YAML::Value << static_cast<int>(trans.conditionType);
                            e << YAML::Key << "parameterName" << YAML::Value << trans.parameterName;
                            e << YAML::Key << "floatValue" << YAML::Value << trans.floatValue;
                            e << YAML::Key << "boolValue" << YAML::Value << trans.boolValue;
                            e << YAML::Key << "transitionDuration" << YAML::Value << trans.transitionDuration;
                            e << YAML::Key << "hasExitTime" << YAML::Value << trans.hasExitTime;
                            e << YAML::Key << "exitTime" << YAML::Value << trans.exitTime;
                            e << YAML::EndMap;
                        }
                        e << YAML::EndSeq;
                        e << YAML::EndMap;
                    }
                    e << YAML::EndSeq;

                    // Serialize parameters
                    e << YAML::Key << "FloatParams" << YAML::Value << YAML::BeginMap;
                    for (const auto& [name, value] : animator->GetFloatParams()) {
                        e << YAML::Key << name << YAML::Value << value;
                    }
                    e << YAML::EndMap;

                    e << YAML::Key << "BoolParams" << YAML::Value << YAML::BeginMap;
                    for (const auto& [name, value] : animator->GetBoolParams()) {
                        e << YAML::Key << name << YAML::Value << value;
                    }
                    e << YAML::EndMap;

                    // Save current state
                    e << YAML::Key << "CurrentStateIndex" << YAML::Value << animator->GetCurrentStateIndex();

                    e << YAML::EndMap;
                }
            },
            // Deserialize
            [](const YAML::Node& data, EntityRegistry& reg, EntityID ent, AssetRegistry& assets) {
                // Create animator component
                auto& animatorComp = reg.get_or_emplace<AnimatorComponent>(ent);

                // Check if we should initialize from model or standalone
                if (reg.all_of<ModelComponent>(ent)) {
                    auto& modelComp = reg.get<ModelComponent>(ent);
                    ModelAsset& modelAsset = assets.Get<ModelAsset>(modelComp.modelID);
                    if (modelAsset.hasJoints) {
                        auto skeletalModel = std::dynamic_pointer_cast<SkeletalModel>(modelAsset.data);
                        if (skeletalModel && skeletalModel->GetAnimator()) {
                            animatorComp.animator = skeletalModel->GetAnimator()->Clone();
                        }
                    }
                }

                // Create animator if not from model
                if (!animatorComp.animator) {
                    animatorComp.animator = std::make_shared<Animator>();
                }

                auto& animator = animatorComp.animator;

                // Deserialize clips
                if (data["Clips"]) {
                    for (const auto& clipNode : data["Clips"]) {
                        std::string filePath = clipNode["filePath"].as<std::string>("");
                        std::string name = clipNode["name"].as<std::string>("");
                        if (!filePath.empty()) {
                            animator->LoadAnimationFromFile(filePath, name);
                        }
                    }
                }

                // Deserialize states
                if (data["States"]) {
                    for (const auto& stateNode : data["States"]) {
                        size_t stateIdx = animator->AddState(
                            stateNode["name"].as<std::string>("State"),
                            stateNode["clipIndex"].as<size_t>(0)
                        );
                        auto* state = animator->GetState(stateIdx);
                        if (state) {
                            state->speed = stateNode["speed"].as<float>(1.0f);
                            state->loop = stateNode["loop"].as<bool>(true);

                            // Deserialize transitions
                            if (stateNode["transitions"]) {
                                for (const auto& transNode : stateNode["transitions"]) {
                                    Animator::Transition trans;
                                    trans.targetStateIndex = transNode["targetStateIndex"].as<size_t>(0);
                                    trans.conditionType = static_cast<Animator::Transition::ConditionType>(
                                        transNode["conditionType"].as<int>(0));
                                    trans.parameterName = transNode["parameterName"].as<std::string>("");
                                    trans.floatValue = transNode["floatValue"].as<float>(0.0f);
                                    trans.boolValue = transNode["boolValue"].as<bool>(false);
                                    trans.transitionDuration = transNode["transitionDuration"].as<float>(0.25f);
                                    trans.hasExitTime = transNode["hasExitTime"].as<bool>(false);
                                    trans.exitTime = transNode["exitTime"].as<float>(0.9f);
                                    state->transitions.push_back(trans);
                                }
                            }
                        }
                    }
                }

                // Deserialize parameters
                if (data["FloatParams"]) {
                    for (const auto& param : data["FloatParams"]) {
                        animator->SetFloat(param.first.as<std::string>(), param.second.as<float>());
                    }
                }
                if (data["BoolParams"]) {
                    for (const auto& param : data["BoolParams"]) {
                        animator->SetBool(param.first.as<std::string>(), param.second.as<bool>());
                    }
                }

                // Restore current state
                if (data["CurrentStateIndex"]) {
                    animator->SetDefaultState(data["CurrentStateIndex"].as<size_t>(0));
                }

                BOOM_INFO("[AnimatorComponent] Deserialized with {} clips, {} states",
                    animator->GetClipCount(), animator->GetStateCount());
            }
        );
        registry.RegisterComponentSerializer(
            "NavAgentComponent",
            // ----- SERIALIZE -----
            [](YAML::Emitter& e, EntityRegistry& reg, EntityID ent)
            {
                if (!reg.all_of<NavAgentComponent>(ent))
                    return;

                auto& nav = reg.get<NavAgentComponent>(ent);

                e << YAML::Key << "NavAgentComponent" << YAML::Value << YAML::BeginMap;

                // target
                e << YAML::Key << "Target" << YAML::Value
                    << YAML::Flow << YAML::BeginSeq
                    << nav.target.x << nav.target.y << nav.target.z
                    << YAML::EndSeq;

                e << YAML::Key << "Speed" << YAML::Value << nav.speed;
                e << YAML::Key << "ArriveRadius" << YAML::Value << nav.arrive;
                e << YAML::Key << "Active" << YAML::Value << nav.active;
                e << YAML::Key << "RepathCooldown" << YAML::Value << nav.repathCooldown;
                e << YAML::Key << "RetargetDistance" << YAML::Value << nav.retargetDist;
                std::string followName = nav.followName;
                // If empty, but follow is valid, derive it from InfoComponent:
                if (followName.empty() &&
                    nav.follow != entt::null &&
                    reg.all_of<InfoComponent>(nav.follow)) {
                    followName = reg.get<InfoComponent>(nav.follow).name;
                }

                e << YAML::Key << "FollowName" << YAML::Value << followName;
                e << YAML::EndMap;
            },
            // ----- DESERIALIZE -----
            [](const YAML::Node& data, EntityRegistry& reg, EntityID ent, AssetRegistry&)
            {
                if (!data || !data.IsMap())
                    return;

                auto& nav = reg.get_or_emplace<NavAgentComponent>(ent);

                // Target (vec3 as [x, y, z])
                if (auto t = data["Target"]; t && t.IsSequence() && t.size() == 3) {
                    nav.target.x = t[0].as<float>(nav.target.x);
                    nav.target.y = t[1].as<float>(nav.target.y);
                    nav.target.z = t[2].as<float>(nav.target.z);
                }

                if (auto v = data["Speed"])            nav.speed = v.as<float>(nav.speed);
                if (auto v = data["ArriveRadius"])     nav.arrive = v.as<float>(nav.arrive);
                if (auto v = data["Active"])           nav.active = v.as<bool>(nav.active);
                if (auto v = data["RepathCooldown"])   nav.repathCooldown = v.as<float>(nav.repathCooldown);
                if (auto v = data["RetargetDistance"]) nav.retargetDist = v.as<float>(nav.retargetDist);

              

                // FollowName -> nav.follow
                if (auto f = data["FollowName"]) {
                    nav.followName = f.as<std::string>(nav.followName);
                    nav.follow = entt::null;   // will resolve lazily later
                    nav.dirty = true;         // so we build path once follow is resolved
                    nav.repathTimer = 0.f;
                }
                // nav.path, nav.waypoint, nav.dirty, nav.follow, nav.repathTimer
            }
        );
        registry.RegisterComponentSerializer(
            "AIComponent",
            // ---------- SERIALIZE ----------
            [](YAML::Emitter& e, EntityRegistry& reg, EntityID ent)
            {
                if (!reg.all_of<AIComponent>(ent))
                    return;

                auto& ai = reg.get<AIComponent>(ent);

                e << YAML::Key << "AIComponent" << YAML::Value << YAML::BeginMap;

                // Mode (store as int)
                e << YAML::Key << "Mode"
                    << YAML::Value << static_cast<int>(ai.mode);

                // Player name (we serialize the name, not the entt::entity)
                e << YAML::Key << "PlayerName"
                    << YAML::Value << ai.playerName;

                // Tuning
                e << YAML::Key << "DetectRadius" << YAML::Value << ai.detectRadius;
                e << YAML::Key << "LoseRadius" << YAML::Value << ai.loseRadius;
                e << YAML::Key << "IdleWait" << YAML::Value << ai.idleWait;

                // IdleTimer is runtime-only, so usually we reset it on load instead of saving.
              
                // e << YAML::Key << "IdleTimer"    << YAML::Value << ai.idleTimer;

                // Patrol points: list of [x, y, z]
                e << YAML::Key << "PatrolPoints" << YAML::Value << YAML::BeginSeq;
                for (const auto& p : ai.patrolPoints) {
                    e << YAML::Flow << YAML::BeginSeq
                        << p.x << p.y << p.z
                        << YAML::EndSeq;
                }
                e << YAML::EndSeq;

                e << YAML::Key << "PatrolIndex"
                    << YAML::Value << ai.patrolIndex;

                e << YAML::EndMap;
            },

            // ---------- DESERIALIZE ----------
            [](const YAML::Node& data, EntityRegistry& reg, EntityID ent, AssetRegistry&)
            {
                if (!data || !data.IsMap())
                    return;

                auto& ai = reg.get_or_emplace<AIComponent>(ent);

                // Mode
                if (auto v = data["Mode"]) {
                    int m = v.as<int>(static_cast<int>(ai.mode));
                    // Clamp to valid enum range (0..3 for Auto/Idle/Patrol/Seek)
                    if (m < 0 || m > static_cast<int>(AIComponent::AIMode::Seek))
                        m = static_cast<int>(AIComponent::AIMode::Auto);
                    ai.mode = static_cast<AIComponent::AIMode>(m);
                }

             
                if (auto v = data["PlayerName"]) {
                    ai.playerName = v.as<std::string>(ai.playerName);
                    ai.player = entt::null;  // resolved lazily using playerName
                }

                // Tuning
                if (auto v = data["DetectRadius"]) ai.detectRadius = v.as<float>(ai.detectRadius);
                if (auto v = data["LoseRadius"])   ai.loseRadius = v.as<float>(ai.loseRadius);
                if (auto v = data["IdleWait"])     ai.idleWait = v.as<float>(ai.idleWait);

                // Idle timer: reset on load
                ai.idleTimer = 0.0f;
                // If you serialized IdleTimer and want to restore:
                // if (auto v = data["IdleTimer"]) ai.idleTimer = v.as<float>(ai.idleTimer);

                // Patrol points
                ai.patrolPoints.clear();
                if (auto pts = data["PatrolPoints"]; pts && pts.IsSequence()) {
                    ai.patrolPoints.reserve(pts.size());
                    for (const auto& n : pts) {
                        if (!n.IsSequence() || n.size() != 3)
                            continue;
                        glm::vec3 p{};
                        p.x = n[0].as<float>(0.0f);
                        p.y = n[1].as<float>(0.0f);
                        p.z = n[2].as<float>(0.0f);
                        ai.patrolPoints.push_back(p);
                    }
                }

                // Patrol index (clamp to valid range)
                if (auto v = data["PatrolIndex"]) {
                    int idx = v.as<int>(ai.patrolIndex);
                    if (!ai.patrolPoints.empty()) {
                        idx = std::clamp(idx, 0, (int)ai.patrolPoints.size() - 1);
                        ai.patrolIndex = idx;
                    }
                    else {
                        ai.patrolIndex = 0;
                    }
                }
                else {
                    if (!ai.patrolPoints.empty())
                        ai.patrolIndex = std::clamp(ai.patrolIndex, 0, (int)ai.patrolPoints.size() - 1);
                    else
                        ai.patrolIndex = 0;
                }

                // Behaviour tree root / internal runtime stuff is NOT serialized.
                // AISystem should rebuild the BT on start based on 'mode', patrol list, etc.
            }
        );
        // === DIRECT LIGHT COMPONENT ===
		RegisterPropertyComponent<DirectLightComponent>("DirectLightComponent");

        // === POINT LIGHT COMPONENT ===
		RegisterPropertyComponent<PointLightComponent>("PointLightComponent");


        // === SPOT LIGHT COMPONENT ===
		RegisterPropertyComponent<SpotLightComponent>("SpotLightComponent");

        // === SKYBOX COMPONENT ===
		RegisterPropertyComponent<SkyboxComponent>("SkyboxComponent");

        // === THIRD PERSON CAMERA COMPONENT ===
        RegisterPropertyComponent<ThirdPersonCameraComponent>("ThirdPersonCameraComponent");

        RegisterPropertyComponent<SpriteComponent>("SpriteComponent");
        
       

        BOOM_INFO("[ComponentSerializers] All component serializers registered");
       
    }
 

   
}