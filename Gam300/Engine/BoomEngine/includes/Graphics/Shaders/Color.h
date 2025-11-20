#pragma once
#include "Shader.h"
#include "../Utilities/Quad.h"
#include "GlobalConstants.h"

namespace Boom {
	struct ColorShader : Shader {
		BOOM_INLINE ColorShader(std::string const& filename, glm::vec4 col)
			: Shader{ filename }
			, color{col}
			, colLoc{ GetUniformVar("color") }
			, quad{ CreateTestQuad2D() }
		{
		}
		BOOM_INLINE void Show() {
			Use();
			SetUniform(colLoc, color);
			quad->Draw(GL_TRIANGLE_STRIP);
			UnUse();
		}

	private:
		glm::vec4 color;
		int32_t colLoc;
		Quad2D quad;
	};

	struct Color3DShader : Shader {
		BOOM_INLINE Color3DShader(std::string const& filename, glm::vec4 col)
			: Shader{ filename }
			, color{ col }
			, colLoc{ GetUniformVar("color") }
			, modelLoc{ GetUniformVar("mat") }
			, model{ std::make_shared<StaticModel>("cube.fbx") }
		{
		}
		BOOM_INLINE void Show() {
			Use();
			SetUniform(colLoc, color);
			Camera3D cam{};
			Transform3D t{ {0.f, 0.f, -2.f}, {0.f, 30.f, 0.f}, glm::vec3{1.f} };
			glm::mat4 p{ cam.Projection(2.f) };
			glm::mat4 m{ p * cam.View({}) * t.Matrix() };
			SetUniform(modelLoc, m);
			model->Draw();
			UnUse();
		}

	private:
		glm::vec4 color;
		int32_t colLoc;
		int32_t modelLoc;
		Model3D model;
	};
}