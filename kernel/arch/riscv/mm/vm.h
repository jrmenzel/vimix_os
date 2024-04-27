/* SPDX-License-Identifier: MIT */
#pragma once

/// Switch h/w page table register to the kernel's page table
/// and enable paging.
void kvm_init_per_cpu();
