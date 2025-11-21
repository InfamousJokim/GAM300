#pragma once

#include "Core.h"
#include <unordered_map>
#include "AI/BehaviourTreeActions.h"
#include "ECS/ECS.hpp"

namespace Boom {

    BOOM_INLINE void ApplyModeSideEffects(entt::registry& reg, entt::entity e, AIComponent& ai)
    {
        auto& ag = reg.get_or_emplace<NavAgentComponent>(e);
        switch (ai.mode)
        {
        case AIComponent::AIMode::Idle:
            ag.follow = entt::null;
            ag.active = false;
            ag.path.clear();
            ag.waypoint = 0;
            ag.dirty = false;
            ag.repathTimer = 0.f;
            break;

        case AIComponent::AIMode::Patrol:
            ag.active = true;
            ag.follow = entt::null;   // patrol ignores follow
            ag.dirty = true;         // force first path build
            ag.repathTimer = 0.f;

            ai.idleTimer = 0.f;
            if (ai.patrolIndex < 0 || ai.patrolIndex >= (int)ai.patrolPoints.size())
                ai.patrolIndex = 0;
            break;

        case AIComponent::AIMode::Seek:
            ag.active = true;
            ag.dirty = true;
            break;

        case AIComponent::AIMode::Auto:
        default:
            ag.active = true;
            break;
        }
    }

    struct AISystem {

        using TreeMap = std::unordered_map<entt::entity, BTNodePtr>;
        TreeMap m_trees;  // one BT root per AI entity

        // Helper to initialize an enemy AI component
        static void InitEnemy(entt::registry& reg, entt::entity e,
            const std::vector<glm::vec3>& patrolPts,
            float detect = 8.f, float lose = 12.f, float idle = 1.f,
            AIComponent::AIMode mode = AIComponent::AIMode::Auto)
        {
            auto& ai = reg.emplace_or_replace<AIComponent>(e);
            ai.patrolPoints = patrolPts;
            ai.detectRadius = detect;
            ai.loseRadius = lose;
            ai.idleWait = idle;
            ai.patrolIndex = 0;
            ai.idleTimer = 0.f;

            ai.mode = mode;
            ai.lastMode = mode;

            ApplyModeSideEffects(reg, e, ai);
            // NOTE: we do NOT build the tree here; update() will do it lazily
        }

        void update(entt::registry& reg, float dt)
        {
            auto view = reg.view<AIComponent>();
            for (auto e : view) {
                auto& ai = view.get<AIComponent>(e);

                BTNodePtr& root = m_trees[e];  // creates empty unique_ptr if missing

                // 3) When Mode changes (ImGui) or no tree yet, rebuild it
                if (ai.mode != ai.lastMode || !root) {
                    ApplyModeSideEffects(reg, e, ai);
                    root = BuildTreeForMode(ai.mode);
                    ai.lastMode = ai.mode;
                }

                if (root) {
                    root->tick(reg, e, dt);
                }
            }
        }
    };

} // namespace Boom
