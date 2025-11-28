using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    // Attach to a small trigger volume located just outside the door.
    // Params format (semicolon or comma separated, case-insensitive keys):
    //   door=MyDoorName; distance=1.5; movespeed=2.0; consumekey=true; autoclose=false; closedelay=0.5
    //
    // Behavior:
    // - When the player with a key enters the trigger, the door slides to the left (relative to its yaw)
    //   by 'distance' meters at 'movespeed' m/s. Optionally consumes a key.
    // - If autoclose=true, when the player exits the trigger the door slides back after 'closedelay' seconds.
    public class DoorTriggerLeft
    {
        public ulong Entity;

        // Config
        private string _doorName = "MoveDoor";
        private float _slideDistance = 5.5f;   // meters
        private float _moveSpeed = 2.0f;       // m/s
        private bool _consumeKey = true;
        private bool _autoCloseOnExit = false;
        private float _closeDelay = 0f;

        // Resolved door
        private ulong _door = 0;
        private Vec3 _basePos;
        private Vec3 _targetPos;
        private Vec3 _leftDir; // unit vector

        // State
        private static readonly Dictionary<ulong, DoorTriggerLeft> s_instances = new Dictionary<ulong, DoorTriggerLeft>();
        private bool _opening = false;
        private bool _closing = false;
        private float _closeTimer = 0f;

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;
            ParseParams(jsonParams);

            // Ensure trigger is configured
            if (!API.HasCollider(Entity))
            {
                API.Log("[DoorTriggerLeft] WARNING: Trigger entity has no collider!");
            }
            else if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
            }

            // Resolve door by name
            _door = API.FindEntity(_doorName);
            if (_door == 0)
            {
                API.Log($"[DoorTriggerLeft] ERROR: Could not find door entity by name '{_doorName}'.");
            }
            else
            {
                // Cache base position and compute left direction from yaw
                _basePos = API.GetPosition(_door);
                float yawDeg = API.GetRotation(_door).Y;
                float yawRad = yawDeg * (float)Math.PI / 180f;

                // Forward = (sin(yaw), cos(yaw)); Left (CCW) = (-cos(yaw), sin(yaw))
                float fx = (float)Math.Sin(yawRad);
                float fz = (float)Math.Cos(yawRad);
                _leftDir = new Vec3(-fz, 0f, fx); // unit left vector in XZ

                // Target position = base + left * distance
                _targetPos = new Vec3(
                    _basePos.X + _leftDir.X * _slideDistance,
                    _basePos.Y,
                    _basePos.Z + _leftDir.Z * _slideDistance
                );

                API.Log($"[DoorTriggerLeft] Door='{_doorName}' basePos=({_basePos.X:F2},{_basePos.Y:F2},{_basePos.Z:F2}) targetPos=({_targetPos.X:F2},{_targetPos.Y:F2},{_targetPos.Z:F2}) leftDir=({_leftDir.X:F2},{_leftDir.Z:F2})");
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log("[DoorTriggerLeft] Registered trigger callbacks.");
        }

        public void OnUpdate(float dt)
        {
            if (_door == 0) return;

            // Handle delayed close
            if (_autoCloseOnExit && _closeDelay > 0f && !_opening && _closing)
            {
                _closeTimer -= dt;
                if (_closeTimer > 0f) return;
                _closeDelay = 0f; // start closing now (delay done)
            }

            // Current position
            Vec3 cur = API.GetPosition(_door);

            if (_opening)
            {
                Vec3 next = MoveTowards(cur, _targetPos, _moveSpeed * dt);
                API.TeleportRigidBody(_door, next);

                if (NearlyEqual(next, _targetPos))
                {
                    API.TeleportRigidBody(_door, _targetPos);
                    _opening = false;
                    _closing = false;
                    API.Log("[DoorTriggerLeft] Door moved left (opened).");
                }
            }
            else if (_closing)
            {
                Vec3 next = MoveTowards(cur, _basePos, _moveSpeed * dt);
                API.TeleportRigidBody(_door, next);

                if (NearlyEqual(next, _basePos))
                {
                    API.TeleportRigidBody(_door, _basePos);
                    _closing = false;
                    API.Log("[DoorTriggerLeft] Door returned (closed).");
                }
            }
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            DoorTriggerLeft inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;
            if (inst._door == 0) return;

            // Only player triggers this
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            if (!PlayerInventory.HasKey())
            {
                API.Log("[DoorTriggerLeft] Player has no key - door will not move.");
                return;
            }

            if (inst._consumeKey && !PlayerInventory.ConsumeKey())
            {
                API.Log("[DoorTriggerLeft] Failed to consume key.");
                return;
            }

            // Start opening (sliding left)
            inst._opening = true;
            inst._closing = false;

            var pos = API.GetPosition(inst._door);
            API.PlaySoundAt("sfx_door_slide_open", "Resources/Audio/playerPunch_1.wav", pos, false);
            API.SetSoundVolume("sfx_door_slide_open", 0.85f);

            API.Log("[DoorTriggerLeft] Sliding door to the left.");
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            DoorTriggerLeft inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;
            if (inst._door == 0) return;

            // Only react to player exiting
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;
            if (!inst._autoCloseOnExit) return;

            // Queue close (optionally with delay)
            inst._opening = false;
            inst._closing = true;
            inst._closeTimer = inst._closeDelay;

            API.Log("[DoorTriggerLeft] Player left trigger - door will slide back.");
        }

        private void ParseParams(string p)
        {
            if (string.IsNullOrWhiteSpace(p)) return;

            var parts = p.Split(new[] { ';', ',' }, StringSplitOptions.RemoveEmptyEntries);
            foreach (var raw in parts)
            {
                var kv = raw.Split(new[] { '=' }, 2);
                if (kv.Length != 2) continue;

                var key = kv[0].Trim().ToLowerInvariant();
                var val = kv[1].Trim();

                float f;
                bool b;

                switch (key)
                {
                    case "door":
                    case "doorname":
                        _doorName = val;
                        break;

                    case "distance":
                    case "slide":
                    case "slidedistance":
                        if (float.TryParse(val, out f)) _slideDistance = Math.Max(0f, f);
                        break;

                    case "movespeed":
                    case "speed":
                        if (float.TryParse(val, out f)) _moveSpeed = Math.Max(0.01f, f);
                        break;

                    case "consumekey":
                        if (bool.TryParse(val, out b)) _consumeKey = b;
                        break;

                    case "autoclose":
                    case "autocloseonexit":
                        if (bool.TryParse(val, out b)) _autoCloseOnExit = b;
                        break;

                    case "closedelay":
                        if (float.TryParse(val, out f)) _closeDelay = Math.Max(0f, f);
                        break;
                }
            }
        }

        // Position helpers
        private static Vec3 MoveTowards(Vec3 current, Vec3 target, float maxDelta)
        {
            float dx = target.X - current.X;
            float dy = target.Y - current.Y;
            float dz = target.Z - current.Z;

            float dist = (float)Math.Sqrt(dx * dx + dy * dy + dz * dz);
            if (dist <= maxDelta || dist < 1e-6f) return target;

            float inv = maxDelta / dist;
            return new Vec3(current.X + dx * inv, current.Y + dy * inv, current.Z + dz * inv);
        }

        private static bool NearlyEqual(Vec3 a, Vec3 b, float eps = 0.001f)
        {
            return Math.Abs(a.X - b.X) <= eps &&
                   Math.Abs(a.Y - b.Y) <= eps &&
                   Math.Abs(a.Z - b.Z) <= eps;
        }
    }
}