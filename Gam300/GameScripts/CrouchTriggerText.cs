using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Attach to a trigger zone where the player needs to crouch to pass
    /// Shows "Hold to Crouch" UI prompt when player enters
    /// Hides the prompt when player exits
    /// </summary>
    public class CrouchTriggerZone
    {
        public ulong Entity;

        private static readonly Dictionary<ulong, CrouchTriggerZone> s_instances = new Dictionary<ulong, CrouchTriggerZone>();

        // Optional: Play a sound when entering the zone
        private bool _playSoundOnEnter = false;
        private string _enterSound = "Resources/Audio/ambient_warning.wav";

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;

            if (!API.HasCollider(Entity))
            {
                API.Log("[CrouchTriggerZone] WARNING: Entity has no collider. Trigger will not work.");
                return;
            }

            // Ensure this collider is a trigger
            if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
                API.Log("[CrouchTriggerZone] Collider set to IsTrigger = true.");
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log("[CrouchTriggerZone] Registered trigger callbacks.");
        }

        public void OnUpdate(float dt)
        {
            // No-op
        }

        public void OnDestroy()
        {
            // Cleanup
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            CrouchTriggerZone inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react when the player enters this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // *** Show the "Hold to Crouch" UI prompt ***
            UIManager.ShowHoldPrompt();

            // Optional: Play warning sound
            if (inst._playSoundOnEnter && API.HasTransform(inst.Entity))
            {
                var p = API.GetPosition(inst.Entity);
                API.PlaySoundAt("sfx_crouch_zone_enter", inst._enterSound, p, false);
                API.SetSoundVolume("sfx_crouch_zone_enter", 0.5f);
                API.Set3DMinMaxDistance("sfx_crouch_zone_enter", 1.0f, 12.0f);  // Zone trigger sound
            }

            API.Log("[CrouchTriggerZone] Player entered crouch zone - showing UI prompt");
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            CrouchTriggerZone inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react when the player exits this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // *** Hide the "Hold to Crouch" UI prompt ***
            UIManager.HideHoldPrompt();

            API.Log("[CrouchTriggerZone] Player exited crouch zone - hiding UI prompt");
        }
    }
}