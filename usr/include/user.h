/* SPDX-License-Identifier: MIT */
#pragma once

#include <sys/types.h>

struct stat;

// system calls
pid_t fork();
void exit(int status) __attribute__((noreturn));
int wait(int *);
int pipe(int pipe_descriptors[2]);
ssize_t write(int fd, const void *buffer, size_t n);
ssize_t read(int fd, void *buffer, size_t n);
int close(int fd);
int kill(int);
int execv(const char *, char **);
int open(const char *, int);
int mknod(const char *path, short, short);
int unlink(const char *pathname);
int fstat(int fd, struct stat *);
int link(const char *from, const char *to);
int mkdir(const char *);
int chdir(const char *path);
int dup(int fd);
pid_t getpid();
void *sbrk(intptr_t increment);
int sleep(int seconds);
int uptime();

// ulib.c
int stat(const char *path, struct stat *buffer);
char *strcpy(char *, const char *);
void *memmove(void *, const void *, int);
char *strchr(const char *, char c);
int strcmp(const char *, const char *);
void fprintf(int, const char *, ...);
void printf(const char *, ...);
char *gets(char *, int max);
uint32_t strlen(const char *);
void *memset(void *, int, uint32_t);
void *malloc(size_t size);
void free(void *ptr);
int atoi(const char *);
int32_t memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *, const void *, uint32_t);
