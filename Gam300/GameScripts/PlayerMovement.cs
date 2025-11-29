using Boom;
using System;
using System.Runtime.CompilerServices;

namespace GameScripts
{
    public static class HUD
    {
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
        public ulong Entity;

        private float _walkSpeed = 3f;
        private float _sprintSpeed = 8f;
        private float _sneakSpeed = 1.5f;
        private float _jumpSpeed = 5f;

        private int _health = 5;
        private int _maxHealth = 5;
        private Vec3 _spawnPoint;
        private bool _isRespawning = false;
        private float _respawnDelay = 1.0f;
        private float _respawnTimer = 0f;
        private bool _isInvulnerable = false;
        private float _invulnerabilityDuration = 2.0f;
        private float _invulnerabilityTimer = 0f;

        private enum FadeState { None, FadingOut, BlackHold, FadingIn }
        private FadeState _fadeState = FadeState.None;
        private float _fadeTimer = 0f;
        private float _fadeOutDuration = 0.5f;
        private float _blackHoldDuration = 0.15f;
        private float _fadeInDuration = 0.75f;

        private FootstepComponent _footstepComponent;

        private bool _wasSpacePressed = false;
        private bool _hasJumped = false;

        private static ulong s_playerEntity = 0;
        private static PlayerMovement s_instance = null;

        private float _modelForwardOffset = 0;

        private const float MOVE_EPS = 0.10f;
        private const float SPEED_DAMP = 10f;
        private double _smoothedSpeed = 0.0;
        private bool _hasAnimator = false;

        private float _rollSpeed = 14.0f;
        private float _rollDuration = 0.9f;
        private float _rollCooldown = 0.35f;

        private bool _isRolling = false;
        private float _rollTimer = 0f;
        private float _rollCooldownTimer = 0f;
        private bool _wasCtrlPressed = false;
        private Vec3 _rollDir = new Vec3(0, 0, 0);

        // ==== Crouch / Stealth Fields ====
        // Using existing key constant (Q) since KEY_C does not exist in API.
        private const int CROUCH_KEY = API.KEY_Q;
        private bool _inCrouchZone = false;
        private bool _isCrouching = false;
        private static bool s_isStealthInvisible = false;

        public void OnStart(string jsonParams)
        {
            API.Log($"[PlayerMovement] OnStart() - Entity: {Entity}");
            API.SetScreenFadeAlpha(0f);
            s_playerEntity = Entity;
            s_instance = this;

            PlayerManager.RegisterPlayer(this);

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

            _spawnPoint = API.GetPosition(Entity);
            _health = _maxHealth;

            _footstepComponent = new FootstepComponent { Entity = Entity };
            _footstepComponent.OnStart("");

            if (API.HasAnimator(Entity))
            {
                _hasAnimator = true;
                API.AnimatorSetFloat(Entity, "Speed", 0f);
                API.AnimatorSetBool(Entity, "IsMoving", false);
                API.AnimatorSetBool(Entity, "Sprint", false);
                API.AnimatorSetBool(Entity, "IsSneaking", false);
                API.AnimatorSetBool(Entity, "IsRolling", false);
                // Always set (engine should ignore unknown params gracefully)
                API.AnimatorSetBool(Entity, "IsCrouching", false);
            }

            RegisterTriggerCallbacksOnAllTriggers();
            HUD.SetHealth(_health, _maxHealth);
        }

        public void OnCaughtByEnemy(ulong enemyEntity)
        {
            if (_isInvulnerable || _isRespawning) return;

            _health--;
            HUD.SetHealth(_health, _maxHealth);

            Vec3 playerPos = API.GetPosition(Entity);
            API.PlaySoundAt("player_damage", "Resources/Audio/playerPunch_1.wav", playerPos, false);
            API.SetSoundVolume("player_damage", 1.0f);

            if (_health <= 0)
                RestartLevel();
            else
                StartRespawn();
        }

