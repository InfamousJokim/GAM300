#pragma once
#include "../Buffers/Mesh.h"
#include "../Textures/Texture.h"
#include "BoomProperties.h"
#include "GlobalConstants.h"

//helper functions
namespace Boom {
	BOOM_INLINE glm::highp_mat4 GetRotationMatrix(glm::vec3 const& rot) {
		return glm::toMat4(glm::quat(glm::radians(rot)));
	}
}

namespace Boom {


	struct Transform3D {
		BOOM_INLINE Transform3D() : translate{}, rotate{}, scale{ 1.f } {}
		BOOM_INLINE Transform3D(Transform3D const& t) = default;
		BOOM_INLINE Transform3D(glm::vec3 t, glm::vec3 r, glm::vec3 s)
			: translate{ t }
			, rotate{ r }
			, scale{ s }
		{
		}

		BOOM_INLINE glm::mat4 Matrix() const {
			return //TRS
				glm::translate(glm::mat4(1.f), translate) *
				GetRotationMatrix(rotate) *
				glm::scale(glm::mat4(1.f), scale);
		}

		glm::vec3 translate;
		glm::vec3 rotate;
		glm::vec3 scale;

		XPROPERTY_DEF
		("Transform3D", Transform3D
			, obj_member<"Translate", &Transform3D::translate>
			, obj_member<"Rotate", &Transform3D::rotate>
			, obj_member<"Scale", &Transform3D::scale>
		)

	};
	
	struct Transform2D {
		BOOM_INLINE Transform2D() : translate{}, rotate{}, scale{ 1.f } {}
		BOOM_INLINE Transform2D(Transform2D const& t) = default;
		BOOM_INLINE Transform2D(glm::vec3 t, float r, glm::vec2 s)
			: translate{ t }
			, rotate{ r }
			, scale{ s }
		{
		}
		BOOM_INLINE Transform2D(Transform3D const& t)
			: translate{ t.translate.x,  t.translate.y, t.translate.z }
			, rotate{ t.rotate.z }
			, scale{ t.scale.x, t.scale.y }
		{
		}
		BOOM_INLINE glm::mat3 Matrix() const {
			float rad{ glm::radians(rotate) };
			return {  cosf(rad) * scale.x, -sinf(rad) * scale.x, 0.f,
					  sinf(rad) * scale.y,  cosf(rad) * scale.y, 0.f,
					  translate.x,		    translate.y,		 1.f };
		}

		glm::vec3 translate; //needs to be vec3 due to z-rendering order
		float rotate;
		glm::vec2 scale;

		XPROPERTY_DEF
		("Transform2D", Transform2D
			, obj_member<"Translate", &Transform2D::translate>
			, obj_member<"Rotate", &Transform2D::rotate>
			, obj_member<"Scale", &Transform2D::scale>
		)
	};

	struct Camera3D {
		//transform here refers to the camera's transformation variables
		BOOM_INLINE glm::mat4 Frustum(Transform3D const& transform, float ratio) const {
			return Projection(ratio) * View(transform);
		}
		//transform here refers to the camera's transformation variables
		BOOM_INLINE glm::mat4 View(Transform3D const& transform) const {
			return glm::inverse(transform.Matrix());

			/*
			glm::quat rotQuat{ glm::radians(transform.rotate) };
			glm::vec3 forward{ rotQuat * glm::vec3{0.f, 0.f, -1.f} };
			glm::vec3 up{ rotQuat * glm::vec3{0.f, 1.f, 0.f} };

			return glm::lookAt(
				transform.translate,			//position
				transform.translate + forward, 	//target
				up								//upwards direction
			);*/
		}
		BOOM_INLINE glm::mat4 Projection(float ratio) const {
			return glm::perspective(glm::radians(FOV), ratio, nearPlane, farPlane);
		}
		BOOM_INLINE void SetFOV(float fovDeg) {
			FOV = glm::clamp(fovDeg, CONSTANTS::MIN_FOV, CONSTANTS::MAX_FOV);
		}

		float nearPlane{0.3000f};
		float farPlane{1000.f};
		float FOV{45.f};

