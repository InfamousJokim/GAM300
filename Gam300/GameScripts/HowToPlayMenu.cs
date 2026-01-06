using System;
using Boom;

namespace GameScripts
{
    public class HowToPlayMenu
    {
        private const int MOUSE_LEFT = 0;
        private const string RETURN_TEX_CLICKED = "Resources/Textures/HowToPlayMenu/ReturnButton_Clicked.png";

        private ulong _returnButtonID;

        private enum MenuState
        {
            Idle,
            ButtonDelay
        }

        private MenuState _currentState = MenuState.Idle;
        private ulong _clickedButtonID = 0;

        public void OnStart(string jsonParams)
        {
            API.Log("HowToPlayMenu OnStart Running...");
            _returnButtonID = API.FindEntity("ReturnButton");

            API.Log("Return ID: " + _returnButtonID);

            if (_returnButtonID == 0) API.Log("Warning: Return Button not found!");

            _currentState = MenuState.Idle;
            _clickedButtonID = 0;
        }

        public void OnUpdate(float dt)
        {
            switch (_currentState)
            {
                case MenuState.Idle:
                    Update_Idle();
                    break;

                case MenuState.ButtonDelay:
                    Update_ButtonDelay(dt);
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

                if (API.Check2DViewportClick(_returnButtonID, mousePos.X, mousePos.Y))
                {
                    StartClickDelay(_returnButtonID);
                }
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

            if (buttonID == _returnButtonID)
                API.SetSpriteTexture(buttonID, RETURN_TEX_CLICKED);
        }

        private void ExecuteClickAction()
        {
            _currentState = MenuState.Idle;

            if (_clickedButtonID == _returnButtonID)
            {
                API.Log(">> Return Button Clicked! Returning to Main Menu...");
                API.LoadScene(Entry.MAIN_MENU_SCENE_NAME);
            }
        }
    }
}