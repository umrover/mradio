// no filesystem, all stubs

#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mmhal_os.h"

int _open(const char *path, int flags, ...)
{
    (void)path;
    (void)flags;
    errno = ENOSYS;
    return -1;
}

int _close(int fd)
{
    (void)fd;
    errno = EBADF;
    return -1;
}

int _read(int fd, char *buf, int len)
{
    (void)fd;
    (void)buf;
    (void)len;
    errno = EBADF;
    return -1;
}

int _write(int fd, const char *buf, int len)
{
    if ((fd != STDOUT_FILENO && fd != STDERR_FILENO) || len < 0)
    {
        errno = EBADF;
        return -1;
    }
    mmhal_log_write((const uint8_t *)buf, (size_t)len);
    return len;
}

int _lseek(int fd, int offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return -1;
}

int _fstat(int fd, struct stat *st)
{
    (void)fd;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int fd)
{
    return fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO;
}
