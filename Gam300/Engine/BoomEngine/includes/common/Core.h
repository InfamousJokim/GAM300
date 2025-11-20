// Core.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.
#define _SILENCE_CXX20_CISO646_REMOVED_WARNING
#define _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS
#pragma once 
#ifndef CORE_H
#define CORE_H
#define GLM_ENABLE_EXPERIMENTAL //needed for GLM_GTX_component_wise as it is an experimental feature
#define GLM_DLL

#pragma warning(push)
#pragma warning(disable : 4101 4244 4267 4365 4458 4100 5054 4189 26819 6262 26495) //library warnings disable
// add headers that you want to pre-compile here
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <queue>
#include <vector>
#include <string>
#include <bitset>
#include <memory>
#include <sstream>
#include <fstream>
#include <assert.h>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <unordered_map>
#include <random>
#include<magic_enum.hpp>
#include <atomic>
#include <thread>
#include <chrono>
#pragma warning(pop)


// include spdlog
//#define FMT_HEADER_ONLY
//#define SPDLOG_FMT_EXTERNAL
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "framework.h"

#ifdef BOOM_EXPORT
	#define BOOM_API __declspec(dllexport)
#else
	#define BOOM_API __declspec(dllimport)
#endif

//-------------ASSERTIONS---------------

//runtime assertion
#define BOOM_ASSERT assert

//static assertion
#define BOOM_STATIC_ASSERT static_assert

// function inlining
#if defined(_MSC_VER)
	#define BOOM_INLINE __forceinline
	#define BOOM_NOINLINE __declspec(noinline)
#else
	#define BOOM_INLINE inline
	#define BOOM_NOINLINE
#endif

template <typename T>
BOOM_INLINE constexpr uint32_t TypeID()
{
	// Use type_info::hash_code() which is consistent across DLL boundaries
	return static_cast<uint32_t>(typeid(T).hash_code());
}

//-------------CONSOLE LOGGING----------------
#ifdef BOOM_ENABLE_LOG

	namespace Boom
	{
		/**
		 * @brief Generates a unique 32-bit identifier for a given type.
		 *
		 * This function uses the address of the `std::type_info` object for type `T`
		 * as the basis for the identifier, casted to a 32-bit unsigned integer.
		 *
		 * It is designed for fast, memory-efficient type identification within the engine.
		 *
		 * @tparam T The type for which the ID is generated.
		 * @return constexpr uint32_t A 32-bit unsigned integer representing the unique ID of the type.
		 *
		 * @note This method is fast but the ID is only guaranteed to be unique within a single program execution.
		 * It is not stable across different runs or builds.
		 *
		 * @warning On 64-bit systems, truncating the address to 32 bits may theoretically cause collisions,
		 * although in practice this is rare.
		 */
	


		// ----------------------------------------------------------------
		// Returns a process-wide, thread-safe spdlog logger instance.
		// Initialized on first call.
		// ----------------------------------------------------------------
		BOOM_API std::shared_ptr<spdlog::logger>& GetLogger();
	}

	// Convenience macros — expand to no-ops when logging is disabled:
	#define BOOM_TRACE(...) Boom::GetLogger()->trace(__VA_ARGS__)
	#define BOOM_DEBUG(...) Boom::GetLogger()->debug(__VA_ARGS__)
	#define BOOM_INFO(...)  Boom::GetLogger()->info(__VA_ARGS__)
	#define BOOM_WARN(...)  Boom::GetLogger()->warn(__VA_ARGS__)
	#define BOOM_ERROR(...) Boom::GetLogger()->error(__VA_ARGS__)
	#define BOOM_FATAL(...) Boom::GetLogger()->critical(__VA_ARGS__)

#else
	#define BOOM_TRACE
	#define BOOM_DEBUG
	#define BOOM_ERROR
	#define BOOM_FATAL
	#define BOOM_INFO
	#define BOOM_WARN
#endif //BOOM_ENABLE LOG

//---------FREE ALLOCATED MEMORY------------
//To improve pointer safety within engine context
#define BOOM_DELETE(ptr) if (ptr != nullptr) { delete (ptr); ptr = nullptr; }

namespace Boom {
	BOOM_INLINE uint64_t RandomU64() {
		static std::random_device rd{};
		static std::mt19937_64 gen{ rd() };
		static std::uniform_int_distribution<uint64_t> dis{}; //default covers all range
		return dis(gen);
	}
}

#endif //CORE_H
