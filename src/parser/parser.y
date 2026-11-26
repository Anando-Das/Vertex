%code requires {
    #include "../ast/ast.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include "../ast/ast.h"

extern int yylex(void);
extern int yylineno;
extern char *yytext;

void yyerror(const char *msg);

ASTNode *ast_root = NULL;
int syntax_errors = 0;
%}

%locations
%define parse.error verbose

%union {
    int int_val;
    double float_val;
    int bool_val;
    char *str_val;
    DataType data_type;
    ASTNode *node;
}

/* Keywords */
%token KW_INT KW_FLOAT KW_BOOL KW_IF KW_ELSE KW_WHILE KW_PRINT

/* Literals and Identifiers */
%token <int_val>   INT_LITERAL
%token <float_val> FLOAT_LITERAL
%token <bool_val>  BOOL_LITERAL
%token <str_val>   IDENTIFIER

/* Operators */
%token OP_PLUS OP_MINUS OP_MUL OP_DIV OP_MOD
%token OP_LT OP_GT OP_LE OP_GE OP_EQ OP_NEQ
%token OP_AND OP_OR OP_NOT
%token OP_ASSIGN

/* Delimiters */
%token LBRACE RBRACE LPAREN RPAREN SEMICOLON

/* Non-terminal semantic value types */
%type <data_type> type
%type <node> program stmt_list stmt matched_stmt unmatched_stmt simple_stmt
%type <node> block declaration assignment print_stmt
%type <node> expr or_expr and_expr equality_expr relational_expr
%type <node> additive_expr multiplicative_expr unary_expr primary_expr

%start program

%%

/*
 * Unambiguous Context-Free Grammar (CFG)
 *
 * - Dangling-else ambiguity is resolved formally via matched_stmt / unmatched_stmt.
 * - Operator precedence and associativity are encoded structurally in the
 *   expression hierarchy (lowest to highest):
 *     1. ||             (left-associative)
 *     2. &&             (left-associative)
 *     3. == !=          (left-associative)
 *     4. < > <= >=      (left-associative)
 *     5. + -            (left-associative)
 *     6. * / %          (left-associative)
 *     7. unary ! and -  (right-associative)
 *     8. literals, identifiers, parenthesized expressions
 */

program
    : stmt_list
      {
          int line = ($1 != NULL) ? $1->line : 1;
          ast_root = ast_create_program($1, line);
          $$ = ast_root;
      }
    ;

stmt_list
    : /* empty */
      { $$ = NULL; }
    | stmt_list stmt
      { $$ = ast_append_stmt($1, $2); }
    ;

stmt
    : matched_stmt
      { $$ = $1; }
    | unmatched_stmt
      { $$ = $1; }
    ;

matched_stmt
    : KW_IF LPAREN expr RPAREN matched_stmt KW_ELSE matched_stmt
      { $$ = ast_create_if($3, $5, $7, @1.first_line); }
    | KW_WHILE LPAREN expr RPAREN matched_stmt
      { $$ = ast_create_while($3, $5, @1.first_line); }
    | simple_stmt
      { $$ = $1; }
    | block
      { $$ = $1; }
    ;

unmatched_stmt
    : KW_IF LPAREN expr RPAREN stmt
      { $$ = ast_create_if($3, $5, NULL, @1.first_line); }
    | KW_IF LPAREN expr RPAREN matched_stmt KW_ELSE unmatched_stmt
      { $$ = ast_create_if($3, $5, $7, @1.first_line); }
    | KW_WHILE LPAREN expr RPAREN unmatched_stmt
      { $$ = ast_create_while($3, $5, @1.first_line); }
    ;

simple_stmt
    : declaration
      { $$ = $1; }
    | assignment
      { $$ = $1; }
    | print_stmt
      { $$ = $1; }
    | error SEMICOLON
      {
          yyerrok;
          $$ = NULL;
      }
    ;

block
    : LBRACE stmt_list RBRACE
      { $$ = ast_create_block($2, @1.first_line); }
    | LBRACE stmt_list error RBRACE
      {
          yyerrok;
          ast_free($2);
          $$ = NULL;
      }
    ;

type
    : KW_INT   { $$ = TYPE_INT; }
    | KW_FLOAT { $$ = TYPE_FLOAT; }
    | KW_BOOL  { $$ = TYPE_BOOL; }
    ;

