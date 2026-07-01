# HW3 — Parking Lot Simulation with Semaphores and Shared Memory

CSE344 Systems Programming — Homework 3

The original problem statement is included as [`assignment.pdf`](./assignment.pdf), and a full written report is included as [`report.pdf`](./report.pdf).

## Overview

A multi-threaded C program that simulates a parking lot with two attendants, one for automobiles and one for pickups. Vehicle owners and parking attendants run as POSIX threads and synchronize access to shared temporary parking spots using **POSIX semaphores**. The program demonstrates classic producer-consumer thread synchronization.

## Design

### Thread Model

Four threads are created:

| Thread | Role |
|--------|------|
| `carOwner()` × 2 | Vehicle owner — checks for available temporary spot; parks or leaves |
| `carAttendant(&forAuto)` | Automobile attendant — waits for a new car, moves it to permanent spot |
| `carAttendant(&forPickup)` | Pickup attendant — waits for a new pickup, moves it to permanent spot |

### Semaphores

| Semaphore | Purpose |
|-----------|---------|
| `newAutomobile` | Signals attendant that a new automobile has arrived (init: 0) |
| `inChargeforAutomobile` | Mutual exclusion over automobile temporary slots (init: 1) |
| `newPickup` | Signals attendant that a new pickup has arrived (init: 0) |
| `inChargeforPickup` | Mutual exclusion over pickup temporary slots (init: 1) |

### Shared State

- `mFree_automobile` (max 8) and `mFree_pickup` (max 4) track available temporary spots.
- A random number generator determines vehicle type at the entrance: odd → automobile, even → pickup.
- Only one vehicle enters the system at a time.
- If no temporary spot is available, the owner leaves immediately.

### Synchronization Flow

1. `carOwner()` generates a random vehicle type.
2. Owner acquires the appropriate mutex (`inChargefor*`), checks `mFree_*`, decrements if available, releases mutex, then posts `new*` to wake the attendant.
3. `carAttendant()` waits on `new*`, parks the vehicle in the permanent lot, and updates availability.
4. All semaphores are properly destroyed at program exit.

## Build & Run

```bash
make          # compiles parking_lot.c → ./parking_lot
make run      # builds and runs
make clean    # removes ./parking_lot and object files
```

Requires GCC with POSIX thread support (`-pthread`).
