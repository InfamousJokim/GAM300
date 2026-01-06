using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Controls the "Hold to Crouch" UI prompt
    /// Shows when player is in a CrouchTriggerZone
    /// Fades in when entering zone, fades out when leaving
    /// </summary>
    public class UIHoldController
    {
        public ulong Entity;

        private string _holdSpriteName = "UI_Hold";
        private ulong _holdSprite = 0;

        private float _fadeSpeed = 4.0f;        // Speed of fade in/out
        private float _currentAlpha = 0.0f;
        private bool _shouldShow = false;

        // Optional: Pulsing effect while visible
        private bool _enablePulse = true;
        private float _pulseSpeed = 2.0f;
        private float _pulseMin = 0.7f;
        private float _pulseMax = 1.0f;
        private float _pulseTimer = 0.0f;

        public void OnStart(string jsonParams)
        {
            _holdSprite = API.FindEntity(_holdSpriteName);

            if (_holdSprite != 0 && API.HasSprite(_holdSprite))
            {
                API.SetSpriteAlpha(_holdSprite, 0f);
                API.Log($"[UIHold] Initialized hold UI sprite");
            }
            else
            {
                //API.LogWarning($"[UIHold] Failed to find sprite: {_holdSpriteName}");
            }
        }

        public void OnUpdate(float dt)
        {
            if (_holdSprite == 0 || !API.HasSprite(_holdSprite)) return;

            // Target alpha based on whether we should show
            float targetAlpha = _shouldShow ? 1.0f : 0.0f;

            // Apply pulsing effect if visible and enabled
            if (_shouldShow && _enablePulse && _currentAlpha > 0.5f)
            {
                _pulseTimer += dt * _pulseSpeed;
                float pulse = _pulseMin + (_pulseMax - _pulseMin) *
                              (0.5f + 0.5f * (float)Math.Sin(_pulseTimer));
                targetAlpha *= pulse;
            }

            // Smooth transition to target alpha
            _currentAlpha = Lerp(_currentAlpha, targetAlpha, _fadeSpeed * dt);
            API.SetSpriteAlpha(_holdSprite, _currentAlpha);
        }

        /// <summary>
        /// Call this when entering a crouch zone
        /// </summary>
        public void Show()
        {
            _shouldShow = true;
            API.Log("[UIHold] Showing hold prompt");
        }

        /// <summary>
        /// Call this when leaving a crouch zone
        /// </summary>
        public void Hide()
        {
            _shouldShow = false;
            API.Log("[UIHold] Hiding hold prompt");
        }

        private float Lerp(float a, float b, float t)
        {
            t = Math.Max(0f, Math.Min(1f, t));
            return a + (b - a) * t;
        }
    }
}