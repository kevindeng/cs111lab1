// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <regex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define true 1
#define false 0

#define DEBUG 0

typedef int bool;

/* FIXME: Define the type 'struct command_stream' here.  This should
 complete the incomplete type declaration in command.h.  */
/* command_stream */
struct command_stream
{
  int size;
  int iterator;
  command_t *commands;
};

/* lexer */
typedef enum
{
  WORD_TOKEN,
  SEMICOLON_TOKEN,
  PIPE_TOKEN,
  AND_TOKEN,
  OR_TOKEN,
  OPEN_PAREN_TOKEN,
  CLOSE_PAREN_TOKEN,
  L_REDIR_TOKEN,
  R_REDIR_TOKEN,
  NEWLINE_TOKEN,
  UNKNOWN_TOKEN
} token_type;

typedef struct
{
  token_type type;
  char* text;
} token;

int
lexer(char* input, token** output, size_t* output_size)
{
  // this regex will tokenize the input
  regex_t r;
  regcomp(&r, "[A-Za-z0-9!%+,-/:@^_.]+|[|]{2}|[&]{2}|[\n;|()<>#]{1}",
      REG_EXTENDED);

  size_t used = 0;
  char** toks = (char**) checked_malloc(sizeof(char*) * strlen(input));
  regmatch_t m;
  size_t input_len = strlen(input);
  size_t i, j, k;
  int first_pass = 1;

  do
  {
    // delete leading whitespaces
    for(i = 0; input[i] == ' ' || input[i] == '\t' || (first_pass && input[i] == '\n'); i++);
    input = memmove(input, input + i, input_len - i + 1);
    input_len -= i;

    if(first_pass)
      first_pass = 0;

    // exit if nothing left
    if(input_len == 0)
      break;

    // match the regex
    int matched = !regexec(&r, input, 1, &m, 0);

    // if no match and input not empty, it's a parse error
    if(!matched && input_len > 0)
      error(1, 0, "Syntax error.");

    // it's a parse error if we skip any characters
    if(m.rm_so != 0)
      error(1, 0, "Syntax error.");

    // store the match result
    size_t len = m.rm_eo - m.rm_so;
    char* f = (char*) malloc(len + 1);
    memcpy(f, input + m.rm_so, len);
    f[len] = 0;
    toks[used++] = f;

    // delete consumed input
    input = memmove(input, input + m.rm_eo, input_len - len + 1);
    input_len -= len;
  }
  while(input_len > 0);

  // assign types to the tokens
  *output = (token*) checked_malloc(sizeof(token) * used);

  int comment = 0;

  for (i = 0, j = 0; i < used; i++)
  {
    token_type type = UNKNOWN_TOKEN;
    char* t = toks[i];
    size_t len = strlen(t);
    if (len == 1)
    {
      char c = t[0];
      switch (c)
      {
      case ';':
        type = SEMICOLON_TOKEN;
        break;
      case '(':
        type = OPEN_PAREN_TOKEN;
        break;
      case ')':
        type = CLOSE_PAREN_TOKEN;
        break;
      case '|':
        type = PIPE_TOKEN;
        break;
      case '<':
        type = L_REDIR_TOKEN;
        break;
      case '>':
        type = R_REDIR_TOKEN;
        break;
      case '\n':
        type = NEWLINE_TOKEN;
        comment = 0;
        break;
      case '#':
        comment = 1;
        break;
      default:
        type = WORD_TOKEN;
        break;
      }
    }
    else if (len == 2)
    {
      if (!strcmp(t, "||"))
        type = OR_TOKEN;
      else if (!strcmp(t, "&&"))
        type = AND_TOKEN;
      else
      {
        type = WORD_TOKEN;
      }
    }
    else if (len > 2)
    {
      type = WORD_TOKEN;
    }
    else
    {
      error(1, 0, "invalid token");
      exit(1);
    }

    // drop all comment tokens, save the rest
    if (!comment)
    {
      (*output)[j].text = t;
      (*output)[j].type = type;
      j++;
    }
    else
    {
      free(t);
    }

    // debug
    /*char* type_names[11] = {"word", ";", "|", "&&", "||", "(", ")", "<", ">", "NL", "?"};
     if(!comment){
     if(t[0] != '\n')
     printf("%s\ttype: %s\n", t, type_names[type]);
     else
     printf("%s\n", "(NEWLINE)");
     }
     else
     printf("<!-- %s -->\n", t);*/
  }

  // delete meaningless newlines
  for (i = 0; i < j - 1; i++)
  {
    if ((*output)[i + 1].type == NEWLINE_TOKEN
        && ((*output)[i].type == SEMICOLON_TOKEN
            || (*output)[i].type == PIPE_TOKEN || (*output)[i].type == AND_TOKEN
            || (*output)[i].type == OR_TOKEN
            || (*output)[i].type == OPEN_PAREN_TOKEN
            || (*output)[i].type == CLOSE_PAREN_TOKEN
            || (*output)[i].type == NEWLINE_TOKEN))
    {
      free((*output)[i + 1].text);
      for (k = i + 2; k < j; k++)
      {
        (*output)[k - 1] = (*output)[k];
      }
      j--;
      i--;
    }
  }

  // delete trailing newline
  if((*output)[j - 1].type == NEWLINE_TOKEN)
    j--;

  // delete leading newline
  if(j > 0 && (*output)[0].type == NEWLINE_TOKEN)
  {
    for (k = 1; k < j; k++)
    {
      (*output)[k - 1] = (*output)[k];
    }
    j--;
  }

  // shrink the array down to the proper size
  *output_size = j;
  *output = (token*) checked_realloc(*output, sizeof(token) * j);

  // clean up
  regfree(&r);
  free(toks);
  return 0;
}

