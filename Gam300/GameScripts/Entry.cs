using System;
using Boom;

namespace GameScripts
{
    public static class Entry
    {
        private static ulong _player;
        private static float _speed = 10f;

        private static float _vy = 0f, _gravity = -20f, _jumpSpeed = 8f, _groundY = 0f;
        private static bool _grounded = true;

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
                _groundY = API.GetPosition(_player).Y;
                API.Log("[C#] Ground Y set to: " + _groundY);
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

            var pos = API.GetPosition(_player);

            bool allowMove = !API.IsMouseDown(API.MOUSE_RIGHT);

            float dx = 0f, dz = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(API.KEY_A)) dx -= 1f;
                if (API.IsKeyDown(API.KEY_D)) dx += 1f;
                if (API.IsKeyDown(API.KEY_W)) dz -= 1f; // forward = -Z
                if (API.IsKeyDown(API.KEY_S)) dz += 1f;

                if (dx != 0f || dz != 0f)
                {
                    float len = (float)Math.Sqrt(dx * dx + dz * dz);
                    dx /= len; dz /= len;
                    pos.X += dx * _speed * dt;
                    pos.Z += dz * _speed * dt;
                }

                if (_grounded && API.IsKeyDown(API.KEY_SPACE))
                {
                    _vy = _jumpSpeed;
                    _grounded = false;
                }
            }

            _vy += _gravity * dt;
            pos.Y += _vy * dt;

            if (pos.Y <= _groundY)
            {
                pos.Y = _groundY;
                _vy = 0f;
                _grounded = true;
            }

            API.SetPosition(_player, pos);
        }
    }
}