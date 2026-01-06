using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Controls 4 blood sprite overlays based on player HP.
    /// Sprites appear sequentially as HP decreases and have an intensifying heartbeat pulsing effect.
    /// The heartbeat becomes faster and more opaque as HP decreases.
    /// Uses sprite color intensity (brightness) instead of alpha blending.
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

        // Base color for blood (red)
        private Vec4 _baseBloodColor = new Vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red (R, G, B, W)

        // Heartbeat effect parameters (will be scaled based on HP)
        private float _baseHeartbeatSpeed = 2.0f;      // Base speed of heartbeat pulse
        private float _maxHeartbeatSpeed = 6.0f;       // Max speed when critically low HP

        // WIDER intensity range for more dramatic pulsing effect
        // At low HP: pulses from very dim to bright
        // At high HP: pulses from barely visible to moderately visible
        private float _baseHeartbeatMin = 0.2f;        // Minimum intensity during pulse
        private float _baseHeartbeatMax = 0.95f;       // Maximum intensity during pulse

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

            // Initialize all sprites to invisible (set intensity to 0)
            Vec4 invisibleColor = new Vec4(1.0f, 0.0f, 0.0f, 0.0f); // Red with 0 intensity
            if (_bloodSprite1 != 0 && API.HasSprite(_bloodSprite1)) API.SetSpriteColor(_bloodSprite1, invisibleColor);
            if (_bloodSprite2 != 0 && API.HasSprite(_bloodSprite2)) API.SetSpriteColor(_bloodSprite2, invisibleColor);
            if (_bloodSprite3 != 0 && API.HasSprite(_bloodSprite3)) API.SetSpriteColor(_bloodSprite3, invisibleColor);
            if (_bloodSprite4 != 0 && API.HasSprite(_bloodSprite4)) API.SetSpriteColor(_bloodSprite4, invisibleColor);

            API.Log("[BloodOverlay] Initialized blood overlay system with dramatic heartbeat pulsing (intensity-based)");
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

            // Calculate pulsing intensity with realistic double-beat pattern (lub-dub)
            // This creates the characteristic heartbeat rhythm
            float pulseIntensity = CalculateHeartbeatIntensity(_heartbeatTimer, intensity);

            // Calculate how many sprites should be visible
            int activeSprites = GetActiveSpriteCount(healthRatio);

            // Update each blood sprite based on HP thresholds
            UpdateBloodSprite(_bloodSprite1, activeSprites >= 1, pulseIntensity, intensity, 1);
            UpdateBloodSprite(_bloodSprite2, activeSprites >= 2, pulseIntensity, intensity, 2);
            UpdateBloodSprite(_bloodSprite3, activeSprites >= 3, pulseIntensity, intensity, 3);
            UpdateBloodSprite(_bloodSprite4, activeSprites >= 4, pulseIntensity, intensity, 4);
        }

        /// <summary>
        /// Calculate realistic heartbeat intensity with double-beat pattern (lub-dub)
        /// Uses brightness/intensity value (W component) instead of alpha
        /// </summary>
        private float CalculateHeartbeatIntensity(float time, float intensity)
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

            // WIDER intensity range based on health for more dramatic effect
            // At low intensity (high HP): gentle pulse (0.2 -> 0.5)
            // At high intensity (low HP): dramatic pulse (0.05 -> 1.0)
            float minIntensity = Lerp(_baseHeartbeatMin, 0.05f, intensity);  // Goes from 0.2 down to 0.05
            float maxIntensity = Lerp(_baseHeartbeatMax, 1.0f, intensity);   // Goes from 0.95 up to 1.0

            return minIntensity + (maxIntensity - minIntensity) * pulse;
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
        /// Uses sprite color intensity (W component) instead of alpha
        /// </summary>
        private void UpdateBloodSprite(ulong spriteEntity, bool shouldShow, float baseIntensity, float intensity, int layer)
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

                // Apply the pulsing intensity directly (less smoothing for more dramatic effect)
                float targetIntensity = baseIntensity;

                // Add layer-specific boost at critical HP
                if (intensity > 0.7f) // When HP < 30%
                {
                    // Boost intensity for layered effect, but keep the pulse dramatic
                    targetIntensity = Math.Max(targetIntensity, 0.1f + (layer * 0.15f * intensity));
                }

                // Use faster interpolation for more responsive pulsing
                Vec4 currentColor = API.GetSpriteColor(spriteEntity);
                float currentIntensity = currentColor.W;
                float newIntensity = Lerp(currentIntensity, targetIntensity, 0.35f); // Increased from 0.2 for faster response

                // Create new color with updated intensity
                Vec4 newColor = new Vec4(_baseBloodColor.X, _baseBloodColor.Y, _baseBloodColor.Z, newIntensity);
                API.SetSpriteColor(spriteEntity, newColor);
            }
            else
            {
                // Set color with 0 intensity when not active
                Vec4 invisibleColor = new Vec4(_baseBloodColor.X, _baseBloodColor.Y, _baseBloodColor.Z, 0.0f);
                API.SetSpriteColor(spriteEntity, invisibleColor);
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
            Vec4 invisibleColor = new Vec4(1.0f, 0.0f, 0.0f, 0.0f);
            if (_bloodSprite1 != 0 && API.HasSprite(_bloodSprite1)) API.SetSpriteColor(_bloodSprite1, invisibleColor);
            if (_bloodSprite2 != 0 && API.HasSprite(_bloodSprite2)) API.SetSpriteColor(_bloodSprite2, invisibleColor);
            if (_bloodSprite3 != 0 && API.HasSprite(_bloodSprite3)) API.SetSpriteColor(_bloodSprite3, invisibleColor);
            if (_bloodSprite4 != 0 && API.HasSprite(_bloodSprite4)) API.SetSpriteColor(_bloodSprite4, invisibleColor);
        }
    }
}