        private void StartRespawn()
        {
            _isRespawning = true;
            _respawnTimer = 0f;
            API.SetLinearVelocity(Entity, new Vec3(0, 0, 0));
            _fadeState = FadeState.FadingOut;
            _fadeTimer = 0f;
            API.SetScreenFadeAlpha(0f);
        }

        private void RestartLevel()
        {
            Vec3 playerPos = API.GetPosition(Entity);
            API.PlaySoundAt("player_death", "Resources/Audio/playerPunch_1.wav", playerPos, false);
            API.SetSoundVolume("player_death", 1.0f);
            API.LoadScene(API.GetCurrentSceneName());
        }

        private void RespawnAtCheckpoint()
        {
            API.TeleportRigidBody(Entity, _spawnPoint);
            API.SetLinearVelocity(Entity, new Vec3(0, 0, 0));
            _isInvulnerable = true;
            _invulnerabilityTimer = 0f;
            HUD.SetHealth(_health, _maxHealth);
            _isRespawning = false;
        }

        public void UpdateCheckpoint(Vec3 newCheckpoint)
        {
            _spawnPoint = newCheckpoint;
            API.PlaySoundAt("checkpoint_save", "Resources/Audio/playerPunch_1.wav", newCheckpoint, false);
            API.SetSoundVolume("checkpoint_save", 0.8f);
        }

        private void RegisterTriggerCallbacksOnAllTriggers()
        {
            string[] triggerNames = { "Checkpoint", "DamageZone", "PowerUp", "DoorTrigger", "TriggerVolume", "AreaTrigger" };
            foreach (string triggerName in triggerNames)
            {
                ulong triggerEntity = API.FindEntity(triggerName);
                if (triggerEntity == 0) continue;
                if (API.HasCollider(triggerEntity) && API.IsTrigger(triggerEntity))
                {
                    API.RegisterTriggerEnterCallback(triggerEntity, OnTriggerEnter);
                    API.RegisterTriggerExitCallback(triggerEntity, OnTriggerExit);
                }
            }

            // Register CrouchTriggerZone separately with detailed logging
            ulong crouchZone = API.FindEntity("CrouchTriggerZone");
            API.Log($"[PlayerMovement] CrouchTriggerZone entity ID: {crouchZone}");

            if (crouchZone != 0)
            {
                bool hasCollider = API.HasCollider(crouchZone);
                API.Log($"[PlayerMovement] CrouchTriggerZone HasCollider: {hasCollider}");

                if (hasCollider)
                {
                    bool isTrigger = API.IsTrigger(crouchZone);
                    API.Log($"[PlayerMovement] CrouchTriggerZone IsTrigger: {isTrigger}");

                    // Try registering anyway, even if IsTrigger returns false
                    API.RegisterTriggerEnterCallback(crouchZone, OnTriggerEnter);
                    API.RegisterTriggerExitCallback(crouchZone, OnTriggerExit);
                    API.Log("[PlayerMovement] Registered callbacks for CrouchTriggerZone (forced)");
                }
                else
                {
                    API.Log("[PlayerMovement] ERROR: CrouchTriggerZone has no collider component!");
                }
            }
            else
            {
                API.Log("[PlayerMovement] ERROR: CrouchTriggerZone entity not found in scene!");
            }
        }

