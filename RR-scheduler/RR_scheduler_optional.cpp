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
    pid_t pid; // Child PID
    float CPU_burst;
    float IO_burst;
} PCB;

// Message
typedef struct {
    long msg_type;
    PCB pcb;
} MSG;


int main() {

    PROCESS child_process[PROCESS_NUM];
    pid_t parentPID = getpid(); // Parent PID
    pid_t pid;
    int status;
    int switching_num = 0;

    // Create 10 Child Processes
    for (int i = 0; i < PROCESS_NUM; i++) {

        pid = fork();
        child_process[i].pid = pid;
        child_process[i].CPU_burst = 60 + rand() % 30; // random CPU burst time (1min ~ 1.5min)
        child_process[i].IO_burst = 20 + rand() % 10; // random IO burst time (20sec ~ 30sec)


        /* Child Process */

        if (pid == 0) {
            // Create Message
            int key_id; // Message ID
            long msg_type = getpid(); // Message Type = Child Process PID
            float CPU_burst = child_process[i].CPU_burst;
            float IO_burst = child_process[i].IO_burst;

            MSG msg;
            msg.msg_type = msg_type;
            key_id = msgget((key_t) 1234, IPC_CREAT|0666);

            if (key_id == -1) {
                perror("- msgget() system call failed.\n");
                return -1;
            }

            do {
                // If Parent Process sends Message
                if (msgrcv(key_id, &msg, sizeof(PCB), msg_type, 0) != -1) {
                    if (CPU_burst > TIME_QUANTUM) {
                        CPU_burst -= TIME_QUANTUM;
                        sleep(TIME_QUANTUM);
                        msg.pcb.flag = true; // CPU burst time remained
                        msg.pcb.CPU_burst = TIME_QUANTUM;
                        msg.pcb.IO_burst = IO_burst;
                        msg.pcb.pid = msg_type;
                        msgsnd(key_id, &msg, sizeof(PCB), IPC_NOWAIT);
                    }
                    else {
                        CPU_burst = 0;
                        sleep(CPU_burst);
                        msg.pcb.flag = false; // CPU burst time not remained
                        msg.pcb.CPU_burst = CPU_burst;
                        msg.pcb.IO_burst = IO_burst;
                        msg.pcb.pid = msg_type;
                        msgsnd(key_id, &msg, sizeof(PCB), IPC_NOWAIT);
                    }
                }
            } while (CPU_burst > 0);
            return 0;
        }
    }

    // Create Log File
    FILE *fp = fopen("schedule_dump.txt", "a");

    // Create Queues
    queue<long> runQueue; // Run Queue
    queue<long> waitQueue; // Wait Queue
    queue<long> printRunQueue; // Queue for print
    queue<long> printWaitQueue; // Queue for print

    float CPU_burst_time[PROCESS_NUM];
    float IO_burst_time[PROCESS_NUM];
    float completion_time[PROCESS_NUM];
    float turnaround_time = 0;

    printf("\n[ Initial Run Queue ]\n\n");
    fprintf(fp, "\n[ Initial Run Queue ]\n\n");

    // Enqueue all the Processes to Run Queue
    for (int i = 0; i < PROCESS_NUM; i++) {
        runQueue.push(i);
        printf("- Process #%d | PID: %4d | CPU Burst Time: %.0f | I/O Burst Time: %.0f\n", i, child_process[i].pid, child_process[i].CPU_burst, child_process[i].IO_burst);
        fprintf(fp, "- Process #%d | PID: %4d | CPU Burst Time: %.0f | I/O Burst Time: %.0f\n", i, child_process[i].pid, child_process[i].CPU_burst, child_process[i].IO_burst);
        CPU_burst_time[i] = child_process[i].CPU_burst;
        IO_burst_time[i] = child_process[i].IO_burst;
    }


    /* Parent Process */

    // Create Message
    int key_id; // Message ID
    MSG msg;
    key_id = msgget((key_t) 1234, IPC_CREAT|0666);
    if (key_id == -1) {
        perror("- msgget() system call failed.\n");
        return 0;
    }

    printf("\n\n[ Process Execution ]\n");
    fprintf(fp, "\n\n[ Process Execution ]\n");

    do {
        switching_num++;
        printf("\nContext Switch #%d\n", switching_num);
        fprintf(fp, "\nContext Switch #%d\n", switching_num);

        // Print Run Queue
        printf("- Run Queue: ");
        fprintf(fp, "- Run Queue: ");
        do {
            printf("[%d] ", child_process[runQueue.front()].pid);
            fprintf(fp, "[%d] ", child_process[runQueue.front()].pid);
            printRunQueue.push(runQueue.front());
            runQueue.pop();
            if (runQueue.size() == 1) {
                printf("[%d]", child_process[runQueue.front()].pid);
                fprintf(fp, "[%d]", child_process[runQueue.front()].pid);
                printRunQueue.push(runQueue.front());
                runQueue.pop();
            }
        } while (!runQueue.empty());
        do {
            runQueue.push(printRunQueue.front());
            printRunQueue.pop();
        } while (!printRunQueue.empty());

        long run = runQueue.front();
        runQueue.pop();

        // Print Running Process Information
        printf("\n- Running Process: [%d] | Remaining CPU Burst Time: %.0f (-> %.0f) | Remaining I/O Burst Time: %.0f (-> %.0f)\n",
               child_process[run].pid, child_process[run].CPU_burst, (child_process[run].CPU_burst - TIME_QUANTUM >= 0) ? child_process[run].CPU_burst - TIME_QUANTUM : 0, child_process[run].IO_burst, (child_process[run].IO_burst - TIME_QUANTUM >= 0) ? child_process[run].IO_burst - TIME_QUANTUM : 0);
        fprintf(fp, "\n- Running Process: [%d] | Remaining CPU Burst Time: %.0f (-> %.0f) | Remaining I/O Burst Time: %.0f (-> %.0f)\n",
                child_process[run].pid, child_process[run].CPU_burst, (child_process[run].CPU_burst - TIME_QUANTUM >= 0) ? child_process[run].CPU_burst - TIME_QUANTUM : 0, child_process[run].IO_burst, (child_process[run].IO_burst - TIME_QUANTUM >= 0) ? child_process[run].IO_burst - TIME_QUANTUM : 0);

        // Send Message to Child Process
        msg.msg_type = child_process[run].pid;
        msgsnd(key_id, &msg, sizeof(PCB), IPC_NOWAIT);

        // If Child Process receives Message
        if (msgrcv(key_id, &msg, sizeof(PCB), child_process[run].pid, 0) != -1) {

            // Enqueue the Current Process to Run Queue
            if (msg.pcb.flag) {
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

                // Send Message to Parent Process to start I/O
                printf("- [%d] CPU burst reaches to zero, do I/O.\n", child_process[run].pid);
                fprintf(fp, "- [%d] CPU burst reaches to zero, do I/O.\n", child_process[run].pid);
                msg.msg_type = parentPID;
                msg.pcb.IO_burst = child_process[run].IO_burst;
                msg.pcb.pid = child_process[run].pid;
                msgsnd(key_id, &msg, sizeof(PCB), IPC_NOWAIT);

                // If Parent Process receives Message
                if (msgrcv(key_id, &msg, sizeof(PCB), parentPID, 0) != -1) {

                    // If Current Child Process has I/O burst value
                    if (msg.pcb.IO_burst > 0) {
                        waitQueue.push(run);
                    }

                    // Decrease I/O Burst value & Print Wait Queue
                    printf("- Wait Queue: ");
                    fprintf(fp, "- Wait Queue: ");

                    do {
                        printf("[%d] ", child_process[waitQueue.front()].pid);
                        fprintf(fp, "[%d] ", child_process[waitQueue.front()].pid);
                        printWaitQueue.push(waitQueue.front());
                        waitQueue.pop();
                        if (waitQueue.size() == 1) {
                            printf("[%d]", child_process[waitQueue.front()].pid);
                            fprintf(fp, "[%d]", child_process[waitQueue.front()].pid);
                            printWaitQueue.push(waitQueue.front());
                            waitQueue.pop();
                        }
                    } while (!waitQueue.empty());
                    do {
                        child_process[printWaitQueue.front()].IO_burst -= TIME_QUANTUM;
                        if (child_process[printWaitQueue.front()].IO_burst <= 0) {
                            child_process[printWaitQueue.front()].IO_burst = 0;
                            printf("\n- [%d] I/O burst reaches to zero, end process.", child_process[printWaitQueue.front()].pid);
                            fprintf(fp, "\n- [%d] I/O burst reaches to zero, end process.", child_process[printWaitQueue.front()].pid);
                        }
                        else {
                            waitQueue.push(printWaitQueue.front());
                        }
                        printWaitQueue.pop();
                    } while (!printWaitQueue.empty());

                    printf("\n");
                    fprintf(fp, "\n");
                }
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

    printf("\n- Run queue empty, end scheduling.\n");
    fprintf(fp, "\n- Run queue empty, end scheduling.\n");

    // Print Completion time & Waiting time
    float sum_completion = 0;
    float min_completion = 123456789.0;
    float max_completion = 0.0;
    float sum_wait = 0;
    float min_wait = 123456789.0;
    float max_wait = 0.0;

    printf("\n\n[ Round-Robin Scheduling Result ]\n\n");
    fprintf(fp, "\n\n[ Round-Robin Scheduling Result ]\n\n");

    printf("--------------------------------------------------------------------------------------------------------\n\n");
    fprintf(fp, "--------------------------------------------------------------------------------------------------------\n\n");
    printf(" PID \t\t CPU Burst Time \t I/O Burst Time \t Completion Time \t Waiting Time\n\n");
    fprintf(fp, " PID \t\t CPU Burst Time \t I/O Burst Time \t Completion Time \t Waiting Time\n\n");
    printf("--------------------------------------------------------------------------------------------------------\n\n");
    fprintf(fp, "--------------------------------------------------------------------------------------------------------\n\n");

    for (int i = 0; i < PROCESS_NUM; i++) {
        sum_completion += completion_time[i];
        min_completion = min(min_completion, completion_time[i]);
        max_completion = max(max_completion, completion_time[i]);
        sum_wait += completion_time[i] - CPU_burst_time[i];
        min_wait = min(min_wait, completion_time[i] - CPU_burst_time[i]);
        max_wait = max(max_wait, completion_time[i] - CPU_burst_time[i]);
        printf(" %d \t\t %.2lf \t\t\t %.2lf \t\t\t %.2lf \t\t %.2lf\n\n", child_process[i].pid, CPU_burst_time[i], IO_burst_time[i], completion_time[i], completion_time[i] - CPU_burst_time[i]);
        fprintf(fp, " %d \t\t %.2lf \t\t\t %.2lf \t\t\t %.2lf \t\t %.2lf\n\n", child_process[i].pid, CPU_burst_time[i], IO_burst_time[i], completion_time[i], completion_time[i] - CPU_burst_time[i]);
    }
    printf("--------------------------------------------------------------------------------------------------------\n\n");
    fprintf(fp, "--------------------------------------------------------------------------------------------------------\n\n");
    printf("- Average Completion Time: %.2f\n", sum_completion / PROCESS_NUM);
    printf("- Minimum Completion Time: %.2f\n", min_completion);
    printf("- Maximum Completion Time: %.2f\n\n", max_completion);
    printf("- Average Waiting Time: %.2f\n", sum_wait / PROCESS_NUM);
    printf("- Minimum Waiting Time: %.2f\n", min_wait);
    printf("- Maximum Waiting Time: %.2f\n\n", max_wait);
    fprintf(fp, "- Average Completion Time: %.2f\n", sum_completion / PROCESS_NUM);
    fprintf(fp, "- Minimum Completion Time: %.2f\n", min_completion);
    fprintf(fp, "- Maximum Completion Time: %.2f\n\n", max_completion);
    fprintf(fp, "- Average Waiting Time: %.2f\n", sum_wait / PROCESS_NUM);
    fprintf(fp, "- Minimum Waiting Time: %.2f\n", min_wait);
    fprintf(fp, "- Maximum Waiting Time: %.2f\n\n", max_wait);

    fclose(fp);

    return 0;
}
