using System;
using Boom;

namespace GameScripts
{
    public static class Entry
    {
        // --- Scene Management ---
        private const string LEVEL_SCENE_NAME = "level";
        private const string PAUSE_SCENE_NAME = "PauseMenu";
        private const string MAIN_MENU_SCENE_NAME = "MainMenu";
        private const string HOW_TO_PLAY_SCENE_NAME = "HowToPlay";
        private static string _currentSceneName;

        // Key state trackers
        private static bool _h_KeyWasDown = false;
        private static bool _p_KeyWasDown = false;
        private static bool _r_KeyWasDown = false;
        private static bool _y_KeyWasDown = false;
        private static bool _m_KeyWasDown = false;
        private static bool _q_KeyWasDown = false;

        public static void Start()
        {
            // Reset keys every time a scene loads
            _h_KeyWasDown = false;
            _p_KeyWasDown = false;
            _r_KeyWasDown = false;
            _y_KeyWasDown = false;
            _m_KeyWasDown = false;
            _q_KeyWasDown = false;

            _currentSceneName = API.GetCurrentSceneName();

            API.Log("[C#] Entry.Start() called for scene: " + _currentSceneName);
        }

        public static void Update(float dt)
        {
            int state = API.GetApplicationState();

            if (state == API.APP_STATE_RUNNING)
            {
                // --- STATE: RUNNING ---
                UpdateGame(dt);
            }
            else if (state == API.APP_STATE_PAUSED)
            {
                // --- STATE: PAUSED ---
                if (API.IsPauseMenuLoaded())
                {
                    UpdatePauseMenu();
                }
            }
        }

        // This function runs all your game logic
        private static void UpdateGame(float dt)
        {
            // Get key states once
            bool p_KeyDown = API.IsKeyDown(API.KEY_P); // Menu Play
            bool h_KeyDown = API.IsKeyDown(API.KEY_H); // Menu How to Play
            bool r_KeyDown = API.IsKeyDown(API.KEY_R); // How to Play return to Menu
            bool q_KeyDown = API.IsKeyDown(API.KEY_Q); // Quit
            bool ctrl_KeyDown = API.IsKeyDown(API.KEY_LEFT_CONTROL);

            // --- LOGIC FOR MAIN MENU ---
            if (_currentSceneName == MAIN_MENU_SCENE_NAME)
            {
                // Check for 'P' (without Ctrl) to START the Level
                if (p_KeyDown && !_p_KeyWasDown && !ctrl_KeyDown)
                {
                    API.Log("Starting game... Loading 'level' scene.");
                    API.LoadScene(LEVEL_SCENE_NAME);
                    _p_KeyWasDown = p_KeyDown;
                    return;
                }

                // Check for 'H' (without Ctrl) to go to HOW TO PLAY
                if (h_KeyDown && !_h_KeyWasDown && !ctrl_KeyDown)
                {
                    API.Log("Loading 'HowToPlay' scene.");
                    API.LoadScene(HOW_TO_PLAY_SCENE_NAME);
                    _h_KeyWasDown = h_KeyDown; // Set tracker
                    return; // Exit after action
                }

                // Check for 'Q' (without Ctrl) to QUIT
                if (q_KeyDown && !_q_KeyWasDown && !ctrl_KeyDown)
                {
                    API.Log("Quitting application from main menu.");
                    API.QuitGame(); // This will close the application
                    _q_KeyWasDown = q_KeyDown; // Set tracker
                    return; // Exit after action
                }

                // Update all menu key trackers if no action was taken
                _p_KeyWasDown = p_KeyDown;
                _h_KeyWasDown = h_KeyDown;
                _r_KeyWasDown = r_KeyDown; // (Doesn't hurt to update this)
                _q_KeyWasDown = q_KeyDown;

                return; // Exit UpdateGame
            }

            // --- LOGIC FOR HOW TO PLAY MENU ---
            else if (_currentSceneName == HOW_TO_PLAY_SCENE_NAME)
            {
                // Check for 'R' (without Ctrl) to RETURN TO MAIN MENU
                if (r_KeyDown && !_r_KeyWasDown && !ctrl_KeyDown)
                {
                    API.Log("Returning to Main Menu from HowToPlay.");
                    API.LoadScene(MAIN_MENU_SCENE_NAME);
                    _r_KeyWasDown = r_KeyDown; // Set tracker
                    return; // Exit after action
                }

                // Update all menu key trackers if no action was taken
                _p_KeyWasDown = p_KeyDown;
                _h_KeyWasDown = h_KeyDown;
                _r_KeyWasDown = r_KeyDown;
                _q_KeyWasDown = q_KeyDown;

                return; // Exit UpdateGame
            }

            // --- LOGIC FOR IN-GAME ---

            // Check for 'P' (without Ctrl) to PAUSE the game
            if (p_KeyDown && !_p_KeyWasDown && !ctrl_KeyDown)
            {
                API.Log("Pausing game (P key)...");
                API.TogglePause();
                API.LoadSceneAdditive(PAUSE_SCENE_NAME);
                _p_KeyWasDown = p_KeyDown;
                return;
            }
            _p_KeyWasDown = p_KeyDown;

            // NOTE: Player movement is now handled by PlayerMovement.cs script
            // The PlayerMovement script should be attached to the player entity
        }

        // This function runs all your pause menu logic
        private static void UpdatePauseMenu()
        {
            // --- Resume Button (R) ---
            bool r_KeyDown = API.IsKeyDown(API.KEY_R);
            if (r_KeyDown && !_r_KeyWasDown)
            {
                API.Log("Resuming game...");

                // --- NEW RESUME LOGIC ---
                API.UnloadPauseMenu(); // 1. Destroy the menu objects
                API.TogglePause();     // 2. Un-freeze the game

                _r_KeyWasDown = r_KeyDown;
                return;
            }
            _r_KeyWasDown = r_KeyDown;

            // --- Main Menu Button (M) ---
            bool m_KeyDown = API.IsKeyDown(API.KEY_M);
            if (m_KeyDown && !_m_KeyWasDown)
            {
                API.Log("Returning to Main Menu...");

                API.TogglePause(); // 1. Un-pause the engine first...
                API.LoadScene(MAIN_MENU_SCENE_NAME); // 2. ...THEN load the new scene (this will clear everything)

                _m_KeyWasDown = m_KeyDown;
                return;
            }
            _m_KeyWasDown = m_KeyDown;

            // --- Restart Button (Y) ---
            bool y_KeyDown = API.IsKeyDown(API.KEY_Y);
            if (y_KeyDown && !_y_KeyWasDown)
            {
                API.Log("Restarting scene...");

                // 1. Un-pause the engine state
                API.TogglePause();

                // 2. Reload the current game scene
                // (This replaces all scenes, including the pause menu)
                API.LoadScene(_currentSceneName);

                _y_KeyWasDown = y_KeyDown;
                return;
            }
            _y_KeyWasDown = y_KeyDown;

            // --- Quit Button (Q) ---
            bool q_KeyDown = API.IsKeyDown(API.KEY_Q);
            if (q_KeyDown && !_q_KeyWasDown)
            {
                API.Log("Quitting game...");
                API.QuitGame();
                _q_KeyWasDown = q_KeyDown;
                return;
            }
            _q_KeyWasDown = q_KeyDown;
        }
    }
}