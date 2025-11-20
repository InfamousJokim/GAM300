#pragma once
#include "common/Core.h"

using namespace	physx;

namespace Boom
{
    // helper function to convert PhysX Vec3 to GLM vec3
    BOOM_INLINE glm::vec3 PxToVec3(const PxVec3& physxVec)
    {
        return glm::vec3(physxVec.x, physxVec.y, physxVec.z);
    }

    // helper function to convert GLM vec3 to PhysX Vec3
    BOOM_INLINE PxVec3 ToPxVec3(const glm::vec3& glmVec)
    {
        return PxVec3(glmVec.x, glmVec.y, glmVec.z);
    }
    BOOM_INLINE glm::vec3 ToGLMVec3(PxVec3 const& pxVec) {
        return glm::vec3(pxVec.x, pxVec.y, pxVec.z);
    }

    BOOM_INLINE PxQuat ToPxQuat(const glm::vec3& eulerDegrees) {
        // Converts Euler degrees (in GLM's order) to a PhysX quaternion
        glm::quat q = glm::yawPitchRoll(
            glm::radians(eulerDegrees.y),
            glm::radians(eulerDegrees.x),
            glm::radians(eulerDegrees.z)
        );
        return PxQuat(q.x, q.y, q.z, q.w);
    }
}