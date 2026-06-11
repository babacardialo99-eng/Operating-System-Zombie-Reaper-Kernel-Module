# Operating-System-Zombie-Reaper-Kernel-Module
Linux kernel module implementing the Producer–Consumer problem using kernel threads, semaphores, and a circular buffer. Producer threads scan for zombie processes owned by a specified UID and add them to the buffer, while consumer threads process entries and signal the corresponding parent processes.
