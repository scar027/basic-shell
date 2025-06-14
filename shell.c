#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#define MAX_BUFFER_SIZE         1024
#define MAX_BACKGROUND_JOBS     64
#define TOKEN_BUFFER_SIZE       64
#define TOKEN_DELIMITERS        " \t\r\n\a"
#define ANSI_COLOR_GREEN        "\x1b[32m"
#define ANSI_COLOR_RESET        "\x1b[0m"

int shell_cd(char **args);
int shell_exit(char **args);

struct job_manager {
        int count;
        pid_t jobs[MAX_BACKGROUND_JOBS];
};

static struct job_manager background_jobs;

void add_background_job(pid_t pid) {
        if (background_jobs.count < MAX_BACKGROUND_JOBS) {
                background_jobs.jobs[background_jobs.count++] = pid;
        }
}

void remove_background_job(pid_t pid) {
        int i;

        for (i = 0; i < background_jobs.count; i++) {
                if (background_jobs.jobs[i] == pid) {
                        background_jobs.jobs[i] = 
                                background_jobs.jobs[--background_jobs.count];
                        break;
                }
        }
}

int is_background_job(pid_t pid) {
        int i;

        for (i = 0; i < background_jobs.count; i++) {
                if (background_jobs.jobs[i] == pid) {
                        return 1;
                }
        }
        return 0;
}

void sigchld_handler(int sig) {
        int status;
        pid_t pid;

        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
                if (is_background_job(pid)) {
                        printf("shell: Background process %d finished\n", 
                               pid);
                        remove_background_job(pid);
                }
        }
}

char *builtin_str[] = {
        "cd",
        "exit"
};

int (*builtin_func[]) (char **) = {
        &shell_cd,
        &shell_exit
};

int count_of_builtins()
{
        return sizeof(builtin_str) / sizeof(char *);
}

int shell_cd(char **args)
{
        if (args[1] == NULL) {
                fprintf(stderr, "shell: Expected argument to \"cd\"\n");
        } else {
                if (chdir(args[1]) == -1) {
                        perror("chdir");
                }
        }
        return 1;
}

int shell_exit(char **args)
{       
        int i;
        int wstatus;
        pid_t cpid;
        
        printf("shell: Exiting...\n");
        for (i = 0; i < background_jobs.count; i++) {
                cpid = background_jobs.jobs[i];
                if (kill(cpid, SIGTERM) == -1) {
                        perror("kill");
                }

                if (waitpid(cpid, &wstatus, 0) > 0) {
                        if (WIFEXITED(wstatus)) {
                                printf("Background job [%d] exited " 
                                       "with status %d\n",
                                       cpid, WEXITSTATUS(wstatus));
                        } else if (WIFSIGNALED(wstatus)) {
                                printf("Background job [%d] killed " 
                                       "by signal %d\n", cpid, 
                                       WTERMSIG(wstatus));
                        } else {
                                perror("waitpid");
                        }
                }
        }
        exit(EXIT_SUCCESS);
}

char *read_line(void) 
{
        int buffer_size = MAX_BUFFER_SIZE;
        int position = 0;
        char *buffer = malloc(sizeof(char) * buffer_size);
        int c;

        if (!buffer) {
                fprintf(stderr, "shell: Allocation error\n");
                exit(EXIT_FAILURE);
        }

        while (1) {
                c = getchar();
                if (c == EOF || c == '\n') {
                        buffer[position] = '\0';
                        return buffer;
                } else {
                        buffer[position] = c;
                }
                position++;

                if (position >= buffer_size) {
                        buffer_size += MAX_BUFFER_SIZE;
                        buffer = realloc(buffer, buffer_size);
                        if (!buffer) {
                                fprintf(stderr, "shell: Allocation error\n");
                                exit(EXIT_FAILURE);
                        }
                }
        }
}

char *read_line_alt(void) 
{
        char *line = NULL;
        size_t buffer_size = 0;
        
        if (getline(&line, &buffer_size, stdin) == -1) {
                if (feof(stdin)) {
                        exit(EXIT_SUCCESS);
                } else {
                        perror("read_line_alt");
                        exit(EXIT_FAILURE);
                }
        }
        return line;
}

char **tokenize(char *line, int *args_count) 
{
        int buffer_size = TOKEN_BUFFER_SIZE;
        int position = 0;
        char **tokens = malloc(sizeof(char *) * buffer_size);
        char *token;

        if (!tokens) {
                fprintf(stderr, "shell: Allocation error\n");
                exit(EXIT_FAILURE);
        }
        
        token = strtok(line, TOKEN_DELIMITERS);
        while (token != NULL) {
                tokens[position] = token;
                position++;

                if (position >= buffer_size) {
                        buffer_size += TOKEN_BUFFER_SIZE;
                        tokens = realloc(tokens, sizeof(char *) * buffer_size);
                        if (!tokens) {
                                fprintf(stderr, "shell: Allocation error\n");
                                exit(EXIT_FAILURE);
                        }
                }
                token = strtok(NULL, TOKEN_DELIMITERS);
        }
        tokens[position] = NULL;
        *args_count = position;
        return tokens;
}

int launch(char **args, int background) 
{
        int wstatus;
        pid_t cpid, w;

        cpid = fork();
        if (cpid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
        }
        if (cpid == 0) {
                execvp(args[0], args);

                perror("execvp");
                _exit(EXIT_FAILURE);
        } else {
                if (!background) {
                        do {
                                w = waitpid(cpid, &wstatus, WUNTRACED);
                                if (w == -1) {
                                        perror("waitpid");
                                        exit(EXIT_FAILURE);
                                }
                        } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
                } else {
                        add_background_job(cpid);
                        printf("[%d] %jd\n", background_jobs.count, 
                               (intmax_t)(cpid));
                }
        }
        return 1;
}

int execute(char **args, int background)
{
        int i;

        if (args[0] == NULL) {
                return 1;
        }
        for (i = 0; i < count_of_builtins(); i++) {
                if (strcmp(args[0], builtin_str[i]) == 0) {
                        return (*builtin_func[i])(args);
                }
        }
        return launch(args, background);
}

int main (int argc, char **argv) 
{
        char *line;
        char **args;
        int status;
        int args_count;
        int background = 0;
        struct sigaction sa;

        sa.sa_handler = sigchld_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
        sigaction(SIGCHLD, &sa, NULL);

        do {
                printf(ANSI_COLOR_GREEN "shell@unix$: " ANSI_COLOR_RESET);
                line = read_line();
                args = tokenize(line, &args_count);

                if (args_count > 0 && strcmp(args[args_count - 1], "&") == 0) {
                        background = 1;
                        args[args_count - 1] = NULL;
                }

                status = execute(args, background);

                background = 0;
                free(line);
                free(args);
        } while (status);
        exit(EXIT_SUCCESS);
}
