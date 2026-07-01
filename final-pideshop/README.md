# Final Project — PideShop: Multi-Threaded Food Production & Delivery Server

CSE344 Systems Programming — Final Project

The original problem statement is included as [`assignment.pdf`](./assignment.pdf), and a full written report (with 5+ test cases) is included as [`report.pdf`](./report.pdf).

## Overview

A multi-threaded TCP server (`PideShop`) that simulates a pide restaurant's full order pipeline — from customer order to delivery — and a client generator (`HungryVeryMuch`) that spawns multiple concurrent customers. The system models real-world resource constraints: limited oven slots, limited oven apparatus, cook and delivery thread pools, and order cancellation at any stage.

## Architecture

```
PideShop [port] [numCooks] [numDelivery] [speed_m_per_min]
    ├── Main thread — accepts TCP connections, spawns connection handler per client
    ├── Cook thread pool (N threads) — waits for orders, prepares + cooks meals
    └── Delivery thread pool (M threads) — waits for cooked meals, delivers by distance

HungryVeryMuch [port] [numClients] [townWidth_km] [townHeight_km]
    └── Generates N clients with random positions, each connecting via TCP
```

## Simulation Model

| Resource | Constraint |
|----------|-----------|
| Oven capacity | 6 meals simultaneously |
| Oven apparatus (fırıncı küreği) | 3 (semaphore) — required to place/remove from oven |
| Oven openings | 2 (only used to place/remove, not blocking) |
| Delivery bag capacity | 3 meals per delivery person |
| Town layout | p × q km rectangle, shop at center, customers at random positions |

### Order Pipeline

1. Manager receives order → assigns to an available cook.
2. Cook prepares meal (time ∝ pseudo-inverse of 30×40 complex matrix computation).
3. Cook acquires apparatus semaphore + oven slot, places meal, returns to table.
4. Cook re-acquires oven when meal is ready (cooking time ≈ half preparation time), removes meal.
5. Manager hands meal to available delivery person.
6. Delivery person fills bag (up to 3 meals), departs, delivers by distance at fixed speed, returns.
7. At end of day, manager promotes the most efficient delivery person.

### Order Cancellation

At any pipeline stage (`^C`/`^D` on client side), the order is discarded immediately: preparation stopped, oven slot freed, meal removed from delivery bag.

### Key Synchronization Primitives

| Primitive | Purpose |
|-----------|---------|
| `sem_t oven_sem` | Guards oven capacity (max 6) |
| `sem_t apparatus_sem` | Guards apparatus availability (max 3) |
| `pthread_mutex_t order_mutex` | Protects shared order queue |
| `pthread_mutex_t log_mutex` | Serializes log file writes |
| `pthread_cond_t order_cond` | Wakes cook threads when a new order arrives |
| TCP sockets (`socket`, `bind`, `listen`, `accept`, `connect`) | Client-server communication |

### Logging

All order state transitions (placed, preparing, cooking, ready, out for delivery, delivered, cancelled) are timestamped and written to `pideshop.log`.

## Build & Run

```bash
make                                              # compiles PideShop and HungryVeryMuch
./PideShop <port> <numCooks> <numDelivery> <speed>
./HungryVeryMuch <port> <numClients> <p> <q>
make clean                                        # removes binaries and pideshop.log
```

Example:
```bash
./PideShop 8080 4 6 1        # 4 cooks, 6 delivery staff, 1 m/min speed
./HungryVeryMuch 8080 50 10 20   # 50 clients in a 10×20 km town
```

Requires GCC with pthread and math library (`-pthread -lm`).
