#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>

int main() {
	int pid, count, child_pid, status;
	printf("- Parent process(PID: %d) is ready.\n", getpid());
	
	// Create 10 Child Processes
	for (count = 0; count < 10; count++) {
		pid = fork();
		
		// Child Process
		if (pid == 0) {
			// Child process
			break;
		}
		// Parent Process
		else if (pid > 0) {
			printf("- Parent process(PID: %d) created #%d Child process(PID: %d)\n", getpid(), count, pid);
		}
		// Error
		else {
			printf("fork() system call failed.\n");
			return -1;
		}
	}
	
	// Child Process
	if (pid == 0) {
		char url[50] = "./child/";
		char idx[10];
		sprintf(idx, "%d", count);
		strcat(url, idx);
		execl(url, idx, NULL);
		sleep(1);
	}
	// Parent Process
	else {
		while (count > 0) {
			child_pid = wait(&status);
			count--;
		}
		printf("- Parent process(PID: %d) finished.\n", getpid());

	}
	
	return 0;
}
