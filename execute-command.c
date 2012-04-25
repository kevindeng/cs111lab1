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

int
command_status(command_t c)
{
  return c->status;
}

void
execute_command(command_t c, int time_travel)
{
  // set status to unknown (indicating not finished yet)
  c->status = -1;

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

/* part C stuff

struct node
{
  void* data;
  struct node* next;
};

typedef struct
{
  struct node* head;
  struct node* tail;
  struct node* iterator;
} list;

list create_list()
{
  list l;
  l.head = NULL;
  l.tail = NULL;
  l.iterator = NULL;
  return l;
}

void list_add(list* list, void* data)
{
  if(list && data)
  {
    struct node* n = (struct node*)checked_malloc(sizeof(struct node));
    n->data = data;
    n->next = NULL;

    if(!list->head)
    {
      list->head = n;
      list->tail = n;
      list->iterator = n;
    }
    else
    {
      list->tail->next = n;
      list->tail = n;
    }
  }
}

void* list_next(list* list)
{
  void* tmp = list->iterator->data;
  list->iterator = list->iterator->next;
  return tmp;
}

void list_reset_iterator(list* list)
{
  list->iterator = list->head;
}

int list_find(list* list, void* item)
{
  int i = 0;
  struct node* n = list->head;
  while(n)
  {
    if(n->data == item)
      return i;
    else
      n = n->next;
  }
  return -1;
}

void delete_list(list* list)
{
  if(list)
  {
    struct node* tmp;
    struct node* n = list->head;
    while(n)
    {
      tmp = n;
      n = n->next;
      free(tmp);
    }
  }
}

void
get_files(list* l, command_t c, int which)
{
  char* f;
  if(which == 0)
    f = c->input;
  else if(which == 1)
    f = c->output;
  else
    return;

  switch(c->type)
  {

  case SIMPLE_COMMAND:
    if(f)
      list_add(l, f);
    break;

  case SUBSHELL_COMMAND:
    get_files(l, c->u.subshell_command, which);
    break;

  case AND_COMMAND:
  case SEQUENCE_COMMAND:
  case OR_COMMAND:
    get_files(l, c->u.command[0], which);
    get_files(l, c->u.command[1], which);
    break;

  default:
    error(1, 0, "Unknown command type.");
    break;

  }
}

int
data_hazard(command_t a, command_t b)
{
  list in_a = create_list();
  list out_a = create_list();
  list in_b = create_list();
  list out_b = create_list();

  get_files(in_a, a, 0);
  get_files(out_a, a, 1);
  get_files(in_b, b, 0);
  get_files(out_b, b, 1);



  return 0;
}

*/
