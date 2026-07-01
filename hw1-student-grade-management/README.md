# HW1 — Student Grade Management System with Process Creation

CSE344 Systems Programming — Homework 1

The original problem statement is included as [`assignment.pdf`](./assignment.pdf).

## Overview

A command-line student grade management system written in C. Every file operation is handled by a **child process** spawned with `fork()`, while the parent waits with `wait()` for it to complete. A log file (`system.log`) records the PID and outcome of every child process.

## Design

Every command follows the same pattern: the parent calls `fork()`, the child performs the actual file I/O, and the parent waits and writes to the log. This keeps file manipulation isolated in child processes, as required by the assignment.

Key system calls used: `fork()`, `wait()`, `open()` / `fopen()`, `read()` / `write()`, `lseek()`.

## Commands

```bash
./gtu                                      # display usage / all available commands
./gtu grades.txt                           # create the grades file
./gtu addStudentGrade "Name Surname" "AA"  # append a student and grade
./gtu searchStudent "Name Surname"         # find and display a student's grade
./gtu sortAll grades.txt                   # print all entries sorted by name
./gtu showAll grades.txt                   # print all entries
./gtu listGrades grades.txt               # print first 5 entries
./gtu listSome 5 2 grades.txt             # print entries 5–10 (page 2 of size 5)
```

## Logging

Every completed operation is appended to `system.log` in the format:

```
Child process finished. PID: <pid>, <operation description>
```

## Build & Run

```bash
make          # compiles gtuStudentGrades.c → ./gtu
make run      # builds and runs ./gtu (no arguments → usage)
make clean    # removes ./gtu and system.log
```

Requires GCC with C99 support (`-std=c99`).
