// Boom/API.cs
// Single API file for all script-side engine calls. You do NOT need a separate Native.cs.
using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

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

        // ========= NEW PHYSICS / RIGIDBODY INTERNAL CALLS =========

        // Get the current linear velocity from the physics engine
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_GetLinearVelocity(ulong handle, out Vec3 vel);

        // Set the current linear velocity in the physics engine
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Boom_API_SetLinearVelocity(ulong handle, ref Vec3 vel);

        // Query if the rigidbody is currently colliding / grounded
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Boom_API_IsColliding(ulong handle);
    }

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

    public static class API
    {
        // ===== Logging =====
        public static void Log(string s) => Native.Boom_API_Log(s);

        // ===== Entity queries =====
        public static ulong FindEntity(string name) => Native.Boom_API_FindEntity(name);

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

        // ===== Component checking =====
        public static bool HasTransform(ulong h) => Native.Boom_API_HasTransform(h);
        public static bool HasScript(ulong h) => Native.Boom_API_HasScript(h);

        // ===== Input =====
        public static bool IsKeyDown(int glfwKey) => Native.Boom_API_IsKeyDown(glfwKey);
        public static bool IsMouseDown(int button) => Native.Boom_API_IsMouseDown(button);

        // ===== PHYSICS / RIGIDBODY HELPERS =====

        /// <summary>
        /// Get the current linear velocity of the rigidbody attached to this entity.
        /// </summary>
        public static Vec3 GetLinearVelocity(ulong h)
        {
            Native.Boom_API_GetLinearVelocity(h, out var v);
            return v;
        }

        /// <summary>
        /// Set the current linear velocity of the rigidbody attached to this entity.
        /// </summary>
        public static void SetLinearVelocity(ulong h, Vec3 v)
        {
            Native.Boom_API_SetLinearVelocity(h, ref v);
        }

        /// <summary>
        /// Returns true if the rigidbody is colliding / grounded according to the engine.
        /// </summary>
        public static bool IsColliding(ulong h) => Native.Boom_API_IsColliding(h);

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

        // ===== Mouse buttons =====
        public const int MOUSE_LEFT = 0;
        public const int MOUSE_RIGHT = 1;
        public const int MOUSE_MIDDLE = 2;
    }

    public static class Transform
    {
        public static Vec3 GetPosition(ulong handle) => API.GetPosition(handle);
        public static void SetPosition(ulong handle, Vec3 p) => API.SetPosition(handle, p);
    }
}
