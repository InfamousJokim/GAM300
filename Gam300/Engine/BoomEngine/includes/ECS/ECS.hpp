#pragma once
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include "Graphics/Utilities/Data.h"
#include "Auxiliaries/Assets.h"
#include "Physics/Utilities.h"  
#include "BoomProperties.h"
#include "AI/BehaviourTree.h"


namespace Boom {
    using EntityRegistry = entt::registry;
    using EntityID = entt::entity;
    constexpr EntityID NENTT = entt::null;

    //for now include only important components instead of all
    //new includes need to add into enum and string_view and within ComponentSelector() in InspectorPanel.cpp
    enum class ComponentID : size_t {
        INFO, TRANSFORM, CAMERA, RIGIDBODY, COLLIDER,
        MODEL, ANIMATOR, DIRECT_LIGHT, POINT_LIGHT, SPOT_LIGHT,
        SOUND, SCRIPT,
        THIRD_PERSON_CAMERA,
        NAV_AGENT_COMPONENT,
        AI_COMPONENT,
        COUNT
    };
    constexpr std::string_view COMPONENT_NAMES[]{
        "Info",                 //0
        "Transform",            //1
        "Camera",               //2
        "Rigidbody",            //3
        "Collider",             //4
        "Model",                //5
        "Animator",             //6
        "Direct Light",         //7
        "Point Light",          //8
        "Spot Light",           //9
        "Sound",                //10
        "Script",               //11
        "Third Person Camera" ,  //12
        "Nav Agent Component",
        "AI Component"

    };

    // transform component
    struct TransformComponent
    {
        BOOM_INLINE TransformComponent(const TransformComponent&) = default;
        BOOM_INLINE TransformComponent() = default;
        Transform3D transform;

        XPROPERTY_DEF
        ("TransformComponent", TransformComponent
            , obj_member<"Transform", &TransformComponent::transform>
        )
    }; 

    // camera component
    struct CameraComponent
    {
        BOOM_INLINE CameraComponent(const CameraComponent&) = default;
        BOOM_INLINE CameraComponent() = default;
        Camera3D camera;

        // CameraComponent
        XPROPERTY_DEF(
            "CameraComponent", CameraComponent,
            obj_member<"Camera", &CameraComponent::camera>
        )
    };

    struct EnttComponent {
        BOOM_INLINE EnttComponent(const EnttComponent&) = default;
        BOOM_INLINE EnttComponent() = default;
        std::string name = "Entity";

        // EnttComponent
        XPROPERTY_DEF(
            "EnttComponent", EnttComponent,
            obj_member<"name", &EnttComponent::name>
        )
    };

    struct MeshComponent
    {
        BOOM_INLINE MeshComponent(const MeshComponent&) = default;
        BOOM_INLINE MeshComponent() = default;
        Mesh3D mesh;

        // MeshComponent
        //XPROPERTY_DEF(
        //    "MeshComponent", MeshComponent,
        //    obj_member<"mesh", &MeshComponent::mesh>
        //)
    };

    struct RigidBodyComponent
    {
        BOOM_INLINE RigidBodyComponent(const RigidBodyComponent&) = default;
        BOOM_INLINE RigidBodyComponent() = default;
        RigidBody3D RigidBody;

        // RigidBodyComponent
        XPROPERTY_DEF(
            "RigidBodyComponent", RigidBodyComponent,
            obj_member<"RigidBody", &RigidBodyComponent::RigidBody>
        )
    };

    struct ColliderComponent
    {
        BOOM_INLINE ColliderComponent(const ColliderComponent&) = default;
        BOOM_INLINE ColliderComponent() = default;
        Collider3D Collider;

        // ColliderComponent
        XPROPERTY_DEF(
            "ColliderComponent", ColliderComponent,
            obj_member<"Collider", &ColliderComponent::Collider>
        )
    };

    ////Model Component
    struct ModelComponent {

        //using AssetID = uint64_t;

        AssetID modelID{ EMPTY_ASSET };
        AssetID materialID{ EMPTY_ASSET };
        std::string modelName;
        std::string materialName;
        std::string modelSource;
        std::string materialSource;

        XPROPERTY_DEF(
            "ModelComponent", ModelComponent,
            obj_member<"ModelID", &ModelComponent::modelID>,
            obj_member<"MaterialID", &ModelComponent::materialID>,
            obj_member<"ModelName", &ModelComponent::modelName>,
            obj_member<"MaterialName", &ModelComponent::materialName>,
            obj_member<"ModelSource", &ModelComponent::modelSource>,
            obj_member<"MaterialSource", &ModelComponent::materialSource>
        )

    };

    //Animator Component
    struct AnimatorComponent
    {
        BOOM_INLINE AnimatorComponent(const AnimatorComponent&) = default;
        BOOM_INLINE AnimatorComponent() = default;
        Animator3D animator;
        //std::vector<std::string> additionalAnimFiles;
    };