typedef enum
{
  COMMAND_AT, TOKEN_AT
} arvore_type;

typedef struct
{
  arvore_type type;
  union
  {
    command_t cmd;
    token* tok;
  } u;
} arvore;

typedef struct
{
  arvore* array;
  size_t size;
  size_t capacity;
} arvore_vec;

void
init_arvore_vec(arvore_vec** v, size_t capacity)
{
  *v = (arvore_vec*) checked_malloc(sizeof(arvore_vec));
  (*v)->array = (arvore*) checked_malloc(sizeof(arvore) * capacity);
  (*v)->size = 0;
  (*v)->capacity = capacity;
}

void
set_arvore_vec(arvore_vec* v, arvore a, size_t position)
{
  if (position >= v->size)
  {
    error(1, 0, "Array index out of bounds. Idiot.");
    exit(1);
  }
  v->array[position] = a;
}

arvore
get_arvore_vec(arvore_vec* v, size_t position)
{
  if (position >= v->size)
  {
    error(1, 0, "Array index out of bounds. Idiot.");
    exit(1);
  }
  return v->array[position];
}

void
append_arvore_vec(arvore_vec* v, arvore a)
{
  if (v->size == v->capacity)
  {
    v->array = (arvore*) checked_realloc(v->array,
        sizeof(arvore) * (v->capacity *= 2));
  }
  v->array[v->size++] = a;
}

void
delete_arvore_vec(arvore_vec* v, size_t position)
{
  size_t i = position + 1;
  for (; i < v->size; i++)
  {
    v->array[i - 1] = v->array[i];
  }
  v->size--;
}

void
splice_arvore_vec(arvore_vec* v, size_t start, size_t size, arvore replacement)
{
  size_t i = 0;
  for (; i < size - 1; i++)
  {
    delete_arvore_vec(v, start);
  }
  set_arvore_vec(v, replacement, start);
}

void
print_arvore_vec(arvore_vec* v)
{
  printf("VEC SIZE\t%u\n", (unsigned int) v->size);
  size_t i = 0;
  for (i = 0; i < v->size; i++)
  {
    if (v->array[i].type == COMMAND_AT)
    {
      printf("%u\t command\t%d\n", (unsigned int) i, v->array[i].u.cmd->type);
    }
    else
    {
      if (v->array[i].u.tok->type != NEWLINE_TOKEN)
        printf("%u\t token\t%s\n", (unsigned int) i, v->array[i].u.tok->text);
      else
        printf("%u\t token\t%s\n", (unsigned int) i, "(NEWLINE)");
    }
  }
}

void
test_arvore_vec(token* tokens, size_t num_toks)
{
  arvore_vec* v;
  init_arvore_vec(&v, num_toks);
  size_t i = 0;
  for (; i < num_toks; i++)
  {
    arvore a;
    a.type = TOKEN_AT;
    a.u.tok = tokens + i;
    append_arvore_vec(v, a);
  }
  print_arvore_vec(v);

  printf("\n-----------------------\n TEST DELETE\n-----------------------\n");
  delete_arvore_vec(v, 0);
  delete_arvore_vec(v, v->size - 1);
  print_arvore_vec(v);

  printf("\n-----------------------\n TEST SPLICE\n-----------------------\n");
  splice_arvore_vec(v, 2, 10, get_arvore_vec(v, v->size - 1));
  print_arvore_vec(v);
}




