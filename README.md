# Systems Programming — CSE344

A collection of homework assignments completed for the CSE344 Systems Programming course at Gebze Technical University. Each subdirectory is a self-contained assignment with its own README, build instructions, and source code.

## Assignments

| # | Name | Topic | Language |
|---|------|-------|----------|
| 1 | [Student Grade Management System with Process Creation](./hw1-student-grade-management) | `fork()`, `wait()`, child processes, file I/O, logging | C |
| 2 | [Two-Process IPC with Named Pipes (FIFOs)](./hw2-ipc-fifo-processes) | `mkfifo()`, `fork()`, `SIGCHLD`, `waitpid()`, named pipes, IPC | C |
| 3 | [Parking Lot Simulation with Semaphores](./hw3-semaphore-parking-lot) | POSIX threads (`pthread`), semaphores (`sem_t`), producer-consumer synchronization | C |
| 4 | [Parallel Directory Copy Utility (MWCp)](./hw4-parallel-directory-copy) | Manager-worker pattern, bounded buffer, `pthread_mutex`, `pthread_cond`, `opendir`/`readdir` | C |
| MT | [Concurrent File Access Server & Client (neHos)](./midterm-concurrent-file-server) | Multi-process server, named FIFOs, POSIX shared memory, semaphores, `fork`+`exec`+`tar`, signal handling | C |
| 5 | [Parallel Directory Copy with Barriers (MWCp v2)](./hw5-parallel-copy-barrier) | `pthread_barrier_t`, condition variables (`pthread_cond`), manager-worker pattern (extended from HW4) | C |
| Final | [PideShop: Multi-Threaded Food Production & Delivery Server](./final-pideshop) | TCP sockets, thread pools, semaphores, mutex, condition variables, order cancellation, delivery simulation | C |

## Notes

- Each assignment folder includes a `README.md` describing the problem, the design approach, and how to build and run the code.
- Course materials and grading rubrics beyond the problem statements are excluded; only original implementation work is included.
