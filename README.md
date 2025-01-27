# Custom Shell Program

This is a simple custom shell implemented in C. It supports basic shell functionalities like executing commands, handling built-in commands, managing background processes, job control, input/output redirection, and pipelining.

## Features

- **Prompt Display**: Displays a user-specific prompt with the current directory, username, and hostname.
- **Built-in Commands**: Implements a few essential built-in commands such as:
  - `pwd` - Prints the current working directory.
  - `cd` - Changes the directory to the specified path.
  - `echo` - Prints arguments to standard output.
  - `exit` - Exits the shell program.
- **Job Control**: Supports job control where users can manage background processes and bring them to the foreground.
- **External Commands**: Supports executing external commands and handles input/output redirection, and piping.
- **Background Execution**: Supports running processes in the background with the `&` symbol.
- **Job Management**: Users can view jobs, kill specific jobs, or terminate all jobs using commands like `jobs`, `kjob`, `fg`, and `overkill`.
- **Piping and Redirection**: Supports multiple piped commands and input/output redirection (`<`, `>`, `>>`).
  
## Structure and Key Components

### Main Functions:
- `display_prompt()`: Displays the shell prompt based on the current working directory, username, and hostname.
- `handle_built_in()`: Handles built-in commands like `pwd`, `cd`, `echo`, and `exit`.
- `show_jobs()`: Displays a list of all current background jobs.
- `kill_job()`: Terminates a specific job by its ID.
- `kill_all_jobs()`: Terminates all background jobs.
- `foreground_job()`: Brings a job to the foreground and waits for it to finish.
- `signal_handler()`: Handles signals for job termination, and removes terminated jobs from the job list.
- `execute_external()`: Handles external commands execution, including background execution, piping, and redirection.
- `execute_command()`: Parses and executes commands input by the user.

### Job Management
The shell tracks background jobs in the `jobs` array. Jobs are stored with their process ID and name. Users can interact with jobs using the following commands:
- `jobs`: Lists all background jobs with their job ID and process ID.
- `kjob <job_id> <signal>`: Sends a specific signal (e.g., `9` for kill) to a job identified by its job ID.
- `fg <job_id>`: Brings a job to the foreground.
- `overkill`: Terminates all running jobs.

### Signal Handling
The shell ignores some signals (e.g., `SIGINT`, `SIGQUIT`, `SIGTSTP`) to maintain control over the terminal. It also handles child process termination to clean up jobs that have finished executing.

### Input/Output Redirection and Piping
- Supports `<`, `>`, and `>>` for input and output redirection.
- Commands can be piped together using the `|` symbol to create command chains.

### Background Processes
Any command can be executed in the background by appending an `&` symbol at the end of the command. Background processes are managed by the shell, and the user can interact with them using job control commands.

## Usage

1. Compile the shell program:
    ```bash
    gcc powershell-mini.c -o shell
    ```

2. Run the shell:
    ```bash
    ./shell
    ```

3. The shell will display the prompt `<username@hostname:directory> $`, and you can input commands.

4. To exit the shell, use the `exit` command.

## Example

```bash
<user@hostname:/home/user> $ cd /tmp
<user@hostname:/tmp> $ echo "Hello, world!"
Hello, world!
<user@hostname:/tmp> $ pwd
/tmp
<user@hostname:/tmp> $ jobs
[1] echo "Hello, world!" [12345]
<user@hostname:/tmp> $ kjob 1 9
