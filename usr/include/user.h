/* SPDX-License-Identifier: MIT */
#pragma once

struct stat;

// system calls
int fork();
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int execv(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid();
char* sbrk(int);
int sleep(int);
int uptime();

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void* memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
uint32_t strlen(const char*);
void* memset(void*, int, uint32_t);
void* malloc(uint32_t);
void free(void*);
int atoi(const char*);
int memcmp(const void*, const void*, uint32_t);
void* memcpy(void*, const void*, uint32_t);
