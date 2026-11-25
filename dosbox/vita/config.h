/* Vita-specific DOSBox 0.74-3 config.h for arm-vita-eabi / newlib
 *
 * Rule: enabled features = #define FEATURE 1
 *       disabled features = leave UNDEFINED (not #define FEATURE 0)
 *       DOSBox uses both #if FEATURE and #if defined(FEATURE) so
 *       defining to 0 would still trigger #if defined() guards.
 */

#ifndef CONFIG_H_VITA
#define CONFIG_H_VITA

/* -------------------------------------------------------------------------
 * Package metadata
 * ------------------------------------------------------------------------- */
#define PACKAGE         "dosbox"
#define PACKAGE_NAME    "DOSBox"
#define PACKAGE_VERSION "0.74-3"
#define VERSION         "0.74-3"
#define PACKAGE_STRING  "DOSBox 0.74-3"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_TARNAME "dosbox"
#define PACKAGE_URL     ""

/* -------------------------------------------------------------------------
 * CPU / recompiler
 * ------------------------------------------------------------------------- */
/* Vita is ARM — not x86, x86_64, PowerPC, or M68k */
#define C_TARGETCPU ARMV4LE	/* selects core_dynrec/risc_armv4le.h */

/* The ARM recompiler (core=dynamic). Its code cache lives in a VM-domain
   memblock (core_dynrec/cache.h); the x86-only dynamic core stays off. */
/* C_DYNAMIC_X86 intentionally left undefined */
#define C_DYNREC 1

/* Use the portable interpreter core */
#define C_CORE_INLINE 1

/* FPU: ARM has a hardware FPU but it is not x86-compatible */
#define C_FPU 1
/* C_FPU_X86 intentionally left undefined */

/* -------------------------------------------------------------------------
 * Optional features — all disabled (leave undefined to be safe)
 * ------------------------------------------------------------------------- */
/* C_DEBUG        — leave undefined */
/* C_HEAVY_DEBUG  — leave undefined */
/* C_OPENGL       — leave undefined */
/* C_SDL_SOUND    — leave undefined (cdrom.h checks defined()) */
/* C_SSHOT        — leave undefined */
/* C_IPX          — leave undefined */
/* C_MODEM        — leave undefined */
/* C_DIRECTSERIAL — leave undefined */
/* C_SET_PRIORITY — leave undefined */
/* C_X11_XKB      — leave undefined */
/* C_HAVE_MPROTECT — leave undefined */

/* ARM is little-endian */
#undef WORDS_BIGENDIAN

/* Unaligned memory: keep off for safety */
/* C_UNALIGNED_MEMORY — leave undefined */

/* -------------------------------------------------------------------------
 * GCC attribute helpers — function-like macros
 * dosbox.h uses GCC_ATTRIBUTE(noreturn); it must expand to __attribute__((x))
 * not to a bare integer.
 * ------------------------------------------------------------------------- */
#define C_HAS_ATTRIBUTE          1
#define C_ATTRIBUTE_ALWAYS_INLINE 1
#define C_HAS_BUILTIN_EXPECT     1
/* C_ATTRIBUTE_FASTCALL is x86-only; leave undefined */

#define GCC_ATTRIBUTE(x)   __attribute__((x))
#define GCC_LIKELY(x)      __builtin_expect(!!(x), 1)
#define GCC_UNLIKELY(x)    __builtin_expect(!!(x), 0)

#define INLINE __inline__

/* -------------------------------------------------------------------------
 * newlib / POSIX headers available on Vita
 * ------------------------------------------------------------------------- */
#define HAVE_INTTYPES_H  1
#define HAVE_STDINT_H    1
#define HAVE_STDLIB_H    1
#define HAVE_STRING_H    1
#define HAVE_STRINGS_H   1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H  1
#define HAVE_MEMORY_H    1
#define HAVE_UNISTD_H    1
#define HAVE_REALPATH    1
/* DIRENT_HAS_D_TYPE — leave undefined; newlib dirent has no d_type */

/* Not available in Vita's newlib — leave undefined */
/* HAVE_PWD_H         — leave undefined */
/* HAVE_ALSA          — leave undefined */
/* HAVE_LIBASOUND     — leave undefined */
/* HAVE_NETINET_IN_H  — leave undefined */
/* HAVE_SYS_SOCKET_H  — leave undefined */
/* HAVE_DDRAW_H       — leave undefined */

/* powf is present in newlib */
/* DB_HAVE_NO_POWF — leave undefined */

/* -------------------------------------------------------------------------
 * sizeof tables for arm-vita-eabi (32-bit ARM)
 * ------------------------------------------------------------------------- */
#define SIZEOF_UNSIGNED_CHAR       1
#define SIZEOF_UNSIGNED_SHORT      2
#define SIZEOF_UNSIGNED_INT        4
#define SIZEOF_UNSIGNED_LONG       4
#define SIZEOF_UNSIGNED_LONG_LONG  8
#define SIZEOF_INT_P               4

/* -------------------------------------------------------------------------
 * DOSBox integer type definitions
 * These normally come from config.h via autoconf substitution.
 * On Vita (32-bit ARM, newlib): int=4, long=4, ptr=4.
 * ------------------------------------------------------------------------- */
typedef double Real64;

typedef unsigned char  Bit8u;
typedef   signed char  Bit8s;

typedef unsigned short Bit16u;
typedef   signed short Bit16s;

typedef unsigned int   Bit32u;
typedef   signed int   Bit32s;

typedef unsigned long long Bit64u;
typedef   signed long long Bit64s;

/* Native pointer-width integer (32-bit on Vita) */
typedef Bit32u Bitu;
typedef Bit32s Bits;

#endif /* CONFIG_H_VITA */
