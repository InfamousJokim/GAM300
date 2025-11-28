using Boom;

namespace GameScripts
{
    // Simple global inventory for keys.
    public static class PlayerInventory
    {
        private static int s_keyCount = 0;

        public static void Reset()
        {
            s_keyCount = 0;
            API.Log("[PlayerInventory] Reset");
        }

        public static void AddKey(int count = 1)
        {
            if (count < 1) return;
            s_keyCount += count;
            API.Log($"[PlayerInventory] Keys: {s_keyCount}");
        }

        public static bool HasKey()
        {
            return s_keyCount > 0;
        }

        // Returns true if a key was consumed.
        public static bool ConsumeKey()
        {
            if (s_keyCount <= 0) return false;
            s_keyCount--;
            API.Log($"[PlayerInventory] Consumed key. Keys left: {s_keyCount}");
            return true;
        }

        public static int GetKeyCount() => s_keyCount;
    }
}