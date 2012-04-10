// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <regex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/* FIXME: Define the type 'struct command_stream' here.  This should
   complete the incomplete type declaration in command.h.  */
/* command_stream */
struct command_stream{
  int size;
  int iterator;
  command_t *commands;
};

/* lexer */
typedef enum {
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

typedef struct {
  token_type type;
  char* text;
} token;

int lexer(char* input, token** output, size_t* output_size){
  // this regex will tokenize the input
  regex_t r;
  regcomp(&r, "[A-Za-z0-9!%+,-/:@^_.]+|[|]{2}|[&]{2}|[\n;|()<>#]{1}", REG_EXTENDED);

  size_t used = 0;
  char** toks = (char**)checked_malloc(sizeof(char*) * strlen(input));
  regmatch_t m;
  size_t offset = 0;

  while(!regexec(&r, input + offset, 1, &m, 0)){
    size_t len = m.rm_eo - m.rm_so;
    char* f = (char*)malloc(len + 1);
    memcpy(f, input + offset + m.rm_so, len);
    f[len] = 0;
    toks[used++] = f;
    offset += m.rm_eo;
  }

  // assign types to the tokens
  *output = (token*)checked_malloc(sizeof(token) * used);

  unsigned int i, j;
  int comment = 0;

  for(i = 0, j = 0; i < used; i++){
    token_type type = UNKNOWN_TOKEN;
    char* t = toks[i];
    size_t len = strlen(t);
    if(len == 1){
      char c = t[0];
      switch(c){
        case ';':
          type = SEMICOLON_TOKEN; break;
        case '(':
          type = OPEN_PAREN_TOKEN; break;
        case ')':
          type = CLOSE_PAREN_TOKEN; break;
        case '|':
          type = PIPE_TOKEN; break;
        case '<':
          type = L_REDIR_TOKEN; break;
        case '>':
          type = R_REDIR_TOKEN; break;
        case '\n':
          type = NEWLINE_TOKEN; 
          comment = 0; break;
        case '#':
          comment = 1; break;
        default:
          type = WORD_TOKEN;
      }
    } 
    else if (len == 2){
      if(!strcmp(t, "||"))
        type = OR_TOKEN;
      else if(!strcmp(t, "&&"))
        type = AND_TOKEN;
      else{
        type = WORD_TOKEN;
      }
    }
    else if (len > 2){
      type = WORD_TOKEN;
    }
    else {
      error(1, 0, "invalid token");
      exit(1);
    }

    // drop all comment tokens, save the rest
    if(!comment){
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
  for(i = 0; i < j - 1; i++){
    if((*output)[i + 1].type == NEWLINE_TOKEN && (
        (*output)[i].type == SEMICOLON_TOKEN ||
        (*output)[i].type == PIPE_TOKEN ||
        (*output)[i].type == AND_TOKEN ||
        (*output)[i].type == OR_TOKEN ||
        (*output)[i].type == OPEN_PAREN_TOKEN ||
        (*output)[i].type == CLOSE_PAREN_TOKEN ||
        (*output)[i].type == NEWLINE_TOKEN
    )){
      for(k = i + 2; k < j; k++){
        (*output)[k - 1] = (*output)[k];
      }
      j--;
      i--;
    }
  }

  // shrink the array down to the proper size
  *output_size = j;
  *output = (token*)checked_realloc(*output, sizeof(token) * j);

  free(toks);
  return 0;
}



/*command_t
make_command(char *text){
  
}*/

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  size_t bufferSize = 1024;
  size_t read = 0;
  int val;
  char* buffer = (char*)checked_malloc(bufferSize);

  while((val = get_next_byte(get_next_byte_argument)) != EOF){
    buffer[read++] = val;
    if(read == bufferSize){
      buffer = (char*)checked_grow_alloc(buffer, &bufferSize);
    }
  }
  if(read == bufferSize){
    buffer = (char*)checked_grow_alloc(buffer, &bufferSize);
  }
  buffer[read] = 0;

  token* tokens;
  size_t num_tokens = 0;
  lexer(buffer, &tokens, &num_tokens);

  size_t i;
  char* type_names[11] = {"word", ";", "|", "&&", "||", "(", ")", "<", ">", "NL", "?"}; 
  for(i = 0; i < num_tokens; i++){
      token t = tokens[i];      
      if(t.type != NEWLINE_TOKEN)
        printf("%s\t%s\n", type_names[t.type], t.text);
      else
        printf("%s\n", type_names[t.type]);
  }

  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
  error (1, 0, "command reading not yet implemented");
  return 0;
}

command_t
read_command_stream (command_stream_t s)
{
  if(s->iterator < s->size - 1)
  {
    command_t ret = s->commands[s->iterator];
    s->iterator++;
    return ret;
  }
  else
    return NULL;
}

/*char** split(token *tokens, size_t array_size, token_type delim){
	size_t size = array_size;
	char** v_temp;
	v_temp = (char**)checked_malloc(sizeof(char*) * size);
	size_t used = 0;
	char* temp;
	bool open_paren =false; 
	bool search = false; //keeps track if needs to search for close paren
	
	//Open paren is a special type of token
	//if the delim passed is an open parenthesis we have to watch for 
	//close parenthesis. So we will set a bool value to determine if we 
	//need to watch for closing parenthesis
	if(delim == OPEN_PAREN_TOKEN){
		open_paren = true;
	}
	int i;
	
	for(i=0; i<array_size, i++){
		if(tokens[i].type != delim){ //not the same
			if(search ==true){ //looking for closing paren
				if(tokens[i].type == CLOSE_PAREN_TOKEN){
					search = false; //stop looking for closing paren
				}	
			}	
				int maxchars = sizeof(temp) -(strnlen(temp)+1);
				temp = strncat(temp, tokens[i], maxchars);
		} else {
			v_temp[used++] = temp;
			if(open_paren == true){
				//has to look for close parenthesis
				search = true;	
			}
				v_temp[used++] = delim;
				//clear cstring temp
				strcpy(temp, "");
			}	
		}
	}
	return v_temp;
}*/










