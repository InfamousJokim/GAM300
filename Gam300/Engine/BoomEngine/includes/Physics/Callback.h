#pragma once
#include "Helpers.h"
#include "ECS/ECS.hpp"

#include "common/Core.h"

// Using the physx namespace for brevity
using namespace physx;

namespace Boom {
	enum class PxEvent {
		UNKNOWN = 0,
		TRIGGER,
		CONTACT,
		SLEEP,
		WAKE
	};

	struct PxPayload {
		EntityID Entity1 = NENTT;
		EntityID Entity2 = NENTT;
		PxEvent Event = PxEvent::UNKNOWN;
	};

	using PxCallbackFunction = std::function<void(const
		PxPayload&)>;

	struct PxEventCallback : public PxSimulationEventCallback
	{
		// override the onContact callback
		BOOM_INLINE void onContact(const PxContactPairHeader&
			header, [[maybe_unused]] const PxContactPair* pairs, [[maybe_unused]] PxU32 nbPairs) override
		{
			// collision actors
			auto actor1 = header.actors[0];
			auto actor2 = header.actors[1];

			// check if actors
			if (m_Callback && actor1 && actor2 && actor1->userData && actor2->userData)
			{
				PxPayload event;
				event.Event = PxEvent::CONTACT;
				event.Entity1 = *static_cast<EntityID*>
					(actor1->userData);
				event.Entity2 = *static_cast<EntityID*>
					(actor2->userData);
				m_Callback(event);
			}

			BOOM_DEBUG("onContact Event!");
		}

		// override the onTrigger callback
		BOOM_INLINE void onTrigger(PxTriggerPair* pairs,
			PxU32 nbPairs) override
		{
			for (PxU32 i = 0; m_Callback && i < nbPairs;
				++i)
			{
				// Access actor pointers from the trigger pair
				const PxRigidActor* actor0 = pairs[i].triggerActor;
				const PxRigidActor* actor1 = pairs[i].otherActor;

				// Check if actors are valid before using
				if (actor0 && actor1 && actor0->userData && actor1->userData)
				{
					PxPayload event;
					event.Event = PxEvent::TRIGGER;
					event.Entity1 = *static_cast<EntityID*>(actor1->userData);
					event.Entity2 = *static_cast<EntityID*>(actor0->userData);
					m_Callback(event);
				}
			}
			BOOM_DEBUG("onTrigger Event!");
		}

		BOOM_INLINE void onAdvance([[maybe_unused]] const PxRigidBody* const*
			bodyBuffer,
			[[maybe_unused]] const PxTransform* poseBuffer, [[maybe_unused]] const PxU32 count)
			override {
		}

		BOOM_INLINE void onSleep([[maybe_unused]] PxActor** actors, [[maybe_unused]] PxU32
			count)
			override {
			BOOM_DEBUG("onSleep Event!");
		}

		BOOM_INLINE void onWake([[maybe_unused]] PxActor** actors, [[maybe_unused]] PxU32
			count)
			override {
			BOOM_DEBUG("onWake Event!");
		}

		BOOM_INLINE void onConstraintBreak([[maybe_unused]] PxConstraintInfo*
			constraints, [[maybe_unused]] PxU32 count) override {
		}

	private:
		PxCallbackFunction m_Callback;
		friend struct PhysicsContext;
	};
}