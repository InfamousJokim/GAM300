#pragma once
#include "Application/Interface.h"   // for Boom::AppContext forward decl if you don’t want a separate fwd

namespace Boom {
    struct AppContext; 
    // If BoomEngine builds as a DLL, export this symbol so Editor can import it
    void BOOM_API CallTriggerEnterCallbacks(uint64_t triggerEntity, uint64_t otherEntity);
    void BOOM_API CallTriggerExitCallbacks(uint64_t triggerEntity, uint64_t otherEntity);

    void BOOM_API ClearAllTriggerCallbacks();

    void BOOM_API RegisterScriptInternalCalls(AppContext* ctx);
}