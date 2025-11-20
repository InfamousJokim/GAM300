#include "Core.h"
#include "Auxiliaries/PropertyAPI.h"
#include "BoomProperties.h"
#include "ECS/ECS.hpp"

namespace Boom {

    /**
     * @brief Macro to implement property access
     */
#define IMPLEMENT_COMPONENT_PROPERTY_API(ComponentType) \
    const xproperty::type::object* Get##ComponentType##Properties(void* pComponent) { \
        if (!pComponent) return xproperty::getObjectByType<ComponentType>(); \
        return xproperty::getObject(*static_cast<ComponentType*>(pComponent)); \
    }

     // One line per component - that's it!
    IMPLEMENT_COMPONENT_PROPERTY_API(TransformComponent)
        IMPLEMENT_COMPONENT_PROPERTY_API(CameraComponent)
        IMPLEMENT_COMPONENT_PROPERTY_API(ModelComponent)
        IMPLEMENT_COMPONENT_PROPERTY_API(InfoComponent)
        IMPLEMENT_COMPONENT_PROPERTY_API(DirectLightComponent)
        IMPLEMENT_COMPONENT_PROPERTY_API(PointLightComponent)
        IMPLEMENT_COMPONENT_PROPERTY_API(SpotLightComponent)
        IMPLEMENT_COMPONENT_PROPERTY_API(SkyboxComponent)
        IMPLEMENT_COMPONENT_PROPERTY_API(RigidBodyComponent)
        IMPLEMENT_COMPONENT_PROPERTY_API(ColliderComponent)
        IMPLEMENT_COMPONENT_PROPERTY_API(ThirdPersonCameraComponent)
        IMPLEMENT_COMPONENT_PROPERTY_API(NavAgentComponent);
        IMPLEMENT_COMPONENT_PROPERTY_API(AIComponent);
        IMPLEMENT_COMPONENT_PROPERTY_API(SpriteComponent);
        // Add more as needed - one line each

} // namespace Boom