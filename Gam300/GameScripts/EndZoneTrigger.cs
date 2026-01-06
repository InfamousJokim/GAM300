using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// EndZoneTrigger handles player collision with the end zone.
    /// When the player enters this trigger, it loads the main menu scene.
    /// </summary>
    public class EndZoneTrigger
    {
        public ulong Entity;

        private bool _hasTriggered = false;

        // Static instance tracking like DoorTriggerLeft
        private static readonly Dictionary<ulong, EndZoneTrigger> s_instances = new Dictionary<ulong, EndZoneTrigger>();

        public void OnStart(string jsonParams)
        {
            // Register this instance
            s_instances[Entity] = this;

            // Ensure trigger is configured
            if (!API.HasCollider(Entity))
            {
                API.Log("[EndZoneTrigger] WARNING: Trigger entity has no collider!");
            }
            else if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
            }

            // Register static callbacks
            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnterCallback);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExitCallback);
            API.Log("[EndZoneTrigger] Registered trigger callbacks.");
        }

        public void OnUpdate(float dt)
        {
            // No update logic needed - scene loads immediately on trigger
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        // Static callback for trigger enter
        private static void OnTriggerEnterCallback(ulong triggerEntity, ulong otherEntity)
        {
            EndZoneTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only player triggers this
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // Prevent multiple triggers
            if (inst._hasTriggered) return;

            API.Log("[EndZoneTrigger] Player entered end zone! Loading MainMenu...");
            inst._hasTriggered = true;

            // Load the main menu scene immediately
            API.LoadScene("MainMenu");
        }

        // Static callback for trigger exit
        private static void OnTriggerExitCallback(ulong triggerEntity, ulong otherEntity)
        {
            EndZoneTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react to player exiting
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            API.Log("[EndZoneTrigger] Player left end zone");
        }
    }
}