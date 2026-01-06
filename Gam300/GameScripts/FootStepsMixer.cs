// Put this in a shared file (e.g., GameScripts/Audio/FootstepMixer.cs)
using System;
using System.Collections.Generic;
using Boom;

public static class FootstepMixer
{
    private const int MAX_AUDIBLE = 6;      // cap
    private const float MAX_HEAR_DIST = 20; // hard cutoff (units)
    private const float MIN_VOL = 0.15f;    // fade floor
    private const float MAX_VOL = 0.9f;

    // register each frame with {key, pos, speed, wantPlay}
    private struct Req { public string key; public Vec3 pos; public float metric; public bool want; }
    private static readonly List<Req> _frame = new List<Req>();

    private static float Clamp(float value, float min, float max)
    {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    // If you have a reliable listener position function, plug it here.
    // Fallback: try to find "Player"
    private static Vec3 GetListenerPos()
    {
        ulong player = API.FindEntity("Player");
        return player != 0 ? API.GetPosition(player) : new Vec3(0, 0, 0);
    }

    public static void Submit(string key, Vec3 pos, float speedXZ, bool wantPlay)
    {
        _frame.Add(new Req { key = key, pos = pos, metric = speedXZ, want = wantPlay });
    }

    public static void Mix(string sfxPath)
    {
        var ear = GetListenerPos();

        // rank by (nearer first, then faster)
        _frame.Sort((a, b) =>
        {
            float da = DistSq(a.pos, ear);
            float db = DistSq(b.pos, ear);
            int cmp = da.CompareTo(db);
            if (cmp != 0) return cmp;
            return b.metric.CompareTo(a.metric);
        });

        // allow top MAX_AUDIBLE within range
        int allowed = 0;
        for (int i = 0; i < _frame.Count; ++i)
        {
            var r = _frame[i];
            float d = (float)Math.Sqrt(DistSq(r.pos, ear));

            bool inRange = d <= MAX_HEAR_DIST;
            bool enabled = r.want && inRange && (allowed < MAX_AUDIBLE);

            if (enabled)
            {
                allowed++;
                // volume by distance
                float ratio = d / MAX_HEAR_DIST;
                if (ratio < 0f) ratio = 0f;
                else if (ratio > 1f) ratio = 1f;

                float t = 1f - ratio;
                float vol = MIN_VOL + t * (MAX_VOL - MIN_VOL);

                if (!API.IsSoundPlaying(r.key))
                    API.PlaySoundAt(r.key, sfxPath, r.pos, loop: true);
                else
                    API.SetSoundPosition(r.key, r.pos);

                API.SetSoundVolume(r.key, vol);
            }
            else
            {
                if (API.IsSoundPlaying(r.key))
                    API.StopSound(r.key);
            }
        }

        _frame.Clear();
    }

    private static float DistSq(Vec3 a, Vec3 b)
    {
        float dx = a.X - b.X, dy = a.Y - b.Y, dz = a.Z - b.Z;
        return dx * dx + dy * dy + dz * dz;
    }
}
