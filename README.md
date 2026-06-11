# Operating-System-Zombie-Reaper-Kernel-Module

## Learning Objectives

This project demonstrates core operating system concepts including process management, synchronization, shared-memory communication, and concurrent execution within the Linux kernel.

## Skills Demonstrated

- C Programming
- Linux Kernel Development
- Kernel Threads (kthreads)
- Process Management
- Producer–Consumer Synchronization
- Semaphores
- Circular Buffers
- Concurrency and Multithreading
- Operating Systems

A Linux kernel module that implements the classic **producer–consumer** synchronization
problem using kernel threads and semaphores. A single **producer** thread scans the
process table for zombie processes belonging to a given UID and places them into a
shared circular buffer. One or more **consumer** threads pull zombies from the buffer
and reap them by sending `SIGKILL` to their parent process.

## Project Layout

```
.
├── producer_consumer.c   # Kernel module source
└── Makefile
```

## Requirements

- A Linux machine or VM (kernel modules cannot be built/loaded on macOS/Windows directly)
- Kernel headers matching your running kernel
- `gcc`, `make`

Install build tools and headers (Debian/Ubuntu example):

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```

## Important: Fix the Makefile's `KDIR`

The Makefile points to:

```makefile
KDIR := /usr/src/linux
```

On most distros the correct path is your installed headers directory:

```makefile
KDIR := /lib/modules/$(shell uname -r)/build
```

Edit the Makefile and change that line before building.

## Building

Build just the kernel module:

```bash
make module
```

This produces `producer_consumer.ko`.

Clean build artifacts:

```bash
make clean
```

## Loading the Module

The module accepts four parameters:

| Parameter | Type | Description |
|-----------|------|-------------|
| `size` | int | Size of the shared circular buffer (1–500) |
| `prod` | int | Number of producer threads (must be 0 or 1) |
| `cons` | int | Number of consumer threads (1–100) |
| `uid`  | int | UID whose zombie processes will be scanned/reaped |

```bash
sudo insmod producer_consumer.ko size=10 prod=1 cons=2 uid=$(id -u)
```

If the parameters don't satisfy validation (`size` 1–500, `prod` 0–1, `cons` 1–100),
the module loads but no threads are started — check `dmesg`.

## Verifying It's Running

```bash
dmesg | tail -n 50
```

Expected log lines:

```
###[thread_init_module]###CSE330 Project Kernel Module Inserted
###[thread_init_module]###Kernel module received the following inputs: UID:1000, Buffer-Size:10, No of Producer:1, No of Consumer:2
[Producer-1] has produced a zombie process with pid 1234 and parent pid 1000
[Consumer-1] has consumed a zombie process with pid 1234 and parent pid 1000
```

Confirm the module is loaded:

```bash
lsmod | grep producer_consumer
```

## Generating Zombie Processes for Testing

Run a process under the target UID that forks children without `wait()`ing on them.
A quick example:

```bash
bash -c 'sleep 100 & exec sleep 0.1' &
```

## Unloading the Module

```bash
sudo rmmod producer_consumer
```

On unload, the module signals waiting threads, joins all kernel threads with
`kthread_stop()`, and logs a removal message to `dmesg`.

## How It Works

- **Buffer:** A fixed-size circular array of `struct task_struct *`, protected by
  three semaphores: `empty` (free slots), `full` (filled slots), and `mutex`
  (binary lock for buffer access).
- **Producer thread:** Iterates over all processes (`for_each_process`), filters
  by `uid` and zombie state (`EXIT_ZOMBIE`), skips duplicates, and inserts new
  zombie task pointers into the buffer. Sleeps 250ms between scans.
- **Consumer thread(s):** Wait for an item, remove it from the buffer, reap the
  zombie by sending `SIGKILL` to its parent, and release the reference taken by
  the producer (`put_task_struct`).
- **Thread naming:** `name_threads()` generates names like `Producer-1`,
  `Consumer-1`, `Consumer-2`, based on the `prod`/`cons` counts.

## Troubleshooting

- **`make: *** /usr/src/linux: No such file or directory`** — fix `KDIR` as above.
- **`insmod: Operation not permitted`** — run with `sudo`; check Secure Boot/module
  signing isn't blocking unsigned modules.
- **No output in `dmesg`** — verify `size`/`prod`/`cons` pass validation.
- **Module won't unload** — ensure `buffSize > 0` so the exit cleanup path runs.

## Author

**Babacar Diallo**  
Computer Systems Engineering, Arizona State University  
CSE330 Operating Systems — Process Management Project
