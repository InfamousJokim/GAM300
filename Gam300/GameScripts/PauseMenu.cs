using System;
using Boom;

namespace GameScripts
{
    public class PauseMenu
    {
        private const int MOUSE_LEFT = 0;

        // --- Texture Constants ---
        private const string RESUME_TEX_NORMAL = "Resources/Textures/PauseMenu/ResumeButton.png";
        private const string RESTART_TEX_NORMAL = "Resources/Textures/PauseMenu/RestartButton.png";
        private const string MAINMENU_TEX_NORMAL = "Resources/Textures/PauseMenu/ReturnMenuButton.png";
        private const string QUIT_TEX_NORMAL = "Resources/Textures/PauseMenu/QuitButton.png";

        private const string RESUME_TEX_CLICKED = "Resources/Textures/PauseMenu/ResumeButton_Clicked.png";
        private const string RESTART_TEX_CLICKED = "Resources/Textures/PauseMenu/RestartButton_Clicked.png";
        private const string MAINMENU_TEX_CLICKED = "Resources/Textures/PauseMenu/ReturnMenuButton_Clicked.png";
        private const string QUIT_TEX_CLICKED = "Resources/Textures/PauseMenu/QuitButton_Clicked.png";

        private ulong _resumeButtonID;
        private ulong _restartButtonID;
        private ulong _mainMenuButtonID;
        private ulong _quitButtonID;

        private enum MenuState
        {
            Idle,
            ButtonDelay,
            WaitingForMouseUp
        }

        private MenuState _currentState = MenuState.Idle;
        private ulong _clickedButtonID = 0;

        private bool _wasPausedLastFrame = false;

        public void OnStart(string jsonParams)
        {
            API.Log("PauseMenu OnStart Running...");
            Entry.s_ActivePauseMenuInstance = this;

            _resumeButtonID = API.FindEntity("ResumeButton");
            _restartButtonID = API.FindEntity("RestartButton");
            _mainMenuButtonID = API.FindEntity("ReturnButton");
            _quitButtonID = API.FindEntity("QuitButton");

            ResetButtonState();
        }

        public void OnUpdate(float dt)
        {
            if (Entry.s_ActivePauseMenuInstance != this)
            {
                Entry.s_ActivePauseMenuInstance = this;
            }

            if (Entry.IsGamePaused && !_wasPausedLastFrame)
            {
                ResetButtonState();
            }
            _wasPausedLastFrame = Entry.IsGamePaused;

            if (!Entry.IsGamePaused) return;
            if (Entry.s_RequestedAction != Entry.PauseMenuAction.None) return;

            switch (_currentState)
            {
                case MenuState.WaitingForMouseUp:
                    if (!API.IsMouseDown(MOUSE_LEFT))
                    {
                        _currentState = MenuState.Idle;
                    }
                    break;

                case MenuState.Idle:
                    Update_Idle();
                    break;

                case MenuState.ButtonDelay:
                    Update_ButtonDelay(dt);
                    break;
            }
        }

        public void ResetButtonState()
        {
            _currentState = MenuState.WaitingForMouseUp;
            _clickedButtonID = 0;

            if (_resumeButtonID != 0)
                API.SetSpriteTexture(_resumeButtonID, RESUME_TEX_NORMAL);
            if (_restartButtonID != 0)
                API.SetSpriteTexture(_restartButtonID, RESTART_TEX_NORMAL);
            if (_mainMenuButtonID != 0)
                API.SetSpriteTexture(_mainMenuButtonID, MAINMENU_TEX_NORMAL);
            if (_quitButtonID != 0)
                API.SetSpriteTexture(_quitButtonID, QUIT_TEX_NORMAL);
        }

        private void Update_Idle()
        {
            if (API.IsMouseDown(MOUSE_LEFT))
            {
                if (!API.GetMousePosInViewport(out Vec2 mousePos)) { return; }

                if (API.Check2DViewportClick(_resumeButtonID, mousePos.X, mousePos.Y))
                    StartClickDelay(_resumeButtonID);
                else if (API.Check2DViewportClick(_mainMenuButtonID, mousePos.X, mousePos.Y))
                    StartClickDelay(_mainMenuButtonID);
                else if (API.Check2DViewportClick(_restartButtonID, mousePos.X, mousePos.Y))
                    StartClickDelay(_restartButtonID);
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

            if (buttonID == _resumeButtonID)
                API.SetSpriteTexture(buttonID, RESUME_TEX_CLICKED);
            else if (buttonID == _mainMenuButtonID)
                API.SetSpriteTexture(buttonID, MAINMENU_TEX_CLICKED);
            else if (buttonID == _restartButtonID)
                API.SetSpriteTexture(buttonID, RESTART_TEX_CLICKED);
            else if (buttonID == _quitButtonID)
                API.SetSpriteTexture(buttonID, QUIT_TEX_CLICKED);
        }

        private void ExecuteClickAction()
        {
            _currentState = MenuState.Idle;

            if (_clickedButtonID == _resumeButtonID)
            {
                Entry.s_RequestedAction = Entry.PauseMenuAction.Resume;
            }
            else if (_clickedButtonID == _mainMenuButtonID)
            {
                Entry.s_RequestedAction = Entry.PauseMenuAction.MainMenu;
            }
            else if (_clickedButtonID == _restartButtonID)
            {
                Entry.s_RequestedAction = Entry.PauseMenuAction.Restart;
            }
            else if (_clickedButtonID == _quitButtonID)
            {
                Entry.s_RequestedAction = Entry.PauseMenuAction.Quit;
            }
        }
    }
}