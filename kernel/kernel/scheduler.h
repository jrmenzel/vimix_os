/* SPDX-License-Identifier: MIT */
/*
 * Based on xv6.
 */

#pragma once

/// Per-CPU process scheduler.
/// Each CPU calls scheduler() after setting itself up.
/// Scheduler never returns.  It loops, doing:
///  - choose a process to run.
///  - context_switch to start running that process.
///  - eventually that process transfers control
///    via context_switch() back to the scheduler (via a
///    timer interrupt, the process does not have to cooperate)
void scheduler() __attribute__((noreturn));
