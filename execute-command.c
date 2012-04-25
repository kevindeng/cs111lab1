// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

int
command_status(command_t c)
{
  return c->status;
}

void
execute_command (command_t c, int time_travel)
{
  // set status to unknown (indicating not finished yet)
  c->status = -1;

  switch(c->type)
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
    if(c->u.command[0]->status == 0)
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
    if(c->u.command[0]->status != 0)
    {
      execute_command(c->u.command[1], time_travel);
      c->status = c->u.command[1]->status;
    }
    else
      c->status = c->u.command[0]->status;
    break;

  //
  case PIPE_COMMAND:
  {
    command_t left = c->u.command[0];
    command_t right = c->u.command[1];

    int pipefd[2];
    if(pipe(pipefd) == -1)
        error(1, 0, "Failed to create pipe.");

    int pid_left = fork();

    // left child
    if(pid_left == 0)
    {
      close(1);
      dup2(pipefd[1], 1);
      close(pipefd[0]);
      close(pipefd[1]);

      execute_command(left, time_travel);
      exit(left->status);
    }

    if(pid_left < 0)
      error(1, 0, "Failed to create process.");

    int pid_right = fork();

    // right child
    if(pid_right == 0)
    {
      close(0);
      dup2(pipefd[0], 0);
      close(pipefd[0]);
      close(pipefd[1]);

      execute_command(right, time_travel);
      exit(right->status);
    }

    if(pid_right < 0)
        error(1, 0, "Failed to create process.");

    close(pipefd[0]);
    close(pipefd[1]);

    int status_left;
    int status_right;
    int status;
    int i;
    for(i = 0; i < 2; i++)
    {
      int id;
      id = wait(&status);
      if(id == pid_left)
        status_left = status;
      else if(id == pid_right)
        status_right = status;
    }
    c->status = status_right;
  } break;

  case SIMPLE_COMMAND:
  {
    // create child process
    int pid = fork();

    // child
    if(pid == 0)
    {
      // file redirects
      char* in_file = c->input;
      char* out_file = c->output;

      if(in_file)
      {
        int fd;
        fd = open(in_file, O_RDONLY);
        if(fd == -1)
          error(1, 0, "%s: file does not exist.\n", in_file);

        close(0);
        dup2(fd, 0);
        close(fd);
      }

      if(out_file)
      {
        int fd;
        fd = open(out_file, O_WRONLY | O_CREAT);
        if(fd == -1)
          error(1, 0, "%s: could not open file.\n", out_file);

        close(1);
        dup2(fd, 1);
        close(fd);
      }

      // execute
      char** words = c->u.word;
      if(strcmp(words[0], ":") == 0)
        exit(0);
      execvp(words[0], words);
      error(1, 0, "%s failed.", words[0]);
    }
    // parent
    else
    {
      int stat;
      wait(&stat);
      c->status = stat;
    }
  } break;

  default:
    error(1, 0, "Unknown command type.");
    break;

  }
}
