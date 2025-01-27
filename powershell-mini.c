#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wait.h>

#define CUR_DIR 200
#define USER_NAME 200
#define HOST_NAME 200
#define MAX_FILE 200
#define MAX_TOKENS 200
#define COMMAND_MAX 200

int bg_flag, last_cmd, total_jobs = 0;
pid_t shell_pgid;
static int shell_term = 0;

typedef struct job{
    char name[COMMAND_MAX];
    pid_t pid;
};

job jobs[2000];

void display_prompt() {
    char curr_dir[CUR_DIR + 1];
    char user[USER_NAME + 1];
    char host[HOST_NAME + 1];

    getlogin_r(user, sizeof(user));
    gethostname(host, sizeof(host));
    getcwd(curr_dir, sizeof(curr_dir));

    char *tokens[MAX_TOKENS], *token;
    int token_count = 0, i;
    for (token = strtok(curr_dir, "/"); token; token = strtok(NULL, "/"))
        tokens[token_count++] = token;
    if (token_count == 1)
        tokens[1] = " ";

    if (!strcmp(tokens[0], "home") && !strcmp(tokens[1], user)) {
        char dir[MAX_DIR + 1] = "~", sep[2] = "/";
        for (i = 2; i != token_count; i++) {
            strcat(dir, sep);
            strcat(dir, tokens[i]);
        }
        printf("<%s@%s:%s> $ ", user, host, dir);
    } else {
        char dir[MAX_DIR + 1] = "", sep[2] = "/";
        for (i = 0; i != token_count; i++) {
            strcat(dir, sep);
            strcat(dir, tokens[i]);
        }
        if (!strcmp(dir, ""))
            strcat(dir, sep);
        printf("<%s@%s:%s> $", user, host, dir);
    }
}

int handle_built_in(char *args[MAX_TOKENS + 1], int argc) {
    if (!strcmp(args[0],"pwd")){
        char *cwd[MAX_CWD_NAME+1];
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
    }
    else if (!strcmp(args[0], "cd")) {
        if (argc == 1) {
            chdir(getenv("HOME"));
        } else {
            if (args[1][0] == '~' && (args[1][1] == '/' || args[1][1] == '\0')) {
                chdir(getenv("HOME"));
                args[1][0] = '.';
            }
            chdir(args[1]);
        }
    }
    else if (!strcmp(args[0],"echo")){
        if (nargs==1) return 10;
        for (int i=1; i<nargs; i++){
            printf("%s ", args[i]);
            printf("\n");
        }
    }
    else if (!strcmp(args[0],"exit")){
        exit(0);

    }
    return 10;
}

void show_jobs() {
    int i;
    for (i = 0; i < total_jobs; i++)
        printf("[%d] %s [%d]\n", i + 1, jobs[i].name, jobs[i].pid);
}

void kill_job(char **args, int argc) {
    if (argc > 3) {
        fprintf(stderr, "error: too many arguments\n");
        return;
    }
    else if (argc < 3) {
        fprintf(stderr, "error: too few arguments\n");
        return;
    }
    else {
        int job_id = (int)args[1][0] - 1 - '0';
        if (job_id >= total_jobs) {
            fprintf(stderr, "error: job id invalid\n");
            return;
        }
        int sig = (int)args[2][0] - '0';
        pid_t job_pid = jobs[job_id].pid;
        printf("%d %d\n", job_pid, sig);
        kill(job_pid, sig);
    }
}

void kill_all_jobs() {
    int i;
    for (i = 0; i < total_jobs; i++)
        kill(jobs[i].pid, 9);
}

void foreground_job(char **args, int argc) {
    if (argc > 2) {
        fprintf(stderr, "error: too many arguments\n");
        return;
    }
    else if (argc < 2) {
        fprintf(stderr, "error: too few arguments\n");
        return;
    }
    else {
        int i, job_id = (int)args[1][0] - 1 - '0', job_pid = jobs[job_id].pid;
        char job_name[MAX_CMD];
        strcpy(job_name, jobs[job_id].name);
        if (job_id >= total_jobs) {
            fprintf(stderr, "error: job id invalid\n");
            return;
        }
        for (i = job_id; i < total_jobs - 1; i++)
            jobs[i] = jobs[i + 1];
        total_jobs--; 
        tcsetpgrp(shell_term, getpgid(job_pid));
        kill(job_pid, SIGCONT);
        int status;
        waitpid(job_pid, &status, WUNTRACED);
        if (WIFSTOPPED(status)) {
            tcsetpgrp(shell_term, shell_pgid);
            strcpy(jobs[total_jobs].name, job_name);
            jobs[total_jobs].pid = job_pid;
            total_jobs++;
        }
        tcsetpgrp(shell_term, shell_pgid);
    }
}

void signal_handler(int signo) {
    int pid, status;
    while (1) {
        pid = waitpid(WAIT_ANY, &status, WNOHANG);
        if (pid > 0) {
            int i, j;
            for (i = 0; i < total_jobs; i++) {
                if (jobs[i].pid == pid) {
                    fprintf(stderr, "%s with pid %d exited normally\n", jobs[i].name, pid);
                    for (j = i; j < total_jobs - 1; j++)
                        jobs[i] = jobs[i + 1];
                    total_jobs--;
                    break;
                }
            }
            break;
        }
        else if (pid == 0)
            break;
        else if (pid < 0) {
            break;
        }
    }
    return;
}