        private void UpdateFade(float dt)
        {
            switch (_fadeState)
            {
                case FadeState.FadingOut:
                    _fadeTimer += dt;
                    API.SetScreenFadeAlpha(Clamp01(_fadeTimer / Math.Max(0.0001f, _fadeOutDuration)));
                    if (_fadeTimer >= _fadeOutDuration)
                    {
                        API.SetScreenFadeAlpha(1f);
                        _fadeState = FadeState.BlackHold;
                        _fadeTimer = 0f;
                        RespawnAtCheckpoint();
                    }
                    break;
                case FadeState.BlackHold:
                    _fadeTimer += dt;
                    if (_fadeTimer >= _blackHoldDuration)
                    {
                        _fadeState = FadeState.FadingIn;
                        _fadeTimer = 0f;
                    }
                    break;
                case FadeState.FadingIn:
                    _fadeTimer += dt;
                    float a = 1f - Clamp01(_fadeTimer / Math.Max(0.0001f, _fadeInDuration));
                    API.SetScreenFadeAlpha(a);
                    if (_fadeTimer >= _fadeInDuration)
                    {
                        _fadeState = FadeState.None;
                        API.SetScreenFadeAlpha(0f);
                    }
                    break;
            }
        }

        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);

        public void OnUpdate(float dt)
        {
            UpdateFade(dt);
            if (!API.HasTransform(Entity) || !API.HasScript(Entity)) return;
            _footstepComponent?.OnUpdate(dt);
            if (_isRespawning) return;

            if (_isInvulnerable)
            {
                _invulnerabilityTimer += dt;
                if (_invulnerabilityTimer >= _invulnerabilityDuration)
                    _isInvulnerable = false;
            }

            // Crouch logic (use hold-Q while inside zone)
            if (_inCrouchZone)
            {
                bool crouchDown = API.IsKeyDown(CROUCH_KEY);
                // Add debugging
                if (crouchDown != _isCrouching)
                {
                    API.Log($"[PlayerMovement] Crouch state changing: crouchDown={crouchDown}, _isCrouching={_isCrouching}");
                }

                if (crouchDown && !_isCrouching)
                {
                    _isCrouching = true;
                    s_isStealthInvisible = true;
                    if (_hasAnimator) API.AnimatorSetBool(Entity, "IsCrouching", true);
                    API.Log("[PlayerMovement] Player entered crouch state");
                }
                else if (!crouchDown && _isCrouching)
                {
                    _isCrouching = false;
                    s_isStealthInvisible = false;
                    if (_hasAnimator) API.AnimatorSetBool(Entity, "IsCrouching", false);
                    API.Log("[PlayerMovement] Player exited crouch state");
                }
            }
            else if (_isCrouching)
            {
                _isCrouching = false;
                s_isStealthInvisible = false;
                if (_hasAnimator) API.AnimatorSetBool(Entity, "IsCrouching", false);
            }

            if (_isCrouching)
            {
                var locked = API.GetLinearVelocity(Entity);
                locked.X = 0f;
                locked.Z = 0f;
                API.SetLinearVelocity(Entity, locked);
                if (_hasAnimator)
                {
                    API.AnimatorSetFloat(Entity, "Speed", 0f);
                    API.AnimatorSetBool(Entity, "IsMoving", false);
                    API.AnimatorSetBool(Entity, "Sprint", false);
                    API.AnimatorSetBool(Entity, "IsSneaking", false);
                }
                return;
            }

            var vel = API.GetLinearVelocity(Entity);
            bool allowMove = !API.IsMouseDown(API.MOUSE_RIGHT);
            bool isGrounded = IsPlayerGrounded();

            float inputX = 0f, inputZ = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(API.KEY_A)) inputX += 1f;
                if (API.IsKeyDown(API.KEY_D)) inputX -= 1f;
                if (API.IsKeyDown(API.KEY_W)) inputZ += 1f;
                if (API.IsKeyDown(API.KEY_S)) inputZ -= 1f;
            }

            bool hasInput = (inputX != 0f || inputZ != 0f);
            bool sprintKey = API.IsKeyDown(API.KEY_LEFT_SHIFT);
            bool sneakKey = API.IsKeyDown(API.KEY_LEFT_CONTROL);

            float currentSpeed = _walkSpeed;
            if (sneakKey) currentSpeed = _sneakSpeed;
            else if (sprintKey) currentSpeed = _sprintSpeed;

            if (hasInput)
            {
                float camYawDeg = API.GetThirdPersonCameraYaw();
                float yawRad = camYawDeg * (float)Math.PI / 180f;
                Vec3 forward = new Vec3((float)Math.Sin(yawRad), 0f, (float)Math.Cos(yawRad));
                Vec3 right = new Vec3((float)Math.Cos(yawRad), 0f, -(float)Math.Sin(yawRad));
                Vec3 moveDir = new Vec3(
                    right.X * inputX + forward.X * inputZ,
                    0f,
                    right.Z * inputX + forward.Z * inputZ
                );
                float len = (float)Math.Sqrt(moveDir.X * moveDir.X + moveDir.Z * moveDir.Z);
                if (len > 0f)
                {
                    vel.X = (moveDir.X / len) * currentSpeed;
                    vel.Z = (moveDir.Z / len) * currentSpeed;
                    float targetYaw = (float)(Math.Atan2(moveDir.X, moveDir.Z) * 180.0 / Math.PI) + _modelForwardOffset;
                    while (targetYaw > 180f) targetYaw -= 360f;
                    while (targetYaw < -180f) targetYaw += 360f;
                    API.SetRotationY(Entity, targetYaw);
                }
            }
            else
            {
                vel.X = 0f;
                vel.Z = 0f;
            }

            bool spaceDown = API.IsKeyDown(API.KEY_SPACE);
            if (isGrounded && _hasJumped && vel.Y <= 0.1f) _hasJumped = false;
            if (allowMove && isGrounded && spaceDown && !_wasSpacePressed && !_hasJumped)
            {
                vel.Y = _jumpSpeed;
                _hasJumped = true;
                if (_hasAnimator) API.AnimatorSetTrigger(Entity, "Jump");
            }
            _wasSpacePressed = spaceDown;

            bool ctrlDown = API.IsKeyDown(API.KEY_LEFT_CONTROL);
            Vec3 desiredMoveDir = new Vec3(0, 0, 0);
            if (hasInput)
            {
                float camYawDeg = API.GetThirdPersonCameraYaw();
                float camYawRad = camYawDeg * (float)Math.PI / 180f;
                Vec3 fwd = new Vec3((float)Math.Sin(camYawRad), 0f, (float)Math.Cos(camYawRad));
                Vec3 right = new Vec3((float)Math.Cos(camYawRad), 0f, -(float)Math.Sin(camYawRad));
                desiredMoveDir = new Vec3(
                    right.X * inputX + fwd.X * inputZ,
                    0f,
                    right.Z * inputX + fwd.Z * inputZ
                );
            }

            if (!_isRolling && _rollCooldownTimer <= 0f && isGrounded && ctrlDown && !_wasCtrlPressed && hasInput)
            {
                float len = (float)Math.Sqrt(desiredMoveDir.X * desiredMoveDir.X + desiredMoveDir.Z * desiredMoveDir.Z);
                _rollDir = (len > 0f)
                    ? new Vec3(desiredMoveDir.X / len, 0f, desiredMoveDir.Z / len)
                    : new Vec3((float)Math.Sin(API.GetRotationY(Entity) * Math.PI / 180f), 0f,
                               (float)Math.Cos(API.GetRotationY(Entity) * Math.PI / 180f));
                _isRolling = true;
                _rollTimer = _rollDuration;
                if (_hasAnimator)
                {
                    API.AnimatorSetTrigger(Entity, "Roll");
                    API.AnimatorSetBool(Entity, "IsRolling", true);
                    API.AnimatorSetBool(Entity, "IsSneaking", false);
                    API.AnimatorSetBool(Entity, "Sprint", false);
                }
                var burst = API.GetLinearVelocity(Entity);
                burst.X = _rollDir.X * _rollSpeed;
                burst.Z = _rollDir.Z * _rollSpeed;
                API.SetLinearVelocity(Entity, burst);
                _isInvulnerable = true;
                _wasCtrlPressed = ctrlDown;
                return;
            }

            if (_isRolling)
            {
                _rollTimer -= dt;
                var rv = API.GetLinearVelocity(Entity);
                rv.X = _rollDir.X * _rollSpeed;
                rv.Z = _rollDir.Z * _rollSpeed;
                API.SetLinearVelocity(Entity, rv);
                if (_rollTimer <= 0f)
                {
                    _isRolling = false;
                    _rollCooldownTimer = _rollCooldown;
                    _isInvulnerable = false;
                    if (_hasAnimator) API.AnimatorSetBool(Entity, "IsRolling", false);
                }
                _wasCtrlPressed = ctrlDown;
                return;
            }

            if (_rollCooldownTimer > 0f)
                _rollCooldownTimer = Math.Max(0f, _rollCooldownTimer - dt);
            _wasCtrlPressed = ctrlDown;

            if (vel.Y > 7.5f) vel.Y = 7.5f;
            API.SetLinearVelocity(Entity, vel);

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

        private bool IsPlayerGrounded()
        {
            Vec3 p = API.GetPosition(Entity);
            Vec3 start = new Vec3(p.X, p.Y + 0.1f, p.Z);
            Vec3 end = new Vec3(p.X, p.Y - 0.6f, p.Z);
            return !API.Linecast(start, end, Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            try
            {
                if (otherEntity != s_playerEntity) return;
                ulong crouchZone = API.FindEntity("CrouchTriggerZone");
                API.Log($"[PlayerMovement] OnTriggerEnter: triggerEntity={triggerEntity}, crouchZone={crouchZone}");

                if (triggerEntity == crouchZone && crouchZone != 0 && s_instance != null)
                {
                    s_instance._inCrouchZone = true;
                    API.Log("[PlayerMovement] Player ENTERED CrouchTriggerZone");
                    return;
                }
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] ERROR in OnTriggerEnter: {ex.Message}");
            }
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            try
            {
                if (otherEntity != s_playerEntity) return;
                ulong crouchZone = API.FindEntity("CrouchTriggerZone");
                API.Log($"[PlayerMovement] OnTriggerExit: triggerEntity={triggerEntity}, crouchZone={crouchZone}");

                if (triggerEntity == crouchZone && crouchZone != 0 && s_instance != null)
                {
                    s_instance._inCrouchZone = false;
                    API.Log("[PlayerMovement] Player EXITED CrouchTriggerZone");

                    if (s_instance._isCrouching)
                    {
                        s_instance._isCrouching = false;
                        s_isStealthInvisible = false;
                        if (s_instance._hasAnimator)
                            API.AnimatorSetBool(s_instance.Entity, "IsCrouching", false);
                    }
                    return;
                }
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] ERROR in OnTriggerExit: {ex.Message}");
            }
        }

        public void OnDestroy()
        {
            PlayerManager.UnregisterPlayer();
            if (s_playerEntity == Entity)
            {
                s_playerEntity = 0;
                s_instance = null;
            }
            _footstepComponent?.OnDestroy();
            if (API.HasCollider(Entity))
                API.UnregisterTriggerCallbacks(Entity);
            API.SetScreenFadeAlpha(0f);
        }

        public void SetFootstepVolume(float volume) => _footstepComponent?.SetFootstepVolume(volume);
        public void SetFootstepInterval(float interval) => _footstepComponent?.SetFootstepInterval(interval);
        public void SetModelForwardOffset(float degrees) => _modelForwardOffset = degrees;
        public void SetMovementSpeeds(float walk, float sprint, float sneak)
        {
            _walkSpeed = walk; _sprintSpeed = sprint; _sneakSpeed = sneak;
        }
        public int GetHealth() => _health;
        public static ulong GetPlayerEntity() => s_playerEntity;
        public static bool IsPlayerInvisibleToEnemies() => s_isStealthInvisible;
    }
}