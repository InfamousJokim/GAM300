using Boom;

using System;
using System.Runtime.CompilerServices;

/// <summary>
/// Handles player movement using PhysX linear velocity control.
/// Supports WASD movement, jumping with Space, and mouse-look control.
/// Movement is relative to the camera's facing direction.
/// Attach this script to any entity with TransformComponent and ScriptComponent.
/// </summary>
namespace GameScripts
{

    public static class HUD
    {
        // 0..1
        public static float HealthRatio = 1f;

        public static void SetHealth(int hp, int max)
        {
            if (max <= 0) { HealthRatio = 0f; return; }
            float r = (float)hp / (float)max;
            if (r < 0f) r = 0f;
            if (r > 1f) r = 1f;
            HealthRatio = r;
        }
    }


    public class PlayerMovement
    {
        // This field is automatically set by the scripting system
        public ulong Entity;

        // Movement parameters (configurable)
        private float _walkSpeed = 3f;
        private float _sprintSpeed = 8f;
        private float _sneakSpeed = 1.5f;
        private float _jumpSpeed = 5f;

        // Health system
        private int _health = 5;
        private int _maxHealth = 5;
        private Vec3 _spawnPoint;
        private bool _isRespawning = false;
        private float _respawnDelay = 1.0f; // no longer used (kept for compatibility)
        private float _respawnTimer = 0f;   // no longer used (kept for compatibility)
        private bool _isInvulnerable = false;
        private float _invulnerabilityDuration = 2.0f;
        private float _invulnerabilityTimer = 0f;

        // Screen fade system (fade-out to black, teleport, then fade-in)
        private enum FadeState { None, FadingOut, BlackHold, FadingIn }
        private FadeState _fadeState = FadeState.None;
        private float _fadeTimer = 0f;
        private float _fadeOutDuration = 0.5f;   // time to fade to black
        private float _blackHoldDuration = 0.15f; // time to stay fully black
        private float _fadeInDuration = 0.75f;   // time to fade back to scene

        // Footstep system
        private FootstepComponent _footstepComponent;

        // Jump state tracking
        private bool _wasSpacePressed = false;
        private bool _hasJumped = false;

        // Static reference to player entity for trigger callbacks
        private static ulong s_playerEntity = 0;

        // NEW: Rotation smoothing
        //private float _rotationSpeed = 10f; // degrees per frame to turn

        // NEW: Model forward direction offset (adjust if model faces wrong way)
        private float _modelForwardOffset = 0; // 180° if model faces backwards, 0° if correct

        // Animation system
        private const float MOVE_EPS = 0.10f;  // animator "moving" threshold
        private const float SPEED_DAMP = 10f;    // smoothing for animator Speed
        private double _smoothedSpeed = 0.0;
        private bool _hasAnimator = false;



        // ===== Roll config/state =====
        private float _rollSpeed = 14.0f;        // burst speed while rolling
        private float _rollDuration = 0.9f;    // seconds, match your roll clip
        private float _rollCooldown = 0.35f;    // small cooldown between rolls

        private bool _isRolling = false;
        private float _rollTimer = 0f;
        private float _rollCooldownTimer = 0f;
        private bool _wasCtrlPressed = false;  // edge detection
        private Vec3 _rollDir = new Vec3(0, 0, 0); // cached direction for the whole roll

