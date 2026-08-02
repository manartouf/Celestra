%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "parser.tab.h"

static char *process_string(const char *raw);
%}

%option noyywrap yylineno

DIGIT   [0-9]
ID      [a-zA-Z_][a-zA-Z0-9_]*

%%

"#"[^\n]*                          { /* line comment, skipped */ }
[ \t\r]+                           { /* whitespace, skipped */ }
\n                                  { /* newline, skipped (yylineno auto-tracked) */ }

"let"                                { return LET; }
"if"                                  { return IF; }
"else"                                { return ELSE; }
"while"                               { return WHILE; }
"for"                                 { return FOR; }
"print"                               { return PRINT; }
"input"                               { return INPUT; }
"def"                                 { return DEF; }
"return"                              { return RETURN; }
"true"                                { return TRUE; }
"false"                               { return FALSE; }
"and"                                 { return AND; }
"or"                                  { return OR; }
"not"                                 { return NOT; }
"break"                               { return BREAK; }
"continue"                            { return CONTINUE; }

">="                                   { return GE; }
"<="                                   { return LE; }
"=="                                   { return EQ; }
"!="                                   { return NE; }
"+="                                   { return PLUSEQ; }
"-="                                   { return MINUSEQ; }
"*="                                   { return MULEQ; }
"/="                                   { return DIVEQ; }

{DIGIT}+(\.{DIGIT}+)?                  { yylval.num = atof(yytext); return NUMBER; }

\"([^"\\]|\\.)*\"                       { yylval.str = process_string(yytext); return STRING; }

{ID}                                    { yylval.str = strdup(yytext); return IDENT; }

[-+*/%()=<>{};,\[\]]                    { return yytext[0]; }

.                                        { fprintf(stderr, "Celestra Lexer Error (line %d): unexpected character '%s'\n", yylineno, yytext); }

%%

/* Strip surrounding quotes and resolve \n \t \" \\ escapes. */
static char *process_string(const char *raw) {
    size_t len = strlen(raw);
    char *out = malloc(len);
    size_t j = 0;
    for (size_t i = 1; i + 1 < len; i++) {
        if (raw[i] == '\\' && i + 2 < len) {
            i++;
            switch (raw[i]) {
                case 'n': out[j++] = '\n'; break;
                case 't': out[j++] = '\t'; break;
                case '"': out[j++] = '"';  break;
                case '\\': out[j++] = '\\'; break;
                default: out[j++] = raw[i]; break;
            }
        } else {
            out[j++] = raw[i];
        }
    }
    out[j] = '\0';
    return out;
}
