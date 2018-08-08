/*
 * 9p utilities
 *
 * Copyright IBM, Corp. 2017
 *
 * Authors:
 *  Greg Kurz <groug@kaod.org>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/xattr.h"
#include "9p-util.h"

int relative_openat_nofollow(int dirfd, const char *path, int flags,
                             mode_t mode)
{
    int fd;

    fd = dup(dirfd);
    if (fd == -1) {
        return -1;
    }

    fprintf(stderr, "relative_openat_nowfollow() dirfd=%d path=%s\n", dirfd, path);

    while (*path) {
        const char *c;
        int next_fd;
        char *head;

        /* Only relative paths without consecutive slashes */
        assert(path[0] != '/');

        head = g_strdup(path);
        c = strchr(path, '/');
        if (c) {
            head[c - path] = 0;
            next_fd = openat_dir(fd, head);
	    fprintf(stderr, "calling openat_dir() fn=%s --> %d\n", head, next_fd);
        } else {
            next_fd = openat_file(fd, head, flags, mode);
	    fprintf(stderr, "calling openat_file() fn=%s --> %d\n", head, next_fd);
        }
        g_free(head);
        if (next_fd == -1) {
            close_preserve_errno(fd);
            return -1;
        }
        close(fd);
        fd = next_fd;

        if (!c) {
            break;
        }
        path = c + 1;
    }

    fprintf(stderr, "relative_openat_nowfollow() final fd=%d\n", fd);
    return fd;
}

ssize_t fgetxattrat_nofollow(int dirfd, const char *filename, const char *name,
                             void *value, size_t size)
{
    char *proc_path = g_strdup_printf("/proc/self/fd/%d/%s", dirfd, filename);
    int ret;

    ret = lgetxattr(proc_path, name, value, size);
    g_free(proc_path);
    return ret;
}
