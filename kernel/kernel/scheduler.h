/* SPDX-License-Identifier: MIT */
/*
 * Based on xv6.
 */

#pragma once

/// Per-CPU process scheduler.
/// Each CPU calls scheduler() after setting itself up.
/// Scheduler never returns.  It loops, doing:
///  - choose a process to run.
///  - swtch to start running that process.
///  - eventually that process transfers control
///    via swtch back to the scheduler.
void scheduler(void) __attribute__((noreturn));
