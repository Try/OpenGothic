// nt5compat.h — Windows API compatibility shim for NT5 dmsynth source files
//
// Allows NT5 dmsynth source files (from tongzx/nt5src
// XPSP1/NT/multimedia/directx/dmusic/dmsynth/) to compile on Linux / macOS /
// any cross-compilation target without the Windows SDK or _X86_ assembly.
//
// Usage:
//   Place this header in game/dmusic/nt5/ alongside the NT5 source files.
//   It is included automatically when OPENGOTHIC_WITH_NT5_DMSYNTH is defined
//   (CMake adds game/dmusic/nt5/ to the include path).
//
// Notes:
//   - _X86_ is intentionally NOT defined here, so the portable C++ fallback
//     paths in mix.cpp / mixf.cpp are compiled instead of the _asm{} blocks.
//   - COM interfaces (IUnknown, IDirectMusic*) are stubbed to void*.
//   - CRITICAL_SECTION is a no-op since we are single-threaded from the
//     game audio callback.
//   - MulDiv uses 64-bit arithmetic matching the NT5 reference exactly.
//
// Required NT5 source files (place next to this header):
//   voice.cpp   — CVoiceEG, CVoiceLFO, CVoiceFilter, CDigitalAudio
//   mix.cpp     — Mix16() 20.12 interpolation inner loop
//   mixf.cpp    — Mix16Filtered() all-pole IIR inner loop
//
// Corresponding NT5 headers that must also be present:
//   synth.h, voice.h, dmsynth.h (from the dmsynth/ subtree)

#pragma once

// ── Do NOT define _X86_ ── the NT5 source files use
//   #ifndef _X86_  ... portable C++ ...  #endif
// to select between assembly and C++ implementations.

// ─────────────────────────────────────────────────────────────────────────────
// Standard C++ headers needed by NT5 source files
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Windows fundamental integer types
// ─────────────────────────────────────────────────────────────────────────────
using BOOL      = int;
using BYTE      = uint8_t;
using WORD      = uint16_t;
using DWORD     = uint32_t;
using LONG      = int32_t;
using ULONG     = uint32_t;
using LONGLONG  = int64_t;
using ULONGLONG = uint64_t;
using UINT      = unsigned int;
using INT       = int;
using SHORT     = int16_t;
using USHORT    = uint16_t;
using WCHAR     = wchar_t;
using CHAR      = char;
using FLOAT     = float;
using DOUBLE    = double;

using LPVOID    = void*;
using LPCVOID   = const void*;
using LPBYTE    = uint8_t*;
using LPDWORD   = uint32_t*;
using LPLONG    = int32_t*;
using LPSTR     = char*;
using LPCSTR    = const char*;
using LPWSTR    = wchar_t*;
using LPCWSTR   = const wchar_t*;

// ─────────────────────────────────────────────────────────────────────────────
// Windows HRESULT / status codes
// ─────────────────────────────────────────────────────────────────────────────
using HRESULT = int32_t;
static constexpr HRESULT S_OK              = 0;
static constexpr HRESULT S_FALSE           = 1;
static constexpr HRESULT E_FAIL            = static_cast<HRESULT>(0x80004005);
static constexpr HRESULT E_OUTOFMEMORY     = static_cast<HRESULT>(0x8007000E);
static constexpr HRESULT E_INVALIDARG      = static_cast<HRESULT>(0x80070057);
static constexpr HRESULT E_NOTIMPL         = static_cast<HRESULT>(0x80004001);
static constexpr HRESULT DMUS_E_BADINSTRUMENT = static_cast<HRESULT>(0x80041000);

inline bool SUCCEEDED(HRESULT hr) { return hr >= 0; }
inline bool FAILED   (HRESULT hr) { return hr <  0; }

// ─────────────────────────────────────────────────────────────────────────────
// Boolean constants
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL  nullptr
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MulDiv — NT5 fixed-point helper: MulDiv(a, b, c) = (int64_t)a * b / c
// Used extensively in voice.cpp for Q2.30 coefficient arithmetic.
// Exact match to NT5 kernel32.dll MulDiv semantics (rounds to nearest).
// ─────────────────────────────────────────────────────────────────────────────
inline LONG MulDiv(LONG nNumber, LONG nNumerator, LONG nDenominator) {
    if(nDenominator == 0) return -1;          // undefined, match Win32 error
    int64_t result = static_cast<int64_t>(nNumber) * nNumerator;
    // Round to nearest (Win32 MulDiv rounds half-away-from-zero)
    const int64_t denom = nDenominator;
    const int64_t half  = (denom > 0) ? (denom / 2) : -((-denom) / 2);
    if((result >= 0) == (denom > 0))
        result += half;
    else
        result -= half;
    return static_cast<LONG>(result / denom);
    }

