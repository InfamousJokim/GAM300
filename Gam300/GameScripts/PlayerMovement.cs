using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Handles player movement with WASD, jumping with Space, and mouse-look control.
    /// Attach this script to any entity with TransformComponent and ScriptComponent.
    /// </summary>
    public class PlayerMovement
    {
        // This field is automatically set by the scripting system
        public ulong Entity;

        // Movement parameters (hardcoded for now - customize in code)
        private float _speed = 50f;
        private float _jumpSpeed = 8f;
        private float _gravity = -20f;

        // Runtime state
        private float _vy = 0f;
        private float _groundY = 0f;
        private bool _grounded = true;

        /// <summary>
        /// Called once when the script is first created.
        /// </summary>
        public void OnStart(string jsonParams)
        {
            API.Log($"[PlayerMovement] OnStart() - Entity: {Entity}");

            // Validate entity has required components
            if (!API.HasTransform(Entity))
            {
                API.Log("[PlayerMovement] ERROR: Entity missing TransformComponent!");
                return;
            }

            if (!API.HasScript(Entity))
            {
                API.Log("[PlayerMovement] ERROR: Entity missing ScriptComponent!");
                return;
            }

            // Note: JSON parsing removed for simplicity
            // To change values, edit _speed, _jumpSpeed, _gravity above
            API.Log($"[PlayerMovement] Using defaults: speed={_speed}, jumpSpeed={_jumpSpeed}, gravity={_gravity}");

            // Store initial ground position
            _groundY = API.GetPosition(Entity).Y;
            API.Log($"[PlayerMovement] Ground Y set to: {_groundY}");
        }

        /// <summary>
        /// Called every frame to update movement.
        /// </summary>
        public void OnUpdate(float dt)
        {
            // Safety check
            if (!API.HasTransform(Entity) || !API.HasScript(Entity))
                return;

            var pos = API.GetPosition(Entity);

            // Check if right mouse button is held (disables movement)
            bool allowMove = !API.IsMouseDown(API.MOUSE_RIGHT);

            // Horizontal movement (WASD)
            float dx = 0f, dz = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(API.KEY_A)) dx -= 1f;
                if (API.IsKeyDown(API.KEY_D)) dx += 1f;
                if (API.IsKeyDown(API.KEY_W)) dz -= 1f; // forward = -Z
                if (API.IsKeyDown(API.KEY_S)) dz += 1f;

                // Normalize diagonal movement
                if (dx != 0f || dz != 0f)
                {
                    float len = (float)Math.Sqrt(dx * dx + dz * dz);
                    dx /= len;
                    dz /= len;
                    pos.X += dx * _speed * dt;
                    pos.Z += dz * _speed * dt;
                }

                // Jump
                if (_grounded && API.IsKeyDown(API.KEY_SPACE))
                {
                    _vy = _jumpSpeed;
                    _grounded = false;
                }
            }

            // Apply gravity
            _vy += _gravity * dt;
            pos.Y += _vy * dt;

            // Ground collision
            if (pos.Y <= _groundY)
            {
                pos.Y = _groundY;
                _vy = 0f;
                _grounded = true;
            }

            // Apply final position
            API.SetPosition(Entity, pos);
        }

        /// <summary>
        /// Called when the script is destroyed (optional cleanup).
        /// </summary>
        public void OnDestroy()
        {
            API.Log($"[PlayerMovement] OnDestroy() - Entity: {Entity}");
        }
    }
}