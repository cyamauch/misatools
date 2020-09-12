#ifndef __TEST_SIMD_H
#define __TEST_SIMD_H 1


#if defined(USE_SIMD) && defined(__SSE2__)
#if (defined(__GNUC__) && __GNUC__ >= 4) || defined(__INTEL_COMPILER)
#define _SSE2_IS_OK 1
// #warning "enabled sse2"
#endif
#endif

#if defined(USE_SIMD) && defined(__SSE3__)
#if (defined(__GNUC__) && __GNUC__ >= 4) || defined(__INTEL_COMPILER)
#define _SSE3_IS_OK 1
// #warning "enabled sse3"
#endif
#endif

#if defined(USE_SIMD) && defined(__SSSE3__)
#if (defined(__GNUC__) && __GNUC__ >= 4) || defined(__INTEL_COMPILER)
#define _SSSE3_IS_OK 1
// #warning "enabled ssse3"
#endif
#endif


#endif
