# HW5 — Parallel Directory Copy with Condition Variables & Barriers (MWCp v2)

CSE344 Systems Programming — Homework 5

The original problem statement is included as [`assignment.pdf`](./assignment.pdf), and a full written report (with test scenarios and valgrind screenshots) is included as [`report.pdf`](./report.pdf).

## Overview

An enhanced version of the [HW4 parallel directory copy utility](../hw4-parallel-directory-copy). The manager-worker architecture and bounded circular buffer are unchanged; this homework adds **POSIX barriers** (`pthread_barrier_t`) to synchronize all threads at a defined rendezvous point before exiting, and makes **condition variable** usage explicit and complete throughout the buffer synchronization logic.

## Changes from HW4

| Area | HW4 | HW5 |
|------|-----|-----|
| Thread barrier | Not present | `pthread_barrier_init` with count `num_workers + 1`; both manager and each worker call `pthread_barrier_wait` before exit |
| Condition variables | Present but some `cond_wait`/`cond_signal` paths were incomplete | All buffer-empty and buffer-full paths use `pthread_cond_wait` / `pthread_cond_signal` / `pthread_cond_broadcast` correctly |
| stdout logging | Most print statements commented out | All progress messages (directory created, FIFO copied, file bytes transferred, copy started) re-enabled under `stdout_mutex` |
| `_DEFAULT_SOURCE` | Not defined | Added to enable POSIX extensions cleanly |

### Barrier Logic

```
Manager thread                    Worker threads (N)
       |                               |
  enqueue files...              dequeue + copy files...
       |                               |
  set done flag                        |
  pthread_barrier_wait() ←—————— pthread_barrier_wait()
       |                               |
    (all N+1 threads meet here before any exits)
       |                               |
  print statistics               free resources, exit
```

This guarantees that statistics (total bytes, file count, elapsed time) are printed only after every worker has finished copying.

## Build & Run

```bash
make                                        # compiles main.c → ./MWCp
./MWCp <buffer_size> <num_workers> <src> <dst>
make clean                                  # removes ./MWCp
```

Example test scenarios from the assignment:
```bash
valgrind ./MWCp 10 10 ./testdir/src ./tocopy   # memory leak check
./MWCp 10 4 ./testdir/src/lib ./toCopy
./MWCp 10 100 ./testdir ./toCopy
```

Requires GCC with POSIX thread support (`-pthread`, `-std=c11`).
