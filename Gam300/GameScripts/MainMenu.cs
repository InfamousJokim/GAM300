using System;
using Boom;

namespace GameScripts
{
    public class MainMenu
    {
        private const int MOUSE_LEFT = 0;

        private const string NEWGAME_TEX_CLICKED = "Resources/Textures/MainMenu/NewGameButton_Clicked.png";
        private const string HOWTOPLAY_TEX_CLICKED = "Resources/Textures/MainMenu/HowToPlayButton_Clicked.png";
        private const string QUIT_TEX_CLICKED = "Resources/Textures/MainMenu/ExitButton_Clicked.png";

        private ulong _newGameButtonID;
        private ulong _howToPlayButtonID;
        private ulong _quitButtonID;

        private enum MenuState
        {
            Idle,
            ButtonDelay,
            FadingOut
        }

        private MenuState _currentState = MenuState.Idle;
        private ulong _clickedButtonID = 0;

        // Fade transition state
        private float _fadeTimer = 0f;
        private float _fadeDuration = 1.0f;
        private string _sceneToLoad = "";

        public void OnStart(string jsonParams)
        {
            API.Log("MainMenu OnStart Running...");
            _newGameButtonID = API.FindEntity("NewGameButton");
            _howToPlayButtonID = API.FindEntity("HowToPlayButton");
            _quitButtonID = API.FindEntity("QuitButton");

            _currentState = MenuState.Idle;
            _clickedButtonID = 0;

            // Fade in from black when menu loads
            API.SetScreenFadeAlpha(1f);
            StartFadeIn();
        }

        public void OnUpdate(float dt)
        {
            switch (_currentState)
            {
                case MenuState.Idle:
                    Update_Idle();
                    UpdateFadeIn(dt);
                    break;

                case MenuState.ButtonDelay:
                    Update_ButtonDelay(dt);
                    break;

                case MenuState.FadingOut:
                    UpdateFadeOut(dt);
                    break;
            }
        }

        private void Update_Idle()
        {
            if (API.IsMouseDown(MOUSE_LEFT))
            {
                if (!API.GetMousePosInViewport(out Vec2 mousePos))
                {
                    return;
                }

                if (API.Check2DViewportClick(_newGameButtonID, mousePos.X, mousePos.Y))
                    StartClickDelay(_newGameButtonID);
                else if (API.Check2DViewportClick(_howToPlayButtonID, mousePos.X, mousePos.Y))
                    StartClickDelay(_howToPlayButtonID);
                else if (API.Check2DViewportClick(_quitButtonID, mousePos.X, mousePos.Y))
                    StartClickDelay(_quitButtonID);
            }
        }

        private void Update_ButtonDelay(float dt)
        {
            ExecuteClickAction();
        }

        private void StartClickDelay(ulong buttonID)
        {
            _currentState = MenuState.ButtonDelay;
            _clickedButtonID = buttonID;

            // Set the texture
            if (buttonID == _newGameButtonID)
                API.SetSpriteTexture(buttonID, NEWGAME_TEX_CLICKED);
            else if (buttonID == _howToPlayButtonID)
                API.SetSpriteTexture(buttonID, HOWTOPLAY_TEX_CLICKED);
            else if (buttonID == _quitButtonID)
                API.SetSpriteTexture(buttonID, QUIT_TEX_CLICKED);
        }

        private void ExecuteClickAction()
        {
            if (_clickedButtonID == _newGameButtonID)
            {
                API.Log(">> New Game Button Clicked! Fading to game scene...");
                _sceneToLoad = Entry.LEVEL_SCENE_NAME;
                _currentState = MenuState.FadingOut;
                _fadeTimer = 0f;
            }
            else if (_clickedButtonID == _howToPlayButtonID)
            {
                API.Log(">> How To Play Button Clicked! Loading HowToPlay...");
                _currentState = MenuState.Idle;
                API.LoadScene("HowToPlay");
            }
            else if (_clickedButtonID == _quitButtonID)
            {
                API.Log(">> Quit Button Clicked! Shutting down...");
                API.ShutdownApplication();
            }
            else
            {
                _currentState = MenuState.Idle;
            }

            _clickedButtonID = 0;
        }

        // Fade in from black (called when menu loads)
        private bool _isFadingIn = false;
        private void StartFadeIn()
        {
            _isFadingIn = true;
            _fadeTimer = 0f;
        }

        private void UpdateFadeIn(float dt)
        {
            if (!_isFadingIn) return;

            _fadeTimer += dt;
            float alpha = 1f - Clamp01(_fadeTimer / _fadeDuration);
            API.SetScreenFadeAlpha(alpha);

            if (_fadeTimer >= _fadeDuration)
            {
                API.SetScreenFadeAlpha(0f);
                _isFadingIn = false;
            }
        }

        // Fade out to black before loading scene
        private void UpdateFadeOut(float dt)
        {
            _fadeTimer += dt;
            float alpha = Clamp01(_fadeTimer / _fadeDuration);
            API.SetScreenFadeAlpha(alpha);

            if (_fadeTimer >= _fadeDuration)
            {
                API.SetScreenFadeAlpha(1f);
                API.Log($"[MainMenu] Loading scene: {_sceneToLoad}");
                API.LoadScene(_sceneToLoad);
            }
        }

        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);
    }
}