        /// <summary>
        /// Called once when the script is first created.
        /// </summary>
        public void OnStart(string jsonParams)
        {

            API.Log($"[PlayerMovement] OnStart() - Entity: {Entity}");

            // Ensure we start fully visible
            API.SetScreenFadeAlpha(0f);

            // Store player entity reference for static callbacks
            s_playerEntity = Entity;

            // Register with PlayerManager for enemy interactions
            PlayerManager.RegisterPlayer(this);

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

            // Initialize spawn point as starting position
            _spawnPoint = API.GetPosition(Entity);
            API.Log($"[PlayerMovement] Spawn point set to: ({_spawnPoint.X:F2}, {_spawnPoint.Y:F2}, {_spawnPoint.Z:F2})");

            // Initialize health
            _health = _maxHealth;
            API.Log($"[PlayerMovement] Health initialized: {_health}/{_maxHealth} HP");

            // Initialize footstep component
            _footstepComponent = new FootstepComponent();
            _footstepComponent.Entity = Entity;
            _footstepComponent.OnStart("");

            // Initialize animator if present
            if (API.HasAnimator(Entity))
            {
                _hasAnimator = true;
                API.AnimatorSetFloat(Entity, "Speed", 0f);
                API.AnimatorSetBool(Entity, "IsMoving", false);
                API.AnimatorSetBool(Entity, "Sprint", false);
                API.AnimatorSetBool(Entity, "IsSneaking", false);
                API.AnimatorSetBool(Entity, "IsRolling", false);
                API.Log("[PlayerMovement] Animator initialized");
            }
            else
            {
                API.Log("[PlayerMovement] No AnimatorComponent found - animations disabled");
            }

            // RE-ENABLE trigger callbacks (simple version)
            API.Log("[PlayerMovement] Registering trigger callbacks...");
            RegisterTriggerCallbacksOnAllTriggers();

            API.Log($"[PlayerMovement] Using camera-relative PhysX movement:");
            API.Log($"  Walk Speed: {_walkSpeed}, Sprint Speed: {_sprintSpeed}, Sneak Speed: {_sneakSpeed}");
            API.Log($"  Jump Speed: {_jumpSpeed}");
            API.Log($"[PlayerMovement] Model forward offset: {_modelForwardOffset} degrees");

            _health = _maxHealth;
            HUD.SetHealth(_health, _maxHealth);

        }

        /// <summary>
        /// Handle getting caught by an enemy
        /// </summary>
        public void OnCaughtByEnemy(ulong enemyEntity)
        {
            // Check if player is invulnerable (just respawned)
            if (_isInvulnerable)
            {
                API.Log("[PlayerMovement] Player is invulnerable, ignoring detection");
                return;
            }

            // Check if already respawning
            if (_isRespawning)
            {
                API.Log("[PlayerMovement] Already respawning, ignoring detection");
                return;
            }

            API.Log($"[PlayerMovement] CAUGHT BY ENEMY (ID: {enemyEntity})! Current HP: {_health}");

            // Lose 1 HP
            _health--;
            HUD.SetHealth(_health, _maxHealth);
            API.Log($"[PlayerMovement] HP reduced to: {_health}/{_maxHealth}");

            // Play damage sound
            Vec3 playerPos = API.GetPosition(Entity);
            API.PlaySoundAt("player_damage", "Resources/Audio/playerPunch_1.wav", playerPos, false);
            API.SetSoundVolume("player_damage", 1.0f);

            // Check if dead
            if (_health <= 0)
            {
                API.Log("[PlayerMovement] HP reached 0! Restarting level...");
                RestartLevel();
            }
            else
            {
                API.Log($"[PlayerMovement] Respawning at checkpoint... {_health} HP remaining");
                StartRespawn();
            }
        }

        /// <summary>
        /// Start the respawn process (fade-out -> teleport -> fade-in)
        /// </summary>
        private void StartRespawn()
        {
            _isRespawning = true;
            _respawnTimer = 0f;

            // Stop player movement
            API.SetLinearVelocity(Entity, new Vec3(0, 0, 0));

            // Begin fade-out sequence
            _fadeState = FadeState.FadingOut;
            _fadeTimer = 0f;
            API.SetScreenFadeAlpha(0f);

            API.Log("[PlayerMovement] Respawn initiated (fade-out)...");
        }

        /// <summary>
        /// Restart the entire level
        /// </summary>
        private void RestartLevel()
        {
            API.Log("[PlayerMovement] === GAME OVER - RESTARTING LEVEL ===");

            // Play death sound
            Vec3 playerPos = API.GetPosition(Entity);
            API.PlaySoundAt("player_death", "Resources/Audio/playerPunch_1.wav", playerPos, false);
            API.SetSoundVolume("player_death", 1.0f);

            // Reload the current scene after a brief delay
            string currentScene = API.GetCurrentSceneName();
            API.Log($"[PlayerMovement] Reloading scene: {currentScene}");

            // Note: For a fancier effect, you can also fade to black here before loading.
            API.LoadScene(currentScene);
        }

