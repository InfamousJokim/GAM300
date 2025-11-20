#pragma once
#include "Core.h"
//#include "BoomProperties.h"


// Forward declare xproperty types so Editor can use them
namespace xproperty::type { struct object; struct members; }
namespace xproperty::settings { struct context; }

namespace Boom {

    /**
     * @brief Macro to export property access for a component
     * Usage: DECLARE_COMPONENT_PROPERTY_API(TransformComponent)
     */
#define DECLARE_COMPONENT_PROPERTY_API(ComponentType) \
    BOOM_API const xproperty::type::object* Get##ComponentType##Properties(void* pComponent)

     // One line per component - that's it!
    DECLARE_COMPONENT_PROPERTY_API(TransformComponent);
    DECLARE_COMPONENT_PROPERTY_API(CameraComponent);
    DECLARE_COMPONENT_PROPERTY_API(ModelComponent);
    DECLARE_COMPONENT_PROPERTY_API(InfoComponent);
    DECLARE_COMPONENT_PROPERTY_API(DirectLightComponent);
    DECLARE_COMPONENT_PROPERTY_API(PointLightComponent);
    DECLARE_COMPONENT_PROPERTY_API(SpotLightComponent);
    DECLARE_COMPONENT_PROPERTY_API(SkyboxComponent);
    DECLARE_COMPONENT_PROPERTY_API(RigidBodyComponent);
    DECLARE_COMPONENT_PROPERTY_API(ColliderComponent);
    DECLARE_COMPONENT_PROPERTY_API(ThirdPersonCameraComponent);
    DECLARE_COMPONENT_PROPERTY_API(NavAgentComponent);
    DECLARE_COMPONENT_PROPERTY_API(AIComponent);
    DECLARE_COMPONENT_PROPERTY_API(SpriteComponent);

    // Add more as needed - one line each
#undef DECLARE_COMPONENT_PROPERTY_API
} // namespace Boom
