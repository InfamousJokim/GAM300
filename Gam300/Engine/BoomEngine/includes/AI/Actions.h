// AI/Actions.h
#pragma once
#include "NavAgent.h"
#include "BehaviourTree.h"
#include "AIComponent.h"
#include "ECS/ECS.hpp"
#include "glm/glm.hpp"

namespace Boom {

    struct SeePlayerCond : BTNode
    {
        BTState tick(entt::registry& reg, entt::entity e, float) override
        {
            if (!reg.all_of<AIComponent, TransformComponent>(e))
                return BTState::Failure;

            auto& ai = reg.get<AIComponent>(e);
            auto& tc = reg.get<TransformComponent>(e);

            // Resolve player by name if needed
            if (ai.player == entt::null && !ai.playerName.empty())
            {
                auto view = reg.view<InfoComponent>();
                for (auto ent : view)
                {
                    const auto& info = view.get<InfoComponent>(ent);
                    if (info.name == ai.playerName)
                    {
                        ai.player = ent;
                        break;
                    }
                }
            }

            if (ai.player == entt::null)
                return BTState::Failure;

            if (!reg.all_of<TransformComponent>(ai.player))
                return BTState::Failure;

            auto& ptc = reg.get<TransformComponent>(ai.player);

            glm::vec3 pos = tc.transform.translate;
            glm::vec3 target = ptc.transform.translate;
            float d2 = glm::distance2(pos, target);

            // Check if we are already chasing via NavAgent
            bool chasingNow = false;
            if (reg.all_of<NavAgentComponent>(e))
            {
                auto& ag = reg.get<NavAgentComponent>(e);
                chasingNow = (ag.follow == ai.player && ag.active);
            }

            // If not chasing yet -> use detectRadius
            // If already chasing -> use loseRadius (hysteresis)
            float radius = chasingNow ? ai.loseRadius : ai.detectRadius;
            float r2 = radius * radius;

            if (d2 <= r2)
            {
                // Still allowed to see/keep chasing
                return BTState::Success;
            }
            else
            {
                // If we were chasing and we are now outside loseRadius -> stop chasing
                if (chasingNow && reg.all_of<NavAgentComponent>(e))
                {
                    auto& ag = reg.get<NavAgentComponent>(e);
                    ag.follow = entt::null;

                    ag.dirty = true;
                    ag.repathTimer = 0.f;
                    ag.path.clear();
                    ag.waypoint = 0;
                }
                return BTState::Failure;
            }
        }
    };


    // When chasing, stop if we lost the player far enough
    struct StillChasingCond : BTNode {
        BTState tick(entt::registry& reg, entt::entity e, float) override {
            auto& tr = reg.get<TransformComponent>(e);
            auto& ai = reg.get<AIComponent>(e);

            if (ai.player == entt::null || !reg.valid(ai.player) || !reg.all_of<TransformComponent>(ai.player))
                return BTState::Failure;

            const glm::vec3 me = tr.transform.translate;
            const glm::vec3 pp = reg.get<TransformComponent>(ai.player).transform.translate;
            const float d2 = glm::distance2(me, pp);
            return (d2 <= ai.loseRadius * ai.loseRadius) ? BTState::Success : BTState::Failure;
        }
    };

    struct IdleAction : BTNode {
        BTState tick(entt::registry& reg, entt::entity e, float dt) override {
            auto& ai = reg.get<AIComponent>(e);
            ai.idleTimer -= dt;
            if (ai.idleTimer <= 0.f) return BTState::Success;
            return BTState::Running;
        }
    };

    // Drives NavAgent to walk patrol loop. Success when it *issued* movement; Running while moving.
    struct PatrolAction : BTNode {
        BTState tick(entt::registry& reg, entt::entity e, float) override {
            if (!reg.all_of<NavAgentComponent, TransformComponent, AIComponent>(e))
                return BTState::Failure;
            auto& ai = reg.get<AIComponent>(e);
            auto& ag = reg.get<NavAgentComponent>(e);

            if (ai.patrolPoints.empty()) return BTState::Failure;

            const glm::vec3 goal = ai.patrolPoints[ai.patrolIndex];
            if (ag.target != goal) { ag.follow = entt::null; ag.target = goal; ag.dirty = true; }

            // Have we reached this patrol point? (use arrive threshold from NavAgent)
            auto& tr = reg.get<TransformComponent>(e);
            if (glm::distance(tr.transform.translate, goal) <= ag.arrive) {
                // advance & idle
                ai.patrolIndex = (ai.patrolIndex + 1) % static_cast<int>(ai.patrolPoints.size());
                ai.idleTimer = ai.idleWait;
                return BTState::Success; // let an IdleAction run next frame
            }
            return BTState::Running;
        }
    };

    // Make agent chase the player by wiring NavAgent.follow
    struct SeekPlayerAction : BTNode
    {
        BTState tick(entt::registry& reg, entt::entity e, float) override
        {
            if (!reg.all_of<AIComponent, TransformComponent>(e))
                return BTState::Failure;

            auto& ai = reg.get<AIComponent>(e);
            if (ai.player == entt::null)
                return BTState::Failure;

            if (!reg.all_of<NavAgentComponent>(e))
                return BTState::Failure;

            auto& ag = reg.get<NavAgentComponent>(e);

            // Follow that player using NavAgent
            if (ag.follow != ai.player || !ag.active)
            {
                ag.follow = ai.player;
                ag.active = true;
                ag.dirty = true;
                ag.repathTimer = 0.0f;
            }

            // Movement is handled by NavAgentSystem, so the BT action itself
            // can be considered "instantly done" this frame.
            return BTState::Success;
        }
    };


} // namespace Boom
