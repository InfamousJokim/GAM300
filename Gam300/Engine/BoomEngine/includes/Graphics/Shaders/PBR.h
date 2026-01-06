#pragma once
#include "Shader.h"
#include "../Utilities/Data.h"
#include "../Models/Model.h"

namespace Boom {
	//UBO must also change in pbr.glsl limits
	inline constexpr int MAX_POINT_LIGHTS = 32;
	inline constexpr int MAX_DIR_LIGHTS = 32;
	inline constexpr int MAX_SPOT_LIGHTS = 128;
	struct GPUPointLight {
		glm::vec4 position_range;       // xyz = position, w = range
		glm::vec4 radiance_intensity;   // rgb = radiance, w = intensity
	};

	struct GPUDirLight {
		glm::vec4 dir_intensity;        // xyz = direction, w = intensity
		glm::vec4 radiance;             // rgb = radiance, w unused
	};

	struct GPUSpotLight {
		glm::vec4 position_falloff;     // xyz = position, w = fallOff
		glm::vec4 dir_cutoff;           // xyz = direction, w = cutOff
		glm::vec4 radiance_intensity;   // rgb = radiance, w = intensity
	};
	struct PBRShader : Shader {
		BOOM_INLINE PBRShader(std::string const& filename)
			: Shader{ filename }
			, noSpotLightLoc{ GetUniformVar("noSpotLight") }
			, noDirLightLoc{ GetUniformVar("noDirLight") }
			, noPointLightLoc{ GetUniformVar("noPointLight") }

			, roughnessMapLoc{ GetUniformVar("material.roughnessMap") }
			, occlusionMapLoc{ GetUniformVar("material.occlusionMap") }
			, emissiveMapLoc{ GetUniformVar("material.emissiveMap") }
			, metallicMapLoc{ GetUniformVar("material.metallicMap") }
			, albedoMapLoc{ GetUniformVar("material.albedoMap") }
			, normalMapLoc{ GetUniformVar("material.normalMap") }

			, isRoughnessMapLoc{ GetUniformVar("material.isRoughnessMap") }
			, isOcclusionMapLoc{ GetUniformVar("material.isOcclusionMap") }
			, isEmissiveMapLoc{ GetUniformVar("material.isEmissiveMap") }
			, isMetallicMapLoc{ GetUniformVar("material.isMetallicMap") }
			, isAlbedoMapLoc{ GetUniformVar("material.isAlbedoMap") }
			, isNormalMapLoc{ GetUniformVar("material.isNormalMap") }

			, albedoLoc{ GetUniformVar("material.albedo") }
			, roughLoc{ GetUniformVar("material.roughness") }
			, metalLoc{ GetUniformVar("material.metallic") }
			, occlusionLoc{ GetUniformVar("material.occlusion") }
			, emissiveLoc{ GetUniformVar("material.emissive") }

			, frustumMatLoc{ GetUniformVar("frustumMat") }
			, modelMatLoc{ GetUniformVar("modelMat") }
			, viewPosLoc{ GetUniformVar("viewPos") }

			, jointsLoc{ GetUniformVar("hasJoints") }
			, isDebugModeLoc{ GetUniformVar("isDebugMode") }
			, ditherThresholdLoc{ GetUniformVar("ditherThreshold") }
			, showNormalTextureLoc{ GetUniformVar("showNormalTexture") }
			, ambientStrengthLoc{ GetUniformVar("ambientStrength") }
			, u_LightSpace{ GetUniformVar("u_lightSpace") }
			, u_DepthMap{ GetUniformVar("u_depthMap") }
		{
			GLuint prog = shaderId; 

			GLuint plIndex = glGetUniformBlockIndex(prog, "PointLightBlock");
			if (plIndex != GL_INVALID_INDEX) {
				glUniformBlockBinding(prog, plIndex, 0);
			}

			GLuint dlIndex = glGetUniformBlockIndex(prog, "DirLightBlock");
			if (dlIndex != GL_INVALID_INDEX) {
				glUniformBlockBinding(prog, dlIndex, 1);
			}

			GLuint slIndex = glGetUniformBlockIndex(prog, "SpotLightBlock");
			if (slIndex != GL_INVALID_INDEX) {
				glUniformBlockBinding(prog, slIndex, 2);
			}
		}

	public: //lights
		