        /// <summary>
        /// Respawn the player at the checkpoint
        /// </summary>
        private void RespawnAtCheckpoint()
        {
            API.Log($"[PlayerMovement] Respawning at checkpoint: ({_spawnPoint.X:F2}, {_spawnPoint.Y:F2}, {_spawnPoint.Z:F2})");

            // Use TeleportRigidBody instead of SetPosition to properly update PhysX
            API.TeleportRigidBody(Entity, _spawnPoint);

            // Velocities are already cleared by TeleportRigidBody, but let's be explicit
            API.SetLinearVelocity(Entity, new Vec3(0, 0, 0));

            // Enable invulnerability
            _isInvulnerable = true;
            _invulnerabilityTimer = 0f;

            HUD.SetHealth(_health, _maxHealth);

            // Reset respawn state
            _isRespawning = false;

            API.Log($"[PlayerMovement] Respawn complete! Invulnerable for {_invulnerabilityDuration} seconds");
        }

        /// <summary>
        /// Update checkpoint position (can be called from trigger zones)
        /// </summary>
        public void UpdateCheckpoint(Vec3 newCheckpoint)
        {
            _spawnPoint = newCheckpoint;
            API.Log($"[PlayerMovement] Checkpoint updated to: ({_spawnPoint.X:F2}, {_spawnPoint.Y:F2}, {_spawnPoint.Z:F2})");

            // Play checkpoint sound
            API.PlaySoundAt("checkpoint_save", "Resources/Audio/playerPunch_1.wav", newCheckpoint, false);
            API.SetSoundVolume("checkpoint_save", 0.8f);
        }

        private void RegisterTriggerCallbacksOnAllTriggers()
        {
            // Try to find common trigger entities and register callbacks on them
            string[] triggerNames = { "Checkpoint", "DamageZone", "PowerUp", "DoorTrigger", "TriggerVolume", "AreaTrigger" };

            int registeredCount = 0;
            foreach (string triggerName in triggerNames)
            {
                ulong triggerEntity = API.FindEntity(triggerName);
                API.Log($"[PlayerMovement] Looking for trigger: {triggerName}, found ID: {triggerEntity}");

                if (triggerEntity != 0)
                {
                    // Validate it has a collider and is a trigger
                    bool hasCollider = API.HasCollider(triggerEntity);
                    bool isTrigger = API.IsTrigger(triggerEntity);

                    API.Log($"[PlayerMovement] {triggerName} - HasCollider: {hasCollider}, IsTrigger: {isTrigger}");

                    if (hasCollider && isTrigger)
                    {
                        API.Log($"[PlayerMovement] Registering callbacks on {triggerName} (ID: {triggerEntity})");
                        API.RegisterTriggerEnterCallback(triggerEntity, OnTriggerEnter);
                        API.RegisterTriggerExitCallback(triggerEntity, OnTriggerExit);
                        registeredCount++;
                        API.Log($"[PlayerMovement] Successfully registered callbacks on {triggerName}");
                    }
                    else
                    {
                        API.Log($"[PlayerMovement] Skipping {triggerName} - not a proper trigger");
                    }
                }
                else
                {
                    API.Log($"[PlayerMovement] Trigger '{triggerName}' not found in scene");
                }
            }

            API.Log($"[PlayerMovement] Total callbacks registered: {registeredCount}");
        }

