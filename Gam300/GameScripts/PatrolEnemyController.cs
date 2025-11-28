using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Enemy controller for NavMesh-patrolled enemies.
    /// Automatically rotates enemy to face movement direction.
    /// Vision follows the rotation for stealth gameplay.
    /// </summary>
    public class PatrolEnemyController
    {
        public ulong Entity;

        // Rotation parameters
        private float _rotationSpeed = 360f; // Degrees per second (fast rotation)
        private float _currentYaw = 0f;
        private float _minMovementForRotation = 0.1f; // Minimum speed to trigger rotation

        // Vision system (follows rotation)
        private VisionComponent _vision;

        // Alert state
        private bool _isAlert = false;
        private Vec3 _alertPosition;
        private Vec3 _lastPosition;

        // Detection tracking (prevent multiple damage per detection)
        private bool _hasDealtDamage = false;

        // Debug logging
        private float _debugLogTimer = 0f;
        private float _debugLogInterval = 1f; // Log every 1 second

        public void OnStart(string jsonParams)
        {
            API.Log($"[PatrolEnemyController] OnStart() - Entity: {Entity}");

            if (!API.HasTransform(Entity))
            {
                API.Log("[PatrolEnemyController] ERROR: Entity missing TransformComponent!");
                return;
            }

            // Store initial rotation and position
            _currentYaw = API.GetRotation(Entity).Y;
            _lastPosition = API.GetPosition(Entity);
            API.Log($"[PatrolEnemyController] Initial rotation: {_currentYaw} degrees");

            // Initialize vision system
            _vision = new VisionComponent { Entity = Entity };
            _vision.OnTargetDetected += OnPlayerDetected;
            _vision.OnTargetLost += OnPlayerLost;
            _vision.OnTargetUpdated += OnPlayerTracking;
            _vision.OnStart(jsonParams);

            _vision.EnableDebugReasons(true);
            _vision.EnableDebugLOS(true);

            API.Log("[PatrolEnemyController] Initialized - Will rotate toward movement direction");
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;

            // Debug logging
            _debugLogTimer += dt;
            if (_debugLogTimer >= _debugLogInterval)
            {
                _debugLogTimer = 0f;
                Vec3 actualRot = API.GetRotation(Entity);
                API.Log($"[PatrolEnemyController] Target Yaw: {_currentYaw:F1}°, Actual Y Rotation: {actualRot.Y:F1}°");
            }

            // Rotate toward movement direction (unless alert and tracking player)
            if (!_isAlert)
            {
                RotateTowardMovementDirection(dt);
            }

            // Update vision system AFTER rotation
            //_vision?.OnUpdate(dt);
        }

        /// <summary>
        /// Calculates movement direction from position change and rotates enemy to face it
        /// </summary>
        private void RotateTowardMovementDirection(float dt)
        {
            Vec3 currentPos = API.GetPosition(Entity);

            // Calculate movement direction from position delta
            Vec3 movementDelta = new Vec3(
                currentPos.X - _lastPosition.X,
                0f,
                currentPos.Z - _lastPosition.Z
            );

            float movementSpeed = (float)Math.Sqrt(
                movementDelta.X * movementDelta.X +
                movementDelta.Z * movementDelta.Z
            ) / dt;

            // Only rotate if moving fast enough (avoid jitter when stationary)
            if (movementSpeed > _minMovementForRotation)
            {
                // Calculate target yaw from movement direction
                float targetYaw = (float)(Math.Atan2(movementDelta.X, movementDelta.Z) * 180.0 / Math.PI);

                // Smoothly rotate toward target
                float angleDifference = targetYaw - _currentYaw;

                // Normalize angle difference to [-180, 180]
                while (angleDifference > 180f) angleDifference -= 360f;
                while (angleDifference < -180f) angleDifference += 360f;

                // Apply rotation
                float rotationStep = _rotationSpeed * dt;
                if (Math.Abs(angleDifference) < rotationStep)
                {
                    _currentYaw = targetYaw;
                }
                else
                {
                    _currentYaw += Math.Sign(angleDifference) * rotationStep;
                }

                // Normalize yaw
                while (_currentYaw >= 360f) _currentYaw -= 360f;
                while (_currentYaw < 0f) _currentYaw += 360f;

                // Use API.SetRotationY for rigid body rotation
                API.SetRotationY(Entity, _currentYaw);

                // VERIFY: Read back the rotation to confirm it was set
                Vec3 verifyRot = API.GetRotation(Entity);
                if (Math.Abs(verifyRot.Y - _currentYaw) > 1f)
                {
                    API.Log($"[PatrolEnemyController] WARNING: Rotation mismatch! Set {_currentYaw:F1}°, Got {verifyRot.Y:F1}°");
                }
            }

            // Store position for next frame
            _lastPosition = currentPos;
        }

        // === VISION EVENT HANDLERS ===
        private void OnPlayerDetected(ulong target, Vec3 position)
        {
            API.Log(">>> PATROL ENEMY ALERTED! PLAYER DETECTED! <<<");
            _isAlert = true;
            _alertPosition = position;

            // Instantly rotate to face player
            Vec3 currentPos = API.GetPosition(Entity);
            Vec3 directionToPlayer = new Vec3(
                position.X - currentPos.X,
                0f,
                position.Z - currentPos.Z
            );

            float distToPlayer = (float)Math.Sqrt(
                directionToPlayer.X * directionToPlayer.X +
                directionToPlayer.Z * directionToPlayer.Z
            );

            if (distToPlayer > 0f)
            {
                float lookAtYaw = (float)(Math.Atan2(directionToPlayer.X, directionToPlayer.Z) * 180.0 / Math.PI);
                _currentYaw = lookAtYaw;

                API.SetRotationY(Entity, _currentYaw);
                API.Log($"[PatrolEnemyController] Rotated to face player at {_currentYaw:F1}°");
            }

            // Play alert sound
            Vec3 enemyPos = API.GetPosition(Entity);
            API.PlaySoundAt("enemy_alert", "Resources/Audio/playerPunch_1.wav", enemyPos, false);
            API.SetSoundVolume("enemy_alert", 0.8f);

            // Damage player (only once per detection)
            if (!_hasDealtDamage)
            {
                _hasDealtDamage = true;
                API.Log($"[PatrolEnemyController] Dealing damage to player!");
                PlayerManager.NotifyPlayerCaught(Entity);
            }
        }

        private void OnPlayerLost(ulong target, Vec3 lastKnownPosition)
        {
            API.Log("[PatrolEnemyController] Lost sight of player, resuming patrol...");
            _isAlert = false;

            // Reset damage flag so player can be caught again
            _hasDealtDamage = false;
        }

        private void OnPlayerTracking(ulong target, Vec3 position)
        {
            _alertPosition = position;

            // Keep facing player while tracking
            Vec3 currentPos = API.GetPosition(Entity);
            Vec3 directionToPlayer = new Vec3(
                position.X - currentPos.X,
                0f,
                position.Z - currentPos.Z
            );

            float distToPlayer = (float)Math.Sqrt(
                directionToPlayer.X * directionToPlayer.X +
                directionToPlayer.Z * directionToPlayer.Z
            );

            if (distToPlayer > 0f)
            {
                float lookAtYaw = (float)(Math.Atan2(directionToPlayer.X, directionToPlayer.Z) * 180.0 / Math.PI);
                _currentYaw = lookAtYaw;

                API.SetRotationY(Entity, _currentYaw);
            }
        }

        // === PUBLIC CONFIGURATION ===

        public void SetRotationSpeed(float degreesPerSecond)
        {
            _rotationSpeed = degreesPerSecond;
        }

        public void OnDestroy()
        {
            _vision?.OnDestroy();
            API.Log($"[PatrolEnemyController] OnDestroy() - Entity: {Entity}");
        }
    }
}