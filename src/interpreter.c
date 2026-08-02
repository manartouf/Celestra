#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include "interpreter.h"

/* --------------------------------------------------------------------
 * interpreter.c -- tree-walking evaluator for the Celestra language
 * ------------------------------------------------------------------ */

typedef enum { SIG_NONE, SIG_BREAK, SIG_CONTINUE, SIG_RETURN } Signal;

/* ---- variable environment (scope chain) ---- */
typedef struct Var {
    char *name;
    Value val;
    struct Var *next;
} Var;

typedef struct Env {
    Var *vars;
    struct Env *parent;
} Env;

/* ---- user-defined functions (global table) ---- */
typedef struct FuncEntry {
    char *name;
    Node *def;
    struct FuncEntry *next;
} FuncEntry;

static FuncEntry *g_functions = NULL;
static Env *g_global = NULL;

static void runtime_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Celestra Runtime Error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

/* ---------------- Value helpers ---------------- */
static Value value_num(double v)  { Value r; r.type = VAL_NUM;  r.num = v; r.str = NULL; r.arr = NULL; return r; }
static Value value_str(const char *s) { Value r; r.type = VAL_STR; r.num = 0; r.str = strdup(s); r.arr = NULL; return r; }
static Value value_bool(int v) { Value r; r.type = VAL_BOOL; r.num = v ? 1 : 0; r.str = NULL; r.arr = NULL; return r; }
static Value value_none(void) { Value r; r.type = VAL_NONE; r.num = 0; r.str = NULL; r.arr = NULL; return r; }

static ArrayObj *array_new(void) {
    ArrayObj *a = malloc(sizeof(ArrayObj));
    a->count = 0;
    a->capacity = 4;
    a->items = malloc(sizeof(Value) * a->capacity);
    return a;
}

static void array_push(ArrayObj *a, Value v) {
    if (a->count == a->capacity) {
        a->capacity *= 2;
        a->items = realloc(a->items, sizeof(Value) * a->capacity);
    }
    a->items[a->count++] = v;
}

static Value value_array(ArrayObj *a) { Value r; r.type = VAL_ARRAY; r.num = 0; r.str = NULL; r.arr = a; return r; }

static int is_truthy(Value v) {
    switch (v.type) {
        case VAL_NUM:  return v.num != 0;
        case VAL_STR:  return v.str && v.str[0] != '\0';
        case VAL_BOOL: return v.num != 0;
        case VAL_ARRAY: return v.arr && v.arr->count > 0;
        case VAL_NONE: return 0;
    }
    return 0;
}

/* Render a Value as a freshly malloc'd display string. */
static char *value_to_display(Value v) {
    char buf[64];
    switch (v.type) {
        case VAL_NUM:
            if (v.num == floor(v.num) && fabs(v.num) < 1e15)
                snprintf(buf, sizeof(buf), "%.0f", v.num);
            else
                snprintf(buf, sizeof(buf), "%g", v.num);
            return strdup(buf);
        case VAL_STR:
            return strdup(v.str ? v.str : "");
        case VAL_BOOL:
            return strdup(v.num != 0 ? "true" : "false");
        case VAL_ARRAY: {
            /* Build "[a, b, c]" by rendering each element and joining. */
            size_t cap = 64, len = 0;
            char *out = malloc(cap);
            out[0] = '\0';
            strcat(out, "[");
            len = 1;
            for (int i = 0; i < v.arr->count; i++) {
                char *elem = value_to_display(v.arr->items[i]);
                size_t need = len + strlen(elem) + 3;
                if (need > cap) { cap = need * 2; out = realloc(out, cap); }
                if (i > 0) { strcat(out, ", "); len += 2; }
                strcat(out, elem);
                len += strlen(elem);
                free(elem);
            }
            strcat(out, "]");
            return out;
        }
        case VAL_NONE:
        default:
            return strdup("none");
    }
}

/* ---------------- Environment helpers ---------------- */
static Env *env_new(Env *parent) {
    Env *e = malloc(sizeof(Env));
    e->vars = NULL;
    e->parent = parent;
    return e;
}