declaration
    : type IDENTIFIER SEMICOLON
      {
          $$ = ast_create_var_decl($1, $2, NULL, @1.first_line);
          free($2);
      }
    | type IDENTIFIER OP_ASSIGN expr SEMICOLON
      {
          $$ = ast_create_var_decl($1, $2, $4, @1.first_line);
          free($2);
      }
    ;

assignment
    : IDENTIFIER OP_ASSIGN expr SEMICOLON
      {
          $$ = ast_create_assign($1, $3, @1.first_line);
          free($1);
      }
    ;

print_stmt
    : KW_PRINT expr SEMICOLON
      { $$ = ast_create_print($2, @1.first_line); }
    ;

expr
    : or_expr
      { $$ = $1; }
    ;

or_expr
    : and_expr
      { $$ = $1; }
    | or_expr OP_OR and_expr
      { $$ = ast_create_binary_op(OP_OR_AST, $1, $3, @2.first_line); }
    ;

and_expr
    : equality_expr
      { $$ = $1; }
    | and_expr OP_AND equality_expr
      { $$ = ast_create_binary_op(OP_AND_AST, $1, $3, @2.first_line); }
    ;

equality_expr
    : relational_expr
      { $$ = $1; }
    | equality_expr OP_EQ relational_expr
      { $$ = ast_create_binary_op(OP_EQ_AST, $1, $3, @2.first_line); }
    | equality_expr OP_NEQ relational_expr
      { $$ = ast_create_binary_op(OP_NEQ_AST, $1, $3, @2.first_line); }
    ;

relational_expr
    : additive_expr
      { $$ = $1; }
    | relational_expr OP_LT additive_expr
      { $$ = ast_create_binary_op(OP_LT_AST, $1, $3, @2.first_line); }
    | relational_expr OP_GT additive_expr
      { $$ = ast_create_binary_op(OP_GT_AST, $1, $3, @2.first_line); }
    | relational_expr OP_LE additive_expr
      { $$ = ast_create_binary_op(OP_LE_AST, $1, $3, @2.first_line); }
    | relational_expr OP_GE additive_expr
      { $$ = ast_create_binary_op(OP_GE_AST, $1, $3, @2.first_line); }
    ;

additive_expr
    : multiplicative_expr
      { $$ = $1; }
    | additive_expr OP_PLUS multiplicative_expr
      { $$ = ast_create_binary_op(OP_ADD_AST, $1, $3, @2.first_line); }
    | additive_expr OP_MINUS multiplicative_expr
      { $$ = ast_create_binary_op(OP_SUB_AST, $1, $3, @2.first_line); }
    ;

multiplicative_expr
    : unary_expr
      { $$ = $1; }
    | multiplicative_expr OP_MUL unary_expr
      { $$ = ast_create_binary_op(OP_MUL_AST, $1, $3, @2.first_line); }
    | multiplicative_expr OP_DIV unary_expr
      { $$ = ast_create_binary_op(OP_DIV_AST, $1, $3, @2.first_line); }
    | multiplicative_expr OP_MOD unary_expr
      { $$ = ast_create_binary_op(OP_MOD_AST, $1, $3, @2.first_line); }
    ;

unary_expr
    : primary_expr
      { $$ = $1; }
    | OP_MINUS unary_expr
      { $$ = ast_create_unary_op(OP_NEG_AST, $2, @1.first_line); }
    | OP_NOT unary_expr
      { $$ = ast_create_unary_op(OP_NOT_AST, $2, @1.first_line); }
    ;

primary_expr
    : INT_LITERAL
      { $$ = ast_create_int_literal($1, @1.first_line); }
    | FLOAT_LITERAL
      { $$ = ast_create_float_literal($1, @1.first_line); }
    | BOOL_LITERAL
      { $$ = ast_create_bool_literal($1, @1.first_line); }
    | IDENTIFIER
      {
          $$ = ast_create_identifier($1, @1.first_line);
          free($1);
      }
    | LPAREN expr RPAREN
      { $$ = $2; }
    ;

%%

void yyerror(const char *msg) {
    int line = (yylloc.first_line > 0) ? yylloc.first_line : yylineno;
    fprintf(stderr, "Syntax Error at line %d: %s\n", line, msg);
    syntax_errors++;
}