        /// <summary>
        /// Per-frame fade controller for respawn sequence
        /// </summary>
        private void UpdateFade(float dt)
        {
            switch (_fadeState)
            {
                case FadeState.None:
                    return;

                case FadeState.FadingOut:
                    {
                        _fadeTimer += dt;
                        float t = Clamp01(_fadeTimer / Math.Max(0.0001f, _fadeOutDuration));
                        API.SetScreenFadeAlpha(t);

                        if (_fadeTimer >= _fadeOutDuration)
                        {
                            // Fully black now -> teleport immediately
                            API.SetScreenFadeAlpha(1f);
                            _fadeState = FadeState.BlackHold;
                            _fadeTimer = 0f;

                            RespawnAtCheckpoint(); // sets _isRespawning = false and invulnerability, etc.
                            API.Log("[PlayerMovement] Fade-out complete, teleported to checkpoint.");
                        }
                        break;
                    }

                case FadeState.BlackHold:
                    {
                        _fadeTimer += dt;
                        API.SetScreenFadeAlpha(1f);
                        if (_fadeTimer >= _blackHoldDuration)
                        {
                            _fadeState = FadeState.FadingIn;
                            _fadeTimer = 0f;
                            API.Log("[PlayerMovement] Holding at black done, starting fade-in...");
                        }
                        break;
                    }

                case FadeState.FadingIn:
                    {
                        _fadeTimer += dt;
                        float t = 1f - Clamp01(_fadeTimer / Math.Max(0.0001f, _fadeInDuration));
                        API.SetScreenFadeAlpha(t);

                        if (_fadeTimer >= _fadeInDuration)
                        {
                            _fadeState = FadeState.None;
                            API.SetScreenFadeAlpha(0f);
                            API.Log("[PlayerMovement] Fade-in complete.");
                        }
                        break;
                    }
            }
        }

        private static float Clamp01(float v)
        {
            if (v < 0f) return 0f;
            if (v > 1f) return 1f;
            return v;
        }

        /// <summary>
        /// Called every frame to update movement using PhysX linear velocity.
        /// </summary>
        public void OnUpdate(float dt)
        {
            // Always update fade first (so it works both during and after respawn)
            UpdateFade(dt);

            // Safety check
            if (!API.HasTransform(Entity) || !API.HasScript(Entity))
                return;

            // Update footstep component
            _footstepComponent?.OnUpdate(dt);

            // Handle respawn: during fade-out/black-hold we block gameplay updates
            if (_isRespawning)
            {
                // While respawning (before teleport), we don't allow movement
                return; // Don't allow movement while respawning (fade-out/black)
            }

            // Handle invulnerability timer
            if (_isInvulnerable)
            {
                _invulnerabilityTimer += dt;
                if (_invulnerabilityTimer >= _invulnerabilityDuration)
                {
                    _isInvulnerable = false;
                    API.Log("[PlayerMovement] Invulnerability expired");
                }
            }

            // =============== CAMERA-RELATIVE PHYSX MOVEMENT =================

            var vel = API.GetLinearVelocity(Entity);

            // Check if the player is allowed to move (RMB held disables movement)
            bool allowMove = !API.IsMouseDown(API.MOUSE_RIGHT);

            // ===== FIX: PROPER GROUND DETECTION =====
            // Use raycast downward to check if player is on ground (not wall)
            bool isGrounded = IsPlayerGrounded();

            // Calculate horizontal movement input
            float inputX = 0f, inputZ = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(API.KEY_A)) inputX += 1f; // Left
                if (API.IsKeyDown(API.KEY_D)) inputX -= 1f; // Right
                if (API.IsKeyDown(API.KEY_W)) inputZ += 1f; // Forward
                if (API.IsKeyDown(API.KEY_S)) inputZ -= 1f; // Backward
            }

            // Track movement state for animations
            bool hasInput = (inputX != 0f || inputZ != 0f);

            // Check for sprint/sneak input - these now affect actual speed!
            bool sprintKey = API.IsKeyDown(API.KEY_LEFT_SHIFT);
            bool sneakKey = API.IsKeyDown(API.KEY_LEFT_CONTROL);

            // Determine current movement speed based on input modifiers
            float currentSpeed = _walkSpeed;
            if (sneakKey)
            {
                currentSpeed = _sneakSpeed;
            }
            else if (sprintKey)
            {
                currentSpeed = _sprintSpeed;
            }

