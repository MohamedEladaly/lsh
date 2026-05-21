
/***************************************************************************//**
  @file         main.c
  @author       Stephen Brennan (Extended with Project 2 requirements)
  @date         Thursday, 8 January 2015
  @brief        LSH (Libstephen SHell)
*******************************************************************************/

#define _GNU_SOURCE

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
#define LSH_HIST_MAX 100

char *lsh_history_log[LSH_HIST_MAX];
int lsh_history_count = 0;

/**
   @brief Adds a command line to the history log.
   @param line The raw string entered by the user.
 */
void lsh_add_history(const char *line)
{
  if (line == NULL || strlen(line) == 0) {
    return;
  }

  if (lsh_history_count < LSH_HIST_MAX) {

    lsh_history_log[lsh_history_count] = strdup(line);

    if (lsh_history_log[lsh_history_count] == NULL) {
      fprintf(stderr, "lsh: allocation error\n");
      return;
    }

    lsh_history_count++;

  } else {

    free(lsh_history_log[0]);

    for (int i = 1; i < LSH_HIST_MAX; i++) {
      lsh_history_log[i - 1] = lsh_history_log[i];
    }

    lsh_history_log[LSH_HIST_MAX - 1] = strdup(line);

    if (lsh_history_log[LSH_HIST_MAX - 1] == NULL) {
      fprintf(stderr, "lsh: allocation error\n");
    }
  }
}

/**
   @brief Frees history memory.
 */
void lsh_free_history(void)
{
  for (int i = 0; i < lsh_history_count; i++) {
    free(lsh_history_log[i]);
  }
}

/*
  Function Declarations for builtin shell commands:
 */
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_echo(char **args);
int lsh_history(char **args);
int lsh_env(char **args);
int lsh_pwd(char **args);

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  "cd",
  "help",
  "exit",
  "echo",
  "history",
  "env",
  "pwd"
};

int (*builtin_func[]) (char **) = {
  &lsh_cd,
  &lsh_help,
  &lsh_exit,
  &lsh_echo,
  &lsh_history,
  &lsh_env,
  &lsh_pwd
};

int lsh_num_builtins()
{
  return sizeof(builtin_str) / sizeof(char *);
}

/*
  Builtin function implementations.
*/

/**
   @brief Builtin command: change directory.
   @param args List of args. args[0] is "cd". args[1] is the directory.
   @return Always returns 1, to continue executing.
 */
int lsh_cd(char **args)
{
  if (args[1] == NULL) {

    char *home = getenv("HOME");

    if (home == NULL) {
      fprintf(stderr, "lsh: HOME not set\n");
    } else {
      if (chdir(home) != 0) {
        perror("lsh");
      }
    }

  } else {

    if (chdir(args[1]) != 0) {
      perror("lsh");
    }
  }

  return 1;
}

/**
   @brief Builtin command: print help.
   @param args List of args. Not examined.
   @return Always returns 1, to continue executing.
 */
int lsh_help(char **args)
{
  int i;

  printf("Stephen Brennan's LSH (Extended)\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < lsh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other programs.\n");

  return 1;
}

/**
   @brief Builtin command: exit.
   @param args List of args. Not examined.
   @return Always returns 0, to terminate execution.
 */
int lsh_exit(char **args)
{
  lsh_free_history();
  return 0;
}

/**
   @brief Builtin command: echo arguments.
   @param args List of args. args[0] is "echo".
   @return Always returns 1, to continue executing.
 */
int lsh_echo(char **args)
{
  for (int i = 1; args[i] != NULL; i++) {

    printf("%s", args[i]);

    if (args[i + 1] != NULL) {
      printf(" ");
    }
  }

  printf("\n");

  return 1;
}

/**
   @brief Builtin command: display command history.
   @param args List of args. Not examined.
   @return Always returns 1, to continue executing.
 */
int lsh_history(char **args)
{
  for (int i = 0; i < lsh_history_count; i++) {
    printf("%d %s\n", i + 1, lsh_history_log[i]);
  }

  return 1;
}

/**
   @brief Builtin command: print environment variables.
   @param args List of args. Not examined.
   @return Always returns 1, to continue executing.
 */
int lsh_env(char **args)
{
  for (char **env = environ; *env != NULL; env++) {
    printf("%s\n", *env);
  }

  return 1;
}

/**
   @brief Builtin command: print current working directory.
   @param args List of args. Not examined.
   @return Always returns 1, to continue executing.
 */
int lsh_pwd(char **args)
{
  char *cwd = getcwd(NULL, 0);

  if (cwd != NULL) {
    printf("%s\n", cwd);
    free(cwd);
  } else {
    perror("lsh: pwd");
  }

  return 1;
}

/**
  @brief Launch a program and wait for it to terminate.
  @param args Null terminated list of arguments (including program).
  @return Always returns 1, to continue execution.
 */
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

/**
   @brief Execute shell built-in or launch program.
   @param args Null terminated list of arguments.
   @return 1 if the shell should continue running, 0 if it should terminate
 */
int lsh_execute(char **args)
{
  int i;

  if (args[0] == NULL) {
    return 1;
  }

  for (i = 0; i < lsh_num_builtins(); i++) {

    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }

  return lsh_launch(args);
}

/**
   @brief Read a line of input from stdin.
   @return The line from stdin.
 */
char *lsh_read_line(void)
{
#ifdef LSH_USE_STD_GETLINE

  char *line = NULL;
  ssize_t bufsize = 0;

  if (getline(&line, &bufsize, stdin) == -1) {

    if (feof(stdin)) {

      free(line);
      return NULL;

    } else {

      perror("lsh: getline");
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
        return NULL;
      }

      buffer[position] = '\0';
      return buffer;

    } else if (c == '\n') {

      buffer[position] = '\0';
      return buffer;

    } else {

      buffer[position] = c;
    }

    position++;

    if (position >= bufsize) {

      bufsize += LSH_RL_BUFSIZE;

      char *temp = realloc(buffer, bufsize);

      if (!temp) {
        free(buffer);
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }

      buffer = temp;
    }
  }

#endif
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

/**
   @brief Split a line into tokens.
   @param line The line.
   @return Null-terminated array of tokens.
 */
char **lsh_split_line(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE;
  int position = 0;

  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

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

      char **temp = realloc(tokens, bufsize * sizeof(char*));

      if (!temp) {
        free(tokens);
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }

      tokens = temp;
    }

    token = strtok(NULL, LSH_TOK_DELIM);
  }

  tokens[position] = NULL;

  return tokens;
}

/**
   @brief Loop getting input and executing it.
 */
void lsh_loop(void)
{
  char *line;
  char **args;
  int status;

  do {

    char *cwd = getcwd(NULL, 0);

    if (cwd != NULL) {
      printf("%s$ ", cwd);
      free(cwd);
    } else {
      printf("> ");
    }

    line = lsh_read_line();

    if (line == NULL) {
      printf("\n");
      break;
    }

    lsh_add_history(line);

    args = lsh_split_line(line);

    status = lsh_execute(args);

    free(line);
    free(args);

  } while (status);
}

/**
   @brief Main entry point.
   @param argc Argument count.
   @param argv Argument vector.
   @return status code
 */
int main(int argc, char **argv)
{
  lsh_loop();

  lsh_free_history();

  return EXIT_SUCCESS;
}

