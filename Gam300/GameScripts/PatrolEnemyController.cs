using Boom;
using System;

namespace GameScripts
{
    public class PatrolEnemyController
    {
        public ulong Entity;

        private const bool WORLD_FORWARD_IS_NEG_Z = false;
        private const bool LOCK_IN_PLACE = false;

        private float _rotationSpeedDeg = 360f;
        private float _minSpeedToRotate = 0.15f;
        private float _yaw;

        private const bool DRIVE_SPEED_PARAM = true;
        private const float SPEED_SMOOTH = 10f;
        private double _smoothedSpeed = 0.0;

        private VisionComponent _vision;
        private bool _isAlert;
        private bool _hasDealtDamage;

        private Vec3 _anchorPos;

        // ====== AUDIO ======
        private const string SFX_FOOTSTEP_PATH = "Resources/Audio/playerRun_02.wav";
        private const string SFX_ALERT_PATH = "Resources/Audio/enemyHurt_2.wav";

        private string _footBase;     // base id for steps (unique per entity)
        private string _alertName;

        // cadence settings
        private const float MOVE_START_SPEED = 0.25f; // start stepping above this (m/s)
        private const float MOVE_STOP_SPEED = 0.15f; // stop stepping below this
        private const float STEP_LENGTH_M = 0.7f;  // meters per step at normal walk (tune!)
        private const float MIN_INTERVAL_S = 0.5f; // clamp so it never gets machine-gun fast
        private const float MAX_INTERVAL_S = 2.0f;  // clamp for slow shuffles
        private const float VOL_BASE = 1.0f; // base volume
        private const float VOL_JITTER = 0.07f; // ± random variance

        private float _stepTimer = 0f;
        private bool _leftNext = true;
        private float _debugTimer;

        public void OnStart(string json)
        {
            if (!API.HasTransform(Entity)) { API.Log("[PatrolEnemyController] Missing Transform."); return; }

            _yaw = API.GetRotation(Entity).Y;

            if (API.HasAnimator(Entity))
            {
                API.AnimatorPlay(Entity, "walking");
                if (DRIVE_SPEED_PARAM) API.AnimatorSetFloat(Entity, "Speed", 0f);
            }

            _anchorPos = API.GetPosition(Entity);

            _vision = new VisionComponent { Entity = Entity };
            _vision.OnTargetDetected += OnPlayerDetected;
            _vision.OnTargetLost += OnPlayerLost;
            _vision.OnTargetUpdated += OnPlayerTracking;
            _vision.OnStart(json);

            _footBase = "foot_" + Entity.ToString();
            _alertName = "alert_" + Entity.ToString();

            // optional: preload step clip once (non-loop)
            API.PreloadSound(_footBase + "_L", SFX_FOOTSTEP_PATH, loop: false);
            API.PreloadSound(_footBase + "_R", SFX_FOOTSTEP_PATH, loop: false);
            API.PreloadSound(_alertName, SFX_ALERT_PATH, loop: false);
        }

