using System;
using Boom;

namespace GameScripts
{

    public static class Entry
    {
        public const string LEVEL_SCENE_NAME = "M3 GAMEPLAY";
        public const string PAUSE_SCENE_NAME = "PauseMenu";
        public const string MAIN_MENU_SCENE_NAME = "MainMenu";
        public const string HOW_TO_PLAY_SCENE_NAME = "HowToPlay";
        public static string _currentSceneName;
        public static bool IsGamePaused = false;

        // GLFW key constants
        public const int KEY_ESCAPE = 256;

        public enum PauseMenuAction
        {
            None,
            Resume,
            Restart,
            MainMenu,
            Quit
        }

        public static PauseMenuAction s_RequestedAction = PauseMenuAction.None;

        private static bool _p_KeyWasDown = false;
        private static bool _escape_KeyWasDown = false;
        public static PauseMenu s_ActivePauseMenuInstance = null;


        public static void Start()
        {
            _p_KeyWasDown = false;
            _escape_KeyWasDown = false;
            IsGamePaused = false;
            s_RequestedAction = PauseMenuAction.None;

            _currentSceneName = API.GetCurrentSceneName();
            API.EnableFileWatcher(true);
            API.SetGameLogicPaused(false);
            s_ActivePauseMenuInstance = null;

            API.Log("[C#] Entry.Start() called for scene: " + _currentSceneName);

            if (_currentSceneName == LEVEL_SCENE_NAME)
            {
                API.Log("Pre-loading pause menu additively...");
                API.LoadSceneAdditive(PAUSE_SCENE_NAME);
            }
        }

        public static void Update(float dt)
        {
            // CRITICAL FIX: Always update game logic pause state FIRST (before any early returns)
            API.SetGameLogicPaused(IsGamePaused);

            if (s_RequestedAction == PauseMenuAction.MainMenu ||
                s_RequestedAction == PauseMenuAction.Restart ||
                s_RequestedAction == PauseMenuAction.Quit)
            {
                UpdatePauseMenu(dt);
                return;
            }

            if (IsGamePaused)
            {
                if (API.IsPauseMenuLoaded())
                {
                    UpdatePauseMenu(dt);
                }
            }
            else
            {
                UpdateGame(dt);
            }
        }

        private static void UpdateGame(float dt)
        {
            bool p_KeyDown = API.IsKeyDown(API.KEY_P);
            bool escape_KeyDown = API.IsKeyDown(KEY_ESCAPE);
            bool ctrl_KeyDown = API.IsKeyDown(API.KEY_LEFT_CONTROL);

            // Handle Escape key to pause
            if (escape_KeyDown && !_escape_KeyWasDown)
            {
                API.Log("Pausing game (Escape key)...");
                IsGamePaused = true;
                API.ShowPauseMenu();
                API.EnableFileWatcher(false);

                _escape_KeyWasDown = escape_KeyDown;
                return;
            }
            _escape_KeyWasDown = escape_KeyDown;

            // Handle P key to pause (legacy support)
            if (p_KeyDown && !_p_KeyWasDown && !ctrl_KeyDown)
            {
                API.Log("Pausing game (P key)...");
                IsGamePaused = true;
                API.ShowPauseMenu();
                API.EnableFileWatcher(false);

                _p_KeyWasDown = p_KeyDown;
                return;
            }
            _p_KeyWasDown = p_KeyDown;
        }


        private static void UpdatePauseMenu(float dt)
        {
            bool escape_KeyDown = API.IsKeyDown(KEY_ESCAPE);

            // Handle Escape key to resume
            if (escape_KeyDown && !_escape_KeyWasDown)
            {
                API.Log("Resuming game (Escape key)...");
                s_RequestedAction = PauseMenuAction.Resume;
                _escape_KeyWasDown = escape_KeyDown;
                return;
            }
            _escape_KeyWasDown = escape_KeyDown;

            if (s_ActivePauseMenuInstance != null)
            {
                s_ActivePauseMenuInstance.OnUpdate(dt);
            }


            switch (s_RequestedAction)
            {
                case PauseMenuAction.Resume:
                    s_RequestedAction = PauseMenuAction.None;
                    ResumeGame();
                    return;

                case PauseMenuAction.MainMenu:
                    API.Log("Returning to Main Menu (Button Click)...");
                    IsGamePaused = false;
                    API.EnableFileWatcher(true);
                    s_RequestedAction = PauseMenuAction.None;
                    API.LoadScene(MAIN_MENU_SCENE_NAME);
                    return;

                case PauseMenuAction.Restart:
                    API.Log("Restarting scene (Button Click)...");
                    IsGamePaused = false;
                    API.EnableFileWatcher(true);
                    s_RequestedAction = PauseMenuAction.None;
                    API.LoadScene(_currentSceneName);
                    return;

                case PauseMenuAction.Quit:
                    API.Log("Quitting game (Button Click)...");
                    s_RequestedAction = PauseMenuAction.None;
                    API.ShutdownApplication();
                    return;
            }
        }

        public static void ResumeGame()
        {
            API.Log("Resuming game (Button click)...");

            if (s_ActivePauseMenuInstance != null)
            {
                s_ActivePauseMenuInstance.ResetButtonState();
            }

            API.UnloadPauseMenu();
            IsGamePaused = false;
            API.EnableFileWatcher(true);
        }
    }
}