arvore_vec*
parse2(arvore_vec *cmds_tokens, token_type* target_types, size_t num_targets)
{
  size_t array_size = cmds_tokens->size;
  arvore_vec *temp;
  init_arvore_vec(&temp, array_size);
  token_type curr_token_type;

  size_t i, z, loc = array_size + 1;
  for (i = 0; i < array_size; i++)
  {
    //look for delimiter, it can be either pipes, and or or
    //once delimiter is found it splits the arvore vect into 2.
    if (cmds_tokens->array[i].type == TOKEN_AT)
    {
      token_type tp = cmds_tokens->array[i].u.tok->type;
      for(z = 0; z < num_targets; z++)
      {
        if(tp == target_types[z])
        {
          //location of first delim found
          //location of delim = i;
          loc = i;
          curr_token_type = cmds_tokens->array[i].u.tok->type;
          i = array_size;
          break;
        }
      } //end if
    }
  } //end for

  if(loc == 0)
  {
    error(1, 0, "Cannot start with a special token.");
    //exit(1);
  }

  if(loc == array_size - 1)
  {
    error(1, 0, "Cannot end with a special token.");
    //exit(1);
  }

  if (loc < array_size)
  {
    if(get_arvore_vec(cmds_tokens, loc - 1).type != COMMAND_AT)
      error(1, 0, "Left hand side not a command.");
    if(get_arvore_vec(cmds_tokens, loc + 1).type != COMMAND_AT)
      error(1, 0, "Right hand side not a command.");

    //left hand side
    //create a command
    command_t left = get_arvore_vec(cmds_tokens, loc - 1).u.cmd;
    command_t right = get_arvore_vec(cmds_tokens, loc + 1).u.cmd;
    command_t new_cmd = (command_t) checked_malloc(sizeof(struct command));
    new_cmd->input = NULL;
    new_cmd->output = NULL;
    if (curr_token_type == PIPE_TOKEN)
    {
      new_cmd->type = PIPE_COMMAND;
    }
    else if (curr_token_type == OR_TOKEN)
    {
      new_cmd->type = OR_COMMAND;
    }
    else if (curr_token_type == AND_TOKEN)
    {
      new_cmd->type = AND_COMMAND;
    }
    else if (curr_token_type == NEWLINE_TOKEN)
    {
      new_cmd->type = SEQUENCE_COMMAND;
    }
    new_cmd->u.command[0] = left;
    new_cmd->u.command[1] = right;

    // copy unused arvores on the left
    size_t j;
    for(j = 0; j < loc - 1; j++)
      append_arvore_vec(temp, get_arvore_vec(cmds_tokens, j));

    //left hand side
    arvore a;
    a.type = COMMAND_AT;
    a.u.cmd = new_cmd;
    append_arvore_vec(temp, a);

    //right hand side
    for (j = loc + 2; j < array_size; j++)
    {
      append_arvore_vec(temp, get_arvore_vec(cmds_tokens, j));
    }
    return parse2(temp, target_types, num_targets);
  }

  return cmds_tokens; //return final command

} //end of function make_command_tree

