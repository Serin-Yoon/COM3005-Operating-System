## Operating System using C/C++

### 1. Process Creating
1) `gcc -o 0.c` ~ `gcc -o 9.c` in child directory
2) `gcc -o main main.c` `./main` in parent directory

### 2. Round-Robin Scheduler
- **Scheduler with CPU burst time**: `g++ -o main RR_scheduler.cpp` `./main` => `schedule_dump.txt`
- **Scheduler with CPU, I/O burst time**: `g++ -o main RR_scheduler_optional.cpp` `./main` => `schedule_dump_2.txt`