    struct SkyboxComponent {
        AssetID skyboxID{ EMPTY_ASSET };

        XPROPERTY_DEF(
            "SkyboxComponent", SkyboxComponent,
            obj_member<"SkyboxID", &SkyboxComponent::skyboxID>
        )
    };

    //helpful for encapsulating information about an entity
    //can be used for entity hierarchies (linked-list)
    struct InfoComponent {
        AssetID parent{ EMPTY_ASSET };
        std::string name{ "Entity" };
        AssetID uid{ RandomU64() };

        XPROPERTY_DEF(
            "InfoComponent", InfoComponent,
            obj_member<"Parent", &InfoComponent::parent>,
            obj_member<"Name", &InfoComponent::name>,
            obj_member<"UID", &InfoComponent::uid>
        )
    };
    BOOM_INLINE entt::entity FindEntityByName(entt::registry& reg, std::string_view name) {
        auto view = reg.view<const InfoComponent>();
        for (auto [e, info] : view.each()) {
            if (info.name == name) return e;
        }
        return entt::null;
    }
    struct DirectLightComponent
    {
        BOOM_INLINE DirectLightComponent(const DirectLightComponent&) = default;
        BOOM_INLINE DirectLightComponent() = default;
        DirectionalLight light;

        XPROPERTY_DEF(
            "DirectLightComponent", DirectLightComponent,
            obj_member<"Light", &DirectLightComponent::light>
        )
    };

    struct PointLightComponent
    {
        BOOM_INLINE PointLightComponent(const PointLightComponent&) = default;
        BOOM_INLINE PointLightComponent() = default;
        PointLight light;

        XPROPERTY_DEF(
            "PointLightComponent", PointLightComponent,
            obj_member<"Light", &PointLightComponent::light>
        )
    };

    struct SpotLightComponent
    {
        BOOM_INLINE SpotLightComponent(const SpotLightComponent&) = default;
        BOOM_INLINE SpotLightComponent() = default;
        SpotLight light;

        XPROPERTY_DEF(
            "SpotLightComponent", SpotLightComponent,
            obj_member<"Light", &SpotLightComponent::light>
        )
    };

    //Chris I have no idea how your sound component works
    struct SoundComponent
    {
        std::string name;      // logical name ("bgm", "jump", etc.)
        std::string filePath;  // actual sound file path
        bool loop = false;
        float volume = 1.0f;
        bool playOnStart = false;

        // --- ADD THIS CODE INSIDE THE STRUCT ---
        void serialize(nlohmann::json& j) const {
            j["name"] = name;
            j["filePath"] = filePath;
            j["loop"] = loop;
            j["volume"] = volume;
            j["playOnStart"] = playOnStart;
        }
        void deserialize(const nlohmann::json& j) {
            if (j.contains("name")) j.at("name").get_to(name);
            if (j.contains("filePath")) j.at("filePath").get_to(filePath);
            if (j.contains("loop")) j.at("loop").get_to(loop);
            if (j.contains("volume")) j.at("volume").get_to(volume);
            if (j.contains("playOnStart")) j.at("playOnStart").get_to(playOnStart);
        }
    };

    struct ScriptComponent
    {
        // Managed type name in C# (e.g., "PhysicsDropDemo" or "MyGame.PlayerController")
        std::string TypeName;

        // Runtime handle returned by script_create_instance(...).
        // Do NOT serialize this; it’s valid only while the game is running.
        uint64_t InstanceId = 0;

        // Allow toggling without removing the component
        bool Enabled = true;

        nlohmann::json Params = nlohmann::json::object();

        // ---- Serialization (save only authoring data) ----
        void serialize(nlohmann::json& j) const {
            j["TypeName"] = TypeName;
            j["Enabled"] = Enabled;
            if (!Params.is_null() && !Params.empty())
                j["Params"] = Params;

            // NOTE: InstanceId is intentionally NOT serialized (runtime only)
        }

        void deserialize(const nlohmann::json& j) {
            if (j.contains("TypeName")) j.at("TypeName").get_to(TypeName);
            if (j.contains("Enabled"))  j.at("Enabled").get_to(Enabled);
            if (j.contains("Params"))   j.at("Params").get_to(Params);

            // Ensure runtime handle starts cleared when loading a scene
            InstanceId = 0;
        }
    };

    struct ThirdPersonCameraComponent {
        AssetID targetUID = 0;       // The UID of the target entity
        glm::vec3 offset = glm::vec3(0.0f, 2.0f, -10.0f);
        float currentDistance = 2.0f;
        float minDistance = 2.0f;
        float maxDistance = 2.0f;
        float currentYaw = 0.0f;
        float currentPitch = 20.0f;
        float mouseSensitivity = 0.2f;
        float scrollSensitivity = 1.0f;

