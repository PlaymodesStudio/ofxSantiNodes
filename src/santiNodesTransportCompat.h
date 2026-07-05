#pragma once

// Auto-enable global transport support only when the feature branch header exists.
// This keeps transport-dependent nodes out of the build on standard ofxOceanode branches.
#if !defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT) && defined(__has_include)
#if __has_include("ofxOceanodeTransport.h")
#define OFX_OCEANODE_HAS_GLOBAL_TRANSPORT 1
#endif
#endif