/* Define (or overwrite) a variable in THIS scope only. */
static void env_define(Env *env, const char *name, Value v) {
    for (Var *p = env->vars; p; p = p->next) {
        if (strcmp(p->name, name) == 0) { p->val = v; return; }
    }
    Var *nv = malloc(sizeof(Var));
    nv->name = strdup(name);
    nv->val = v;
    nv->next = env->vars;
    env->vars = nv;
}

/* Search the scope chain and update the first match; 1 = success. */
static int env_assign(Env *env, const char *name, Value v) {
    for (Env *e = env; e; e = e->parent)
        for (Var *p = e->vars; p; p = p->next)
            if (strcmp(p->name, name) == 0) { p->val = v; return 1; }
    return 0;
}

static int env_get(Env *env, const char *name, Value *out) {
    for (Env *e = env; e; e = e->parent)
        for (Var *p = e->vars; p; p = p->next)
            if (strcmp(p->name, name) == 0) { *out = p->val; return 1; }
    return 0;
}

/* ---------------- Function table ---------------- */
static void func_define(const char *name, Node *def) {
    for (FuncEntry *f = g_functions; f; f = f->next)
        if (strcmp(f->name, name) == 0) { f->def = def; return; }
    FuncEntry *f = malloc(sizeof(FuncEntry));
    f->name = strdup(name);
    f->def = def;
    f->next = g_functions;
    g_functions = f;
}

static Node *func_find(const char *name) {
    for (FuncEntry *f = g_functions; f; f = f->next)
        if (strcmp(f->name, name) == 0) return f->def;
    return NULL;
}

/* forward decls */
static Value eval(Node *n, Env *env);
static Signal exec_stmt(Node *n, Env *env, Value *retval);
static Signal exec_block(NodeList *list, Env *env, Value *retval);

/* ---------------- Binary operators ---------------- */
static Value apply_binop(int op, Value l, Value r) {
    if (op == OP_ADD && (l.type == VAL_STR || r.type == VAL_STR)) {
        char *ls = value_to_display(l);
        char *rs = value_to_display(r);
        char *cat = malloc(strlen(ls) + strlen(rs) + 1);
        strcpy(cat, ls); strcat(cat, rs);
        Value out = value_str(cat);
        free(ls); free(rs); free(cat);
        return out;
    }

    if (op == OP_EQ || op == OP_NE) {
        int eq;
        if (l.type == VAL_STR && r.type == VAL_STR) eq = (strcmp(l.str, r.str) == 0);
        else if (l.type == VAL_STR || r.type == VAL_STR) eq = 0;
        else eq = (l.num == r.num);
        return value_bool(op == OP_EQ ? eq : !eq);
    }

    if (l.type == VAL_STR && r.type == VAL_STR &&
        (op == OP_GT || op == OP_LT || op == OP_GE || op == OP_LE)) {
        int c = strcmp(l.str, r.str);
        switch (op) {
            case OP_GT: return value_bool(c > 0);
            case OP_LT: return value_bool(c < 0);
            case OP_GE: return value_bool(c >= 0);
            case OP_LE: return value_bool(c <= 0);
        }
    }

    if (l.type == VAL_STR || r.type == VAL_STR)
        runtime_error("cannot apply operator to a string in a numeric context");

    double a = l.num, b = r.num;
    switch (op) {
        case OP_ADD: return value_num(a + b);
        case OP_SUB: return value_num(a - b);
        case OP_MUL: return value_num(a * b);
        case OP_DIV:
            if (b == 0) runtime_error("division by zero");
            return value_num(a / b);
        case OP_MOD:
            if (b == 0) runtime_error("modulo by zero");
            return value_num(fmod(a, b));
        case OP_GT: return value_bool(a > b);
        case OP_LT: return value_bool(a < b);
        case OP_GE: return value_bool(a >= b);
        case OP_LE: return value_bool(a <= b);
        default: runtime_error("unknown operator");
    }
    return value_none();
}

/* ---------------- Built-in standard library ---------------- */
/* Evaluate a NodeList of argument expressions into a small fixed-size
 * array of Values. `need` is the exact number of arguments required. */
