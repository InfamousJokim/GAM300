#pragma once
#include <tuple>
#include <string_view>
#include <glm/vec3.hpp>

namespace Boom {

	using EntityId = std::uint32_t;
	using Vec3 = glm::vec3;
	extern float g_ScreenFadeAlpha;

	namespace CONSTANTS {
		int const CHAR_BUFFER_SIZE{ 256 };
		constexpr std::tuple<float, float, float, float> DEFAULT_BACKGROUND_COLOR = { .3f, .3f, .3f, 1.f };
		constexpr std::string_view SHADERS_LOCATION = "Resources/Shaders/";
		constexpr float CAM_PAN_SPEED{ .25f };
		constexpr float CAM_RUN_MULTIPLIER{ 1.5f };
		constexpr float MIN_FOV{ 45.f };
		constexpr float MAX_FOV{ 100.f };
		int const WINDOW_WIDTH = 1800;
		int const WINDOW_HEIGHT = 900;
		float const PI = 3.14159265358979323846f;
		constexpr std::string_view DND_PAYLOAD_TEXTURE{ "DND_TEX" };
		constexpr std::string_view DND_PAYLOAD_MODEL{ "DND_MDL" };
		constexpr std::string_view DND_PAYLOAD_MATERIAL{ "DND_MAT" };
		constexpr std::string_view DND_PAYLOAD_ANIM_FILE{ "DND_ANIM" };
		constexpr std::string_view DND_PAYLOAD_SKYBOX{ "DND_SKYBOX" };
		constexpr std::string_view COMPRESSED_TEXTURE_OUTPUT_PATH{ "CompressedTexture" };
		constexpr std::string_view DND_PAYLOAD_PHYSICS_MESH{ "DND_PHY_MSH" };
		constexpr std::string_view DND_PAYLOAD_VIDEO{ "DND_VID" };
	}
}