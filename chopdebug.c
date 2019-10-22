#include "chopdebug.h"

// errno is preserved through this function,
// it will not change between this function calling and returning.
void debug_print(const char *format, const char *function, ...) {
  // check valid arguments
  if (format == NULL) {
    return;
  }

  // saving errno
  int errsav = errno;

  // capturing variable argument list
  va_list args;
  va_start(args, format);

  // printing argument
  dprintf(debug_fd, debug_header_ext, function);
  vdprintf(debug_fd, format, args);
  dprintf(debug_fd, "\n");

  // in case errno is nonzero
  if (errsav > 0) {
    dprintf(debug_fd, debug_header_err, errsav);
    dprintf(debug_fd, "%s", strerror(errsav));
    dprintf(debug_fd, "\n");
  }

  // cleaning up
  va_end(args);
  errno = errsav;
  return;
}
