#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include "kernel.h"

#undef errno
extern int errno;

void _exit() {
    asm("cli\n .loop: hlt\n jmp .loop");
}

int close(int file) {
  return -1;
}

char *__env[1] = { 0 };
char **environ = __env;

int execve(char *name, char **argv, char **env) {
  errno = ENOMEM;
  return -1;
}

int fork(void) {
  errno = EAGAIN;
  return -1;
}

int fstat(int file, struct stat *st) {
  st->st_mode = S_IFCHR;
  return 0;
}

int getpid(void) {
  return 1;
}

int isatty(int file) {
  return 1;
}

int kill(int pid, int sig) {
  errno = EINVAL;
  return -1;
}

int link(char *old, char *new) {
  errno = EMLINK;
  return -1;
}

int lseek(int file, int ptr, int dir) {
  return 0;
}

int open(const char *name, int flags, int mode) {
  return -1;
}

int read(int file, char *ptr, int len) {
  return 0;
}

void *sbrk(int incr) {
    static uint32_t size = 0, rest = 0;
    uint32_t allocmin = incr - rest, allocp = (allocmin / pagealloc_kernel->unit) + ((allocmin % pagealloc_kernel->unit) ? 1 : 0), 
        alloc = allocp * pagealloc_kernel->unit;
    if(pagealloc_kernel->alloc(pagealloc_kernel, (void*)0xE0000000 + size, allocp) != (void*)0xE0000000 + size) {
        errno = ENOMEM;
        return 0;
    }
    uint32_t ret = size - rest + 0xE0000000;
    size += alloc;
    rest += alloc;
    rest -= incr;
    return (void*)ret;
}

int stat(const char *file, struct stat *st) {
  st->st_mode = S_IFCHR;
  return 0;
}

int unlink(char *name) {
  errno = ENOENT;
  return -1; 
}

int wait(int *status) {
  errno = ECHILD;
  return -1;
}

int write(int file, char *ptr, int len) {
  for (int todo = 0; todo < len; todo++) {
    inout_kernel->out->ch(inout_kernel->out, ptr[todo]);
  }
  return len;
}