#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

static Node *alloc_node(NodeType t) {
    Node *n = calloc(1, sizeof(Node));
    if (!n) { fprintf(stderr, "Celestra: out of memory\n"); exit(1); }
    n->type = t;
    return n;
}

Node *node_num(double v) {
    Node *n = alloc_node(N_NUM);
    n->num = v;
    return n;
}

Node *node_str(const char *s) {
    Node *n = alloc_node(N_STR);
    n->str = strdup(s);
    return n;
}

Node *node_bool(int v) {
    Node *n = alloc_node(N_BOOL);
    n->num = v ? 1 : 0;
    return n;
}

Node *node_ident(const char *name) {
    Node *n = alloc_node(N_IDENT);
    n->str = strdup(name);
    return n;
}

Node *node_binop(int op, Node *l, Node *r) {
    Node *n = alloc_node(N_BINOP);
    n->op = op;
    n->a = l;
    n->b = r;
    return n;
}

Node *node_neg(Node *e) {
    Node *n = alloc_node(N_UNARY_NEG);
    n->a = e;
    return n;
}

Node *node_not(Node *e) {
    Node *n = alloc_node(N_NOT);
    n->a = e;
    return n;
}

Node *node_assign(const char *name, Node *expr) {
    Node *n = alloc_node(N_ASSIGN);
    n->str = strdup(name);
    n->a = expr;
    return n;
}

Node *node_let(const char *name, Node *expr) {
    Node *n = alloc_node(N_LET);
    n->str = strdup(name);
    n->a = expr;
    return n;
}

Node *node_print(NodeList *args) {
    Node *n = alloc_node(N_PRINT);
    n->list = args;
    return n;
}

Node *node_input(const char *name) {
    Node *n = alloc_node(N_INPUT);
    n->str = strdup(name);
    return n;
}

Node *node_if(Node *cond, Node *thenB, Node *elseB) {
    Node *n = alloc_node(N_IF);
    n->a = cond;
    n->b = thenB;
    n->c = elseB;
    return n;
}

Node *node_while(Node *cond, Node *body) {
    Node *n = alloc_node(N_WHILE);
    n->a = cond;
    n->b = body;
    return n;
}

Node *node_for(Node *init, Node *cond, Node *update, Node *body) {
    Node *n = alloc_node(N_FOR);
    n->a = init;
    n->b = cond;
    n->c = update;
    n->d = body;
    return n;
}

Node *node_block(NodeList *stmts) {
    Node *n = alloc_node(N_BLOCK);
    n->list = stmts;
    return n;
}

Node *node_funcdef(const char *name, NodeList *params, Node *body) {
    Node *n = alloc_node(N_FUNCDEF);
    n->str = strdup(name);
    n->list = params;
    n->a = body;
    return n;
}

Node *node_call(const char *name, NodeList *args) {
    Node *n = alloc_node(N_CALL);
    n->str = strdup(name);
    n->list = args;
    return n;
}

Node *node_return(Node *expr) {
    Node *n = alloc_node(N_RETURN);
    n->a = expr;
    return n;
}

Node *node_break(void) { return alloc_node(N_BREAK); }
Node *node_continue(void) { return alloc_node(N_CONTINUE); }

Node *node_exprstmt(Node *expr) {
    Node *n = alloc_node(N_EXPRSTMT);
    n->a = expr;
    return n;
}

Node *node_array_lit(NodeList *elems) {
    Node *n = alloc_node(N_ARRAY_LIT);
    n->list = elems;
    return n;
}

Node *node_index(Node *array, Node *idx) {
    Node *n = alloc_node(N_INDEX);
    n->a = array;
    n->b = idx;
    return n;
}

/* Append n to the end of list, returning the (possibly new) head. */
NodeList *list_append(NodeList *list, Node *n) {
    NodeList *item = malloc(sizeof(NodeList));
    item->node = n;
    item->next = NULL;
    if (!list) return item;
    NodeList *cur = list;
    while (cur->next) cur = cur->next;
    cur->next = item;
    return list;
}