		template<class TYPE>
		void SetLight(TYPE const& light, Transform3D const& transform, int32_t index) {
			static_assert(
				std::is_same<TYPE, PointLight>::value ||
				std::is_same<TYPE, DirectionalLight>::value ||
				std::is_same<TYPE, SpotLight>::value,
				"SetLight<TYPE>() only supports Spot, Point or Directional light.");
		}
		template <>
		BOOM_INLINE void SetLight(SpotLight const& light, Transform3D const& transform, int32_t index) {
			std::string intensity{ "spotLights[" + std::to_string(index) + "].intensity" };
			std::string direction{ "spotLights[" + std::to_string(index) + "].dir" };
			std::string radiance{ "spotLights[" + std::to_string(index) + "].radiance" };
			std::string position{ "spotLights[" + std::to_string(index) + "].position" };
			std::string fallOff{ "spotLights[" + std::to_string(index) + "].fallOff" };
			std::string cutOff{ "spotLights[" + std::to_string(index) + "].cutOff" };

			SetUniform(GetUniformVar(radiance), light.radiance);
			SetUniform(GetUniformVar(direction), transform.rotate);
			SetUniform(GetUniformVar(intensity), light.intensity);
			SetUniform(GetUniformVar(position), transform.translate);
			SetUniform(GetUniformVar(fallOff), light.fallOff);
			SetUniform(GetUniformVar(cutOff), light.cutOff);
		}
		template <>
		BOOM_INLINE void SetLight(DirectionalLight const& light, Transform3D const& transform, int32_t index) {
			std::string intensity{ "dirLights[" + std::to_string(index) + "].intensity" };
			std::string direction{ "dirLights[" + std::to_string(index) + "].dir" };
			std::string radiance{ "dirLights[" + std::to_string(index) + "].radiance" };

			SetUniform(GetUniformVar(radiance), light.radiance);
			SetUniform(GetUniformVar(direction), transform.rotate);
			SetUniform(GetUniformVar(intensity), light.intensity);
		}
		template <>
		BOOM_INLINE void SetLight(PointLight const& light, Transform3D const& transform, int32_t index) {
			std::string intensity{ "pointLights[" + std::to_string(index) + "].intensity" };
			std::string radiance{ "pointLights[" + std::to_string(index) + "].radiance" };
			std::string position{ "pointLights[" + std::to_string(index) + "].position" };
			std::string range{ "pointLights[" + std::to_string(index) + "].range" };
			SetUniform(GetUniformVar(position), transform.translate);
			SetUniform(GetUniformVar(radiance), light.radiance);
			SetUniform(GetUniformVar(intensity), light.intensity);
			SetUniform(GetUniformVar(range), light.range);
		}
		BOOM_INLINE void SetSpotLightCount(int32_t count) {
			SetUniform(noSpotLightLoc, count);
		}
		BOOM_INLINE void SetDirectionalLightCount(int32_t count) {
			SetUniform(noDirLightLoc, count);
		}
		BOOM_INLINE void SetPointLightCount(int32_t count) {
			SetUniform(noPointLightLoc, count);
		}
		BOOM_INLINE void SetLightSpaceMatrix(const glm::mat4& lightSpaceMtx)
		{
			// set view projection matrix
			SetUniform(u_LightSpace, lightSpaceMtx);
		}
	public:
		BOOM_INLINE void SetEnvMaps(uint32_t, uint32_t, uint32_t, uint32_t depthMap) {
			Use();

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthMap);
			glUniform1i(u_DepthMap, 0);
		}
		BOOM_INLINE void SetCamera(Camera3D const& cam, Transform3D const& transform, float ratio) {
			SetUniform(frustumMatLoc, cam.Frustum(transform, ratio));
			SetUniform(viewPosLoc, transform.translate);
		}
		BOOM_INLINE void Draw(Mesh3D const& mesh, Transform3D const& transform) {
			SetUniform(isDebugModeLoc, false); 
			SetUniform(ditherThresholdLoc, showDither ? ditherThreshold : 0.f);
			SetUniform(showNormalTextureLoc, false);
			SetUniform(modelMatLoc, transform.Matrix());
			mesh->Draw(GL_TRIANGLES);
		}