static void eval_args(NodeList *args, Env *env, Value *out, int need, const char *fname) {
    int i = 0;
    for (NodeList *p = args; p; p = p->next, i++) {
        if (i >= need) runtime_error("built-in '%s' expects %d argument(s)", fname, need);
        out[i] = eval(p->node, env);
    }
    if (i != need) runtime_error("built-in '%s' expects %d argument(s)", fname, need);
}

/* Returns 1 and fills *out if `name` is a recognized built-in;
 * returns 0 otherwise so the caller falls back to user-defined functions. */
static int call_builtin(const char *name, NodeList *args, Env *env, Value *out) {
    Value a[2];

    if (strcmp(name, "length") == 0) {
        eval_args(args, env, a, 1, name);
        if (a[0].type == VAL_ARRAY) { *out = value_num(a[0].arr->count); return 1; }
        if (a[0].type == VAL_STR)   { *out = value_num((double)strlen(a[0].str)); return 1; }
        runtime_error("length() expects an array or a string");
    }
    if (strcmp(name, "push") == 0) {
        eval_args(args, env, a, 2, name);
        if (a[0].type != VAL_ARRAY) runtime_error("push() expects an array as its first argument");
        array_push(a[0].arr, a[1]);
        *out = value_none();
        return 1;
    }
    if (strcmp(name, "pop") == 0) {
        eval_args(args, env, a, 1, name);
        if (a[0].type != VAL_ARRAY) runtime_error("pop() expects an array");
        if (a[0].arr->count == 0) runtime_error("pop() called on an empty array");
        *out = a[0].arr->items[--a[0].arr->count];
        return 1;
    }
    if (strcmp(name, "upper") == 0) {
        eval_args(args, env, a, 1, name);
        if (a[0].type != VAL_STR) runtime_error("upper() expects a string");
        char *s = strdup(a[0].str);
        for (char *c = s; *c; c++) *c = toupper((unsigned char)*c);
        *out = value_str(s); free(s);
        return 1;
    }
    if (strcmp(name, "lower") == 0) {
        eval_args(args, env, a, 1, name);
        if (a[0].type != VAL_STR) runtime_error("lower() expects a string");
        char *s = strdup(a[0].str);
        for (char *c = s; *c; c++) *c = tolower((unsigned char)*c);
        *out = value_str(s); free(s);
        return 1;
    }
    if (strcmp(name, "sqrt") == 0) {
        eval_args(args, env, a, 1, name);
        if (a[0].type != VAL_NUM || a[0].num < 0) runtime_error("sqrt() expects a non-negative number");
        *out = value_num(sqrt(a[0].num));
        return 1;
    }
    if (strcmp(name, "pow") == 0) {
        eval_args(args, env, a, 2, name);
        if (a[0].type != VAL_NUM || a[1].type != VAL_NUM) runtime_error("pow() expects two numbers");
        *out = value_num(pow(a[0].num, a[1].num));
        return 1;
    }
    if (strcmp(name, "abs") == 0) {
        eval_args(args, env, a, 1, name);
        if (a[0].type != VAL_NUM) runtime_error("abs() expects a number");
        *out = value_num(fabs(a[0].num));
        return 1;
    }
    if (strcmp(name, "round") == 0) {
        eval_args(args, env, a, 1, name);
        if (a[0].type != VAL_NUM) runtime_error("round() expects a number");
        *out = value_num(round(a[0].num));
        return 1;
    }
    if (strcmp(name, "random") == 0) {
        if (args) runtime_error("random() expects no arguments");
        *out = value_num((double)rand() / ((double)RAND_MAX + 1.0));
        return 1;
    }
    if (strcmp(name, "typeof") == 0) {
        eval_args(args, env, a, 1, name);
        switch (a[0].type) {
            case VAL_NUM:   *out = value_str("number");  break;
            case VAL_STR:   *out = value_str("string");  break;
            case VAL_BOOL:  *out = value_str("boolean"); break;
            case VAL_ARRAY: *out = value_str("array");   break;
            case VAL_NONE:  *out = value_str("none");    break;
        }
        return 1;
    }

    return 0; /* not a built-in */
}

