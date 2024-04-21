/* SPDX-License-Identifier: MIT */
#pragma once

void printk(char*, ...);
void panic(char*) __attribute__((noreturn));
void printk_init();
