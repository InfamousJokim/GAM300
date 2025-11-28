// Boom/API.cs - FIXED VERSION
using Boom;
using System;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Boom
{

    

    // Internal calls implemented in C++ and registered with Mono
    internal static class Native
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_Log(string msg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static ulong Boom_API_FindEntity(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetPosition(ulong handle, out Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetPosition(ulong handle, ref Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsKeyDown(int key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsMouseDown(int button);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasTransform(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasScript(ulong handle);

        // ========= PHYSICS / RIGIDBODY INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetRotation(ulong handle, out Vec3 outRot);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetRotation(ulong handle, ref Vec3 rot);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_DrawDebugVisionCone(ulong entityHandle,
            float range, float angle, float r, float g, float b, float a);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Boom_API_GetThirdPersonCameraYaw();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_HasCollider(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_IsTrigger(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetTrigger(ulong handle, bool isTrigger);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_RegisterTriggerEnterCallback(ulong triggerHandle, object delegateObj);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_RegisterTriggerExitCallback(ulong triggerHandle, object delegateObj);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_UnregisterTriggerCallbacks(ulong triggerHandle);

        // ========= NEW PHYSICS / RIGIDBODY INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetLinearVelocity(ulong handle, out Vec3 vel);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetLinearVelocity(ulong handle, ref Vec3 vel);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsColliding(ulong handle);

        // ========= ANIMATOR INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetFloat(ulong h, string name, float v);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetFloat(ulong h, string name, double v);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetBool(ulong h, string name, bool v);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorSetTrigger(ulong h, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AnimatorPlay(ulong h, string state);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_HasAnimator(ulong handle);

        // ========= TRANSFORM STRUCT INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetTransform(ulong handle, out TransformData transform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetTransform(ulong handle, ref TransformData transform);

        // ========= SCENE MANAGEMENT =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_LoadScene(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string Boom_API_GetCurrentSceneName();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_QuitGame();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_LoadSceneAdditive(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_UnloadPauseMenu();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_TogglePause();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int Boom_API_GetApplicationState();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_IsPauseMenuLoaded();

        //AI STUFF
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int Boom_API_AI_GetPatrolPointCount(ulong handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_AI_GetPatrolPoint(ulong handle, int index, out Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int Boom_API_AI_GetMode(ulong handle);

        //Animator Stuff


        // ========= SOUND / AUDIO INTERNAL CALLS =========
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PlaySound(string name, string filePath, bool loop);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PlaySoundAt(string name, string filePath, ref Vec3 position, bool loop);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_StopSound(string name);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundVolume(string name, float volume);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Boom_API_IsSoundPlaying(string name);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PauseSound(string name, bool pause);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_PreloadSound(string name, string filePath, bool loop);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetSoundPosition(string name, ref Vec3 position);

        // Raycasting & Main Menu
        [MethodImpl(MethodImplOptions.InternalCall)] 
        internal static extern ulong Boom_API_PickGameEntity();
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_Linecast(ref Vec3 from, ref Vec3 to, ulong ignoreEntity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetRotationY(ulong handle, float yawDegrees);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool LinecastIgnoreBoth(Vec3 from, Vec3 to, ulong ignoreEntity1, ulong ignoreEntity2);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_TeleportRigidBody(ulong handle, ref Vec3 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Boom_API_SetScreenFadeAlpha(float alpha);
    }

    // ========= DELEGATES =========
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TriggerCallback(ulong triggerEntity, ulong otherEntity);

    // ========= DATA STRUCTURES =========
    [StructLayout(LayoutKind.Sequential)]
    public struct Vec3
    {
        public float X, Y, Z;

        public Vec3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vec4
    {
        public float X, Y, Z, W;

        public Vec4(float x, float y, float z, float w)
        {
            X = x; Y = y; Z = z; W = w;
        }
    }

    // RENAMED to avoid conflict with the static class below
    [StructLayout(LayoutKind.Sequential)]
    public struct TransformData
    {
        public float PositionX, PositionY, PositionZ;
        public float RotationX, RotationY, RotationZ;
        public float ScaleX, ScaleY, ScaleZ;

        public Vec3 Position
        {
            get => new Vec3(PositionX, PositionY, PositionZ);
            set { PositionX = value.X; PositionY = value.Y; PositionZ = value.Z; }
        }

        public Vec3 Rotation
        {
            get => new Vec3(RotationX, RotationY, RotationZ);
            set { RotationX = value.X; RotationY = value.Y; RotationZ = value.Z; }
        }

        public Vec3 Scale
        {
            get => new Vec3(ScaleX, ScaleY, ScaleZ);
            set { ScaleX = value.X; ScaleY = value.Y; ScaleZ = value.Z; }
        }
    }

    // ========= PUBLIC API =========
    public static class API
    {
        // Cache trigger callbacks to prevent garbage collection
        private static System.Collections.Generic.Dictionary<ulong, TriggerCallback> s_TriggerEnterCallbacks = new System.Collections.Generic.Dictionary<ulong, TriggerCallback>();
        private static System.Collections.Generic.Dictionary<ulong, TriggerCallback> s_TriggerExitCallbacks = new System.Collections.Generic.Dictionary<ulong, TriggerCallback>();

        // ===== Logging =====
        public static void Log(string s) => Native.Boom_API_Log(s);

        // ===== Entity queries =====
        public static ulong FindEntity(string name) => Native.Boom_API_FindEntity(name);

        //AI Helpers
        public enum AIMode
        {
            Auto = 0,
            Idle = 1,
            Patrol = 2,
            Seek = 3
        }
        public static int GetAIPatrolPointCount(ulong h)
    => Native.Boom_API_AI_GetPatrolPointCount(h);

        public static Vec3 GetAIPatrolPoint(ulong h, int index)
        {
            Native.Boom_API_AI_GetPatrolPoint(h, index, out var p);
            return p;
        }

        public static AIMode GetAIMode(ulong h)
        {
            int mode = Native.Boom_API_AI_GetMode(h);
            return (AIMode)mode;
        }
        // ===== Transform with validation =====
        public static Vec3 GetPosition(ulong h)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent!");
                return new Vec3(0, 0, 0);
            }
            Native.Boom_API_GetPosition(h, out var p);
            return p;
        }

        public static void SetPosition(ulong h, Vec3 p)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent! Cannot set position.");
                return;
            }
            Native.Boom_API_SetPosition(h, ref p);
        }

        public static Vec3 GetRotation(ulong h)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent!");
                return new Vec3(0, 0, 0);
            }
            Native.Boom_API_GetRotation(h, out var r);
            return r;
        }

        public static void SetRotation(ulong h, Vec3 r)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent! Cannot set rotation.");
                return;
            }
            Native.Boom_API_SetRotation(h, ref r);
        }

        // ===== Rotation axis helpers =====
        public static float GetRotationX(ulong h)
        {
            var r = GetRotation(h);   // uses Native.Boom_API_GetRotation
            return r.X;
        }

        public static float GetRotationY(ulong h)
        {
            var r = GetRotation(h);
            return r.Y;
        }

        public static float GetRotationZ(ulong h)
        {
            var r = GetRotation(h);
            return r.Z;
        }

        public static void SetRotationX(ulong h, float pitchDegrees)
        {
            var r = GetRotation(h);
            r.X = pitchDegrees;
            SetRotation(h, r);        // uses Native.Boom_API_SetRotation
        }

        public static void SetRotationYDeg(ulong h, float yawDegrees)
        {
            SetRotationY(h, yawDegrees); // calls Native.Boom_API_SetRotationY
        }

        public static void SetRotationZ(ulong h, float rollDegrees)
        {
            var r = GetRotation(h);
            r.Z = rollDegrees;
            SetRotation(h, r);
        }

        public static TransformData GetTransform(ulong h)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent!");
                return new TransformData();
            }
            Native.Boom_API_GetTransform(h, out var t);
            return t;
        }

        public static void SetTransform(ulong h, TransformData t)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent! Cannot set transform.");
                return;
            }
            Native.Boom_API_SetTransform(h, ref t);
        }

        // ===== Component checking =====
        public static bool HasTransform(ulong h) => Native.Boom_API_HasTransform(h);
        public static bool HasScript(ulong h) => Native.Boom_API_HasScript(h);
        public static bool HasAnimator(ulong h) => Native.Boom_API_HasAnimator(h);
        public static bool HasCollider(ulong entity) => Native.Boom_API_HasCollider(entity);

        // ===== Input =====
        public static bool IsKeyDown(int glfwKey) => Native.Boom_API_IsKeyDown(glfwKey);
        public static bool IsMouseDown(int button) => Native.Boom_API_IsMouseDown(button);

        // ===== Physics / Rigidbody =====
        public static Vec3 GetLinearVelocity(ulong h)
        {
            Native.Boom_API_GetLinearVelocity(h, out var v);
            return v;
        }

        public static void SetLinearVelocity(ulong h, Vec3 v)
        {
            Native.Boom_API_SetLinearVelocity(h, ref v);
        }

        public static bool IsColliding(ulong h) => Native.Boom_API_IsColliding(h);

        // ===== Triggers =====
        public static bool IsTrigger(ulong entity) => Native.Boom_API_IsTrigger(entity);

        public static void SetTrigger(ulong entity, bool isTrigger)
        {
            Native.Boom_API_SetTrigger(entity, isTrigger);
        }

        public static void RegisterTriggerEnterCallback(ulong triggerEntity, TriggerCallback callback)
        {
            // Cache the delegate to prevent garbage collection
            s_TriggerEnterCallbacks[triggerEntity] = callback;
            Native.Boom_API_RegisterTriggerEnterCallback(triggerEntity, callback);
        }

        public static void RegisterTriggerExitCallback(ulong triggerEntity, TriggerCallback callback)
        {
            // Cache the delegate to prevent garbage collection
            s_TriggerExitCallbacks[triggerEntity] = callback;
            Native.Boom_API_RegisterTriggerExitCallback(triggerEntity, callback);
        }

        public static void UnregisterTriggerCallbacks(ulong triggerEntity)
        {
            // Remove from cache
            s_TriggerEnterCallbacks.Remove(triggerEntity);
            s_TriggerExitCallbacks.Remove(triggerEntity);
            Native.Boom_API_UnregisterTriggerCallbacks(triggerEntity);
        }

        // ===== Debug Visualization =====
        public static void DrawDebugVisionCone(ulong entityHandle, float range, float halfAngle, Vec4 color)
        {
            Native.Boom_API_DrawDebugVisionCone(entityHandle, range, halfAngle,
                color.X, color.Y, color.Z, color.W);
        }

        public static void DrawDebugVisionCone(ulong entityHandle, float range, float halfAngle)
        {
            DrawDebugVisionCone(entityHandle, range, halfAngle, new Vec4(0f, 1f, 0f, 0.3f));
        }

        public static bool IsGrounded(ulong entity, float probeDistance = 0.25f)
        {
            // cast a short ray straight down from the entity
            var p = GetPosition(entity);
            var from = new Vec3(p.X, p.Y + 0.05f, p.Z);
            var to = new Vec3(p.X, p.Y - probeDistance, p.Z);

            // true if we hit anything (ignore self)
            return Linecast(from, to, entity);
        }

        // ===== Camera =====
        public static float GetThirdPersonCameraYaw() => Native.Boom_API_GetThirdPersonCameraYaw();

        // ===== Scene Management =====
        public static void LoadScene(string name) => Native.Boom_API_LoadScene(name);
        public static string GetCurrentSceneName() => Native.Boom_API_GetCurrentSceneName();
        public static void QuitGame() => Native.Boom_API_QuitGame();
        public static void LoadSceneAdditive(string name) => Native.Boom_API_LoadSceneAdditive(name);
        public static void UnloadPauseMenu() => Native.Boom_API_UnloadPauseMenu();
        public static void TogglePause() => Native.Boom_API_TogglePause();
        public static int GetApplicationState() => Native.Boom_API_GetApplicationState();
        public static bool IsPauseMenuLoaded() => Native.Boom_API_IsPauseMenuLoaded();

        // ===== Animator =====
        public static void AnimatorSetFloat(ulong h, string n, float v) => Native.Boom_API_AnimatorSetFloat(h, n, v);
        public static void AnimatorSetFloat(ulong h, string n, double v) => Native.Boom_API_AnimatorSetFloat(h, n, v);
        public static void AnimatorSetBool(ulong h, string n, bool v) => Native.Boom_API_AnimatorSetBool(h, n, v);
        public static void AnimatorSetTrigger(ulong h, string n) => Native.Boom_API_AnimatorSetTrigger(h, n);
        public static void AnimatorPlay(ulong h, string state) => Native.Boom_API_AnimatorPlay(h, state);

        // ===== Input Constants =====
        // ===== SOUND / AUDIO API =====
        
        /// <summary>
        /// Play a 2D sound effect
        /// </summary>
        public static void PlaySound(string name, string filePath, bool loop = false)
        {
            Native.Boom_API_PlaySound(name, filePath, loop);
        }
        
        /// <summary>
        /// Play a 3D positional sound at specific world coordinates
        /// </summary>
        public static void PlaySoundAt(string name, string filePath, Vec3 position, bool loop = false)
        {
            Native.Boom_API_PlaySoundAt(name, filePath, ref position, loop);
        }
        
        /// <summary>
        /// Stop a playing sound
        /// </summary>
        public static void StopSound(string name)
        {
            Native.Boom_API_StopSound(name);
        }
        
        /// <summary>
        /// Set the volume of a sound (0.0 - 1.0)
        /// </summary>
        public static void SetSoundVolume(string name, float volume)
        {
            Native.Boom_API_SetSoundVolume(name, volume);
        }
        
        /// <summary>
        /// Check if a sound is currently playing
        /// </summary>
        public static bool IsSoundPlaying(string name)
        {
            return Native.Boom_API_IsSoundPlaying(name);
        }
        
        /// <summary>
        /// Pause or unpause a sound
        /// </summary>
        public static void PauseSound(string name, bool pause)
        {
            Native.Boom_API_PauseSound(name, pause);
        }
        
        /// <summary>
        /// Preload a sound for faster playback later
        /// </summary>
        public static void PreloadSound(string name, string filePath, bool loop = false)
        {
            Native.Boom_API_PreloadSound(name, filePath, loop);
        }
        
        /// <summary>
        /// Update the position of a 3D sound that's already playing
        /// </summary>
        public static void SetSoundPosition(string name, Vec3 position)
        {
            Native.Boom_API_SetSoundPosition(name, ref position);
        }

        public static bool LinecastIgnoreBoth(Vec3 from, Vec3 to, ulong ignoreEntity1, ulong ignoreEntity2)
        {
            return Native.LinecastIgnoreBoth(from, to, ignoreEntity1, ignoreEntity2);
        }

        // Raycasting
        public static ulong PickGameEntity() => Native.Boom_API_PickGameEntity();

        
        public static bool Linecast(Vec3 from, Vec3 to, ulong ignoreEntity = 0)
        {
            return Native.Boom_API_Linecast(ref from, ref to, ignoreEntity);
        }

        public static void SetRotationY(ulong h, float yawDegrees)
        {
            Native.Boom_API_SetRotationY(h, yawDegrees);
        }

        public static void TeleportRigidBody(ulong h, Vec3 p)
        {
            if (!Native.Boom_API_HasTransform(h))
            {
                Log($"[WARNING] Entity {h} does not have TransformComponent! Cannot teleport.");
                return;
            }
            Native.Boom_API_TeleportRigidBody(h, ref p);
        }

        public static void SetScreenFadeAlpha(float alpha)
        {
            Native.Boom_API_SetScreenFadeAlpha(alpha);
        }

        // ===== GLFW key codes =====
        public const int KEY_LEFT = 263;
        public const int KEY_RIGHT = 262;
        public const int KEY_UP = 265;
        public const int KEY_DOWN = 264;
        public const int KEY_W = 87;
        public const int KEY_A = 65;
        public const int KEY_S = 83;
        public const int KEY_D = 68;
        public const int KEY_SPACE = 32;
        public const int KEY_H = 72;
        public const int KEY_P = 80;
        public const int KEY_R = 82;
        public const int KEY_Y = 89;
        public const int KEY_M = 77;
        public const int KEY_Q = 81;
        public const int KEY_LEFT_CONTROL = 341;
        public const int KEY_LEFT_SHIFT = 340;

        public const int MOUSE_LEFT = 0;
        public const int MOUSE_RIGHT = 1;
        public const int MOUSE_MIDDLE = 2;

        // ===== Application State Constants =====
        public const int APP_STATE_RUNNING = 0;
        public const int APP_STATE_PAUSED = 1;
        public const int APP_STATE_STOPPED = 2;
    }

    // Helper static class for legacy code (optional - can be removed if not used elsewhere)
    public static class Transform
    {
        public static Vec3 GetPosition(ulong handle) => API.GetPosition(handle);
        public static void SetPosition(ulong handle, Vec3 p) => API.SetPosition(handle, p);
    }
}

