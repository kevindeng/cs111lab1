// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

/* -- vector -- */

typedef struct
{
  void** data;
  size_t size;
  size_t capacity;
} vector_t;

vector_t*
vector_create(size_t initial_size)
{
  vector_t* t = (vector_t*)checked_malloc(sizeof(vector_t));
  t->size = 0;
  t->capacity = initial_size;
  t->data = (void**)checked_malloc(initial_size * sizeof(void*));
  memset(t->data, 0, initial_size * sizeof(void*));
  return t;
}

int
vector_set(vector_t* vector, void* data, size_t position)
{
  if(position >= vector->size)
    return 0;

  vector->data[position] = data;
  return 1;
}

void
vector_append(vector_t* vector, void* data)
{
  if(vector->size >= vector->capacity)
  {
    vector->capacity *= 2;
    vector->data = (void**)checked_realloc(vector->data, vector->capacity * sizeof(void*));
  }

  vector->data[vector->size] = data;
  vector->size++;
}

void
vector_append_vector(vector_t* a, vector_t* b)
{
  size_t i;
  for(i = 0; i < b->size; i++)
    vector_append(a, b->data[i]);
}

int
vector_contains(vector_t* vector, void* data, int (*compare)(void* a, void* b))
{
  size_t i;
  for(i = 0; i < vector->size; i++)
    if(vector->data[i] == data || compare(vector->data[i], data))
      return 1;
  return 0;
}

void
vector_delete(vector_t* vector)
{
  free(vector->data);
}

/* ------------------------------- */

vector_t*
get_used_files(command_t c)
{
  vector_t* f = vector_create(8);
  switch(c->type)
  {

  // recursively traverse the command structure
  case SEQUENCE_COMMAND:
  case AND_COMMAND:
  case OR_COMMAND:
  case PIPE_COMMAND:
  {
    vector_t* a = get_used_files(c->u.command[0]);
    vector_t* b = get_used_files(c->u.command[1]);
    vector_append_vector(f, a);
    vector_append_vector(f, b);
    vector_delete(a);
    vector_delete(b);
    free(a);
    free(b);
    break;
  }

  case SUBSHELL_COMMAND:
  {
    vector_t* a = get_used_files(c->u.subshell_command);
    vector_append_vector(f, a);
    vector_delete(a);
    free(a);
    break;
  }

  case SIMPLE_COMMAND:
  {
    // put arguments in
    char** w;
    for(w = c->u.word + 1; *w != NULL; w++)
      vector_append(f, *w);
    // put io redirects in
    if(c->input)
      vector_append(f, c->input);
    if(c->output)
      vector_append(f, c->output);
    break;
  }

  }
  return f;
}

/* ------------------------------- */

typedef struct
{
  size_t id;
  command_t cmd;
  vector_t* files;
  vector_t* waitfor;
  int running;
  pid_t pid;
} cmd_des;

static vector_t* cmd_des_vec = NULL;
static size_t cmd_id = 0;
static int timetravel = 0;

inline int
check_overlap(vector_t* a, vector_t* b)
{
  size_t i, j;
  for(i = 0; i < a->size; i++)
    for(j = 0; j < b->size; j++)
      if(strcmp(a->data[i], b->data[j]) == 0)
        return 1;
  return 0;
}

/* ------------------------------- */

/*static vector_t* files_in_use = NULL;*/

inline int
command_status(command_t c)
{
  return c->status;
}

