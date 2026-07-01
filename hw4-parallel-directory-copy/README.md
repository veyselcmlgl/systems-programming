# HW4 — Parallel Directory Copy Utility (MWCp)

CSE344 Systems Programming — Homework 4

The original problem statement is included as [`assignment.pdf`](./assignment.pdf), and a full written report (with test scenarios and screenshots) is included as [`report.pdf`](./report.pdf).

## Overview

A multi-threaded directory copying utility written in C, modelled on the **manager-worker (producer-consumer)** pattern. A single manager thread walks the source directory tree and pushes open file descriptor pairs into a bounded circular buffer; a pool of worker threads drains the buffer and copies the file contents in parallel. Execution time and copy statistics are reported on completion.

## Usage

```bash
./MWCp <buffer_size> <num_workers> <source_dir> <destination_dir>
```

Example:
```bash
./MWCp 10 4 ./src ./dst
```

## Design

### Thread Architecture

| Thread | Count | Role |
|--------|-------|------|
| Manager (producer) | 1 | Walks source directory recursively with `opendir`/`readdir`, opens source+destination file pairs, pushes them into the shared buffer |
| Worker (consumer) | N (CLI arg) | Pulls file descriptor pairs from the buffer, copies data in chunks, closes descriptors, updates statistics |

### Shared Buffer

A **circular queue** of fixed size (CLI arg) holds open file descriptor pairs. Access is guarded by:

- `pthread_mutex_t mutex` — protects the buffer itself.
- `pthread_cond_t full` — workers wait on this when the buffer is empty.
- `pthread_cond_t empty` — manager waits on this when the buffer is full.
- `int done` flag — set by the manager when all files have been enqueued; workers drain the buffer and exit.

Additional mutexes protect stdout (`stdout_mutex`), file descriptor allocation (`new_fd_mutex`), and the shared byte counter (`total_bytes_mutex`).

### Key Implementation Details

- **No busy waiting** — all synchronization uses `pthread_cond_wait` / `pthread_cond_broadcast`.
- **Graceful termination** — when `done` is set, the manager broadcasts on both condition variables so all waiting workers wake up and exit cleanly.
- **Existing destination files** are opened and truncated (`O_TRUNC`).
- **`SIGINT` (Ctrl+C)** is handled — the signal handler sets `done` and broadcasts to unblock all threads.
- **Statistics** reported on exit: number of files copied, total bytes transferred, execution time (min:sec.ms).
- Tested with `valgrind` for memory leaks.

## Build & Run

```bash
make        # compiles main.c → ./MWCp
make clean  # removes ./MWCp
```

Requires GCC with POSIX thread support (`-pthread`, `-std=c11`, `-D_DEFAULT_SOURCE`).
