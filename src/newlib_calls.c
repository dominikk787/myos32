#include "arch/io.h"
#include "kernel.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

#undef errno
extern int errno;

#define UNUSED(x) (void)(x)

void _exit(void) { asm("cli\n .loop: hlt\n jmp .loop"); }

int close(int file) {
    printf("close %u\n", file);
    UNUSED(file);
    return -1;
}

char *__env[1] = {0};
char **environ = __env;

int execve(char *name, char **argv, char **env) {
    UNUSED(name);
    UNUSED(argv);
    UNUSED(env);
    errno = ENOMEM;
    return -1;
}

int fork(void) {
    errno = EAGAIN;
    return -1;
}

int fstat(int file, struct stat *st) {
    printf("fstat %u\n", file);
    UNUSED(file);
    UNUSED(st);
    st->st_mode = S_IFCHR;
    return 0;
}

int getpid(void) { return 1; }

int isatty(int file) {
    printf("isatty %u\n", file);
    UNUSED(file);
    if(file <= 2) return 1;
    return 1;
}

int kill(int pid, int sig) {
    UNUSED(pid);
    UNUSED(sig);
    errno = EINVAL;
    return -1;
}

int link(char *old, char *new) {
    UNUSED(old);
    UNUSED(new);
    errno = EMLINK;
    return -1;
}

int lseek(int file, int ptr, int dir) {
    printf("lseek %u\n", file);
    UNUSED(file);
    UNUSED(ptr);
    UNUSED(dir);
    return 0;
}

int open(const char *name, int flags, int mode) {
    printf("open %s\n", name);
    UNUSED(name);
    UNUSED(flags);
    UNUSED(mode);
    return -1;
}

int read(int file, char *ptr, int len) {
    printf("read %u\n", file);
    UNUSED(file);
    UNUSED(ptr);
    UNUSED(len);
    return 0;
}

void *sbrk(unsigned int incr) {
    static uint32_t size = 0, rest = 0;
    uint32_t allocmin = incr - rest,
             allocp = (allocmin / pagealloc_kernel->unit)
                      + ((allocmin % pagealloc_kernel->unit) ? 1 : 0),
             alloc = allocp * pagealloc_kernel->unit;
    if(pagealloc_kernel->alloc(pagealloc_kernel, (void *)(0xE0000000 + size),
                               allocp)
       != (void *)(0xE0000000 + size)) {
        errno = ENOMEM;
        return 0;
    }
    uint32_t ret = size - rest + 0xE0000000;
    size += alloc;
    rest += alloc;
    rest -= incr;
    return (void *)ret;
}

int stat(const char *file, struct stat *st) {
    printf("stat %s\n", file);
    UNUSED(file);
    UNUSED(st);
    st->st_mode = S_IFCHR;
    return 0;
}

int unlink(char *name) {
    UNUSED(name);
    errno = ENOENT;
    return -1;
}

int wait(int *status) {
    UNUSED(status);
    errno = ECHILD;
    return -1;
}

int write(int file, char *ptr, int len) {
    if(file == 1 || file == 2) {
        for(int i = 0; i < len; i++) {
            write_serial(ptr[i]);
            if(inout_kernel) inout_kernel->out->ch(inout_kernel->out, ptr[i]);
            if(ptr[i] == '\n') write_serial('\r');
        }
    } else
        printf("write %u\n", file);
    return len;
}