            // Apply camera-relative movement
            if (hasInput)
            {
                // Get camera's yaw angle in degrees
                float cameraYawDegrees = API.GetThirdPersonCameraYaw();
                float cameraYawRadians = cameraYawDegrees * (float)Math.PI / 180f;

                // Calculate camera's forward and right vectors
                Vec3 cameraForward = new Vec3(
                    (float)Math.Sin(cameraYawRadians),
                    0f,
                    (float)Math.Cos(cameraYawRadians)
                );

                Vec3 cameraRight = new Vec3(
                    (float)Math.Cos(cameraYawRadians),
                    0f,
                    -(float)Math.Sin(cameraYawRadians)
                );

                // Calculate world-space movement direction
                Vec3 moveDirection = new Vec3(
                    cameraRight.X * inputX + cameraForward.X * inputZ,
                    0f,
                    cameraRight.Z * inputX + cameraForward.Z * inputZ
                );

                // Normalize and apply speed
                float len = (float)Math.Sqrt(moveDirection.X * moveDirection.X + moveDirection.Z * moveDirection.Z);
                if (len > 0f)
                {
                    vel.X = (moveDirection.X / len) * currentSpeed;
                    vel.Z = (moveDirection.Z / len) * currentSpeed;

                    // Calculate rotation
                    float targetYaw = (float)(Math.Atan2(moveDirection.X, moveDirection.Z) * 180.0 / Math.PI);
                    targetYaw += _modelForwardOffset;

                    // Normalize angle
                    while (targetYaw > 180f) targetYaw -= 360f;
                    while (targetYaw < -180f) targetYaw += 360f;

                    API.SetRotationY(Entity, targetYaw);
                }
            }
            else
            {
                // No input - stop horizontal movement
                vel.X = 0f;
                vel.Z = 0f;
            }

            // =============== JUMPING LOGIC =================

            bool spacePressed = API.IsKeyDown(API.KEY_SPACE);

            // Reset jump state when we land
            if (isGrounded && _hasJumped && vel.Y <= 0.1f)
            {
                _hasJumped = false;
            }

            // Jump only on space key press and when grounded
            if (allowMove && isGrounded && spacePressed && !_wasSpacePressed && !_hasJumped)
            {
                vel.Y = _jumpSpeed;
                _hasJumped = true;

                // Trigger jump animation
                if (_hasAnimator)
                {
                    API.AnimatorSetTrigger(Entity, "Jump");
                }

                // Play jump sound
                Vec3 playerPos = API.GetPosition(Entity);
                API.PlaySoundAt("jump_sound", "Resources/Audio/playerPunch_1.wav", playerPos, false);
                API.SetSoundVolume("jump_sound", 0.9f);

                API.Log("[PlayerMovement] Jump executed!");
            }

            _wasSpacePressed = spacePressed;

            bool ctrlDown = API.IsKeyDown(API.KEY_LEFT_CONTROL);

            Vec3 desiredMoveDir = new Vec3(0, 0, 0);
            if (hasInput)
            {
                float camYawDeg = API.GetThirdPersonCameraYaw();
                float camYawRad = camYawDeg * (float)Math.PI / 180f;

                Vec3 camForward = new Vec3((float)Math.Sin(camYawRad), 0f, (float)Math.Cos(camYawRad));
                Vec3 camRight = new Vec3((float)Math.Cos(camYawRad), 0f, -(float)Math.Sin(camYawRad));

                desiredMoveDir = new Vec3(
                    camRight.X * inputX + camForward.X * inputZ,
                    0f,
                    camRight.Z * inputX + camForward.Z * inputZ
                );
            }

