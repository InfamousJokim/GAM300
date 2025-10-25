#pragma once
#include "Helpers.h"

namespace Boom {
    struct RigidBody3D
    {
        BOOM_INLINE RigidBody3D(const RigidBody3D&) = default;
        BOOM_INLINE RigidBody3D() = default;

        // body actor pointer
        PxRigidActor* actor = nullptr;

        // body density
        float density = 1.0f;

        float mass = 1.0f;
        glm::vec3 initialVelocity = glm::vec3(0.0f); // Default to zero
        glm::vec3 previousScale = glm::vec3(0.0f);
        // rigidbody type
        enum Type{
            DYNAMIC = 0,
            STATIC,
        } type;


        // RigidBody3D
        XPROPERTY_DEF(
            "RigidBody3D", Boom::RigidBody3D,
            obj_member<"Density", &Boom::RigidBody3D::density>,
            obj_member<"Mass", &Boom::RigidBody3D::mass>,
            obj_member<"InitialVelocity", &Boom::RigidBody3D::initialVelocity>,
            obj_member<"Previous Scale", &Boom::RigidBody3D::previousScale>,
            obj_member<"Type", &Boom::RigidBody3D::type,
            member_enum_value<"DYNAMIC", Boom::RigidBody3D::Type::DYNAMIC>,
            member_enum_value<"STATIC", Boom::RigidBody3D::Type::STATIC>
            >
        )
    };

    struct Collider3D
    {
        BOOM_INLINE Collider3D(const Collider3D&) = default;
        BOOM_INLINE Collider3D() = default;



        // collider matrial pointer
        PxMaterial* material = nullptr;

        // collider material data
        float dynamicFriction = 0.5f;
        float staticFriction = 0.0f;
        float restitution = 0.1f;

        // mesh for custom shape
        PxConvexMeshGeometry mesh;

        // collider geometry shape
        PxShape* Shape = nullptr;

        // collider shape type
        enum Type{
            BOX = 0,
            SPHERE,
            CAPSULE,
            MESH
        } type;


        // Collider3D
        XPROPERTY_DEF(
            "Collider3D", Boom::Collider3D,
            obj_member<"DynamicFriction", &Boom::Collider3D::dynamicFriction>,
            obj_member<"StaticFriction", &Boom::Collider3D::staticFriction>,
            obj_member<"Restitution", &Boom::Collider3D::restitution>,
            obj_member<"Type", &Boom::Collider3D::type,
            member_enum_value<"BOX", Boom::Collider3D::Type::BOX>,
            member_enum_value<"SPHERE", Boom::Collider3D::Type::SPHERE>,
			member_enum_value<"CAPSULE", Boom::Collider3D::Type::CAPSULE>,
            member_enum_value<"MESH", Boom::Collider3D::Type::MESH>
            >
        )
    };
}