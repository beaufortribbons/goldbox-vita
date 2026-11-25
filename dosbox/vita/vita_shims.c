/* Stubs for POSIX functions not available in Vita newlib */

#include <stdarg.h>
#include <stdlib.h>

/* execlp: used in sdlmain.cpp to launch an external editor/file manager.
   On Vita there is no shell or subprocess support; stub returns failure. */
int execlp(const char *file, const char *arg0, ...) {
    (void)file; (void)arg0;
    return -1;
}
