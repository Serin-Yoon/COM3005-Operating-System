#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
	printf("\t- #%s Child process(PID: %d) working.\n", argv[0], getpid());
	printf("\t- #%s Child process(PID: %d) finished.\n", argv[0], getpid());
	return 0;
}
