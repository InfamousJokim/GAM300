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
        private string _pickupSound = "Resources/Audio/pickup.wav";

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
            UIManager.ShowKeyPickup();

            // Play pickup SFX at key's position
            if (API.HasTransform(inst.Entity))
            {
                var p = API.GetPosition(inst.Entity);
                API.PlaySoundAt("sfx_key_pickup", inst._pickupSound, p, false);
                API.SetSoundVolume("sfx_key_pickup", 0.9f);
            }

            // "Destroy" key: unregister callbacks and teleport it far below the map
            API.UnregisterTriggerCallbacks(inst.Entity);

            // Teleport key to bottom of map (far below Y = -100)
            var currentPos = API.GetPosition(inst.Entity);
            API.SetPosition(inst.Entity, new Vec3(currentPos.X, -100f, currentPos.Z));

            API.Log("[KeyPickup] Key collected! Total keys: " + PlayerInventory.GetKeyCount());
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            // Not needed for pickup
        }
    }
}