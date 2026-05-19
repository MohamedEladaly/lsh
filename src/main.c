/***************************************************************************//**

  @file         main.c

  @author       Stephen Brennan (Modified)

  @brief        LSH (Libstephen SHell) with fixes for history and memory leaks.

*******************************************************************************/

#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern char **environ;

/*
  History support
 */
#define HISTORY_SIZE 100

char *history[HISTORY_SIZE];
int history_count = 0;

void add_history(char *line)
{

  if (line == NULL || line[0] == '\n' || line[0] == '\0') {
    return;
  }

  if (history_count < HISTORY_SIZE) {
    history[history_count] = strdup(line);
    history_count++;
  } else {
    // History is full. Free the oldest command and shift everything left.
    free(history[0]);
    for (int i = 1; i < HISTORY_SIZE; i++) {
      history[i - 1] = history[i];
    }
    // Add the new command at the end
    history[HISTORY_SIZE - 1] = strdup(line);
  }
}

void free_history()
{
  // Clean up history memory on exit
  for (int i = 0; i < history_count; i++) {
    free(history[i]);
  }
}

/*
  Function Declarations for builtin shell commands:
 */
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_pwd(char **args);
int lsh_echo(char **args);
int lsh_history(char **args);
int lsh_env(char **args);

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  "cd",
  "help",
  "exit",
  "pwd",
  "echo",
  "history",
  "env"
};

int (*builtin_func[]) (char **) = {
  &lsh_cd,
  &lsh_help,
  &lsh_exit,
  &lsh_pwd,
  &lsh_echo,
  &lsh_history,
  &lsh_env
};

int lsh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

/* Builtin function implementations. */

int lsh_cd(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("lsh");
    }
  }
  return 1;
}

int lsh_help(char **args)
{
  int i;
  printf("Stephen Brennan's LSH\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < lsh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}

int lsh_exit(char **args)
{
  return 0; // Returning 0 terminates the loop
}
///////////////////////////
int lsh_pwd(char **args)
{
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s\n", cwd);
  } else {
    perror("pwd");
  }
  return 1;
}

int lsh_echo(char **args)
{
  int i = 1;
  while (args[i] != NULL) {
    printf("%s ", args[i]);
    i++;
  }
  printf("\n");
  return 1;
}

int lsh_history(char **args)
{
  for (int i = 0; i < history_count; i++) {
    // Print without the trailing newline that fgets/getline might leave
    printf("%d %s", i + 1, history[i]);
    if (history[i][strlen(history[i])-1] != '\n') {
        printf("\n");
    }
  }
  return 1;
}

int lsh_env(char **args)
{
  int i = 0;
  while (environ[i] != NULL) {
    printf("%s\n", environ[i]);
    i++;
  }
  return 1;
}

int lsh_launch(char **args)
{
  pid_t pid;
  int status;

  pid = fork();
  if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
      perror("lsh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    // Error forking
    perror("lsh");
  } else {
    // Parent process
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  return 1;
}

int lsh_execute(char **args)
{
  int i;
  if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
  }
  for (i = 0; i < lsh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }
  return lsh_launch(args);
}

char *lsh_read_line(void)
{
#ifdef LSH_USE_STD_GETLINE
  char *line = NULL;
  ssize_t bufsize = 0;
  if (getline(&line, &bufsize, stdin) == -1) {
    if (feof(stdin)) {
      free(line);
      return NULL;  // Return NULL on EOF so loop can exit cleanly
    } else  {
      perror("lsh: getline\n");
      free(line);
      exit(EXIT_FAILURE);
    }
  }
  return line;
#else
#define LSH_RL_BUFSIZE 1024
  int bufsize = LSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    c = getchar();
    if (c == EOF) {
      if (position == 0) {
        free(buffer);
        return NULL; // Return NULL cleanly on EOF
      } else {
        buffer[position] = '\0';
        return buffer;
      }
    } else if (c == '\n') {
      buffer[position] = '\n';
      buffer[position + 1] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    if (position >= bufsize - 1) { // Leave room for \n and \0
      bufsize += LSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
#endif
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

char **lsh_split_line(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token, **tokens_backup;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        free(tokens_backup);
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, LSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

void lsh_loop(void)
{
  char *line;
  char **args;
  int status = 1;

  do {
    printf("> ");
    line = lsh_read_line();

    // Handle Ctrl+D / EOF Gracefully
    if (line == NULL) {
        printf("\n");
        break;
    }

    add_history(line);
    args = lsh_split_line(line);
    status = lsh_execute(args);

    free(line);
    free(args);
  } while (status);
}

int main(int argc, char **argv)
{
  // Run command loop.
  lsh_loop();

  // Perform shutdown/cleanup to prevent memory leaks.
  free_history();

  return EXIT_SUCCESS;
}