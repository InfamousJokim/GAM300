using Boom;
using System;

namespace GameScripts
{
    public class PatrolEnemyController
    {
        public ulong Entity;

        // ===== World convention =====
        private const bool FORWARD_IS_NEG_Z = true;

        // ===== New options =====
        private const bool INVERT_FACING = true;     // NEW: face the opposite direction
        private const bool LOCK_IN_PLACE = true;     // NEW: keep XZ position fixed (walk-in-place)
        private const float YAW_OFFSET_DEG = 180f;     // NEW: amount to rotate to invert

        // ===== Rotation tuning =====
        private float _rotationSpeedDeg = 360f;
        private float _minSpeedToRotate = 0.15f;
        private float _yaw;

        // ===== Animator (optional) =====
        private const bool DRIVE_SPEED_PARAM = true;
        private const float SPEED_SMOOTH = 10f;
        private double _smoothedSpeed = 0.0;

        // ===== Vision =====
        private VisionComponent _vision;
        private bool _isAlert;
        private bool _hasDealtDamage;
        private float _debugTimer;

        // ===== Anchor to cancel root motion =====
        private Vec3 _anchorPos;                        // NEW

        public void OnStart(string json)
        {
            if (!API.HasTransform(Entity))
            {
                API.Log("[PatrolEnemyController] Missing Transform.");
                return;
            }

            _yaw = API.GetRotation(Entity).Y;

            if (API.HasAnimator(Entity))
            {
                API.AnimatorPlay(Entity, "walking");
                if (DRIVE_SPEED_PARAM) API.AnimatorSetFloat(Entity, "Speed", 0f);
            }

            // Anchor current XZ so we can cancel drift if desired
            _anchorPos = API.GetPosition(Entity);       // NEW

            _vision = new VisionComponent { Entity = Entity };
            _vision.OnTargetDetected += OnPlayerDetected;
            _vision.OnTargetLost += OnPlayerLost;
            _vision.OnTargetUpdated += OnPlayerTracking;
            _vision.OnStart(json);
        }

        public void OnUpdate(float dt)
        {
            if (Entity == 0 || dt <= 0f) return;

            if (!_isAlert)
                FaceVelocity(dt);

            if (API.HasAnimator(Entity) && DRIVE_SPEED_PARAM)
            {
                var v = API.GetLinearVelocity(Entity);
                float speedXZ = (float)Math.Sqrt(v.X * v.X + v.Z * v.Z);
                _smoothedSpeed += (speedXZ - _smoothedSpeed) * Math.Min(1.0, SPEED_SMOOTH * dt);
                API.AnimatorSetFloat(Entity, "Speed", (float)_smoothedSpeed);
            }

            // === Keep walk in place (kill root motion / forward drift) ===
            if (LOCK_IN_PLACE)
            {
                // Prevent animation from pushing enemy forward
                var v = API.GetLinearVelocity(Entity);
                API.SetLinearVelocity(Entity, new Vec3(0f, v.Y, 0f));

                // Keep the enemy anchored in place (XZ locked)
                var p = API.GetPosition(Entity);
                API.SetPosition(Entity, new Vec3(_anchorPos.X, p.Y, _anchorPos.Z));
            }

            _vision?.OnUpdate(dt);

            _debugTimer += dt;
            if (_debugTimer >= 1f)
            {
                _debugTimer = 0f;
                var r = API.GetRotation(Entity);
                API.Log($"[PatrolEnemyController] yaw={_yaw:F1}°, rot.Y={r.Y:F1}°");
            }
        }

        // Rotate toward horizontal velocity
        private void FaceVelocity(float dt)
        {
            var vel = API.GetLinearVelocity(Entity);
            float vx = vel.X, vz = vel.Z;
            float speedXZ = (float)Math.Sqrt(vx * vx + vz * vz);
            if (speedXZ < _minSpeedToRotate) return;

            // Facing from velocity, with optional inversion
            float baseYaw = FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(vx, -vz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(vx, vz) * 180.0 / Math.PI);

            float targetYawDeg = baseYaw + (INVERT_FACING ? YAW_OFFSET_DEG : 0f); // NEW

            float delta = Wrap180(targetYawDeg - _yaw);
            float maxStep = _rotationSpeedDeg * dt;
            _yaw = (Math.Abs(delta) <= maxStep) ? targetYawDeg : _yaw + Math.Sign(delta) * maxStep;
            _yaw = Wrap360(_yaw);
            API.SetRotationY(Entity, _yaw);
        }

        private void OnPlayerDetected(ulong target, Vec3 pos)
        {
            _isAlert = true;
            var self = API.GetPosition(Entity);
            float dx = pos.X - self.X, dz = pos.Z - self.Z;

            float baseYaw = FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(dx, -dz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(dx, dz) * 180.0 / Math.PI);

            _yaw = Wrap360(baseYaw + (INVERT_FACING ? YAW_OFFSET_DEG : 0f));      // NEW
            API.SetRotationY(Entity, _yaw);

            if (!_hasDealtDamage)
            {
                _hasDealtDamage = true;
                PlayerManager.NotifyPlayerCaught(Entity);
            }
        }

        private void OnPlayerLost(ulong target, Vec3 lastPos)
        {
            _isAlert = false;
            _hasDealtDamage = false;
        }

        private void OnPlayerTracking(ulong target, Vec3 pos)
        {
            var self = API.GetPosition(Entity);
            float dx = pos.X - self.X, dz = pos.Z - self.Z;

            float baseYaw = FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(dx, -dz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(dx, dz) * 180.0 / Math.PI);

            _yaw = Wrap360(baseYaw + (INVERT_FACING ? YAW_OFFSET_DEG : 0f));      // NEW
            API.SetRotationY(Entity, _yaw);
        }

        public void OnDestroy() { _vision?.OnDestroy(); }

        // helpers
        private static float Wrap360(float a) { while (a >= 360f) a -= 360f; while (a < 0f) a += 360f; return a; }
        private static float Wrap180(float a) { while (a > 180f) a -= 360f; while (a <= -180f) a += 360f; return a; }
    }
}
