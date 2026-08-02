%{
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "interpreter.h"

extern int yylex(void);
extern int yylineno;
extern char *yytext;
extern FILE *yyin;

void yyerror(const char *s);

NodeList *g_program = NULL;   /* top-level statement list, filled in by grammar */
%}

%union {
    double  num;
    char   *str;
    Node   *node;
    NodeList *list;
}

%token <str> IDENT STRING
%token <num> NUMBER
%token LET IF ELSE WHILE FOR PRINT INPUT DEF RETURN
%token TRUE FALSE AND OR NOT BREAK CONTINUE
%token GE LE EQ NE
%token PLUSEQ MINUSEQ MULEQ DIVEQ

%type <node> stmt simple_stmt let_stmt assign_stmt print_stmt input_stmt
%type <node> if_stmt while_stmt for_stmt func_def return_stmt block expr_stmt
%type <node> break_stmt continue_stmt for_init for_update
%type <node> expr
%type <list> stmt_list arg_list param_list

%right '='
%left OR
%left AND
%nonassoc NOT
%nonassoc '>' '<' GE LE EQ NE
%left '+' '-'
%left '*' '/' '%'
%right UMINUS
%left '['

%start program

%%

program:
      stmt_list                { g_program = $1; }
    ;

stmt_list:
      /* empty */               { $$ = NULL; }
    | stmt_list stmt             { $$ = list_append($1, $2); }
    ;

stmt:
      simple_stmt ';'            { $$ = $1; }
    | if_stmt                    { $$ = $1; }
    | while_stmt                 { $$ = $1; }
    | for_stmt                   { $$ = $1; }
    | func_def                   { $$ = $1; }
    | block                      { $$ = $1; }
    ;

simple_stmt:
      let_stmt                   { $$ = $1; }
    | assign_stmt                { $$ = $1; }
    | print_stmt                 { $$ = $1; }
    | input_stmt                 { $$ = $1; }
    | return_stmt                { $$ = $1; }
    | break_stmt                 { $$ = $1; }
    | continue_stmt              { $$ = $1; }
    | expr_stmt                  { $$ = $1; }
    ;

let_stmt:
      LET IDENT '=' expr          { $$ = node_let($2, $4); free($2); }
    ;

assign_stmt:
      IDENT '=' expr              { $$ = node_assign($1, $3); free($1); }
    | IDENT PLUSEQ expr           { $$ = node_assign($1, node_binop(OP_ADD, node_ident($1), $3)); free($1); }
    | IDENT MINUSEQ expr          { $$ = node_assign($1, node_binop(OP_SUB, node_ident($1), $3)); free($1); }
    | IDENT MULEQ expr            { $$ = node_assign($1, node_binop(OP_MUL, node_ident($1), $3)); free($1); }
    | IDENT DIVEQ expr            { $$ = node_assign($1, node_binop(OP_DIV, node_ident($1), $3)); free($1); }
    ;

print_stmt:
      PRINT '(' arg_list ')'      { $$ = node_print($3); }
    ;

input_stmt:
      INPUT '(' IDENT ')'         { $$ = node_input($3); free($3); }
    ;

return_stmt:
      RETURN expr                 { $$ = node_return($2); }
    | RETURN                      { $$ = node_return(NULL); }
    ;

break_stmt:
      BREAK                       { $$ = node_break(); }
    ;

continue_stmt:
      CONTINUE                    { $$ = node_continue(); }
    ;

expr_stmt:
      expr                        { $$ = node_exprstmt($1); }
    ;

block:
      '{' stmt_list '}'           { $$ = node_block($2); }
    ;

if_stmt:
      IF '(' expr ')' block ELSE block   { $$ = node_if($3, $5, $7); }
    | IF '(' expr ')' block             { $$ = node_if($3, $5, NULL); }
    ;

while_stmt:
      WHILE '(' expr ')' block    { $$ = node_while($3, $5); }
    ;