/* set(arr, index, value) needs 3 arguments, which the fixed 2-slot
 * eval_args() helper above can't express, so it is handled here with
 * its own small block before the generic built-in dispatch. */
static int call_set_builtin(const char *name, NodeList *args, Env *env, Value *out) {
    if (strcmp(name, "set") != 0) return 0;
    if (!args || !args->next || !args->next->next || args->next->next->next)
        runtime_error("set() expects exactly 3 arguments: set(array, index, value)");
    Value arr = eval(args->node, env);
    Value idxv = eval(args->next->node, env);
    Value val = eval(args->next->next->node, env);
    if (arr.type != VAL_ARRAY) runtime_error("set() expects an array as its first argument");
    if (idxv.type != VAL_NUM) runtime_error("set() expects a numeric index");
    int idx = (int)idxv.num;
    if (idx < 0 || idx >= arr.arr->count) runtime_error("set() index %d out of bounds", idx);
    arr.arr->items[idx] = val;
    *out = value_none();
    return 1;
}

/* ---------------- Function calls ---------------- */
static Value call_function(const char *name, NodeList *args, Env *callerEnv) {
    Value builtinResult;
    if (call_set_builtin(name, args, callerEnv, &builtinResult)) return builtinResult;
    if (call_builtin(name, args, callerEnv, &builtinResult)) return builtinResult;

    Node *def = func_find(name);
    if (!def) runtime_error("call to undefined function '%s'", name);

    Env *fnEnv = env_new(g_global);

    NodeList *param = def->list;
    NodeList *arg = args;
    while (param && arg) {
        Value v = eval(arg->node, callerEnv);
        env_define(fnEnv, param->node->str, v);
        param = param->next;
        arg = arg->next;
    }
    if (param || arg)
        runtime_error("function '%s' called with the wrong number of arguments", name);

    Value retval = value_none();
    exec_stmt(def->a, fnEnv, &retval);
    return retval;
}

/* ---------------- Expression evaluation ---------------- */
static Value eval(Node *n, Env *env) {
    if (!n) return value_none();
    switch (n->type) {
        case N_NUM:  return value_num(n->num);
        case N_STR:  return value_str(n->str);
        case N_BOOL: return value_bool(n->num != 0);
        case N_IDENT: {
            Value v;
            if (!env_get(env, n->str, &v))
                runtime_error("undefined variable '%s'", n->str);
            return v;
        }
        case N_UNARY_NEG: {
            Value v = eval(n->a, env);
            if (v.type != VAL_NUM) runtime_error("unary '-' requires a number");
            return value_num(-v.num);
        }
        case N_NOT:
            return value_bool(!is_truthy(eval(n->a, env)));
        case N_BINOP:
            if (n->op == OP_AND) {
                Value l = eval(n->a, env);
                if (!is_truthy(l)) return value_bool(0);
                return value_bool(is_truthy(eval(n->b, env)));
            }
            if (n->op == OP_OR) {
                Value l = eval(n->a, env);
                if (is_truthy(l)) return value_bool(1);
                return value_bool(is_truthy(eval(n->b, env)));
            }
            return apply_binop(n->op, eval(n->a, env), eval(n->b, env));
        case N_CALL:
            return call_function(n->str, n->list, env);
        case N_ARRAY_LIT: {
            ArrayObj *a = array_new();
            for (NodeList *p = n->list; p; p = p->next)
                array_push(a, eval(p->node, env));
            return value_array(a);
        }
        case N_INDEX: {
            Value base = eval(n->a, env);
            Value idxv = eval(n->b, env);
            if (idxv.type != VAL_NUM) runtime_error("array/string index must be a number");
            int idx = (int)idxv.num;
            if (base.type == VAL_ARRAY) {
                if (idx < 0 || idx >= base.arr->count) runtime_error("array index %d out of bounds", idx);
                return base.arr->items[idx];
            }
            if (base.type == VAL_STR) {
                int len = (int)strlen(base.str);
                if (idx < 0 || idx >= len) runtime_error("string index %d out of bounds", idx);
                char buf[2] = { base.str[idx], '\0' };
                return value_str(buf);
            }
            runtime_error("indexing is only supported on arrays and strings");
        }
        default:
            runtime_error("invalid expression node");
    }
    return value_none();
}

