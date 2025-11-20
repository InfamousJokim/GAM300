#include "Core.h"
#include "BoomProperties.h"
#include "Graphics\Utilities\Data.h"
#include "ECS/ECS.hpp"

using namespace Boom;

//===============DATA TYPES=================
XPROPERTY_REG(Transform3D)
XPROPERTY_REG(Camera3D)
XPROPERTY_REG(PbrMaterial)
XPROPERTY_REG(PointLight)
XPROPERTY_REG(DirectionalLight)
XPROPERTY_REG(SpotLight)
XPROPERTY_REG(Skybox)
XPROPERTY_REG(RigidBody3D)
XPROPERTY_REG(Collider3D)
XPROPERTY_REG(Texture2D)
XPROPERTY_REG(StaticModel)
XPROPERTY_REG(SkeletalModel)

//===============COMPONENTS=================
XPROPERTY_REG(TransformComponent)
XPROPERTY_REG(CameraComponent)
XPROPERTY_REG(EnttComponent)
//XPROPERTY_REG(MeshComponent)
XPROPERTY_REG(RigidBodyComponent)
XPROPERTY_REG(ColliderComponent)
XPROPERTY_REG(ModelComponent)
//XPROPERTY_REG(AnimatorComponent)
XPROPERTY_REG(SkyboxComponent)
XPROPERTY_REG(InfoComponent)
XPROPERTY_REG(DirectLightComponent)
XPROPERTY_REG(PointLightComponent)
XPROPERTY_REG(SpotLightComponent)
XPROPERTY_REG(ThirdPersonCameraComponent)
XPROPERTY_REG(SpriteComponent)

//===============ASSETS================= 
XPROPERTY_REG(MaterialAsset)
XPROPERTY_REG(TextureAsset)
XPROPERTY_REG(ModelAsset)
XPROPERTY_REG(SkyboxAsset)

