#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <queue>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define PROCESS_NUM 10
#define TIME_QUANTUM 20 // 20sec

using namespace std;

// Process
typedef struct {
    pid_t pid;
    float CPU_burst;
    float IO_burst;
    int arrival_time;
} PROCESS;

// PCB
typedef struct {
    bool flag; // CPU burst time remained?
    float burst;
} PCB;

// Message
typedef struct {
    long msg_type;
    PCB pcb;
} MSG;


int main() {

    PROCESS child_process[10];
    pid_t pid;
    int status;
    int switching_num = 0;

    // Create 10 Child Processes
    for (int i = 0; i < PROCESS_NUM; i++) {

        pid = fork();
        child_process[i].pid = pid;
        child_process[i].CPU_burst = 60 + rand() % 30; // random CPU burst time (1min ~ 1.5min)
        child_process[i].IO_burst = rand() % 10 + 1; // random IO burst time (1sec ~ 10sec)

        // Child Process
        if (pid == 0) {
            // Create Message
            int key_id; // Message ID
            long msg_type = getpid(); // Message Type = Child Process PID
            float CPU_burst = child_process[i].CPU_burst;

            MSG msg;
            msg.msg_type = msg_type;
            key_id = msgget((key_t) 1234, IPC_CREAT|0666);

            if (key_id == -1) {
                perror("- msgget() system call failed.\n");
                return -1;
            }

            // Start Child Process
            do {
                // If Parent Process sends Message
                if (msgrcv(key_id, &msg, sizeof(PCB), msg_type, 0) != -1) {
                    if (CPU_burst > TIME_QUANTUM) {
                        CPU_burst -= TIME_QUANTUM;
                        sleep(TIME_QUANTUM);
                        msg.pcb.flag = true; // CPU burst time remained
                        msg.pcb.burst = TIME_QUANTUM;
                        msgsnd(key_id, &msg, sizeof(PCB), IPC_NOWAIT);
                    }
                    else {
                        CPU_burst = 0;
                        sleep(CPU_burst);
                        msg.pcb.flag = false; // CPU burst time not remained
                        msg.pcb.burst = CPU_burst;
                        msgsnd(key_id, &msg, sizeof(PCB), IPC_NOWAIT);
                    }
                }
            } while (CPU_burst > 0);
            exit(0);
        }
    }

    // Create Log File
    FILE *fp = fopen("schedule_dump.txt", "a");

    // Create Queues
    queue<long> runQueue; // Run Queue
    queue<long> waitQueue; // Wait Queue
    queue<long> printQueue; // Queue for print
    float burst_time[PROCESS_NUM];
    float completion_time[PROCESS_NUM];
    float turnaround_time = 0;

    printf("\n[ Initial Run Queue ]\n\n");
    fprintf(fp, "\n[ Initial Run Queue ]\n\n");

    for (int i = 0; i < PROCESS_NUM; i++) {
        runQueue.push(i); // Enqueue
        printf("- Process #%2d | PID: %4d | Burst Time: %.0f\n", i + 1, child_process[i].pid, child_process[i].CPU_burst);
        fprintf(fp, "- Process #%2d | PID: %4d | Burst Time: %.0f\n", i + 1, child_process[i].pid, child_process[i].CPU_burst);
        burst_time[i] = child_process[i].CPU_burst;
    }

    // Create Message
    int key_id; // Message ID
    MSG msg;
    key_id = msgget((key_t) 1234, IPC_CREAT|0666);
    if (key_id == -1) {
        perror("- msgget() system call failed.\n");
        exit(0);
    }

    // Start Parent Process
    printf("\n[ Process Execution ]\n\n");
    fprintf(fp, "\n[ Process Execution ]\n\n");
    do {

        switching_num++;
        printf("Context Switch #%d\n", switching_num);
        fprintf(fp, "Context Switch #%d\n", switching_num);

        // Print Run queue
        printf("- Run Queue: ");
        fprintf(fp, "- Run Queue: ");
        do {
            printf("[%d] ", child_process[runQueue.front()].pid);
            printQueue.push(runQueue.front());
            fprintf(fp, "[%d] ", child_process[runQueue.front()].pid);
            runQueue.pop();
            if (runQueue.size() == 1) {
                printf("[%d]", child_process[runQueue.front()].pid);
                printQueue.push(runQueue.front());
                fprintf(fp, "[%d]", child_process[runQueue.front()].pid);
                runQueue.pop();
            }
        } while (!runQueue.empty());
        do {
            runQueue.push(printQueue.front());
            printQueue.pop();
        } while (!printQueue.empty());

        long run = runQueue.front(); // Next Run Status Process
        runQueue.pop(); // Dequeue

        // Print Running Process Information
        printf("\n- Running Process: Process #%ld [%d] | Remaining Burst Time: %.0f (-> %.0f)\n\n",
               run + 1, child_process[run].pid, child_process[run].CPU_burst, (child_process[run].CPU_burst - TIME_QUANTUM >= 0) ? child_process[run].CPU_burst - TIME_QUANTUM : 0);
        fprintf(fp, "\n- Running Process: Process #%ld [%d] | Remaining Burst Time: %.0f (-> %.0f)\n\n",
                run + 1, child_process[run].pid, child_process[run].CPU_burst, (child_process[run].CPU_burst - TIME_QUANTUM >= 0) ? child_process[run].CPU_burst - TIME_QUANTUM : 0);

        msg.msg_type = child_process[run].pid;
        msgsnd(key_id, &msg, sizeof(PCB), IPC_NOWAIT);

        // If Child Process receives Message
        if (msgrcv(key_id, &msg, sizeof(PCB), child_process[run].pid, 0) != -1) {
            // Enqueue to Run queue
            if (msg.pcb.flag == true) {
                if (child_process[run].CPU_burst > 0) {
                    runQueue.push(run);
                    child_process[run].CPU_burst -= TIME_QUANTUM;
                    turnaround_time += TIME_QUANTUM;
                }
            }
            else {
                turnaround_time += child_process[run].CPU_burst;
                child_process[run].CPU_burst = 0;
                completion_time[run] = turnaround_time;
            }
            if (child_process[run].CPU_burst <= 0) {
                turnaround_time += child_process[run].CPU_burst;
                child_process[run].CPU_burst = 0;
                completion_time[run] = turnaround_time;
            }
        }
    } while(!runQueue.empty());

    msgsnd(key_id, &msg, sizeof(PCB), IPC_NOWAIT);
    wait(&status);

    printf("- Run queue empty, end scheduling.\n");
    fprintf(fp, "- Run queue empty, end scheduling.\n");

    // Print Completion time & Waiting time

    printf("\n[ Round-Robin Scheduling Result ]\n\n");
    printf(" PID\t\tBurst Time\tCompletion Time\t\tWaiting Time\n\n");
    fprintf(fp, "\n\n[ Round-Robin Scheduling Result ]\n\n");
    fprintf(fp, " PID\t\tBurst Time\tCompletion Time\t\tWaiting Time\n\n");

    for (int i = 0; i < PROCESS_NUM; i++) {
        printf(" %d\t\t%.2lf\t\t%.2lf\t\t\t%.2lf\n\n", child_process[i].pid, burst_time[i], completion_time[i], completion_time[i] - burst_time[i]);
        fprintf(fp, " %d\t\t%.2lf\t\t%.2lf\t\t\t%.2lf\n\n", child_process[i].pid, burst_time[i], completion_time[i], completion_time[i] - burst_time[i]);
    }

    fclose(fp);
    return 0;
}
