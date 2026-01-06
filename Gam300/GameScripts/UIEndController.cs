using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Controls the end game UI overlay
    /// Fades in when player reaches the end trigger
    /// Can optionally freeze at full opacity or continue to black
    /// </summary>
    public class UIEndController
    {
        public ulong Entity;

        private string _endSpriteName = "UI_End";
        private ulong _endSprite = 0;

       // private bool _isTriggered = false;
        private float _fadeSpeed = 0.5f;        // Slow dramatic fade
        private float _currentAlpha = 0.0f;
        private float _holdAtFullDuration = 3.0f;  // Hold at full opacity before proceeding
        private float _holdTimer = 0.0f;

        private enum State { Hidden, FadingIn, Holding, Complete }
        private State _currentState = State.Hidden;

        public void OnStart(string jsonParams)
        {
            _endSprite = API.FindEntity(_endSpriteName);

            if (_endSprite != 0 && API.HasSprite(_endSprite))
            {
                API.SetSpriteAlpha(_endSprite, 0f);
                API.Log($"[UIEnd] Initialized end game UI sprite");
            }
            else
            {
                //API.LogWarning($"[UIEnd] Failed to find sprite: {_endSpriteName}");
            }
        }

        public void OnUpdate(float dt)
        {
            if (_endSprite == 0 || !API.HasSprite(_endSprite)) return;

            switch (_currentState)
            {
                case State.Hidden:
                    // Waiting for trigger
                    break;

                case State.FadingIn:
                    _currentAlpha = Lerp(_currentAlpha, 1.0f, _fadeSpeed * dt);
                    API.SetSpriteAlpha(_endSprite, _currentAlpha);

                    if (_currentAlpha >= 0.98f)
                    {
                        _currentAlpha = 1.0f;
                        _currentState = State.Holding;
                        _holdTimer = 0.0f;
                        API.Log("[UIEnd] Reached full opacity, holding...");
                    }
                    break;

                case State.Holding:
                    _holdTimer += dt;
                    if (_holdTimer >= _holdAtFullDuration)
                    {
                        _currentState = State.Complete;
                        API.Log("[UIEnd] End sequence complete");
                        // Here you could trigger level transition, credits, etc.
                    }
                    break;

                case State.Complete:
                    // Stay at full opacity
                    // Could trigger scene transition here
                    break;
            }
        }

        /// <summary>
        /// Trigger the end game fade sequence
        /// </summary>
        public void TriggerEnd()
        {
            if (_currentState == State.Hidden)
            {
                _currentState = State.FadingIn;
                //_isTriggered = true;
                API.Log("[UIEnd] End game sequence triggered");
            }
        }

        private float Lerp(float a, float b, float t)
        {
            t = Math.Max(0f, Math.Min(1f, t));
            return a + (b - a) * t;
        }
    }
}