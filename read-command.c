// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"

#include <error.h>

enum TYPE {subshell, command, token};

struct NodeList;

//tree
typedef struct{
    enum TYPE type;
    char* value;
    struct NodeList *children;
    int count;
} Node;

// linked list
typedef struct{
    Node* data;
    struct NodeList *next;
} NodeList;

//Function Prototypes
//Linked List Methods
void createNode(enum TYPE type, char *value);
void insertNode(NodeList * nodeList_ptr, Node *node);
void deleteNode(NodeList * head);



/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

/* FIXME: Define the type 'struct command_stream' here.  This should
   complete the incomplete type declaration in command.h.  */

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
  error (1, 0, "command reading not yet implemented");
  return 0;
}

command_t
read_command_stream (command_stream_t s)
{
  /* FIXME: Replace this with your implementation too.  */
  error (1, 0, "command reading not yet implemented");
  return 0;
}


enum TYPE {subshell, command, token};

struct NodeList;

//tree
typedef struct{
    enum TYPE type;
    char* value;
    struct NodeList *children;
    int count;
} Node;

// linked list
typedef struct{
    Node* data;
    struct NodeList *next;
} NodeList;

//Function Prototypes
//Linked List Methods
void createNode(enum TYPE type, char *value);
void insertNode(NodeList * nodeList_ptr, Node *node);
void deleteNode(NodeList * head);
