#pragma once
#include "Core.h"
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <string>
#include <vector>

namespace Boom {

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4251) // “needs to have dll-interface” (STL member in exported class)
#  pragma warning(disable: 4275) // (optional) non-DLL-interface base — only if you ever hit it
#endif

    class BOOM_API MonoRuntime {
    public:
        bool Init(const char* domainName, const char* assembliesPath /* may be nullptr */);
        void UnloadDomain();
        void Shutdown();

        // Load a C# assembly (e.g., "GameScripts.dll")
        MonoAssembly* LoadAssembly(const char* path);
        // Find & invoke static method: "Namespace.TypeName:MethodName(signature)"
        bool InvokeStatic(const char* fullMethodDesc, void** args = nullptr, int argCount = 0);

        MonoClass*          FindClassByName(const char* fullName) const;   
        MonoMethod*         FindMethod(MonoClass* klass, const char* name, int argc) const; 
        MonoClassField*     FindField(MonoClass* klass, const char* name) const;            
        void                LogException(MonoObject* exc, const char* prefix) const;

        // For logging/verification
        const char* RuntimeInfo() const;

    private:
        static MonoDomain* m_RootDomain;
        static MonoDomain* m_AppDomain;


        std::vector<MonoAssembly*> m_LoadedAssemblies;
    };

} // namespace Boom
