#pragma once
#include "Shader.h"

namespace Boom {
	struct PickingShader : Shader {
		BOOM_INLINE PickingShader(std::string const& filename)
			: Shader{ filename }

			, frustumMatLoc{ GetUniformVar("frustumMat") }
			, modelMatLoc{ GetUniformVar("modelMat") }
			, jointsLoc{ GetUniformVar("hasJoints") }
			, enttIDLoc{ GetUniformVar("uEntityID") }
		{
		}

		BOOM_INLINE void SetCamera(Camera3D const& cam, Transform3D const& transform, float ratio) {
			SetUniform(frustumMatLoc, cam.Frustum(transform, ratio));
		}

		BOOM_INLINE void SetIDUniform(uint32_t id) {
			SetUniform(enttIDLoc, id);
		}

		BOOM_INLINE void Draw(Model3D const& model, Transform3D const& transform) {
			Use();
			SetUniform(modelMatLoc, transform.Matrix() * model->modelTransform.Matrix());
			SetUniform(jointsLoc, model->HasJoint());
			model->Draw();
		}
		BOOM_INLINE void Draw(Transform3D const& transform) {
			static Quad3D base{ CreateQuad3D() };
			Use();
			SetUniform(modelMatLoc, transform.Matrix());
			SetUniform(jointsLoc, false);
			base->Draw(GL_TRIANGLE_STRIP);
		}
		BOOM_INLINE void Draw(Transform2D const& transform) {
			static Quad2D base{ CreateQuad2D() };
			Use();
			SetUniform(frustumMatLoc, glm::mat4(1.f));
			SetUniform(modelMatLoc, transform.To3D().Matrix());
			SetUniform(jointsLoc, false);
			base->Draw(GL_TRIANGLE_STRIP);
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

	private:
		int32_t frustumMatLoc;
		int32_t modelMatLoc;
		int32_t jointsLoc;
		int32_t enttIDLoc;
	};
}