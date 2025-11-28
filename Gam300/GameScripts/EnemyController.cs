using System;
using System.Threading;
using Boom;

namespace GameScripts
{
    public class EnemyController
    {
        public ulong Entity;

        // Rotation parameters
        private float _rotationTimer = 0f;
        private float _rotationInterval = 2f;
        private float _currentYRotation = 0f;
        private float _targetYRotation = 0f;
        private float _rotationSpeed = 90f; // Degrees per second for smooth rotation
        private bool _isRotating = false;

        // Vision system
        private VisionComponent _vision;

        // Detection tracking (prevent multiple damage per detection)
        private bool _hasDealtDamage = false;

        public void OnStart(string jsonParams)
        {
            API.Log($"[EnemyController] OnStart() - Entity: {Entity}");

            if (!API.HasTransform(Entity))
            {
                API.Log("[EnemyController] ERROR: Entity missing TransformComponent!");
                return;
            }

            // Initialize vision system
            _vision = new VisionComponent { Entity = Entity };
            _vision.OnTargetDetected += OnPlayerDetected;
            _vision.OnTargetLost += OnPlayerLost;
            _vision.OnTargetUpdated += OnPlayerTracking;
            _vision.OnStart(jsonParams);

            // Initialize rotation from current entity rotation
            _currentYRotation = API.GetRotation(Entity).Y;
            _targetYRotation = _currentYRotation;

            API.Log("[EnemyController] Controller initialized with vision system and smooth rotation");

            _vision.EnableDebugReasons(true);  // See why targets are rejected
            _vision.EnableDebugLOS(true);      // See LOS results
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;

            // Update vision system
            //_vision?.OnUpdate(dt);

            // Handle rotation (only when not alert)
            if (_vision?.GetState() != VisionComponent.VisionState.Alert)
            {
                UpdateRotation(dt);
            }
        }

        private void UpdateRotation(float dt)
        {
            // Check if it's time to pick a new target rotation
            if (!_isRotating)
            {
                _rotationTimer += dt;

                if (_rotationTimer >= _rotationInterval)
                {
                    _rotationTimer = 0f;

                    // Set new target rotation (90 degrees from current target)
                    _targetYRotation += 90f;

                    // Normalize to [0, 360) range
                    if (_targetYRotation >= 360f)
                        _targetYRotation -= 360f;

                    _isRotating = true;
                    API.Log($"[EnemyController] Starting rotation to {_targetYRotation}°");
                }
            }

            // Smoothly interpolate toward target rotation
            if (_isRotating)
            {
                // Calculate shortest angle difference
                float angleDifference = _targetYRotation - _currentYRotation;

                // Normalize angle difference to [-180, 180] range for shortest path
                while (angleDifference > 180f) angleDifference -= 360f;
                while (angleDifference < -180f) angleDifference += 360f;

                // Calculate rotation step for this frame
                float rotationStep = _rotationSpeed * dt;

                // Check if we're close enough to snap to target
                if (Math.Abs(angleDifference) <= rotationStep)
                {
                    // Snap to target and stop rotating
                    _currentYRotation = _targetYRotation;
                    _isRotating = false;
                    API.Log($"[EnemyController] Completed rotation at {_currentYRotation}°");
                }
                else
                {
                    // Smoothly rotate toward target
                    _currentYRotation += Math.Sign(angleDifference) * rotationStep;

                    // Normalize current rotation to [0, 360) range
                    while (_currentYRotation >= 360f) _currentYRotation -= 360f;
                    while (_currentYRotation < 0f) _currentYRotation += 360f;
                }

                // Apply the smoothed rotation to the entity
                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);
            }
        }

        // === VISION EVENT HANDLERS ===
        private void OnPlayerDetected(ulong target, Vec3 position)
        {
            API.Log(">>> ENEMY ALERTED! STOPPING PATROL! <<<");

            // Instantly rotate to face the player
            Vec3 enemyPos = API.GetPosition(Entity);
            Vec3 directionToPlayer = new Vec3(
                position.X - enemyPos.X,
                0f,
                position.Z - enemyPos.Z
            );

            float distToPlayer = (float)Math.Sqrt(
                directionToPlayer.X * directionToPlayer.X +
                directionToPlayer.Z * directionToPlayer.Z
            );

            if (distToPlayer > 0f)
            {
                // Calculate angle to player
                float lookAtYaw = (float)(Math.Atan2(directionToPlayer.X, directionToPlayer.Z) * 180.0 / Math.PI);

                // Set target and current to face player immediately
                _targetYRotation = lookAtYaw;
                _currentYRotation = lookAtYaw;
                _isRotating = false;

                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);

                API.Log($"[EnemyController] Snapped to face player at {_currentYRotation:F1}°");
            }

            // Play alert sound
            API.PlaySoundAt("enemy_alert", "Resources/Audio/playerPunch_1.wav", enemyPos, false);
            API.SetSoundVolume("enemy_alert", 0.8f);

            // Damage player (only once per detection)
            if (!_hasDealtDamage)
            {
                _hasDealtDamage = true;
                API.Log($"[EnemyController] Dealing damage to player!");
                PlayerManager.NotifyPlayerCaught(Entity);
            }
        }

        private void OnPlayerLost(ulong target, Vec3 lastKnownPosition)
        {
            API.Log("[EnemyController] Lost sight of player, searching...");

            // Reset damage flag so player can be caught again
            _hasDealtDamage = false;

            // Resume rotation patrol
            _rotationTimer = 0f;
            _isRotating = false;
            // TODO: Search behavior, investigate last known position
        }

        private void OnPlayerTracking(ulong target, Vec3 position)
        {
            // Smoothly track the player while they're visible
            Vec3 enemyPos = API.GetPosition(Entity);
            Vec3 directionToPlayer = new Vec3(
                position.X - enemyPos.X,
                0f,
                position.Z - enemyPos.Z
            );

            float distToPlayer = (float)Math.Sqrt(
                directionToPlayer.X * directionToPlayer.X +
                directionToPlayer.Z * directionToPlayer.Z
            );

            if (distToPlayer > 0f)
            {
                float lookAtYaw = (float)(Math.Atan2(directionToPlayer.X, directionToPlayer.Z) * 180.0 / Math.PI);

                // Update target to follow player
                _targetYRotation = lookAtYaw;
                _currentYRotation = lookAtYaw;

                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);
            }
        }

        // === PUBLIC CONFIGURATION ===

        /// <summary>
        /// Set how fast the enemy rotates (degrees per second)
        /// </summary>
        public void SetRotationSpeed(float degreesPerSecond)
        {
            _rotationSpeed = degreesPerSecond;
            API.Log($"[EnemyController] Rotation speed set to {_rotationSpeed}°/s");
        }

        /// <summary>
        /// Set interval between rotation changes
        /// </summary>
        public void SetRotationInterval(float seconds)
        {
            _rotationInterval = seconds;
            API.Log($"[EnemyController] Rotation interval set to {_rotationInterval}s");
        }

        public void OnDestroy()
        {
            _vision?.OnDestroy();
            API.Log($"[EnemyController] OnDestroy() - Entity: {Entity}");
        }
    }
}