for_init:
      LET IDENT '=' expr          { $$ = node_let($2, $4); free($2); }
    | IDENT '=' expr              { $$ = node_assign($1, $3); free($1); }
    ;

for_update:
      IDENT '=' expr              { $$ = node_assign($1, $3); free($1); }
    | IDENT PLUSEQ expr           { $$ = node_assign($1, node_binop(OP_ADD, node_ident($1), $3)); free($1); }
    | IDENT MINUSEQ expr          { $$ = node_assign($1, node_binop(OP_SUB, node_ident($1), $3)); free($1); }
    | IDENT MULEQ expr            { $$ = node_assign($1, node_binop(OP_MUL, node_ident($1), $3)); free($1); }
    | IDENT DIVEQ expr            { $$ = node_assign($1, node_binop(OP_DIV, node_ident($1), $3)); free($1); }
    ;

for_stmt:
      FOR '(' for_init ';' expr ';' for_update ')' block
                                    { $$ = node_for($3, $5, $7, $9); }
    ;

func_def:
      DEF IDENT '(' param_list ')' block
                                    { $$ = node_funcdef($2, $4, $6); free($2); }
    ;

param_list:
      /* empty */                  { $$ = NULL; }
    | IDENT                        { $$ = list_append(NULL, node_ident($1)); free($1); }
    | param_list ',' IDENT         { $$ = list_append($1, node_ident($3)); free($3); }
    ;

arg_list:
      /* empty */                  { $$ = NULL; }
    | expr                         { $$ = list_append(NULL, $1); }
    | arg_list ',' expr            { $$ = list_append($1, $3); }
    ;

expr:
      NUMBER                       { $$ = node_num($1); }
    | STRING                       { $$ = node_str($1); free($1); }
    | TRUE                         { $$ = node_bool(1); }
    | FALSE                        { $$ = node_bool(0); }
    | IDENT                        { $$ = node_ident($1); free($1); }
    | IDENT '(' arg_list ')'       { $$ = node_call($1, $3); free($1); }
    | '(' expr ')'                 { $$ = $2; }
    | '[' arg_list ']'             { $$ = node_array_lit($2); }
    | expr '[' expr ']'            { $$ = node_index($1, $3); }
    | '-' expr %prec UMINUS        { $$ = node_neg($2); }
    | NOT expr                     { $$ = node_not($2); }
    | expr '+' expr                { $$ = node_binop(OP_ADD, $1, $3); }
    | expr '-' expr                { $$ = node_binop(OP_SUB, $1, $3); }
    | expr '*' expr                { $$ = node_binop(OP_MUL, $1, $3); }
    | expr '/' expr                { $$ = node_binop(OP_DIV, $1, $3); }
    | expr '%' expr                { $$ = node_binop(OP_MOD, $1, $3); }
    | expr '>' expr                { $$ = node_binop(OP_GT,  $1, $3); }
    | expr '<' expr                { $$ = node_binop(OP_LT,  $1, $3); }
    | expr GE expr                 { $$ = node_binop(OP_GE,  $1, $3); }
    | expr LE expr                 { $$ = node_binop(OP_LE,  $1, $3); }
    | expr EQ expr                 { $$ = node_binop(OP_EQ,  $1, $3); }
    | expr NE expr                 { $$ = node_binop(OP_NE,  $1, $3); }
    | expr AND expr                { $$ = node_binop(OP_AND, $1, $3); }
    | expr OR expr                 { $$ = node_binop(OP_OR,  $1, $3); }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Celestra Syntax Error (line %d): %s near '%s'\n", yylineno, s, yytext);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program.cel>\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "Celestra: could not open file '%s'\n", argv[1]);
        return 1;
    }
    yyin = f;
    if (yyparse() != 0) {
        fclose(f);
        return 1;
    }
    fclose(f);

    interpret(g_program);
    return 0;
}