		BOOM_INLINE void SetMaterial(PbrMaterial const& material, int32_t unit) {
			glBindTexture(GL_TEXTURE_2D, 0);
			SetUniform(albedoLoc, material.albedo);
			SetUniform(roughLoc, material.roughness);
			SetUniform(metalLoc, material.metallic);
			SetUniform(emissiveLoc, material.emissive);
			SetUniform(occlusionLoc, material.occlusion);

			bool isMap{};
			isMap = material.albedoMap != nullptr;
			SetUniform(isAlbedoMapLoc, isMap);
			if (isMap) {
				material.albedoMap->Use(albedoMapLoc, unit++);
			}

			isMap = material.normalMap != nullptr;
			SetUniform(isNormalMapLoc, isMap);
			if (isMap) {
				material.normalMap->Use(normalMapLoc, unit++);
			}

			isMap = material.metallicMap != nullptr;
			SetUniform(isMetallicMapLoc, isMap);
			if (isMap) {
				material.metallicMap->Use(metallicMapLoc, unit++);
			}

			isMap = material.emissiveMap != nullptr;
			SetUniform(isEmissiveMapLoc, isMap);
			if (isMap) {
				material.emissiveMap->Use(emissiveMapLoc, unit++);
			}

			isMap = material.occlusionMap != nullptr;
			SetUniform(isOcclusionMapLoc, isMap);
			if (isMap) {
				material.occlusionMap->Use(occlusionMapLoc, unit++);
			}

			isMap = material.roughnessMap != nullptr;
			SetUniform(isRoughnessMapLoc, isMap);
			if (isMap) {
				material.roughnessMap->Use(roughnessMapLoc, unit++);
			}
		}

		BOOM_INLINE void Draw(Model3D const& model, Transform3D const& transform, PbrMaterial const& material, bool showNormal = false) {
			Use();
			SetUniform(isDebugModeLoc, false);
			SetUniform(ditherThresholdLoc, showDither ? ditherThreshold : 0.f);
			SetUniform(showNormalTextureLoc, showNormal);
			SetUniform(ambientStrengthLoc, ambientStrength);
			//world transformation * model transformation
			SetUniform(modelMatLoc, transform.Matrix() * model->modelTransform.Matrix());
			
			//material texture maps
			SetMaterial(material, 1);

			SetUniform(jointsLoc, model->HasJoint());
			model->Draw();
		}
		
		BOOM_INLINE void DrawDebug(Model3D const& model, Transform3D const& transform, glm::vec3 albedo, bool showNormal = false) {
			Use();
			SetUniform(isDebugModeLoc, true);
			SetUniform(ditherThresholdLoc, showDither ? ditherThreshold : 0.f);
			SetUniform(showNormalTextureLoc, showNormal);
			SetUniform(modelMatLoc, transform.Matrix() * model->modelTransform.Matrix());
			SetUniform(albedoLoc, albedo);
			SetUniform(ambientStrengthLoc, ambientStrength);
			SetUniform(jointsLoc, model->HasJoint());
			model->Draw(GL_LINES);
		}

		//Animation 
		BOOM_INLINE void SetJoints(std::vector<glm::mat4>& transforms)
		{
			for (size_t i = 0; i < transforms.size() && i < 100; ++i)
			{
				std::string uniform = "jointsMat[" + std::to_string(i) + "]";
				SetUniform(GetUniformVar(uniform.c_str()), transforms[i]);
			}
		}

		bool showDither{};
		float ditherThreshold{ 0.1f };
		float ambientStrength{ 0.1f };
	private:
		int32_t noSpotLightLoc;
		int32_t noDirLightLoc;
		int32_t noPointLightLoc;
		
		int32_t roughnessMapLoc;
		int32_t occlusionMapLoc;
		int32_t emissiveMapLoc;
		int32_t metallicMapLoc;
		int32_t albedoMapLoc;
		int32_t normalMapLoc;

		int32_t isRoughnessMapLoc;
		int32_t isOcclusionMapLoc;
		int32_t isEmissiveMapLoc;
		int32_t isMetallicMapLoc;
		int32_t isAlbedoMapLoc;
		int32_t isNormalMapLoc;

		int32_t albedoLoc;
		int32_t roughLoc;
		int32_t metalLoc;
		int32_t occlusionLoc;
		int32_t emissiveLoc;

		int32_t frustumMatLoc;
		int32_t modelMatLoc;
		int32_t viewPosLoc;

		int32_t jointsLoc;
		int32_t isDebugModeLoc;
		int32_t ditherThresholdLoc;
		int32_t showNormalTextureLoc;

		int32_t u_LightSpace = 0;
		int32_t u_DepthMap = 0;

		int32_t ambientStrengthLoc;
	};
}
