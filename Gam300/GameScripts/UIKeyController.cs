using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Controls the key pickup UI sprite with smooth fade in/out effects
    /// Shows when player picks up a key and remains visible until the key is used
    /// </summary>
    public class UIKeyController
    {
        public ulong Entity;

        private string _keySpriteName = "UI_Key";
        private ulong _keySprite = 0;

        // Animation state
        //private bool _isShowing = false;
        private float _fadeInSpeed = 3.0f;      // Speed of fade in
        private float _fadeOutSpeed = 2.0f;     // Speed of fade out
        private float _currentAlpha = 0.0f;
        //private float _targetAlpha = 0.0f;

        // Track key count
        private int _lastKeyCount = 0;

        // State machine
        private enum State { Hidden, FadingIn, Displaying, FadingOut }
        private State _currentState = State.Hidden;

        public void OnStart(string jsonParams)
        {
            _keySprite = API.FindEntity(_keySpriteName);

            if (_keySprite != 0 && API.HasSprite(_keySprite))
            {
                API.SetSpriteAlpha(_keySprite, 0f);
                API.Log($"[UIKey] Initialized key UI sprite");
            }
            else
            {
                //API.LogWarning($"[UIKey] Failed to find sprite: {_keySpriteName}");
            }

            _lastKeyCount = PlayerInventory.GetKeyCount();
        }

        public void OnUpdate(float dt)
        {
            if (_keySprite == 0 || !API.HasSprite(_keySprite)) return;

            // Check if key count has changed
            int currentKeyCount = PlayerInventory.GetKeyCount();

            // If we picked up a key, fade it in
            if (currentKeyCount > _lastKeyCount)
            {
                if (_currentState == State.Hidden || _currentState == State.FadingOut)
                {
                    _currentState = State.FadingIn;
                    _currentAlpha = 0.0f;
                    API.Log("[UIKey] Key picked up - showing UI");
                }
                _lastKeyCount = currentKeyCount;
            }
            // If a key was used, fade it out
            else if (currentKeyCount < _lastKeyCount)
            {
                _currentState = State.FadingOut;
                API.Log("[UIKey] Key used - hiding UI");
                _lastKeyCount = currentKeyCount;
            }

            switch (_currentState)
            {
                case State.Hidden:
                    // Do nothing, waiting for ShowKey() call
                    break;

                case State.FadingIn:
                    _currentAlpha = Lerp(_currentAlpha, 1.0f, _fadeInSpeed * dt);
                    API.SetSpriteAlpha(_keySprite, _currentAlpha);

                    if (_currentAlpha >= 0.98f)
                    {
                        _currentAlpha = 1.0f;
                        _currentState = State.Displaying;
                    }
                    break;

                case State.Displaying:
                    // Stay visible while player has keys
                    if (currentKeyCount > 0)
                    {
                        API.SetSpriteAlpha(_keySprite, 1.0f);
                    }
                    break;

                case State.FadingOut:
                    _currentAlpha = Lerp(_currentAlpha, 0.0f, _fadeOutSpeed * dt);
                    API.SetSpriteAlpha(_keySprite, _currentAlpha);

                    if (_currentAlpha <= 0.02f)
                    {
                        _currentAlpha = 0.0f;
                        API.SetSpriteAlpha(_keySprite, 0f);
                        _currentState = State.Hidden;
                    }
                    break;
            }
        }

        /// <summary>
        /// Call this method to trigger the key UI display
        /// </summary>
        public void ShowKey()
        {
            if (_currentState == State.Hidden || _currentState == State.FadingOut)
            {
                _currentState = State.FadingIn;
                _currentAlpha = 0.0f;
                API.Log("[UIKey] Showing key pickup UI");
            }
        }

        private float Lerp(float a, float b, float t)
        {
            t = Math.Max(0f, Math.Min(1f, t));
            return a + (b - a) * t;
        }
    }
}