command_t
parse(arvore_vec* v)
{
  if(v->size == 0)
    error(1, 0, "Input is empty.");

  if(DEBUG)
  {
    printf("-----------------------\n  INITIAL\n-----------------------\n");
    print_arvore_vec(v);
  }

  size_t i, j, k;
  int started = 0;

  // parse subshell
  int has_sub = 0;
  do
  {
    // find the longest subshell, skipping over any nested subshells
    size_t paren_start = 0;
    size_t paren_end = 0;
    size_t open_count = 0;
    started = 0;
    for (i = 0; i < v->size; i++)
    {
      if (v->array[i].type == TOKEN_AT)
      {
        if (v->array[i].u.tok->type == OPEN_PAREN_TOKEN)
        {
          if (!started)
          {
            started = 1;
            paren_start = i;
          }
          open_count++;
        }
        else if (v->array[i].u.tok->type == CLOSE_PAREN_TOKEN)
        {
          open_count--;
          if (started && open_count == 0)
          {
            started = 0;
            paren_end = i;
            break;
          }
        }
      }
    }

    if(open_count != 0)
      error(1, 0, "Parenthesis do not match.");

    // if there is a subshell, recursively parse the contents inside
    if (paren_end > 0)
    {
      // copy the subsequence
      arvore_vec* w;
      size_t w_len = paren_end - paren_start + 1 - 2;
      init_arvore_vec(&w, w_len);
      for (i = 0; i < w_len; i++)
        append_arvore_vec(w, v->array[paren_start + 1 + i]);

      // parse the subsequence
      command_t ss = parse(w);

      // create the subshell command and replace the original tokens
      arvore a;
      a.type = COMMAND_AT;
      a.u.cmd = (command_t) checked_malloc(sizeof(struct command));
      a.u.cmd->input = NULL;
      a.u.cmd->output = NULL;
      a.u.cmd->type = SUBSHELL_COMMAND;
      a.u.cmd->u.subshell_command = ss;
      splice_arvore_vec(v, paren_start, w_len + 2, a);

      has_sub = 1;
    }
    else
      has_sub = 0;
  }
  while (has_sub);

  if(DEBUG)
  {
    printf("-----------------------\n  SUBSHELL\n-----------------------\n");
    print_arvore_vec(v);
  }

  // parse simple commands
  size_t cmd_start = 0;
  size_t cmd_len = 0;
  started = 0;
  for (i = 0;
      i <= v->size /* iterate one past to deal with file ending on a word */;
      i++)
  {
    if (i < v->size && v->array[i].type == TOKEN_AT
        && v->array[i].u.tok->type == WORD_TOKEN)
    {
      // check it's not a IO redirect
      if (i == 0
          || (v->array[i - 1].type == TOKEN_AT
              && v->array[i - 1].u.tok->type != L_REDIR_TOKEN
              && v->array[i - 1].u.tok->type != R_REDIR_TOKEN))
      {
        if (!started)
        {
          cmd_start = i;
          started = 1;
        }
        if (started)
          cmd_len++;
      }
    }
    else if (started)
    {
      char** words = (char**) checked_malloc(sizeof(char*) * (cmd_len + 1));
      for (j = 0; j < cmd_len; j++)
        words[j] = v->array[cmd_start + j].u.tok->text;
      words[cmd_len] = NULL;

      command_t com = (command_t) checked_malloc(sizeof(struct command));
      com->input = NULL;
      com->output = NULL;
      com->type = SIMPLE_COMMAND;
      com->u.word = words;

      arvore a;
      a.type = COMMAND_AT;
      a.u.cmd = com;

      splice_arvore_vec(v, cmd_start, cmd_len, a);
      cmd_len = 0;
      i = cmd_start;
      started = 0;
    }
  }

  if(DEBUG)
  {
    printf(
        "-----------------------\n  SIMPLE COMMANDS\n-----------------------\n");
    print_arvore_vec(v);
  }

  // parse IO redirects
  for (i = 0; i < v->size; i++)
  {
    if (v->array[i].type == TOKEN_AT)
    {
      if (v->array[i].u.tok->type == L_REDIR_TOKEN
          || v->array[i].u.tok->type == R_REDIR_TOKEN)
      {
        // left side must be a command
        if (i == 0 || v->array[i - 1].type == TOKEN_AT)
        {
          //printf("----------\n fail at %u\n----------\n", (unsigned int) i);
          //print_arvore_vec(v);
          error(1, 0, "Invalid input: IO redirection in wrong place.");
          //exit(1);
        }
        // next token must be a word
        if (i + 1 >= v->size || v->array[i + 1].type != TOKEN_AT
            || v->array[i + 1].u.tok->type != WORD_TOKEN)
        {
          //printf("----------\n fail at %u\n----------\n", (unsigned int) i);
          //print_arvore_vec(v);
          error(1, 0, "Invalid input: redirect source/destination invalid.");
          //exit(1);
        }

        // store the redirect
        if (v->array[i].u.tok->type == L_REDIR_TOKEN)
          v->array[i - 1].u.cmd->input = v->array[i + 1].u.tok->text;
        else
          v->array[i - 1].u.cmd->output = v->array[i + 1].u.tok->text;

        // delete the 2 tokens consumed
        delete_arvore_vec(v, i);
        delete_arvore_vec(v, i);

        i--;
      }
    }
  }

  if(DEBUG)
  {
    printf("-----------------------\n  IO REDIRECTS\n-----------------------\n");
    print_arvore_vec(v);
  }

  // parse the rest
  token_type pp = PIPE_TOKEN;
  v = parse2(v, &pp, 1);
  if(DEBUG)
  {
    printf("-----------------------\n  PIPES\n-----------------------\n");
    print_arvore_vec(v);
  }

  token_type and_n_ors[2] = {AND_TOKEN, OR_TOKEN};
  v = parse2(v, and_n_ors, 2);
  if(DEBUG)
  {
    printf("-----------------------\n  ANDS & ORS\n-----------------------\n");
    print_arvore_vec(v);
  }

  token_type nls_n_scs[2] = {NEWLINE_TOKEN, SEMICOLON_TOKEN};
  v = parse2(v, nls_n_scs, 2);
  if(DEBUG)
  {
    printf("-----------------------\n  NEWLINE & SEMICOLONSs\n-----------------------\n");
    print_arvore_vec(v);
  }

  return get_arvore_vec(v, 0).u.cmd;;
}