        public void OnUpdate(float dt)
        {
            if (Entity == 0 || dt <= 0f) return;

            var v = API.GetLinearVelocity(Entity);
            float speedXZ = (float)Math.Sqrt(v.X * v.X + v.Z * v.Z);

            if (!_isAlert) FaceVelocity(dt, v.X, v.Z);

            if (API.HasAnimator(Entity) && DRIVE_SPEED_PARAM)
            {
                _smoothedSpeed += (speedXZ - _smoothedSpeed) * Min(1.0, SPEED_SMOOTH * dt);
                API.AnimatorSetFloat(Entity, "Speed", (float)_smoothedSpeed);
            }

            //if (LOCK_IN_PLACE)
            //{
            //    API.SetLinearVelocity(Entity, new Vec3(0f, v.Y, 0f));
            //    var p = API.GetPosition(Entity);
            //    API.SetPosition(Entity, new Vec3(_anchorPos.X, p.Y, _anchorPos.Z));
            //}

            // ======= DISCRETE FOOTSTEPS =======
            bool grounded = API.IsColliding(Entity);              // your RB “grounded” flag
            bool moving = speedXZ >= MOVE_START_SPEED;

            if (grounded && moving)
            {
                // cadence: steps per second ≈ speed / stepLength
                float cadence = Math.Max(0.0001f, speedXZ / STEP_LENGTH_M);
                float interval = 1.0f / cadence;
                if (interval < MIN_INTERVAL_S) interval = MIN_INTERVAL_S;
                if (interval > MAX_INTERVAL_S) interval = MAX_INTERVAL_S;

                _stepTimer -= dt;
                if (_stepTimer <= 0f)
                {
                    var pos = API.GetPosition(Entity);

                    // Check vertical distance to player (Y-axis)
                    // Only play footsteps if on same floor (within 10 units vertically)
                    ulong playerEntity = PlayerMovement.GetPlayerEntity();
                    bool shouldPlayFootstep = true;

                    if (playerEntity != 0 && API.HasTransform(playerEntity))
                    {
                        var playerPos = API.GetPosition(playerEntity);
                        float verticalDistance = Math.Abs(pos.Y - playerPos.Y);

                        // Floor separation is 12 units, so 10 units ensures same floor only
                        shouldPlayFootstep = verticalDistance < 10.0f;
                    }

                    if (shouldPlayFootstep)
                    {
                        // choose L/R alternating channel names so overlapping clicks don't stomp each other
                        string chName = _footBase + (_leftNext ? "_L" : "_R");
                        _leftNext = !_leftNext;

                        // play one-shot at position
                        API.PlaySoundAt(chName, SFX_FOOTSTEP_PATH, pos, loop: false);

                        // subtle volume variance
                        float jitter = (float)(Random01() * 2.0 - 1.0) * VOL_JITTER; // [-VOL_JITTER, +VOL_JITTER]
                        float vol = Clamp01(VOL_BASE + jitter);
                        API.SetSoundVolume(chName, vol);

                        // Set 3D distance: with vertical check, we can use larger max distance for horizontal range
                        // Min 6.0 = full volume within 6 units
                        // Max 30.0 = can hear across entire floor when visible
                        API.Set3DMinMaxDistance(chName, 6.0f, 30.0f);
                    }

                    // restart timer regardless of whether we played the sound
                    _stepTimer += interval;
                }
            }
            else
            {
                // reset so the next move fires promptly, not after an old leftover remainder
                _stepTimer = 0f;
            }

            _vision?.OnUpdate(dt);

            _debugTimer += dt;
            if (_debugTimer >= 1f)
            {
                _debugTimer = 0f;
                var r = API.GetRotation(Entity);
                API.Log($"[PatrolEnemyController] yaw={_yaw:F1}°, rotY={r.Y:F1}°, speed={speedXZ:F2} m/s");
            }
        }

        // ---- Helpers (unchanged except we keep them here) ----
        private void FaceVelocity(float dt, float vx, float vz)
        {
            float speedXZ = (float)Math.Sqrt(vx * vx + vz * vz);
            if (speedXZ < _minSpeedToRotate) return;

            float baseYaw = ComputeYawFromVelocity(vx, vz);
            float targetYawDeg = baseYaw;

            float delta = Wrap180(targetYawDeg - _yaw);
            float maxStep = _rotationSpeedDeg * dt;

            _yaw = (Math.Abs(delta) <= maxStep) ? targetYawDeg : _yaw + Math.Sign(delta) * maxStep;
            _yaw = Wrap360(_yaw);
            API.SetRotationY(Entity, _yaw);
        }

        private float ComputeYawFromVelocity(float vx, float vz)
        {
            return WORLD_FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(vx, -vz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(vx, vz) * 180.0 / Math.PI);
        }

        private void OnPlayerDetected(ulong target, Vec3 pos)
        {
            _isAlert = true;
            var self = API.GetPosition(Entity);
            float dx = pos.X - self.X, dz = pos.Z - self.Z;
            float baseYaw = WORLD_FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(dx, -dz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(dx, dz) * 180.0 / Math.PI);
            _yaw = Wrap360(baseYaw);
            API.SetRotationY(Entity, _yaw);

            API.PlaySoundAt(_alertName, SFX_ALERT_PATH, self, loop: false);
            API.SetSoundVolume(_alertName, 0.5f);
            // Set 3D distance for alert sound - should be heard from medium distance
            API.Set3DMinMaxDistance(_alertName, 1.0f, 25.0f);

            if (!_hasDealtDamage) { _hasDealtDamage = true; PlayerManager.NotifyPlayerCaught(Entity); }
        }

        private void OnPlayerLost(ulong t, Vec3 lastPos) { _isAlert = false; _hasDealtDamage = false; }

        private void OnPlayerTracking(ulong t, Vec3 pos)
        {
            var self = API.GetPosition(Entity);
            float dx = pos.X - self.X, dz = pos.Z - self.Z;
            float baseYaw = WORLD_FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(dx, -dz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(dx, dz) * 180.0 / Math.PI);
            _yaw = Wrap360(baseYaw);
            API.SetRotationY(Entity, _yaw);
        }

        public void OnDestroy()
        {
            // no looping channels to stop now, but if a step is mid-play it will auto-stop
            _vision?.OnDestroy();
        }

        // utils
        private static double Min(double a, double b) => (a < b) ? a : b;
        private static float Wrap360(float a) { while (a >= 360f) a -= 360f; while (a < 0f) a += 360f; return a; }
        private static float Wrap180(float a) { while (a > 180f) a -= 360f; while (a <= -180f) a += 360f; return a; }
        private static double Random01() => new Random().NextDouble();
        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);
    }
}
