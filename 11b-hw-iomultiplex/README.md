# I/O Multiplexing

The purpose of this assignment is to give you hands-on experience with
I/O multiplexing.  Code is provided for an echo server that listens for clients
to connect over a TCP connection (socket of type `SOCK_STREAM`) and echoes back
their messages, one line at a time, until they disconnect.  The server is
single-threaded and uses I/O multiplexing with epoll, nonblocking sockets, and
edge-triggered monitoring to accomplish this.


# Maintain Your Repository

 Before beginning:
 - [Mirror the class repository](../01a-hw-private-repo-mirror), if you haven't
   already.
 - [Merge upstream changes](../01a-hw-private-repo-mirror#update-your-mirrored-repository-from-the-upstream)
   into your private repository.

 As you complete the assignment:
 - [Commit changes to your private repository](../01a-hw-private-repo-mirror#commit-and-push-local-changes-to-your-private-repo).


# Preparation

 1. Read the following in preparation for this assignment:

    - Sections ?? and ?? in the book

    Additionally, man pages for the following are also referenced throughout the
    assignment:

    - ??

 2. Run `make` to build the server `echoservere`.

 3. Start a tmux session with five panes open.  You are welcome to arrange them
    however you want, but it might be easiest to do it something like this:

    ```
    -------------------------------------
    | client 1  | client 2  | client 3  |
    -------------------------------------
    |             server                |
    -------------------------------------
    |            analysis               |
    -------------------------------------
    ```


# I/O Multiplexing

Start the `echoservere` server in the "server" pane using the following command line:

(Replace "port" with a port of your choosing, an integer between 1024 and
65535.  Use of ports with values less than 1023 require root privileges).

```bash
./echoservere port
```

In each of the three "client" panes run the following:

(Replace "port" with the port on which the server program is now listening.)

```bash
nc localhost port
```

"localhost" is a domain name that always refers to the local system.  This is
used in the case where client and server are running on the same system.

After all three are running, type some text in the first of the three "client"
panes, and press enter.  Repeat with the second and third "client" panes, _in
that order_.  In the "analysis" pane run the following:

```bash
ps -Lo user,pid,ppid,nlwp,lwp,state,ucmd -C echoservere | grep ^$(whoami)\\\|USER
```

The `ps` command lists information about processes that currently exist on the
system.  The `-L` option tells us to show threads ("lightweight processes") as
if they were processes.  The `-o` option is used to show only the following
fields:

 - user: the username of the user running the process
 - pid: the process ID of the process
 - ppid: the process ID of the parent process
 - nlwp: the number of threads (light-weight processes) being run
 - lwp: the thread ID of the thread
 - state: the state of the process, e.g., Running, Sleep, Zombie
 - ucmd: the command executed.

While some past homework assignments required you to use the `ps` command without
a pipeline (e.g., to send its output grep), `ps` has the shortcoming that it
can't simultaneously filter by both command name and user, so the above command
line is a workaround.

 1. Show the output from the `ps` command.

 2. From the `ps` output, how many (unique) processes are running and why?
    Use the PID and LWP to identify different threads or processes.

 3. From the `ps` output, how many (unique) threads are running with each
    process and why?  Use the PID and LWP to identify different threads or
    processes.

Stop the server by using `ctrl`+`c` in the appropriate pane.

Answer the following questions about the epoll-based concurrency.  Use the man
pages for `read(2)` and `recv(2)` and/or the `echoservere.c` code to help you
answer.

 4. What does it mean when `read()` or `recv()` returns a value greater than 0
    on a blocking or nonblocking socket?

 5. What does it mean when `read()` or `recv()` returns 0 on a blocking or
    nonblocking socket?

 6. What does it mean when `read()` or `recv()` returns a value less than 0 on
    a blocking socket?

 7. What does it mean when `read()` or `recv()` returns a value less than 0 on
    a nonblocking socket?

 8. Which values of `errno` have a special meaning for nonblocking sockets when
    `read()` or `recv()` returns a value less than 0?
