/*
    glibc compatibility shim for Linux x86_64 builds.

    glibc 2.43 added new symbol versions for some libm functions. A plugin
    built against glibc >= 2.43 then fails to load in hosts that run with an
    older glibc, e.g. Flatpak apps (LMMS, Ardour, Carla...) whose runtime
    ships glibc 2.42:

        libm.so.6: version `GLIBC_2.43' not found (required by libSIDStep2.so)

    The linker is told to redirect these calls (-Wl,--wrap=<name>) to the
    wrappers below, which bind to the original GLIBC_2.2.5 versions that every
    x86_64 glibc provides. Behaviour is identical.
*/

#include <features.h> /* defines __GLIBC__ */

#if defined(__linux__) && defined(__x86_64__) && defined(__GLIBC__)

float sidstep2_compat_sqrtf (float);
float sidstep2_compat_atan2f (float, float);

__asm__ (".symver sidstep2_compat_sqrtf, sqrtf@GLIBC_2.2.5");
__asm__ (".symver sidstep2_compat_atan2f, atan2f@GLIBC_2.2.5");

float __wrap_sqrtf (float x)           { return sidstep2_compat_sqrtf (x); }
float __wrap_atan2f (float y, float x) { return sidstep2_compat_atan2f (y, x); }

#else

/* Not glibc (e.g. musl): no symbol versions to pin, but the linker still
   redirects calls here, so forward them to the normal functions. */
float __real_sqrtf (float);
float __real_atan2f (float, float);

float __wrap_sqrtf (float x)           { return __real_sqrtf (x); }
float __wrap_atan2f (float y, float x) { return __real_atan2f (y, x); }

#endif
