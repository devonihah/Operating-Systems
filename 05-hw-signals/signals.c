#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<errno.h>

#define MAXPIDLEN 10

int foo;
int block;

// 1: prints 1, sleeps for 4, prints 2
void sig_handler1(int signum) {
	printf("1\n"); fflush(stdout);
	sleep(4);
	printf("2\n"); fflush(stdout);
}

// 2: prints 8, kills sigint, sleeps for 4, prints 9
void sig_handler2(int signum) {
	printf("8\n"); fflush(stdout);
	kill(getpid(), SIGINT);
	sleep(4);
	printf("9\n"); fflush(stdout);
}

// 3: prints the value of foo
void sig_handler3(int signum) {
	//sigact.sa_handler = sighandler3;
	//sigaction(SIGTERM, &sigact, NULL);
	printf("%d\n", foo); fflush(stdout);
}

// 4: prints 6 if foo is greater than 0
void sig_handler4(int signum) {
	if (foo > 0) {
		foo = 6;
	}
}

// 5: forks and sets foo equal to fork, if it's the child it exits after seven
void sig_handler5(int signum) {
	foo = fork();
	if (foo == 0) {
		exit(7);
	}
}

// 6: waits for any child process to change state or check statuses
// if no child has exited it returns 0, otherwise it returns errno
// ECHILD = 10 which is no child processes  EINTR = 4 interrupted system ca;;
void sig_handler6(int signum) {
	int pid, status;
	pid = waitpid(-1, &status, WNOHANG);
	if (pid < 0) {
		printf("%d\n", errno); fflush(stdout);
	}
}

// 7: if block is greater than 0, make it 0, otherwise make it 1
void sig_handler7(int signum) {
	if (block) {
		block = 0;
	} else {
		block = 1;
	}
}

// 8: when the program receives a sigterm signal, it terminates and restarts any
// system calls automatically that were interrupted
void sig_handler8(int signum) {
	struct sigaction sigact;

	sigact.sa_flags = SA_RESTART;
	sigact.sa_handler = SIG_DFL;
	sigaction(SIGTERM, &sigact, NULL);
}

// 9: wait for the termination of a child process, then print exit status of the child
void sig_handler9(int signum) {
	int status;
	waitpid(-1, &status, 0);
	printf("%d\n", WEXITSTATUS(status)); fflush(stdout);
}

void install_sig_handlers() {
	struct sigaction sigact;

	// zero out flags
	sigact.sa_flags = SA_RESTART;

	sigact.sa_handler = sig_handler1;
	sigaction(SIGHUP, &sigact, NULL);

	sigact.sa_handler = sig_handler1;
	sigaction(SIGINT, &sigact, NULL);

	sigact.sa_handler = sig_handler2;
	sigaction(SIGQUIT, &sigact, NULL);

	sigact.sa_handler = sig_handler3;
	sigaction(SIGTERM, &sigact, NULL);

	// SIGUSR1 and SIGUSR2
	sigact.sa_handler = sig_handler4;
	sigaction(30, &sigact, NULL);

	sigact.sa_handler = sig_handler5;
	sigaction(10, &sigact, NULL);

	sigact.sa_handler = sig_handler6;
	sigaction(16, &sigact, NULL);

	sigact.sa_handler = sig_handler7;
	sigaction(31, &sigact, NULL);

	sigact.sa_handler = sig_handler8;
	sigaction(12, &sigact, NULL);

	sigact.sa_handler = sig_handler9;
	sigaction(SIGCHLD, &sigact, NULL);
}

void sleep_block_loop() {
	int i;
	sigset_t mask;

	for (i = 0; i < 20; i++) {
		sigemptyset(&mask);
		if (block) {
			sigaddset(&mask, SIGINT);
			sigaddset(&mask, SIGCHLD);
		}
		sigprocmask(SIG_SETMASK, &mask, NULL);
		sleep(1);
	}
	printf("25\n"); fflush(stdout);
	exit(0);
}

void start_killer(int pid, char *cmd, char *scenario_num) {
	char *args[4];
	char *env[] = { NULL };
	char pidstr[32];

	sprintf(pidstr, "%d", pid);
	args[0] = cmd;
	args[1] = scenario_num;
	args[2] = pidstr;
	args[3] = NULL;
	if (execve(args[0], &args[0], env) < 0) {
		perror("execve");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	int pid;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s <killer> [0-9]\n", argv[0]);
		exit(1);
	}

	foo = -1;
	block = 0;

	install_sig_handlers();

	pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	}

	if (pid > 0) {
		start_killer(pid, argv[1], argv[2]);
	} else {
		sleep_block_loop();
	}
}
