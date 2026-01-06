using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Controls location name UI overlays (e.g., "Garden", "Beginning")
    /// Fades in briefly to show location, then fades out
    /// </summary>
    public class UILocationController
    {
        public ulong Entity;

        // Multiple location sprites
        private string _gardenSpriteName = "UI_Garden";
        private string _beginningSpriteName = "UI_Beginning";

        private ulong _gardenSprite = 0;
        private ulong _beginningSprite = 0;

        // Currently active location sprite
        private ulong _activeSprite = 0;

        // Animation parameters
        private float _fadeInSpeed = 2.0f;
        private float _fadeOutSpeed = 1.5f;
        private float _displayDuration = 3.0f;  // How long to show location name
        private float _currentAlpha = 0.0f;
        private float _displayTimer = 0.0f;

        private enum State { Hidden, FadingIn, Displaying, FadingOut }
        private State _currentState = State.Hidden;

        public void OnStart(string jsonParams)
        {
            _gardenSprite = API.FindEntity(_gardenSpriteName);
            _beginningSprite = API.FindEntity(_beginningSpriteName);

            // Initialize all location sprites to invisible
            if (_gardenSprite != 0 && API.HasSprite(_gardenSprite))
            {
                API.SetSpriteAlpha(_gardenSprite, 0f);
                API.Log($"[UILocation] Initialized garden location sprite");
            }

            if (_beginningSprite != 0 && API.HasSprite(_beginningSprite))
            {
                API.SetSpriteAlpha(_beginningSprite, 0f);
                API.Log($"[UILocation] Initialized beginning location sprite");
            }
        }

        public void OnUpdate(float dt)
        {
            if (_activeSprite == 0 || !API.HasSprite(_activeSprite)) return;

            switch (_currentState)
            {
                case State.Hidden:
                    // Waiting for location trigger
                    break;

                case State.FadingIn:
                    _currentAlpha = Lerp(_currentAlpha, 1.0f, _fadeInSpeed * dt);
                    API.SetSpriteAlpha(_activeSprite, _currentAlpha);

                    if (_currentAlpha >= 0.98f)
                    {
                        _currentAlpha = 1.0f;
                        _currentState = State.Displaying;
                        _displayTimer = 0.0f;
                    }
                    break;

                case State.Displaying:
                    _displayTimer += dt;
                    if (_displayTimer >= _displayDuration)
                    {
                        _currentState = State.FadingOut;
                    }
                    break;

                case State.FadingOut:
                    _currentAlpha = Lerp(_currentAlpha, 0.0f, _fadeOutSpeed * dt);
                    API.SetSpriteAlpha(_activeSprite, _currentAlpha);

                    if (_currentAlpha <= 0.02f)
                    {
                        _currentAlpha = 0.0f;
                        API.SetSpriteAlpha(_activeSprite, 0f);
                        _currentState = State.Hidden;
                        _activeSprite = 0;
                    }
                    break;
            }
        }

        /// <summary>
        /// Show the Garden location indicator
        /// </summary>
        public void ShowGarden()
        {
            ShowLocation(_gardenSprite, "Garden");
        }

        /// <summary>
        /// Show the Beginning location indicator
        /// </summary>
        public void ShowBeginning()
        {
            ShowLocation(_beginningSprite, "Beginning");
        }

        /// <summary>
        /// Generic method to show any location sprite
        /// </summary>
        private void ShowLocation(ulong sprite, string locationName)
        {
            if (sprite == 0 || !API.HasSprite(sprite))
            {
                //API.LogWarning($"[UILocation] Cannot show location '{locationName}' - sprite not found");
                return;
            }

            // If already showing something, hide it first
            if (_activeSprite != 0 && _activeSprite != sprite)
            {
                API.SetSpriteAlpha(_activeSprite, 0f);
            }

            _activeSprite = sprite;
            _currentState = State.FadingIn;
            _currentAlpha = 0.0f;
            API.Log($"[UILocation] Showing location: {locationName}");
        }

        private float Lerp(float a, float b, float t)
        {
            t = Math.Max(0f, Math.Min(1f, t));
            return a + (b - a) * t;
        }
    }
}