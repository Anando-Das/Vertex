%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ast/ast.h"
#include "../parser/parser.tab.h"

int lexical_errors = 0;
static int comment_start_line = 0;

static char *lex_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (!copy) {
        fprintf(stderr, "Fatal Error: Out of memory in lexer\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, s, len);
    return copy;
}

/* Keep Bison's yylloc synchronized with Flex's yylineno */
#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno;
%}

%option noyywrap
%option yylineno
%option nounput
%option noinput

%x COMMENT

DIGIT       [0-9]
LETTER      [a-zA-Z_]
ID          {LETTER}({LETTER}|{DIGIT})*
INT_LIT     {DIGIT}+
FLOAT_LIT   {DIGIT}+"."{DIGIT}+

%%

"//"[^\r\n]*            { /* Discard single-line comment */ }

"/*"                    { comment_start_line = yylineno; BEGIN(COMMENT); }
<COMMENT>"*/"           { BEGIN(INITIAL); }
<COMMENT>\r?\n          { /* Line number automatically incremented by yylineno */ }
<COMMENT>.              { /* Discard block comment content */ }
<COMMENT><<EOF>>        {
                            fprintf(stderr, "Lexical Error at line %d: Unterminated block comment\n",
                                    comment_start_line);
                            lexical_errors++;
                            BEGIN(INITIAL);
                            return 0;
                        }

[ \t\r\n]+              { /* Discard whitespace */ }

    /* Keywords (must appear before generic identifier pattern) */
"int"                   { return KW_INT; }
"float"                 { return KW_FLOAT; }
"bool"                  { return KW_BOOL; }
"if"                    { return KW_IF; }
"else"                  { return KW_ELSE; }
"while"                 { return KW_WHILE; }
"print"                 { return KW_PRINT; }
"true"                  { yylval.bool_val = 1; return BOOL_LITERAL; }
"false"                 { yylval.bool_val = 0; return BOOL_LITERAL; }

    /* Multi-character relational and logical operators */
"=="                    { return OP_EQ; }
"!="                    { return OP_NEQ; }
"<="                    { return OP_LE; }
">="                    { return OP_GE; }
"&&"                    { return OP_AND; }
"||"                    { return OP_OR; }

    /* Single-character arithmetic, relational, logical, and assignment operators */
"+"                     { return OP_PLUS; }
"-"                     { return OP_MINUS; }
"*"                     { return OP_MUL; }
"/"                     { return OP_DIV; }
"%"                     { return OP_MOD; }
"<"                     { return OP_LT; }
">"                     { return OP_GT; }
"!"                     { return OP_NOT; }
"="                     { return OP_ASSIGN; }

    /* Delimiters */
"{"                     { return LBRACE; }
"}"                     { return RBRACE; }
"("                     { return LPAREN; }
")"                     { return RPAREN; }
";"                     { return SEMICOLON; }

    /* Malformed numeric / identifier tokens */
{FLOAT_LIT}({LETTER}|".")({LETTER}|{DIGIT}|".")* {
                            fprintf(stderr, "Lexical Error at line %d: Invalid floating-point literal '%s'\n",
                                    yylineno, yytext);
                            lexical_errors++;
                        }
{INT_LIT}{LETTER}({LETTER}|{DIGIT})* {
                            fprintf(stderr, "Lexical Error at line %d: Invalid identifier or numeric literal '%s'\n",
                                    yylineno, yytext);
                            lexical_errors++;
                        }
{DIGIT}+"."             {
                            fprintf(stderr, "Lexical Error at line %d: Malformed floating-point literal '%s'\n",
                                    yylineno, yytext);
                            lexical_errors++;
                        }
"."{DIGIT}+             {
                            fprintf(stderr, "Lexical Error at line %d: Malformed floating-point literal '%s'\n",
                                    yylineno, yytext);
                            lexical_errors++;
                        }

    /* Valid literals and identifiers */
{FLOAT_LIT}             { yylval.float_val = atof(yytext); return FLOAT_LITERAL; }
{INT_LIT}               { yylval.int_val = atoi(yytext); return INT_LITERAL; }
{ID}                    { yylval.str_val = lex_strdup(yytext); return IDENTIFIER; }

    /* Any unrecognized character is a lexical error */
.                       {
                            fprintf(stderr, "Lexical Error at line %d: Invalid token '%s'\n",
                                    yylineno, yytext);
                            lexical_errors++;
                        }

%%
