using Boom;

namespace GameScripts
{
    /// <summary>
    /// Singleton UI manager that coordinates all UI controllers
    /// Attach this to the PlayerUI empty game object
    /// Other scripts can call UIManager.ShowKeyPickup(), etc.
    /// </summary>
    public class UIManager
    {
        public ulong Entity;

        private static UIManager s_instance = null;

        private UIKeyController _keyUI;
        private UIHoldController _holdUI;
        private UIEndController _endUI;
        private UILocationController _locationUI;
        private UIHeartController _heartUI;  // *** NEW: Heart UI controller ***

        public void OnStart(string jsonParams)
        {
            // Register as singleton
            s_instance = this;

            // Initialize all UI controllers
            _keyUI = new UIKeyController { Entity = Entity };
            _keyUI.OnStart(jsonParams);

            _holdUI = new UIHoldController { Entity = Entity };
            _holdUI.OnStart(jsonParams);

            _endUI = new UIEndController { Entity = Entity };
            _endUI.OnStart(jsonParams);

            _locationUI = new UILocationController { Entity = Entity };
            _locationUI.OnStart(jsonParams);

            // *** NEW: Initialize heart UI ***
            _heartUI = new UIHeartController { Entity = Entity };
            _heartUI.OnStart(jsonParams);

            API.Log("[UIManager] All UI systems initialized (including hearts)");
        }

        public void OnUpdate(float dt)
        {
            // Update all UI controllers
            _keyUI?.OnUpdate(dt);
            _holdUI?.OnUpdate(dt);
            _endUI?.OnUpdate(dt);
            _locationUI?.OnUpdate(dt);
            _heartUI?.OnUpdate(dt);  // *** NEW: Update heart UI ***
        }

        public void OnDestroy()
        {
            if (s_instance == this)
            {
                s_instance = null;
                API.Log("[UIManager] Singleton unregistered");
            }
        }

        // ===== Static Public API =====

        /// <summary>
        /// Show the key pickup UI notification
        /// </summary>
        public static void ShowKeyPickup()
        {
            if (s_instance != null)
            {
                s_instance._keyUI?.ShowKey();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot show key pickup UI");
            }
        }

        /// <summary>
        /// Show the "Hold to Crouch" prompt
        /// </summary>
        public static void ShowHoldPrompt()
        {
            if (s_instance != null)
            {
                s_instance._holdUI?.Show();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot show hold prompt");
            }
        }

        /// <summary>
        /// Hide the "Hold to Crouch" prompt
        /// </summary>
        public static void HideHoldPrompt()
        {
            if (s_instance != null)
            {
                s_instance._holdUI?.Hide();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot hide hold prompt");
            }
        }

        /// <summary>
        /// Trigger the end game sequence
        /// </summary>
        public static void TriggerEndGame()
        {
            if (s_instance != null)
            {
                s_instance._endUI?.TriggerEnd();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot trigger end game");
            }
        }

        /// <summary>
        /// Show the Garden location indicator
        /// </summary>
        public static void ShowGardenLocation()
        {
            if (s_instance != null)
            {
                s_instance._locationUI?.ShowGarden();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot show garden location");
            }
        }

        /// <summary>
        /// Show the Beginning location indicator
        /// </summary>
        public static void ShowBeginningLocation()
        {
            if (s_instance != null)
            {
                s_instance._locationUI?.ShowBeginning();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot show beginning location");
            }
        }

        /// <summary>
        /// Check if UIManager is initialized
        /// </summary>
        public static bool IsInitialized()
        {
            return s_instance != null;
        }
    }
}