            if (!_isRolling && _rollCooldownTimer <= 0f && isGrounded && ctrlDown && !_wasCtrlPressed && hasInput)
{
    // Normalize desiredMoveDir
    float len = (float)Math.Sqrt(desiredMoveDir.X * desiredMoveDir.X + desiredMoveDir.Z * desiredMoveDir.Z);
    if (len > 0f)
    {
        _rollDir = new Vec3(desiredMoveDir.X / len, 0f, desiredMoveDir.Z / len);
    }
    else
    {
        // fallback: face direction
        float yaw = API.GetRotationY(Entity) * (float)Math.PI / 180f;
        _rollDir = new Vec3((float)Math.Sin(yaw), 0f, (float)Math.Cos(yaw));
    }

    _isRolling = true;
    _rollTimer = _rollDuration;

    // Fire animation
    if (_hasAnimator)
    {
        API.AnimatorSetTrigger(Entity, "Roll");
        API.AnimatorSetBool(Entity, "IsRolling", true);
        API.AnimatorSetBool(Entity, "IsSneaking", false); // keep sneak off during roll
        API.AnimatorSetBool(Entity, "Sprint", false);
    }

    // First burst this frame
    var velBurst = API.GetLinearVelocity(Entity);
    velBurst.X = _rollDir.X * _rollSpeed;
    velBurst.Z = _rollDir.Z * _rollSpeed;
    API.SetLinearVelocity(Entity, velBurst);

    // Optional: tiny i-frames
    _isInvulnerable = true;

    // Cancel normal update this frame so WASD doesn’t override the burst
    _wasCtrlPressed = ctrlDown;
    return;
}

// Continue roll if active
if (_isRolling)
{
    _rollTimer -= dt;

    var v = API.GetLinearVelocity(Entity);
    v.X = _rollDir.X * _rollSpeed;
    v.Z = _rollDir.Z * _rollSpeed;
    API.SetLinearVelocity(Entity, v);

    // keep animator visually coherent
    if (_hasAnimator)
    {
        float displaySpeed = _rollSpeed;
        _smoothedSpeed += (displaySpeed - _smoothedSpeed) * 0.5;
        API.AnimatorSetFloat(Entity, "Speed", (float)_smoothedSpeed);
        API.AnimatorSetBool(Entity, "IsMoving", true);
        API.AnimatorSetBool(Entity, "IsRolling", true);
        API.AnimatorSetBool(Entity, "Sprint", false);
        API.AnimatorSetBool(Entity, "IsSneaking", false);
    }

    if (_rollTimer <= 0f)
    {
        _isRolling = false;
        _rollCooldownTimer = _rollCooldown;
        _isInvulnerable = false;
        if (_hasAnimator) API.AnimatorSetBool(Entity, "IsRolling", false);
    }

    _wasCtrlPressed = ctrlDown;
    return; // lock out normal movement while rolling
}

// tick cooldown & store ctrl edge
if (_rollCooldownTimer > 0f)
    _rollCooldownTimer = Math.Max(0f, _rollCooldownTimer - dt);
_wasCtrlPressed = ctrlDown;

            // === CLAMP UPWARD VELOCITY ===
            const float MaxUpwardVelocity = 7.5f;
            if (vel.Y > MaxUpwardVelocity)
            {
                vel.Y = MaxUpwardVelocity;
            }

            API.SetLinearVelocity(Entity, vel);