/* ---------------- Statement execution ---------------- */
static Signal exec_stmt(Node *n, Env *env, Value *retval) {
    if (!n) return SIG_NONE;
    switch (n->type) {
        case N_LET:
            env_define(env, n->str, eval(n->a, env));
            return SIG_NONE;

        case N_ASSIGN:
            if (!env_assign(env, n->str, eval(n->a, env)))
                runtime_error("assignment to undeclared variable '%s' (use 'let' first)", n->str);
            return SIG_NONE;

        case N_INPUT: {
            char line[4096];
            if (!fgets(line, sizeof(line), stdin)) line[0] = '\0';
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

            char *end;
            double num = strtod(line, &end);
            Value v;
            if (len > 0 && *end == '\0') v = value_num(num);
            else v = value_str(line);
            env_define(env, n->str, v);
            return SIG_NONE;
        }

        case N_PRINT: {
            int first = 1;
            for (NodeList *p = n->list; p; p = p->next) {
                if (!first) fputc(' ', stdout);
                first = 0;
                Value v = eval(p->node, env);
                char *s = value_to_display(v);
                fputs(s, stdout);
                free(s);
            }
            fputc('\n', stdout);
            return SIG_NONE;
        }

        case N_IF: {
            Env *child = env_new(env);
            if (is_truthy(eval(n->a, env)))
                return exec_stmt(n->b, child, retval);
            else if (n->c)
                return exec_stmt(n->c, child, retval);
            return SIG_NONE;
        }

        case N_WHILE: {
            while (is_truthy(eval(n->a, env))) {
                Env *child = env_new(env);
                Signal sig = exec_stmt(n->b, child, retval);
                if (sig == SIG_BREAK) break;
                if (sig == SIG_RETURN) return SIG_RETURN;
                /* SIG_CONTINUE / SIG_NONE: fall through to next iteration */
            }
            return SIG_NONE;
        }

        case N_FOR: {
            Env *loopEnv = env_new(env);
            exec_stmt(n->a, loopEnv, retval);           /* init */
            while (is_truthy(eval(n->b, loopEnv))) {      /* cond */
                Env *child = env_new(loopEnv);
                Signal sig = exec_stmt(n->d, child, retval); /* body */
                if (sig == SIG_BREAK) break;
                if (sig == SIG_RETURN) return SIG_RETURN;
                exec_stmt(n->c, loopEnv, retval);          /* update */
            }
            return SIG_NONE;
        }

        case N_BLOCK:
            return exec_block(n->list, env, retval);

        case N_FUNCDEF:
            func_define(n->str, n);
            return SIG_NONE;

        case N_RETURN:
            *retval = n->a ? eval(n->a, env) : value_none();
            return SIG_RETURN;

        case N_BREAK:
            return SIG_BREAK;

        case N_CONTINUE:
            return SIG_CONTINUE;

        case N_EXPRSTMT:
            eval(n->a, env);
            return SIG_NONE;

        default:
            runtime_error("invalid statement node");
    }
    return SIG_NONE;
}

static Signal exec_block(NodeList *list, Env *env, Value *retval) {
    for (NodeList *p = list; p; p = p->next) {
        Signal sig = exec_stmt(p->node, env, retval);
        if (sig != SIG_NONE) return sig;
    }
    return SIG_NONE;
}

/* ---------------- Entry point ---------------- */
void interpret(NodeList *program) {
    srand((unsigned int)time(NULL));
    g_global = env_new(NULL);

    /* Pre-scan: register all top-level function definitions first so
     * that functions may be called before their textual definition
     * (needed for mutual recursion / calling helpers defined lower down). */
    for (NodeList *p = program; p; p = p->next)
        if (p->node->type == N_FUNCDEF)
            func_define(p->node->str, p->node);

    Value retval = value_none();
    exec_block(program, g_global, &retval);
}