void
execute_command(command_t c, int time_travel)
{

  switch (c->type)
  {

  // sequence < A; B > execute A, then B.
  case SEQUENCE_COMMAND:
    execute_command(c->u.command[0], time_travel);
    execute_command(c->u.command[1], time_travel);
    c->status = c->u.command[1]->status;
    break;

  // subshell < ( A ) > execute A
  case SUBSHELL_COMMAND:
    execute_command(c->u.subshell_command, time_travel);
    c->status = c->u.subshell_command->status;
    break;

  // and < A && B > execute A; execute B if and only if return code from A is zero
  case AND_COMMAND:
    execute_command(c->u.command[0], time_travel);
    if (c->u.command[0]->status == 0)
    {
      execute_command(c->u.command[1], time_travel);
      c->status = c->u.command[1]->status;
    }
    else
      c->status = 0;
    break;

  // and < A || B > execute A; execute B if and only if return code from A is non-zero
  case OR_COMMAND:
    execute_command(c->u.command[0], time_travel);
    if (c->u.command[0]->status != 0)
    {
      execute_command(c->u.command[1], time_travel);
      c->status = c->u.command[1]->status;
    }
    else
      c->status = c->u.command[0]->status;
    break;

  /* pipe < A | B > fork two child processes, one to execute A,
   * one to execute B, linking them with a pipe
   */
  case PIPE_COMMAND:
  {
    command_t left = c->u.command[0];
    command_t right = c->u.command[1];

    int pipefd[2];
    if (pipe(pipefd) == -1)
      error(1, 0, "Failed to create pipe.");

    int pid_left = fork();

    // left child
    if (pid_left == 0)
    {
      close(1);
      dup2(pipefd[1], 1);
      close(pipefd[0]);
      close(pipefd[1]);

      execute_command(left, time_travel);
      exit(left->status);
    }

    if (pid_left < 0)
      error(1, 0, "Failed to create process.");

    int pid_right = fork();

    // right child
    if (pid_right == 0)
    {
      close(0);
      dup2(pipefd[0], 0);
      close(pipefd[0]);
      close(pipefd[1]);

      execute_command(right, time_travel);
      exit(right->status);
    }

    if (pid_right < 0)
      error(1, 0, "Failed to create process.");

    close(pipefd[0]);
    close(pipefd[1]);

    // wait for children to finish
    int status_left;
    int status_right;
    waitpid(pid_left, &status_left, 0);
    waitpid(pid_right, &status_right, 0);
    c->status = status_right;
  }
    break;

  /* simple command < A > fork a child process to execute the program,
   * handling file redirects as appropriate
   */
  case SIMPLE_COMMAND:
  {
    // create child process
    int pid = fork();

    // child
    if (pid == 0)
    {
      // file redirects
      char* in_file = c->input;
      char* out_file = c->output;

      if (in_file)
      {
        int fd;
        fd = open(in_file, O_RDONLY);
        if (fd == -1)
          error(1, 0, "%s: file does not exist.\n", in_file);

        close(0);
        dup2(fd, 0);
        close(fd);
      }

      if (out_file)
      {
        int fd;
        fd = open(out_file, O_WRONLY | O_CREAT);
        if (fd == -1)
          error(1, 0, "%s: could not open file.\n", out_file);

        close(1);
        dup2(fd, 1);
        close(fd);
      }

      // execute
      char** words = c->u.word;
      if (strcmp(words[0], ":") == 0)
        exit(0);
      execvp(words[0], words);
      error(1, 0, "%s failed.", words[0]);
    }
    // parent
    else
    {
      int stat;
      waitpid(pid, &stat, 0);
      c->status = stat;
    }
  }
    break;

  default:
    error(1, 0, "Unknown command type.");
    break;

  }
}

void
execute()
{
  size_t num_undone = cmd_des_vec->size;
  while(num_undone > 0)
  {
    size_t id;
    for(id = 0; id < cmd_des_vec->size; id++)
    {
      cmd_des* des = (cmd_des*)cmd_des_vec->data[id];

      if(des->running)
      {
        int stat;
        if(waitpid(des->pid, &stat, num_undone == 1 ? 0 : WNOHANG) == des->pid)
        {
          des->running = 0;
          des->cmd->status = stat;
          num_undone--;
        }
      }

      if(des->cmd->status != -1 || des->running)
        continue;

      int dependencies_done = 1;
      size_t i;
      vector_t* wf = des->waitfor;
      for(i = 0; i < wf->size; i++)
      {
        int d = *((int*)wf->data[i]);
        cmd_des* t = (cmd_des*)cmd_des_vec->data[d];
        if(t->cmd->status == -1)
        {
          dependencies_done = 0;
          break;
        }
      }

      if(dependencies_done)
      {
        pid_t p = fork();
        if(p == -1)
          error(1, 0, "Fork failed\n");

        des->running = 1;

        if(p == 0)
        {
          command_t cmd = des->cmd;
          execute_command(cmd, timetravel);
          exit(cmd->status);
        }
        else
        {
          des->pid = p;
        }
      }
    }
  }

  //cleanup

}

void
load_command(command_t c, int time_travel)
{
  // set status to unknown (indicating not finished yet)
  c->status = -1;

  if(!cmd_des_vec)
    cmd_des_vec = vector_create(10);

  if(time_travel)
    timetravel = 1;
  else
  {
    execute_command(c, time_travel);
    return;
  }

  vector_t* f = get_used_files(c);
  cmd_des* u = (cmd_des*)checked_malloc(sizeof(cmd_des));
  u->id = cmd_id++;
  u->files = f;
  u->cmd = c;
  u->running = 0;
  u->pid = 0;
  vector_append(cmd_des_vec, u);

  vector_t* waitfor = vector_create(1);
  size_t id;
  for(id = 0; id < cmd_id - 1; id++)
  {
    if(check_overlap(((cmd_des*)cmd_des_vec->data[id])->files, f))
    {
      int* x = (int*)checked_malloc(sizeof(int));
      *x = id;
      vector_append(waitfor, x);
    }
  }
  u->waitfor = waitfor;
}