		XPROPERTY_DEF(
			"Camera3D", Camera3D,
			obj_member<"NearPlane", &Camera3D::nearPlane>,
			obj_member<"FarPlane", &Camera3D::farPlane>,
			obj_member<"FOV", &Camera3D::FOV>
		)
			//enum of camera types
			enum CameraType {
			Main,
			Sub
		}cameraType;
	};

	struct PbrMaterial {
		BOOM_INLINE PbrMaterial()
			: emissive{}
			, albedo{ glm::vec3(1.f) }
			, roughness{ 0.4f }
			, metallic{ 0.5f }
			, occlusion{ 1.f }
			, occlusionMap{}, roughnessMap{}, metallicMap{}, emissiveMap{}, albedoMap{}, normalMap{}
		{
		}
		BOOM_INLINE PbrMaterial(PbrMaterial const&) = default;
		BOOM_INLINE PbrMaterial(glm::vec3 em, glm::vec3 alb, float rough, float metal, float occlu)
			: emissive{ em }
			, albedo{ alb }
			, roughness{ rough }
			, metallic{ metal }
			, occlusion{ occlu }
			, occlusionMap{}, roughnessMap{}, metallicMap{}, emissiveMap{}, albedoMap{}, normalMap{}
		{
		}
		glm::vec3 emissive;
		glm::vec3 albedo;
		float roughness;
		float metallic;
		float occlusion;

		Texture occlusionMap;
		Texture roughnessMap;
		Texture metallicMap;
		Texture emissiveMap;
		Texture albedoMap;
		Texture normalMap;

		// ===== PbrMaterial =====
		XPROPERTY_DEF(
			"PbrMaterial", PbrMaterial,
			obj_member<"emissive", &PbrMaterial::emissive>,
			obj_member<"albedo", &PbrMaterial::albedo>,
			obj_member<"roughness", &PbrMaterial::roughness>,
			obj_member<"metallic", &PbrMaterial::metallic>,
			obj_member<"occlusion", &PbrMaterial::occlusion>
		)


	};

	struct PointLight {
		BOOM_INLINE PointLight(glm::vec3 radi = glm::vec3(1.f), float intense = 1.f)
			: radiance{ radi }, intensity{ intense }
		{

		}

		glm::vec3 radiance;
		float intensity;

		// ===== PointLight =====
		XPROPERTY_DEF(
			"PointLight", PointLight,
			obj_member<"Radiance", &PointLight::radiance>,
			obj_member<"Intensity", &PointLight::intensity>
		)

	};

	struct DirectionalLight {
		BOOM_INLINE DirectionalLight(glm::vec3 radi = glm::vec3(1.f), float intense = 2.f)
			: radiance{ radi }, intensity{ intense }
		{
		}

		glm::vec3 radiance;
		float intensity;

		// ===== DirectionalLight =====
		XPROPERTY_DEF(
			"DirectionalLight", DirectionalLight,
			obj_member<"Radiance", &DirectionalLight::radiance>,
			obj_member<"Intensity", &DirectionalLight::intensity>
		)

	};

	struct SpotLight {
		//falloff and cutoff angles should be inputted as degrees
		BOOM_INLINE SpotLight(
			glm::vec3 radi = glm::vec3(1.f),
			float intense = 3.5f,
			float fall = 60.5f,
			float cut = 20.f
		)
			: radiance{ radi }
			, intensity{ intense }
			, fallOff{ glm::radians(fall) }
			, cutOff{ glm::radians(cut) }
		{
			assert(fallOff > cutOff && "Innter cone angle must be smaller than outer cone");
		}

		glm::vec3 radiance;
		float intensity;
		float fallOff;
		float cutOff;

		// ===== SpotLight =====
		XPROPERTY_DEF(
			"SpotLight", SpotLight,
			obj_member<"Radiance", &SpotLight::radiance>,
			obj_member<"Intensity", &SpotLight::intensity>,
			obj_member<"FallOff", &SpotLight::fallOff>,  // stored in radians
			obj_member<"CutOff", &SpotLight::cutOff>    // stored in radians
		)

	};

	struct Skybox {
		uint32_t cubeMap{};

		// ===== Skybox =====
		XPROPERTY_DEF(
			"Skybox", Skybox,
			obj_member<"CubeMap", &Skybox::cubeMap>
		)


	};

	struct Bounds {
		//Sphere
		glm::vec3 localCentre{};
		float localRadius{ 0.f };
	};
}