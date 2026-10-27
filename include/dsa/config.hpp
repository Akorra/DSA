#pragma once

#define DSA_VERSION_MAJOR 0
#define DSA_VERSION_MINOR 1
#define DSA_VERSION_PATCH 0
#define DSA_VERSION (DSA_VERSION_MAJOR * 10000 + DSA_VERSION_MINOR * 100 + DSA_VERSION_PATCH)

#ifndef DSA_ENABLE_ASSERTS        // user didn't choose (e.g. no -DDSA_ENABLE_ASSERTS=...)
#  ifdef NDEBUG
#    define DSA_ENABLE_ASSERTS 0  // release default
#  else
#    define DSA_ENABLE_ASSERTS 1  // debug default
#  endif
#endif