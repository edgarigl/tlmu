/*
 * TLM remoteport sockets
 *
 * Copyright (c) 2013 Xilinx Inc
 * Written by Edgar E. Iglesias
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define _LARGEFILE64_SOURCE
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "remote-port-sk.h"
#include "safeio.h"

#define UNIX_PREFIX "unix:"

static int sk_unix_client(const char *descr)
{
	struct sockaddr_un addr;
	int fd, nfd;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	printf("connect to %s\n", descr + strlen(UNIX_PREFIX));

	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, descr + strlen(UNIX_PREFIX),
		sizeof addr.sun_path);
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) >= 0)
		return fd;

	printf("Failed to connect to %s, attempt to listen\n", addr.sun_path);
	unlink(addr.sun_path);
	/* Failed to connect. Bind, listen and accept.  */
	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		goto fail;

	listen(fd, 5);
	nfd = accept(fd, NULL, NULL);
	close(fd);
	return nfd;
fail:
	close(fd);
	return -1;
}

int sk_open(const char *descr)
{
	int fd = -1;

	if (descr == NULL)
		return -1;

	if (memcmp(UNIX_PREFIX, descr, strlen(UNIX_PREFIX)) == 0) {
		/* UNIX.  */
		fd = sk_unix_client(descr);
		return fd;
	}
	return -1;
}
