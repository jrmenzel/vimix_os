/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/riscv.h>
#include <kernel/kernel.h>

typedef uint64 pte_t;
typedef uint64* pagetable_t;  // 512 PTEs

void kvm_init();
void kvm_init_per_cpu();
void kvm_map_or_panic(pagetable_t, uint64, uint64, uint64, int);
int kvm_map(pagetable_t, uint64, uint64, uint64, int);
pagetable_t uvm_create();
uint64 uvm_alloc(pagetable_t, uint64, uint64, int);
uint64 uvm_dealloc(pagetable_t, uint64, uint64);
int uvm_copy(pagetable_t, pagetable_t, uint64);
void uvm_free(pagetable_t, uint64);
void uvm_unmap(pagetable_t, uint64, uint64, int);
void uvm_clear(pagetable_t, uint64);
pte_t* vm_walk(pagetable_t, uint64, int);
uint64 uvm_get_physical_addr(pagetable_t, uint64);
int uvm_copy_out(pagetable_t, uint64, char*, uint64);
int uvm_copy_in(pagetable_t, char*, uint64, uint64);
int uvm_copy_in_str(pagetable_t, char*, uint64, uint64);