// ─────────────────────────────────────────────────────────────────────────────
// GUID — 128-bit globally unique identifier
// ─────────────────────────────────────────────────────────────────────────────
struct GUID {
    DWORD Data1;
    WORD  Data2;
    WORD  Data3;
    BYTE  Data4[8];
    };
using REFGUID    = const GUID&;
using REFIID     = const GUID&;
using REFCLSID   = const GUID&;
inline bool IsEqualGUID(REFGUID a, REFGUID b) {
    return std::memcmp(&a, &b, sizeof(GUID)) == 0;
    }
#define DEFINE_GUID(name, l, w1, w2, b1,b2,b3,b4,b5,b6,b7,b8) \
    static constexpr GUID name = {l, w1, w2, {b1,b2,b3,b4,b5,b6,b7,b8}}

// ─────────────────────────────────────────────────────────────────────────────
// COM stub — IUnknown / basic COM plumbing
// NT5 dmsynth classes inherit from IUnknown but we do not use COM dispatch.
// ─────────────────────────────────────────────────────────────────────────────
struct IUnknown {
    virtual HRESULT QueryInterface(REFIID /*riid*/, void** /*ppv*/) { return E_NOTIMPL; }
    virtual ULONG   AddRef()  { return 1; }
    virtual ULONG   Release() { return 1; }
    virtual ~IUnknown() = default;
    };
using LPUNKNOWN = IUnknown*;
using REFIID_t  = REFIID;

// ─────────────────────────────────────────────────────────────────────────────
// CRITICAL_SECTION — no-op (audio callback is single-threaded in our use)
// ─────────────────────────────────────────────────────────────────────────────
struct CRITICAL_SECTION { int _dummy = 0; };
inline void InitializeCriticalSection(CRITICAL_SECTION*) {}
inline void DeleteCriticalSection    (CRITICAL_SECTION*) {}
inline void EnterCriticalSection     (CRITICAL_SECTION*) {}
inline void LeaveCriticalSection     (CRITICAL_SECTION*) {}

// ─────────────────────────────────────────────────────────────────────────────
// Memory allocation helpers (NT5 uses HeapAlloc / new interchangeably)
// ─────────────────────────────────────────────────────────────────────────────
#define DECLARE_MEMALLOC_NEW_DELETE()  /* no-op — use standard new/delete */

// ─────────────────────────────────────────────────────────────────────────────
// Common macros used in NT5 sources
// ─────────────────────────────────────────────────────────────────────────────
#ifndef ASSERT
#  define ASSERT(x) assert(x)
#endif

#ifndef SIZEOF_ARRAY
#  define SIZEOF_ARRAY(a) (sizeof(a)/sizeof((a)[0]))
#endif

// dB ↔ linear amplitude (NT5 uses __min/__max from <algorithm>)
#ifndef __min
#  define __min(a,b) (std::min((a),(b)))
#endif
#ifndef __max
#  define __max(a,b) (std::max((a),(b)))
#endif

// ─────────────────────────────────────────────────────────────────────────────
// NT5 dmsynth-specific type aliases (from synth.h)
// ─────────────────────────────────────────────────────────────────────────────
// These match the typedefs in the NT5 source exactly so that voice.cpp, mix.cpp
// etc. compile without modification when this header is included first.
using PFRACT = int32_t;   // 20.12 fixed-point pitch:  4096 = 1 sample/step
using VFRACT = int32_t;   // Volume fraction:          4095 = 0 dB
using COEFF  = int32_t;   // Q2.30 filter coefficient: 1<<30 = 1.0
using STIME  = int64_t;   // Absolute time in samples
using PREL   = int32_t;   // Pitch cents (relative)
using VREL   = int32_t;   // Volume 1/100 dB (0 = 0 dB, −9600 = −96 dB)

// ─────────────────────────────────────────────────────────────────────────────
// WAVEFORMATEX — standard Windows PCM format descriptor
// ─────────────────────────────────────────────────────────────────────────────
struct WAVEFORMATEX {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
    };

// ─────────────────────────────────────────────────────────────────────────────
// Disabled warnings that NT5 source commonly triggers
// ─────────────────────────────────────────────────────────────────────────────
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsign-compare"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wshadow"
#endif
