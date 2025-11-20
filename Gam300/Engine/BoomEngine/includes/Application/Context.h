#pragma once
#ifndef CONTEXT_H
#define CONTEXT_H
#include "AppWindow.h"
#include "Graphics/Renderer.h"
#include "GlobalConstants.h"
#include "Auxiliaries/Assets.h"
#include "ECS/ECS.hpp"
#include "Physics/Context.h"
#include "Auxiliaries/Profiler.h"
#include "Audio/Audio.hpp"

namespace Boom
{
	// Forward declarations
	struct AppInterface;
	class ScriptingSystem;

	/**
	* @brief Holds global state and owns all attached layers.
	*/
	struct AppContext
	{
		// DECLARE the constructor - don't define it yet
		BOOM_INLINE AppContext();

		/** @brief Destructor that deletes and nulls out all layer pointers. */
		~AppContext(); // Defined in Context.cpp

		/**
		 * @brief Container of all active layers in the application.
		 *
		 * Stores pointers to AppInterface-derived layers. Pointers
		 * are managed manually and cleaned up in the destructor.
		 */
		std::vector<AppInterface*> layers;
		EventDispatcher dispatcher;
		std::unique_ptr<AppWindow> window;
		std::unique_ptr<GraphicsRenderer> renderer;
		std::unique_ptr<PhysicsContext> physics;
		std::unique_ptr<AssetRegistry> assets;
		std::unique_ptr<ScriptingSystem> scriptingSystem;
		Boom::Profiler profiler;
		double DeltaTime{};
		EntityRegistry scene;
		bool ShowNavDebug = false;
		bool ShowNavCorridor = false;
	};
}// namespace Boom

// NOW include ScriptingSystem.h AFTER AppContext is fully declared
#include "Scripting/ScriptingSystem.h"

// NOW define the inline constructor with full knowledge of ScriptingSystem
namespace Boom
{
	/// BOOM_INLINE hints to the compiler to inline destructor calls
	/// reducing function-call overhead in the engine's core update loop
	BOOM_INLINE AppContext::AppContext()
		: dispatcher{}
		, window{ std::make_unique<AppWindow>(&dispatcher, CONSTANTS::WINDOW_WIDTH, CONSTANTS::WINDOW_HEIGHT, "Boom Engine") }
		, renderer{ std::make_unique<GraphicsRenderer>(CONSTANTS::WINDOW_WIDTH, CONSTANTS::WINDOW_HEIGHT) }
		, assets{ std::make_unique<AssetRegistry>() }
		, physics{ std::make_unique<PhysicsContext>() }
		, scriptingSystem{ std::make_unique<ScriptingSystem>() }
		, scene{}
	{
		SoundEngine::Instance().Init();
	}
}

#endif // CONTEXT_H