namespace GameScripts
{
    public static class ScriptRegistry
    {
        public static string[] GetAvailableScriptTypes()
        {
            try
            {
                var scriptTypes = System.Reflection.Assembly.GetExecutingAssembly()
                    .GetTypes()
                    .Where(t => t.IsClass && !t.IsAbstract && IsScriptType(t))
                    .Select(t => t.FullName ?? t.Name)
                    .OrderBy(name => name)
                    .ToArray();

                return scriptTypes;
            }
            catch (Exception ex)
            {
                Boom.API.Log($"[C# ScriptRegistry] Error getting script types: {ex.Message}");
                return Array.Empty<string>();
            }
        }

        private static bool IsScriptType(Type type)
        {
            bool hasOnStart = type.GetMethod("OnStart",
                BindingFlags.Public | BindingFlags.Instance,
                null, new[] { typeof(string) }, null) != null;

            bool hasOnUpdate = type.GetMethod("OnUpdate",
                BindingFlags.Public | BindingFlags.Instance,
                null, new[] { typeof(float) }, null) != null;

            bool hasOnDestroy = type.GetMethod("OnDestroy",
                BindingFlags.Public | BindingFlags.Instance,
                null, Type.EmptyTypes, null) != null;

            return hasOnStart || hasOnUpdate || hasOnDestroy;
        }

        
    }
}