        // Add this back in
        XPROPERTY_DEF(
            "ThirdPersonCameraComponent", ThirdPersonCameraComponent,

            obj_member<"Offset", &ThirdPersonCameraComponent::offset>,
            obj_member<"Current Distance", &ThirdPersonCameraComponent::currentDistance>,
            obj_member<"Min Distance", &ThirdPersonCameraComponent::minDistance>,
            obj_member<"Max Distance", &ThirdPersonCameraComponent::maxDistance>,
            obj_member<"Current Yaw", &ThirdPersonCameraComponent::currentYaw>,
            obj_member<"Current Pitch", &ThirdPersonCameraComponent::currentPitch>,
            obj_member<"Mouse Sensitivity", &ThirdPersonCameraComponent::mouseSensitivity>,
            obj_member<"Scroll Sensitivity", &ThirdPersonCameraComponent::scrollSensitivity>
        )
    };
    struct NavAgentComponent {
        glm::vec3 target{ 0.f };
        std::vector<glm::vec3> path; // straight path
        int   waypoint = 0;
        float speed = 2.5f;  // m/s
        float arrive = 0.15f; // meters
        bool  active = true;
        bool  dirty = false; // set true when target changes
        std::string followName;
        entt::entity follow = entt::null; //this is player entity to follow
        float repathCooldown = 0.25f;     // seconds between path rebuilds
        float retargetDist = 0.5f;      // re-path if player moved this far
        float repathTimer = 0.f;       // internal timer
        XPROPERTY_DEF
        ("NavAgentComponent", NavAgentComponent
            , obj_member<"Target", &NavAgentComponent::target>
            , obj_member<"Speed", &NavAgentComponent::speed>
            , obj_member<"ArriveRadius", &NavAgentComponent::arrive>
            , obj_member<"Active", &NavAgentComponent::active>
            , obj_member<"RepathCooldown", &NavAgentComponent::repathCooldown>
            , obj_member<"RetargetDistance", &NavAgentComponent::retargetDist>
        )
    };
    struct AIComponent {
        enum class AIMode : int { Auto = 0, Idle = 1, Patrol = 2, Seek = 3 };
        AIMode mode = AIMode::Auto;   // exposed in Inspector
        AIMode lastMode = AIMode::Auto;
        float detectRadius = 8.0f;    // start seeking when within this distance
        float loseRadius = 12.0f;   // stop seeking when beyond this distance
        float idleWait = 1.0f;    // wait at patrol points
        float idleTimer = 0.0f;
        std::string playerName = "Samurai";   // find by name instead of PlayerTag
        entt::entity player = entt::null;     // cached after first successful lookup
        // Patrol
        std::vector<glm::vec3> patrolPoints;
        int patrolIndex = 0;


        // BT root
        BTNodePtr root;

        XPROPERTY_DEF
        ("AIComponent", AIComponent
            , obj_member<"DetectRadius", &AIComponent::detectRadius>
            , obj_member<"LoseRadius", &AIComponent::loseRadius>
            , obj_member<"IdleWait", &AIComponent::idleWait>
            , obj_member<"IdleTimer", &AIComponent::idleTimer>    // include if you want to see the live timer
            , obj_member<"PlayerName", &AIComponent::playerName>
           
            , obj_member<"PatrolIndex", &AIComponent::patrolIndex>
        )
    };
    struct Entity
    {
        BOOM_INLINE Entity(EntityRegistry* registry, EntityID entity) :
            m_Registry(registry), m_EnttID(entity)
        {
        }

        BOOM_INLINE Entity(EntityRegistry* registry) :
            m_Registry(registry)
        {
            m_EnttID = m_Registry->create();
        }

        BOOM_INLINE virtual ~Entity() = default;
        BOOM_INLINE Entity() = default;

        BOOM_INLINE operator EntityID ()
        {
            return m_EnttID;
        }

        BOOM_INLINE operator bool()
        {
            return m_Registry != nullptr &&
                m_Registry->valid(m_EnttID);
        }

        BOOM_INLINE EntityID ID()
        {
            return m_EnttID;
        }


        template<typename T, typename... Args>
        BOOM_INLINE T& Attach(Args&&... args)
        {
            return m_Registry->get_or_emplace<T>(m_EnttID, std::forward<Args>(args)...);
        }

        template<typename T>
        BOOM_INLINE void Detach()
        {
            m_Registry->remove<T>(m_EnttID);
        }

        BOOM_INLINE void Destroy()
        {
            if (m_Registry)
            {
                m_Registry->destroy(m_EnttID);
            }
        }

        template<typename T>
        BOOM_INLINE bool Has() const
        {
            return m_Registry != nullptr &&
                m_Registry->all_of<T>(m_EnttID);
        }

        template<typename T>
        BOOM_INLINE T& Get()
        {
            return m_Registry->get<T>(m_EnttID);
        }

    protected:
        EntityRegistry* m_Registry = nullptr;
        EntityID m_EnttID = NENTT;
    };
}
