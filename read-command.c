// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <regex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define true 1
#define false 0

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
  size_t offset = 0;

  while (!regexec(&r, input + offset, 1, &m, 0))
  {
    size_t len = m.rm_eo - m.rm_so;
    char* f = (char*) malloc(len + 1);
    memcpy(f, input + offset + m.rm_so, len);
    f[len] = 0;
    toks[used++] = f;
    offset += m.rm_eo;
  }

  // assign types to the tokens
  *output = (token*) checked_malloc(sizeof(token) * used);

  unsigned int i, j;
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
  size_t k;
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
      for (k = i + 2; k < j; k++)
      {
        (*output)[k - 1] = (*output)[k];
      }
      j--;
      i--;
    }
  }

  // shrink the array down to the proper size
  *output_size = j;
  *output = (token*) checked_realloc(*output, sizeof(token) * j);

  free(toks);
  return 0;
}

//split function prototype 
char**
split(token *tokens, size_t array_size, token_type delim, size_t *vector_size);

void
test_arvore_vec(token*, size_t);
command_t
parse(token*, size_t);

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

  parse(tokens, num_tokens);

  exit(0);

  // debug
  size_t i;
  char* type_names[11] =
  { "word", ";", "|", "&&", "||", "(", ")", "<", ">", "NL", "?" };

  for (i = 0; i < num_tokens; i++)
  {
    token t = tokens[i];
    if (t.type != NEWLINE_TOKEN)
      printf("%s\t%s\n", type_names[t.type], t.text);
    else
      printf("%s\n", type_names[t.type]);
  }

  //split tokens into "vectors"
  char** vector;
  char** vector_pipe;
  char** vector_or;
  char** vector_and;
  size_t vector_size, vector_size_pipe, vector_size_or, vector_size_and;

  vector = split(tokens, num_tokens, PIPE_TOKEN, &vector_size);

  size_t j;
  //printf("Vector Size: %i", vector_size);
  for (j = 0; j < vector_size; j++)
  {

    printf("Vector[%u]: %s \n", (unsigned int) j, vector[j]);
  }

  /* FIXME: Replace this with your implementation.  You may need to
   add auxiliary functions and otherwise modify the source code.
   You can also use external functions defined in the GNU C Library.  */
  error(1, 0, "command reading not yet implemented");
  return 0;
}

command_t
read_command_stream(command_stream_t s)
{
  if (s->iterator < s->size - 1)
  {
    command_t ret = s->commands[s->iterator];
    s->iterator++;
    return ret;
  }
  else
    return NULL;
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
  if (position >= v->capacity)
  {
    error(1, 0, "Array index out of bounds. Idiot.");
    exit(1);
  }
  v->array[position] = a;
}

arvore
get_arvore_vec(arvore_vec* v, size_t position)
{
  if (position >= v->capacity)
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
  printf("VEC SIZE\t%u\n", (unsigned int)v->size);
  size_t i = 0;
  for (i = 0; i < v->size; i++)
  {
    if (v->array[i].type == COMMAND_AT)
    {
      printf("command\t%d\n", v->array[i].u.cmd->type);
    }
    else
    {
      if (v->array[i].u.tok->type != NEWLINE_TOKEN)
        printf("token\t%s\n", v->array[i].u.tok->text);
      else
        printf("token\t%s\n", "(NEWLINE)");
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

command_t
parse(token* tokens, size_t num_tokens)
{
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

  printf("-----------------------\n  INITIAL\n-----------------------\n");
  print_arvore_vec(v);

  // parse simple commands
  size_t cmd_start = 0;
  size_t cmd_len = 0;
  int started = 0;
  size_t j, k;
  for (i = 0; i < v->size; i++)
  {
    if (v->array[i].type == TOKEN_AT && v->array[i].u.tok->type == WORD_TOKEN)
    {
      if (!started){
        cmd_start = i;
        started = 1;
      }
      if(started)
        cmd_len++;
    }
    else if(started)
    {
      char** words = (char**) checked_malloc(sizeof(char*) * cmd_len);
      for (j = 0; j < cmd_len; j++)
        words[j] = v->array[cmd_start + j].u.tok->text;

      command_t com = (command_t) checked_malloc(sizeof(struct command));
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

  printf(
      "-----------------------\n  SIMPLE COMMANDS\n-----------------------\n");
  print_arvore_vec(v);

  return NULL;
}

// split
char**
split(token *tokens, size_t array_size, token_type delim, size_t *vector_size)
{
  size_t size = array_size;
  char** v_temp;
  v_temp = (char**) checked_malloc(sizeof(char*) * size);
  *vector_size = 0;
  size_t used = 0;

  char* temp = (char*) malloc(104876);

  bool open_paren = false;
  bool search = false; //keeps track if needs to search for close paren

  //Open paren is a special type of token
  //if the delim passed is an open parenthesis we have to watch for
  //close parenthesis. So we will set a bool value to determine if we
  //need to watch for closing parenthesis
  if (delim == OPEN_PAREN_TOKEN)
  {
    open_paren = true;
  }
  size_t i;

  printf("Array Size %u", (unsigned int) array_size);

  for (i = 0; i < array_size; i++)
  {
    //printf("Current token %s \n", tokens[i].text);
    if (tokens[i].type != delim)
    { //not the same
      if (search == true)
      { //looking for closing paren
        if (tokens[i].type == CLOSE_PAREN_TOKEN)
        {
          search = false; //stop looking for closing paren
        }
      }
      int maxchars = strlen(tokens[i].text);
      char* new_str = (char*) malloc(1000);
      strcpy(new_str, tokens[i].text);
      strcat(new_str, " ");
      temp = strncat(temp, new_str, maxchars + 1);

    }

    if (tokens[i].type == delim)
    { //delimiter found
      printf("DELIM FOUND \n");
      if (open_paren == true)
      { //has to look for close parenthesis
        search = true; //look for close parenthesis
      } //end if

      //insert created temp into vector

      v_temp[used++] = temp;
      printf("INSERT TO VECTOR: %s \n", temp);
      //printf("VECTOR VALUE AT THIS MOMEMNT: %s \n", v_temp[used]);
      (*vector_size)++;

      //insert the delimiter into vector
      v_temp[used++] = tokens[i].text;
      printf("INSERT TO VECTOR: %s \n", tokens[i].text);
      //printf("VECTOR VALUE AT THIS MOMEMNT: %s \n", v_temp[used]);
      (*vector_size)++;

      //clear temp to allocate new sequence
      temp = (char*) malloc(104876);
      strcpy(temp, "");

    } //end if delim found

    //delim never found, insert everything into vector
    if ((i == array_size - 1) && tokens[i].type != delim)
    {

      v_temp[used++] = temp;
      printf("INSERT TO VECTOR: %s \n", temp);
      //printf("VECTOR VALUE AT THIS MOMEMNT LAST: %s \n", v_temp[used]);
      (*vector_size)++;
    }

  } //end for

  size_t k = 0;
  // printf("VECTOR SIZE BEFORE END: %u", *vector_size);
  /*
   for(k=0; k< *vector_size;k++){
   printf("Vector[%u] = %s \n", k, v_temp[k]);
   }
   */
  return v_temp;
}
;

