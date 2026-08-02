#ifndef ZEPHYR_AST_H
#define ZEPHYR_AST_H

/* ---------------------------------------------------------------------
 * ast.h  --  Abstract Syntax Tree definitions for the Celestra language
 * ------------------------------------------------------------------- */

typedef enum {
    N_NUM, N_STR, N_BOOL, N_IDENT,
    N_BINOP, N_UNARY_NEG, N_NOT,
    N_ASSIGN, N_LET,
    N_PRINT, N_INPUT,
    N_IF, N_WHILE, N_FOR, N_BLOCK,
    N_FUNCDEF, N_CALL, N_RETURN,
    N_BREAK, N_CONTINUE,
    N_EXPRSTMT,
    N_ARRAY_LIT, N_INDEX
} NodeType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_GT, OP_LT, OP_GE, OP_LE, OP_EQ, OP_NE,
    OP_AND, OP_OR
} BinOp;

typedef struct Node Node;

typedef struct NodeList {
    Node *node;
    struct NodeList *next;
} NodeList;

struct Node {
    NodeType type;
    int op;              /* BinOp code, when type == N_BINOP            */
    double num;           /* numeric literal value                       */
    char *str;             /* identifier name / string literal / func name */
    int line;               /* source line number, for error messages      */

    Node *a, *b, *c, *d;      /* generic children: cond/then/else, etc.      */
    NodeList *list;            /* statement lists, arg lists, param lists     */
};

/* Constructors */
Node *node_num(double v);
Node *node_str(const char *s);
Node *node_bool(int v);
Node *node_ident(const char *name);
Node *node_binop(int op, Node *l, Node *r);
Node *node_neg(Node *e);
Node *node_not(Node *e);
Node *node_assign(const char *name, Node *expr);
Node *node_let(const char *name, Node *expr);
Node *node_print(NodeList *args);
Node *node_input(const char *name);
Node *node_if(Node *cond, Node *thenB, Node *elseB);
Node *node_while(Node *cond, Node *body);
Node *node_for(Node *init, Node *cond, Node *update, Node *body);
Node *node_block(NodeList *stmts);
Node *node_funcdef(const char *name, NodeList *params, Node *body);
Node *node_call(const char *name, NodeList *args);
Node *node_return(Node *expr);
Node *node_break(void);
Node *node_continue(void);
Node *node_exprstmt(Node *expr);
Node *node_array_lit(NodeList *elems);
Node *node_index(Node *array, Node *idx);

NodeList *list_append(NodeList *list, Node *n);

#endif
