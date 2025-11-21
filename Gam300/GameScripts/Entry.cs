using System;
using Boom;

namespace GameScripts
{
    public static class Entry
    {
        private static ulong _player;
        private static float _speed = 10f;   // movement speed (units per second)
        private static float _jumpSpeed = 8f; // vertical jump velocity

        public static void Start()
        {
            API.Log("[C#] Entry.Start() called");

            _player = API.FindEntity("Samurai");
            API.Log("[C#] Samurai handle = " + _player);

            if (_player != 0)
            {
                // Check if entity has required components
                if (!API.HasTransform(_player))
                {
                    API.Log("[C#] ERROR: Samurai entity does not have TransformComponent!");
                    _player = 0; // Invalidate so Update won't try to use it
                    return;
                }

                if (!API.HasScript(_player))
                {
                    API.Log("[C#] ERROR: Samurai entity does not have ScriptComponent!");
                    API.Log("[C#] Player movement requires a ScriptComponent to be attached.");
                    _player = 0; // Invalidate so Update won't try to use it
                    return;
                }

                API.Log("[C#] Samurai has required components (Transform + Script) - OK");
            }
            else
            {
                API.Log("[C#] WARNING: Could not find Samurai entity");
            }
        }

        public static void Update(float dt)
        {
            // Only process if we have a valid player with required components
            if (_player == 0)
                return;

            // Double-check components still exist (in case they were removed at runtime)
            if (!API.HasTransform(_player))
            {
                API.Log("[C#] ERROR: Player lost TransformComponent!");
                _player = 0;
                return;
            }

            if (!API.HasScript(_player))
            {
                API.Log("[C#] ERROR: Player lost ScriptComponent! Movement disabled.");
                _player = 0;
                return;
            }

            // =============== PHYSX-BASED MOVEMENT =================

            // 1. Get the player's CURRENT velocity from PhysX
            var vel = API.GetLinearVelocity(_player);

            // 2. Check if the player is allowed to move (RMB held disables movement)
            bool allowMove = !API.IsMouseDown(API.MOUSE_RIGHT);

            // 3. Check if the player is "grounded" via collision flag
            bool isGrounded = API.IsColliding(_player);

            // 4. Calculate horizontal movement input
            float dx = 0f, dz = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(API.KEY_A)) dx -= 1f;
                if (API.IsKeyDown(API.KEY_D)) dx += 1f;
                if (API.IsKeyDown(API.KEY_W)) dz -= 1f; // forward = -Z
                if (API.IsKeyDown(API.KEY_S)) dz += 1f;
            }

            // 5. Apply horizontal velocity
            if (dx != 0f || dz != 0f)
            {
                // Normalize to prevent faster diagonal movement
                float len = (float)Math.Sqrt(dx * dx + dz * dz);
                vel.X = (dx / len) * _speed;
                vel.Z = (dz / len) * _speed;
            }
            else
            {
                // No input, so stop horizontal movement
                vel.X = 0f;
                vel.Z = 0f;
            }

            // 6. Apply vertical velocity (Jumping)
            if (allowMove && isGrounded && API.IsKeyDown(API.KEY_SPACE))
            {
                // Apply a one-time upward velocity.
                vel.Y = _jumpSpeed;
            }
            // NOTE: We do NOT apply gravity here.
            // PhysX already applies gravity every simulation step.
            // We just modify vel.X / vel.Z + jump impulse, and let PhysX handle the rest.

            // 7. Set the FINAL velocity back into PhysX
            API.SetLinearVelocity(_player, vel);

            // No more manual position / gravity / ground clamping.
            // Transform is driven entirely by the physics simulation.
        }
    }
}
