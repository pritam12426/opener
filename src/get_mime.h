#ifndef _GET_MIME_H_
#define _GET_MIME_H_


#include <stddef.h>

/*
 * Detect the MIME type of a file using the `file` command.
 *
 * path      - path to the file
 * mime      - output buffer
 * mime_size - size of the output buffer
 *
 * Returns 0 on success, -1 on failure.
 */
int get_mime_type(const char *path, char *mime, size_t mime_size);


#endif  // _GET_MIME_H_