void execute_external(char *args[MAX_TOKENS + 1], int argc, int cmd_num, int in_fd, int out_fd) {
    bg_flag = 0;

    if (!strcmp(args[argc - 1], "&")) {
        bg_flag = 1;
        args[argc - 1] = NULL;
        argc--;
    }
    if (args[argc - 1][strlen(args[argc - 1]) - 1] == '&') {
        bg_flag = 1;
        char *temp = (char *)malloc(strlen(args[argc - 1]) * sizeof(char));
        strncpy(temp, args[argc - 1], strlen(args[argc - 1]) - 1);
        strcpy(args[argc - 1], temp);
        args[argc - 1][strlen(args[argc - 1]) - 1] == '.';
        argc--;
    }

    int i, j, out_file = 0, in_file = 0, append = 0;
    char input_file[MAX_FILE] = "", output_file[MAX_FILE] = "";
    char temp[2];
    
    for (i = 0; i < argc; i++) {
        for (j = 0; j < strlen(args[i]); j++) {
            if (out_file) {
                temp[0] = args[i][j];
                temp[1] = '\0';
                strcat(output_file, temp);
            }
            if (args[i][j] == '>') {
                if (strlen(args[i]) > 1)
                    if (args[i][j + 1] == '>') 
                        append = 1;
                out_file = (j) ? (2) : (1);
                args[i][j] = '\0';
            }
        }
        if (out_file == 2) out_file = 1;
        else if (out_file == 1) args[i] = NULL;
    }

    for (i = 0; i < argc && args[i] != NULL; i++) {
        for (j = 0; j < strlen(args[i]); j++) {
            if (in_file) {
                temp[0] = args[i][j];
                temp[1] = '\0';
                strcat(input_file, temp);
            }
            if (args[i][j] == '<') {
                in_file = (j) ? (2) : (1);
                args[i][j] = '\0';
            }
        }
        if (in_file == 2) in_file = 1;
        else if (in_file == 1) args[i] = NULL;
    }

    pid_t pid;
    signal(SIGCHLD, signal_handler);
    switch (pid = fork()) {
        case -1:
            perror("Could not create child process");
            exit(-1);
        case 0:
            if (bg_flag) {
                setpgid(0, 0);
            }
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            if (cmd_num != last_cmd) {
                if (in_fd != 0) {
                    dup2(in_fd, 0);
                    close(in_fd);
                }
                if (out_fd != 1) {
                    dup2(out_fd, 1);
                    close(out_fd);
                }
            }
            if (in_file) {
                int in_fd = open(input_file, O_RDONLY);
                dup2(in_fd, 0);
                close(in_fd);
            }
            if (out_file) {
                int out_fd;
                if (append) {
                    out_fd = open(output_file, O_RDONLY | O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
                } else {
                    out_fd = open(output_file, O_RDONLY | O_WRONLY | O_CREAT, S_IRWXU);
                }
                dup2(out_fd, 1);
                close(out_fd);
            }
            if (execvp(*args, args) == -1) {
                fprintf(stderr, "%s: command not found\n", args[0]);
                exit(1);
            }
        default:
            if (!bg_flag) {
                int status;
                waitpid(pid, &status, WUNTRACED);
                if (WIFSTOPPED(status)) {
                    tcsetpgrp(shell_term, shell_pgid);
                    strcpy(jobs[total_jobs].name, args[0]);
                    jobs[total_jobs].pid = pid;
                    total_jobs++;
                }
            } else {
                strcpy(jobs[total_jobs].name, args[0]);
                jobs[total_jobs].pid = pid;
                total_jobs++;
            }
    }
    return;
}

int execute_command() {
    int saved_in = dup(0);
    int saved_out = dup(1);
    char cmd[MAX_CMD + 1];
    cmd[0] = '\0';
    scanf("%[^\n]", cmd);
    getchar();
    if (cmd[0] == '\0') {
        return 1;
    }

    char *commands[MAX_CMD + 1], *token;
    int num_commands = 0, i, j;
    for (token = strtok(cmd, ";"); token != NULL; token = strtok(NULL, ";"))
        commands[num_commands++] = token;

    for (i = 0; i != num_commands; i++) {
        char *cmds[MAX_TOKENS + 1], *q;
        int num_cmds = 0;
        for (q = strtok(commands[i], "|"); q != NULL; q = strtok(NULL, "|"))
            cmds[num_cmds++] = q;
        cmds[num_cmds] = '\0';
        last_cmd = num_cmds - 1;
        int fd[2], in_fd = 0;

        for (j = 0; j < num_cmds; j++) {
            pipe(fd);
            if (j != last_cmd) pipe(fd);
            if (j == last_cmd) if (in_fd != 0) dup2(in_fd, 0);

            char *args[MAX_TOKENS + 1], *r;
            int arg_count = 0;
            for (r = strtok(cmds[j], " \t"); r != NULL; r = strtok(NULL, " \t"))
                args[arg_count++] = r;
            args[arg_count] = '\0';

            if (!strcmp(args[0], "pwd") || !strcmp(args[0], "cd") || !strcmp(args[0], "echo") || !strcmp(args[0], "exit")) {
                if (!handle_built_in(args, arg_count)) 
                    return 0;
            } 
            else if (!strcmp(args[0], "jobs")) {
                show_jobs();
            } 
            else if (!strcmp(args[0], "kjob")) {
                kill_job(args, arg_count);
            } 
            else if (!strcmp(args[0], "fg")) {
                foreground_job(args, arg_count);
            } 
            else if (!strcmp(args[0], "overkill")) {
                kill_all_jobs();
            } 
            else if (!strcmp(args[0], "quit")) {
                return 0;
            } 
            else {
                execute_external(args, arg_count, j, in_fd, fd[1]);
            }
            if (j != last_cmd) close(fd[1]);
            if (j != last_cmd) in_fd = fd[0];
        }
        dup2(saved_in, 0);
        dup2(saved_out, 1);
    }
    return 1;
}

int main() {
    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(shell_term, shell_pgid);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    while (1) {
        display_prompt();
        if (!execute_command()) 
            break;
    }
    return 0;
}
