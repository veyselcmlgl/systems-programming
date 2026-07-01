# Midterm — neHos: Concurrent File Access Server & Client

CSE344 Systems Programming — Midterm Project

The original problem statement is included as [`assignment.pdf`](./assignment.pdf), and a full written report is included as [`report.pdf`](./report.pdf).

## Overview

A concurrent file server (`neHosServer`) and a command-line client (`neHosClient`) written in C. Multiple clients connect to the server simultaneously; the server forks a child process for each connected client and serves file operations concurrently. Communication between server and clients uses **named pipes (FIFOs)** and **POSIX shared memory**; a **POSIX semaphore** guards the client queue. The project demonstrates a complete multi-process, multi-IPC-mechanism system.

## Architecture

```
neHosServer <dirname> <maxClients>
    ├── Main server process — manages client queue, listens on server FIFO
    └── Child process (fork per client) — serves one client's requests
            ↕ per-client FIFOs (reader + writer)
neHosClient <connect|tryConnect> <ServerPID>
```

### IPC Mechanisms

| Mechanism | Purpose |
|-----------|---------|
| Named FIFOs (`mkfifo`) | Server ↔ client bidirectional communication; one FIFO pair per client (`/tmp/fifo_client_reader.<pid>`, `/tmp/fifo_client_writer.<pid>`) |
| POSIX shared memory (`shmget`, `shmat`) | Shared `child_comm` struct between server and its forked children for queue management |
| POSIX semaphore (`sem_t`) | Guards the client queue — controls how many clients can connect simultaneously |
| Signals (`SIGINT`, `SIGCHLD`) | `sigaction`-based handlers; server broadcasts `SIGINT` to all child processes on kill request |

### Client Commands

| Command | Description |
|---------|-------------|
| `help` | Display available commands |
| `list` | List files in the server's directory |
| `readF <file> [line]` | Read a specific line or the whole file |
| `writeT <file> [line] <string>` | Write to a specific line or append to file |
| `upload <file>` | Upload a file from client's CWD to the server |
| `download <file>` | Download a file from the server |
| `archServer <name>.tar` | Archive all server files using `fork` + `exec` + `tar` |
| `killServer` | Send a kill request to the server |
| `quit` | Write to server log and disconnect |

### Connection Modes

- **`connect`** — waits in queue if the server is full until a slot is available.
- **`tryConnect`** — leaves immediately if the queue is full.

## Build & Run

```bash
make              # compiles neHosServer and neHosClient
make clean        # removes binaries and FIFOs

# Start the server
./neHosServer <dirname> <maxClients>

# Connect a client (in a separate terminal)
./neHosClient connect <ServerPID>
./neHosClient tryConnect <ServerPID>
```

Requires GCC with C11 support (`-std=c11`).
