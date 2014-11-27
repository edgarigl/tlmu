/*
 *  Safe IO interface.
 * 
 *  Written by Edgar E. Iglesias.
 *
 */

#ifndef _SAFEIO_H_
#define _SAFEIO_H_

ssize_t safe_read(int fd, void *buf, size_t count);
ssize_t safe_write(int fd, const void *buf, size_t count);
ssize_t safe_copyfd(int s, off64_t off, size_t len, int d);

#endif
