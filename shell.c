#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#define MAX_BUFFER_SIZE         1024
#define TOKEN_BUFFER_SIZE       64
#define TOKEN_DELIMITERS        " \t\r\n\a"
#define ANSI_COLOR_GREEN        "\x1b[32m"
#define ANSI_COLOR_RESET        "\x1b[0m"

int shell_cd(char **args);
int shell_exit(char **args);

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
        ssize_t buffer_size = 0;
        
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

char **tokenize(char *line) 
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
        return tokens;
}

int launch(char **args) 
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
                do {
                        w = waitpid(cpid, &wstatus, WUNTRACED);
                } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
        }
        return 1;
}

int execute(char **args)
{
        int i;

        if (args[0] == NULL) {
                return 1;
        }
        while (i < count_of_builtins()) {
                if (strcmp(args[0], builtin_str[i]) == 0) {
                        return (*builtin_func[i])(args);
                }
                i++;
        }
        return launch(args);
}

int main (int argc, char **argv) 
{
        char *line;
        char **args;
        int status;

        do {
                printf(ANSI_COLOR_GREEN "shell@unix$: " ANSI_COLOR_RESET);
                line = read_line();
                args = tokenize(line);
                status = execute(args);

                free(line);
                free(args);
        } while (status);
        exit(EXIT_SUCCESS);
}
