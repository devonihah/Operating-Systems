#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include<string.h>

int main(int argc, char *argv[]) {
	int pid;

	printf("Starting program; process has pid %d\n", getpid());
	FILE *file = fopen("fork-output.txt", "w");
	int fd = fileno(file);
	fprintf(file, "BEFORE FORK(%d)\n", fd);
	fflush(file);

	int pipe_fds [2];
	pipe(pipe_fds);

	if ((pid = fork()) < 0) {
		fprintf(stderr, "Could not fork()");
		exit(1);
	}

	/* BEGIN SECTION A */

	printf("Section A;  pid %d\n", getpid());
	//sleep(5);

	/* END SECTION A */
	if (pid == 0) {
		/* BEGIN SECTION B */
		printf("Section B\n");
		fprintf(file, "SECTION B (%d)\n", fd);
		close(pipe_fds[0]);
		sleep(10);
		char *comment = "hello from Section B\n";
		write(pipe_fds[1], comment, strlen(comment));
		close(pipe_fds[1]);
		sleep(10);
		//sleep(30);
		//sleep(30);
		//printf("Section B done sleeping\n");

		char *newenviron[] = { NULL };

		printf("Program \"%s\" has pid %d. Sleeping.\n", argv[0], getpid());
		//sleep(30);

		if (argc <= 1) {
			printf("No program to exec.  Exiting...\n");
			exit(0);
		}

		printf("Running exec of \"%s\"\n", argv[1]);
		execve(argv[1], &argv[1], newenviron);
		dup2(fd, pipe_fds[1]);
		printf("End of program \"%s\".\n", argv[0]);

		exit(0);

		/* END SECTION B */
	} else {
		/* BEGIN SECTION C */

		printf("Section C\n");
		fprintf(file, "SECTION C (%d)\n", fd);
		fclose(file);

		close(pipe_fds[1]);
		char buffer[1024];
		
		ssize_t bytes_read = read(pipe_fds[0], buffer, 1024);
		buffer[bytes_read] = '\0';
		printf("%ld\n", bytes_read);
		printf("%s", buffer);
		bytes_read = read(pipe_fds[0], buffer, 1024);
		printf("%ld\n", bytes_read);
		close(pipe_fds[0]);
		//sleep(5);
		// wait(NULL);
		//sleep(30);
		//printf("Section C done sleeping\n");
		exit(0);

		/* END SECTION C */
	}
	/* BEGIN SECTION D */

	printf("Section D\n");
	//sleep(30);

	/* END SECTION D */
}

