#ifndef ZEPHYR_INTERPRETER_H
#define ZEPHYR_INTERPRETER_H

#include "ast.h"

typedef enum { VAL_NUM, VAL_STR, VAL_BOOL, VAL_NONE, VAL_ARRAY } ValType;

/* Arrays are heap-allocated and shared by pointer, so passing an array
 * to a function or storing it in another variable gives a real
 * reference to the same underlying data (mutations are visible through
 * every alias) -- similar to how lists work in Python or JavaScript. */
typedef struct ArrayObj {
    struct Value *items;
    int count;
    int capacity;
} ArrayObj;

typedef struct Value {
    ValType type;
    double num;
    char *str;
    ArrayObj *arr;
} Value;

/* Runs a fully-parsed Celestra program (a N_BLOCK-style NodeList of
 * top-level statements). */
void interpret(NodeList *program);

#endif