command_stream_t
make_command_stream(int
(*get_next_byte)(void *), void *get_next_byte_argument)
{
  size_t bufferSize = 1024;
  size_t read = 0;
  int val;
  char* buffer = (char*) checked_malloc(bufferSize);

  while ((val = get_next_byte(get_next_byte_argument)) != EOF)
  {
    buffer[read++] = val;
    if (read == bufferSize)
    {
      buffer = (char*) checked_grow_alloc(buffer, &bufferSize);
    }
  }
  if (read == bufferSize)
  {
    buffer = (char*) checked_grow_alloc(buffer, &bufferSize);
  }
  buffer[read] = 0;

  token* tokens;
  size_t num_tokens = 0;
  lexer(buffer, &tokens, &num_tokens);
  //test_arvore_vec(tokens, num_tokens);
  //exit(0);

  // put all the tokens into a vector
  arvore_vec* v;
  init_arvore_vec(&v, num_tokens);
  size_t i = 0;
  for (; i < num_tokens; i++)
  {
    arvore a;
    a.type = TOKEN_AT;
    a.u.tok = tokens + i;
    append_arvore_vec(v, a);
  }

  command_t top_cmd = parse(v);
  //print_command(top_cmd);

  // break top level sequence commands down
  arvore_vec* stack;
  init_arvore_vec(&stack, 10);
  arvore x;
  x.type = COMMAND_AT;
  x.u.cmd = top_cmd;
  append_arvore_vec(stack, x);

  arvore_vec* list;
  init_arvore_vec(&list, 10);

  while(stack->size > 0)
  {
    arvore pop = get_arvore_vec(stack, stack->size - 1);
    command_t cmd = pop.u.cmd;
    stack->size--;
    if(cmd->type == SEQUENCE_COMMAND)
    {
      arvore a;
      a.type = COMMAND_AT;
      a.u.cmd = cmd->u.command[0];
      arvore b;
      b.type = COMMAND_AT;
      b.u.cmd = cmd->u.command[1];
      append_arvore_vec(stack, b);
      append_arvore_vec(stack, a);
    }
    else
    {
      append_arvore_vec(list, pop);
    }
  }

  command_stream_t cst = (command_stream_t)checked_malloc(sizeof(struct command_stream));
  cst->commands = (command_t*)checked_malloc(sizeof(command_t) * list->size);
  for(i = 0; i < list->size; i++)
    cst->commands[i] = get_arvore_vec(list, i).u.cmd;
  cst->size = list->size;
  cst->iterator = 0;
  return cst;

  // debug
  /*char* type_names[11] =
  { "word", ";", "|", "&&", "||", "(", ")", "<", ">", "NL", "?" };

  for (i = 0; i < num_tokens; i++)
  {
    token t = tokens[i];
    if (t.type != NEWLINE_TOKEN)
      printf("%s\t%s\n", type_names[t.type], t.text);
    else
      printf("%s\n", type_names[t.type]);
  }*/
}

command_t
read_command_stream(command_stream_t s)
{
  if (s->iterator < s->size)
  {
    command_t ret = s->commands[s->iterator];
    s->iterator++;
    return ret;
  }
  else
    return NULL;
}

