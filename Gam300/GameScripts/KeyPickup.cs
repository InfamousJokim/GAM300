using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    // Attach to a key entity with a Collider marked as IsTrigger.
    // When the player enters, grant a key and hide/disable this entity.
    public class KeyPickup
    {
        public ulong Entity;

        // Optional: sound to play on pickup
        private string _pickupSound = "Resources/Audio/playerPunch_1.wav";

        private static readonly Dictionary<ulong, KeyPickup> s_instances = new Dictionary<ulong, KeyPickup>();
        private bool _collected = false;

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;

            if (!API.HasCollider(Entity))
            {
                API.Log("[KeyPickup] WARNING: Entity has no collider. Pickup will not trigger.");
                return;
            }

            // Ensure this collider is a trigger
            if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
                API.Log("[KeyPickup] Collider set to IsTrigger = true.");
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log("[KeyPickup] Registered trigger callbacks.");
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
            KeyPickup inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            if (inst._collected) return;

            // Only react when the player enters this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._collected = true;
            PlayerInventory.AddKey(1);

            // Play pickup SFX at key's position
            if (API.HasTransform(inst.Entity))
            {
                var p = API.GetPosition(inst.Entity);
                API.PlaySoundAt("sfx_key_pickup", inst._pickupSound, p, false);
                API.SetSoundVolume("sfx_key_pickup", 0.9f);
            }

            // "Destroy" key: disable trigger and hide it
            API.UnregisterTriggerCallbacks(inst.Entity);
            API.SetTrigger(inst.Entity, false);

            // Hide: scale to zero (visible off)
            var t = API.GetTransform(inst.Entity);
            t.Scale = new Vec3(0f, 0f, 0f);
            API.SetTransform(inst.Entity, t);

            API.Log($"[KeyPickup] Key collected and removed. Entity={inst.Entity}");
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            // Not needed for pickup
        }
    }
}