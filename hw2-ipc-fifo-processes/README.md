# HW2 — Two-Process IPC with Named Pipes (FIFOs)

CSE344 Systems Programming — Homework 2

The original problem statement is included as [`assignment.pdf`](./assignment.pdf), and a full written report is included as [`report.pdf`](./report.pdf).

## Overview

A C program that demonstrates inter-process communication (IPC) between two child processes using **named pipes (FIFOs)**. The parent spawns two children, each assigned to a FIFO; the children perform arithmetic operations on a shared array and communicate results through the pipes. The parent manages child lifecycle using `SIGCHLD` signal handling and `waitpid()`.

## Design

### Process Structure

```
Parent
├── Child 1 (FIFO1) — reads array → computes sum → writes result to FIFO2
└── Child 2 (FIFO2) — reads "multiply" command + array → computes product → prints final result
```

### Key System Calls & Mechanisms

- **`mkfifo()`** — creates two named pipes (`myfifo1`, `myfifo2`) for bidirectional data flow between children.
- **`fork()`** — spawns two child processes; each sleeps 10 seconds before executing its task.
- **`SIGCHLD` + `waitpid(WNOHANG)`** — the parent installs a signal handler that reaps each terminated child non-blockingly; a counter tracks how many children have exited and triggers program exit when all are done.
- **`open()` / `read()` / `write()`** — all pipe I/O uses low-level file descriptors.

### Data Flow

1. Parent initializes an integer array and sends it to both FIFOs, along with a `"multiply"` command string to FIFO2.
2. Child 1 reads the array from FIFO1, computes the **sum**, and writes the result to FIFO2.
3. Child 2 reads the `"multiply"` command and the array from FIFO2, computes the **product**, and prints the combined result (sum + product).
4. Parent loops, printing `"proceeding"` every 2 seconds until both children have exited.

### Zombie Protection (Bonus)

`SIGCHLD` is handled with `WNOHANG` in a loop so that all terminated children are reaped immediately, preventing zombie processes from accumulating.

## Build & Run

```bash
make          # compiles main.c → ./gtu
./gtu <n>     # run with an integer argument (array size)
make clean    # removes ./gtu, myfifo1, myfifo2
```

Requires GCC with C11 support (`-std=c11`).
