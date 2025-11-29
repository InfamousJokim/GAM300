using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Controls 4 blood sprite overlays based on player HP.
    /// Sprites appear sequentially as HP decreases and have an intensifying heartbeat pulsing effect.
    /// The heartbeat becomes faster and more opaque as HP decreases.
    /// Attach this script to a UI entity that will manage the blood overlays.
    /// </summary>
    public class BloodOverlayController
    {
        public ulong Entity;

        // Entity names for the 4 blood sprite overlays
        private string _bloodSprite1Name = "Blood1";
        private string _bloodSprite2Name = "Blood2";
        private string _bloodSprite3Name = "Blood3";
        private string _bloodSprite4Name = "Blood4";

        // Entity handles
        private ulong _bloodSprite1 = 0;
        private ulong _bloodSprite2 = 0;
        private ulong _bloodSprite3 = 0;
        private ulong _bloodSprite4 = 0;

        // Heartbeat effect parameters (will be scaled based on HP)
        private float _baseHeartbeatSpeed = 2.0f;      // Base speed of heartbeat pulse
        private float _maxHeartbeatSpeed = 6.0f;       // Max speed when critically low HP

        // WIDER alpha range for more dramatic pulsing effect
        // At low HP: pulses from very dim to nearly opaque
        // At high HP: pulses from barely visible to moderately visible
        private float _baseHeartbeatMin = 0.2f;        // Minimum alpha during pulse (low visibility)
        private float _baseHeartbeatMax = 0.95f;       // Maximum alpha during pulse (nearly opaque)

        private float _heartbeatTimer = 0f;

        // HP thresholds (when to show each sprite)
        // Sprite 1: HP <= 80% (light damage)
        // Sprite 2: HP <= 60% (moderate damage)
        // Sprite 3: HP <= 40% (heavy damage)
        // Sprite 4: HP <= 20% (critical damage)
        private float _threshold1 = 0.8f;
        private float _threshold2 = 0.6f;
        private float _threshold3 = 0.4f;
        private float _threshold4 = 0.2f;

        public void OnStart(string jsonParams)
        {
            // Find blood sprite entities
            _bloodSprite1 = API.FindEntity(_bloodSprite1Name);
            _bloodSprite2 = API.FindEntity(_bloodSprite2Name);
            _bloodSprite3 = API.FindEntity(_bloodSprite3Name);
            _bloodSprite4 = API.FindEntity(_bloodSprite4Name);

            // Initialize all sprites to invisible
            if (_bloodSprite1 != 0 && API.HasSprite(_bloodSprite1)) API.SetSpriteAlpha(_bloodSprite1, 0f);
            if (_bloodSprite2 != 0 && API.HasSprite(_bloodSprite2)) API.SetSpriteAlpha(_bloodSprite2, 0f);
            if (_bloodSprite3 != 0 && API.HasSprite(_bloodSprite3)) API.SetSpriteAlpha(_bloodSprite3, 0f);
            if (_bloodSprite4 != 0 && API.HasSprite(_bloodSprite4)) API.SetSpriteAlpha(_bloodSprite4, 0f);

            API.Log("[BloodOverlay] Initialized blood overlay system with dramatic heartbeat pulsing");
        }

        public void OnUpdate(float dt)
        {
            // Get current health ratio from HUD
            float healthRatio = HUD.HealthRatio;

            // Calculate intensity multiplier based on HP (lower HP = more intense)
            // Inverted: 100% HP = 0.0 intensity, 0% HP = 1.0 intensity
            float intensity = 1.0f - healthRatio;

            // Scale heartbeat speed based on intensity (faster when lower HP)
            float currentHeartbeatSpeed = Lerp(_baseHeartbeatSpeed, _maxHeartbeatSpeed, intensity);

            // Update heartbeat timer with scaled speed
            _heartbeatTimer += dt * currentHeartbeatSpeed;

            // Calculate pulsing alpha with realistic double-beat pattern (lub-dub)
            // This creates the characteristic heartbeat rhythm
            float pulseAlpha = CalculateHeartbeatAlpha(_heartbeatTimer, intensity);

            // Calculate how many sprites should be visible
            int activeSprites = GetActiveSpriteCount(healthRatio);

            // Update each blood sprite based on HP thresholds
            UpdateBloodSprite(_bloodSprite1, activeSprites >= 1, pulseAlpha, intensity, 1);
            UpdateBloodSprite(_bloodSprite2, activeSprites >= 2, pulseAlpha, intensity, 2);
            UpdateBloodSprite(_bloodSprite3, activeSprites >= 3, pulseAlpha, intensity, 3);
            UpdateBloodSprite(_bloodSprite4, activeSprites >= 4, pulseAlpha, intensity, 4);
        }

        /// <summary>
        /// Calculate realistic heartbeat alpha with double-beat pattern (lub-dub)
        /// NOW WITH WIDER ALPHA RANGE FOR MORE DRAMATIC PULSING
        /// </summary>
        private float CalculateHeartbeatAlpha(float time, float intensity)
        {
            // Normalize time to 0-1 cycle
            float cycle = (time % 1.0f);

            float pulse;

            if (cycle < 0.15f)
            {
                // First beat (lub) - sharp rise
                pulse = cycle / 0.15f;
            }
            else if (cycle < 0.25f)
            {
                // First beat fall
                pulse = 1.0f - ((cycle - 0.15f) / 0.1f);
            }
            else if (cycle < 0.35f)
            {
                // Brief pause
                pulse = 0.0f;
            }
            else if (cycle < 0.45f)
            {
                // Second beat (dub) - slightly smaller
                pulse = ((cycle - 0.35f) / 0.1f) * 0.7f;
            }
            else if (cycle < 0.55f)
            {
                // Second beat fall
                pulse = 0.7f - ((cycle - 0.45f) / 0.1f) * 0.7f;
            }
            else
            {
                // Long pause until next cycle
                pulse = 0.0f;
            }

            // WIDER alpha range based on intensity for more dramatic effect
            // At low intensity (high HP): gentle pulse (0.2 -> 0.5)
            // At high intensity (low HP): dramatic pulse (0.05 -> 1.0)
            float minAlpha = Lerp(_baseHeartbeatMin, 0.05f, intensity);  // Goes from 0.2 down to 0.05 (nearly invisible)
            float maxAlpha = Lerp(_baseHeartbeatMax, 1.0f, intensity);   // Goes from 0.95 up to 1.0 (fully opaque)

            return minAlpha + (maxAlpha - minAlpha) * pulse;
        }

        /// <summary>
        /// Determine how many blood sprites should be active based on HP
        /// </summary>
        private int GetActiveSpriteCount(float healthRatio)
        {
            if (healthRatio <= _threshold4) return 4; // Critical: all 4 sprites
            if (healthRatio <= _threshold3) return 3; // Heavy damage: 3 sprites
            if (healthRatio <= _threshold2) return 2; // Moderate damage: 2 sprites
            if (healthRatio <= _threshold1) return 1; // Light damage: 1 sprite
            return 0; // Full health: no sprites
        }

        /// <summary>
        /// Update individual blood sprite with layer-specific intensity
        /// ENHANCED: Each layer pulses more dramatically at lower HP
        /// </summary>
        private void UpdateBloodSprite(ulong spriteEntity, bool shouldShow, float baseAlpha, float intensity, int layer)
        {
            if (spriteEntity == 0 || !API.HasSprite(spriteEntity))
                return;

            if (shouldShow)
            {
                // Each layer gets progressively more intense
                // Layer 1 (first blood): baseline intensity
                // Layer 2-4: increasingly more dramatic pulsing
                float layerIntensity = intensity * (0.5f + (layer * 0.15f));
                layerIntensity = Math.Min(layerIntensity, 1.0f);

                // Apply the pulsing alpha directly (less smoothing for more dramatic effect)
                float targetAlpha = baseAlpha;

                // Add layer-specific boost at critical HP
                if (intensity > 0.7f) // When HP < 30%
                {
                    // Boost opacity for layered effect, but keep the pulse dramatic
                    targetAlpha = Math.Max(targetAlpha, 0.1f + (layer * 0.15f * intensity));
                }

                // Use faster interpolation for more responsive pulsing
                float currentAlpha = API.GetSpriteAlpha(spriteEntity);
                float newAlpha = Lerp(currentAlpha, targetAlpha, 0.35f); // Increased from 0.2 for faster response
                API.SetSpriteAlpha(spriteEntity, newAlpha);
            }
            else
            {
                // Immediately set alpha to 0 when not active (no fade out)
                API.SetSpriteAlpha(spriteEntity, 0f);
            }
        }

        private float Lerp(float a, float b, float t)
        {
            t = Math.Min(Math.Max(t, 0f), 1f); // Clamp t to [0, 1]
            return a + (b - a) * t;
        }

        public void OnDestroy()
        {
            // Clean up - hide all sprites
            if (_bloodSprite1 != 0 && API.HasSprite(_bloodSprite1)) API.SetSpriteAlpha(_bloodSprite1, 0f);
            if (_bloodSprite2 != 0 && API.HasSprite(_bloodSprite2)) API.SetSpriteAlpha(_bloodSprite2, 0f);
            if (_bloodSprite3 != 0 && API.HasSprite(_bloodSprite3)) API.SetSpriteAlpha(_bloodSprite3, 0f);
            if (_bloodSprite4 != 0 && API.HasSprite(_bloodSprite4)) API.SetSpriteAlpha(_bloodSprite4, 0f);
        }
    }
}