            // =============== UPDATE ANIMATOR =================
            if (_hasAnimator)
            {
                float speedXZ = (float)Math.Sqrt(vel.X * vel.X + vel.Z * vel.Z);
                _smoothedSpeed += (speedXZ - _smoothedSpeed) * Math.Min(1.0, SPEED_DAMP * dt);

                API.AnimatorSetFloat(Entity, "Speed", (float)_smoothedSpeed);
                API.AnimatorSetBool(Entity, "IsMoving", _smoothedSpeed > MOVE_EPS || hasInput);
                API.AnimatorSetBool(Entity, "Sprint", sprintKey && hasInput);
                API.AnimatorSetBool(Entity, "IsSneaking", sneakKey && hasInput);
            }
        }

        /// <summary>
        /// Check if the player is on the ground using a downward raycast.
        /// This prevents wall-climbing by only detecting surfaces below the player.
        /// </summary>
        private bool IsPlayerGrounded()
        {
            // Get player position
            Vec3 playerPos = API.GetPosition(Entity);

            // Raycast parameters (adjust based on your capsule height)
            float rayLength = 0.6f;      // Distance to check below player
            float rayOffsetY = 0.1f;     // Start slightly above feet to avoid self-collision

            // Cast ray downward from player position
            Vec3 rayStart = new Vec3(playerPos.X, playerPos.Y + rayOffsetY, playerPos.Z);
            Vec3 rayEnd = new Vec3(playerPos.X, playerPos.Y - rayLength, playerPos.Z);

            // FIX: Invert the result! 
            // Linecast returns TRUE if the path is CLEAR.
            // We want to return TRUE if we HIT something (path is NOT clear).
            bool hitGround = !API.Linecast(rayStart, rayEnd, Entity);

            return hitGround;
        }

        /// <summary>
        /// Trigger enter callback - called when the player enters a trigger volume
        /// </summary>
        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            try
            {
                API.Log($"[PlayerMovement] Trigger Enter - Trigger: {triggerEntity}, Other: {otherEntity}, Player: {s_playerEntity}");

                // Check if the "other" entity is actually the player
                if (otherEntity != s_playerEntity)
                {
                    API.Log($"[PlayerMovement] Trigger not involving player - ignoring");
                    return;
                }

                API.Log($"[PlayerMovement] Player entered trigger! Trigger ID: {triggerEntity}");

                // Get the position for 3D sound
                Vec3 triggerPos = new Vec3(0, 0, 0);
                if (API.HasTransform(triggerEntity))
                {
                    triggerPos = API.GetPosition(triggerEntity);
                }

                // Try to identify trigger by name lookup
                // Note: This approach searches for known trigger names
                bool soundPlayed = false;

                // Try common trigger names
                ulong checkpoint = API.FindEntity("Checkpoint");
                ulong damageZone = API.FindEntity("DamageZone");
                ulong powerup = API.FindEntity("PowerUp");
                ulong door = API.FindEntity("DoorTrigger");

                if (triggerEntity == checkpoint && checkpoint != 0)
                {
                    API.Log(">>> CHECKPOINT REACHED! <<<");
                    API.PlaySoundAt("checkpoint", "Resources/Audio/playerPunch_1.wav", triggerPos, false);
                    API.SetSoundVolume("checkpoint", 0.95f);
                    soundPlayed = true;
                }
                else if (triggerEntity == damageZone && damageZone != 0)
                {
                    API.Log(">>> PLAYER TAKING DAMAGE! <<<");
                    API.PlaySoundAt("damage", "Resources/Audio/playerPunch_1.wav", triggerPos, false);
                    API.SetSoundVolume("damage", 0.8f);
                    soundPlayed = true;
                }
                else if (triggerEntity == powerup && powerup != 0)
                {
                    API.Log(">>> POWER-UP COLLECTED! <<<");
                    API.PlaySoundAt("powerup", "Resources/Audio/playerPunch_1.wav", triggerPos, false);
                    API.SetSoundVolume("powerup", 0.9f);
                    soundPlayed = true;
                }
                else if (triggerEntity == door && door != 0)
                {
                    API.Log(">>> DOOR ACTIVATED! <<<");
                    API.PlaySoundAt("door_open", "Resources/Audio/playerPunch_1.wav", triggerPos, false);
                    API.SetSoundVolume("door_open", 0.85f);
                    soundPlayed = true;
                }

                // If no specific trigger was found, play a generic trigger sound
                if (!soundPlayed)
                {
                    API.Log($"[PlayerMovement] Generic trigger entered with entity ID: {triggerEntity}");
                    API.PlaySoundAt("generic_trigger", "Resources/Audio/playerPunch_1.wav", triggerPos, false);
                    API.SetSoundVolume("generic_trigger", 0.7f);
                }
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] ERROR in OnTriggerEnter: {ex.Message}");
            }
        }

        /// <summary>
        /// Trigger exit callback - called when the player exits a trigger volume
        /// </summary>
        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            try
            {
                API.Log($"[PlayerMovement] Trigger Exit - Trigger: {triggerEntity}, Other: {otherEntity}, Player: {s_playerEntity}");

                // Check if the "other" entity is actually the player
                if (otherEntity != s_playerEntity)
                {
                    API.Log($"[PlayerMovement] Trigger exit not involving player - ignoring");
                    return;
                }

                API.Log($"[PlayerMovement] Player exited trigger! Trigger ID: {triggerEntity}");

                // Find the trigger entity's name for more specific handling
                ulong damageZone = API.FindEntity("DamageZone");
                ulong door = API.FindEntity("DoorTrigger");

                // Handle trigger exit events
                if (triggerEntity == damageZone && damageZone != 0)
                {
                    API.Log(">>> PLAYER SAFE FROM DAMAGE! <<<");
                    API.StopSound("damage");
                }
                else if (triggerEntity == door && door != 0)
                {
                    API.Log(">>> PLAYER LEFT DOOR AREA <<<");
                    // Get door position for 3D sound
                    Vec3 doorPos = new Vec3(0, 0, 0);
                    if (API.HasTransform(triggerEntity))
                    {
                        doorPos = API.GetPosition(triggerEntity);
                    }
                    API.PlaySoundAt("door_close", "Resources/Audio/playerPunch_1.wav", doorPos, false);
                    API.SetSoundVolume("door_close", 0.75f);
                }
                else
                {
                    API.Log($"[PlayerMovement] Exited unknown trigger with entity ID: {triggerEntity}");
                }
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] ERROR in OnTriggerExit: {ex.Message}");
            }
        }

        /// <summary>
        /// Called when the script is destroyed (optional cleanup).
        /// </summary>
        public void OnDestroy()
        {
            API.Log($"[PlayerMovement] OnDestroy() - Entity: {Entity}");

            // Unregister from PlayerManager
            PlayerManager.UnregisterPlayer();

            // Clear static reference
            if (s_playerEntity == Entity)
            {
                s_playerEntity = 0;
            }

            // Cleanup footstep component
            _footstepComponent?.OnDestroy();

            // Unregister trigger callbacks to prevent memory leaks
            if (API.HasCollider(Entity))
            {
                API.UnregisterTriggerCallbacks(Entity);
                API.Log("[PlayerMovement] Unregistered trigger callbacks for player");
            }

            // Make sure we leave the screen visible
            API.SetScreenFadeAlpha(0f);
        }

        // Public methods for customization
        public void SetFootstepVolume(float volume)
        {
            _footstepComponent?.SetFootstepVolume(volume);
        }

        public void SetFootstepInterval(float interval)
        {
            _footstepComponent?.SetFootstepInterval(interval);
        }

        /// <summary>
        /// Set the model forward direction offset if your model faces the wrong way
        /// Common values: 0° (correct), 180° (backwards), 90° (right), -90° (left)
        /// </summary>
        public void SetModelForwardOffset(float degrees)
        {
            _modelForwardOffset = degrees;
            API.Log($"[PlayerMovement] Model forward offset set to: {_modelForwardOffset} degrees");
        }

        /// <summary>
        /// Configure movement speeds
        /// </summary>
        public void SetMovementSpeeds(float walkSpeed, float sprintSpeed, float sneakSpeed)
        {
            _walkSpeed = walkSpeed;
            _sprintSpeed = sprintSpeed;
            _sneakSpeed = sneakSpeed;
            API.Log($"[PlayerMovement] Speeds updated - Walk: {_walkSpeed}, Sprint: {_sprintSpeed}, Sneak: {_sneakSpeed}");
        }

        /// <summary>
        /// Get current health
        /// </summary>
        public int GetHealth()
        {
            return _health;
        }

        /// <summary>
        /// Get player entity ID (for enemy detection callbacks)
        /// </summary>
        public static ulong GetPlayerEntity()
        {
            return s_playerEntity;
        }
    }
}