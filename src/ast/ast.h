#ifndef AST_H
#define AST_H

#include <stdio.h>

/* Data types supported by the Mini Programming Language */
typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_UNKNOWN,
    TYPE_ERROR
} DataType;

/* Binary operators */
typedef enum {
    OP_ADD_AST,
    OP_SUB_AST,
    OP_MUL_AST,
    OP_DIV_AST,
    OP_MOD_AST,
    OP_LT_AST,
    OP_GT_AST,
    OP_LE_AST,
    OP_GE_AST,
    OP_EQ_AST,
    OP_NEQ_AST,
    OP_AND_AST,
    OP_OR_AST
} BinaryOp;

/* Unary operators */
typedef enum {
    OP_NEG_AST,
    OP_NOT_AST
} UnaryOp;

/* Abstract Syntax Tree node kinds */
typedef enum {
    AST_PROGRAM,
    AST_BLOCK,
    AST_VAR_DECL,
    AST_ASSIGN,
    AST_IF,
    AST_WHILE,
    AST_PRINT,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_INT_LITERAL,
    AST_FLOAT_LITERAL,
    AST_BOOL_LITERAL,
    AST_IDENTIFIER
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    int line;                 /* Source line number where this construct begins */
    DataType eval_type;       /* Inferred type (populated during semantic analysis) */
    struct ASTNode *next;     /* Linked list pointer for statement sequences */

    union {
        /* AST_PROGRAM, AST_BLOCK */
        struct {
            struct ASTNode *statements;
        } block;

        /* AST_VAR_DECL */
        struct {
            DataType var_type;
            char *name;
            struct ASTNode *init_expr; /* NULL if uninitialized */
        } var_decl;

        /* AST_ASSIGN */
        struct {
            char *name;
            struct ASTNode *value;
        } assign;

        /* AST_IF */
        struct {
            struct ASTNode *condition;
            struct ASTNode *then_branch;
            struct ASTNode *else_branch; /* NULL if no else clause */
        } if_stmt;

        /* AST_WHILE */
        struct {
            struct ASTNode *condition;
            struct ASTNode *body;
        } while_stmt;

        /* AST_PRINT */
        struct {
            struct ASTNode *expr;
        } print_stmt;

        /* AST_BINARY_OP */
        struct {
            BinaryOp op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binary_op;

        /* AST_UNARY_OP */
        struct {
            UnaryOp op;
            struct ASTNode *operand;
        } unary_op;

        /* AST_INT_LITERAL */
        struct {
            int value;
        } int_lit;

        /* AST_FLOAT_LITERAL */
        struct {
            double value;
        } float_lit;

        /* AST_BOOL_LITERAL */
        struct {
            int value; /* 0 = false, 1 = true */
        } bool_lit;

        /* AST_IDENTIFIER */
        struct {
            char *name;
        } ident;
    } data;
} ASTNode;

/* AST node constructors */
ASTNode *ast_create_program(ASTNode *statements, int line);
ASTNode *ast_create_block(ASTNode *statements, int line);
ASTNode *ast_create_var_decl(DataType var_type, const char *name, ASTNode *init_expr, int line);
ASTNode *ast_create_assign(const char *name, ASTNode *value, int line);
ASTNode *ast_create_if(ASTNode *condition, ASTNode *then_branch, ASTNode *else_branch, int line);
ASTNode *ast_create_while(ASTNode *condition, ASTNode *body, int line);
ASTNode *ast_create_print(ASTNode *expr, int line);
ASTNode *ast_create_binary_op(BinaryOp op, ASTNode *left, ASTNode *right, int line);
ASTNode *ast_create_unary_op(UnaryOp op, ASTNode *operand, int line);
ASTNode *ast_create_int_literal(int value, int line);
ASTNode *ast_create_float_literal(double value, int line);
ASTNode *ast_create_bool_literal(int value, int line);
ASTNode *ast_create_identifier(const char *name, int line);

/* Statement list helper */
ASTNode *ast_append_stmt(ASTNode *list, ASTNode *stmt);

/* String helpers */
const char *ast_data_type_to_str(DataType type);
const char *ast_binary_op_to_str(BinaryOp op);
const char *ast_unary_op_to_str(UnaryOp op);

/* Printing and cleanup */
void ast_print(const ASTNode *node, int indent);
void ast_free(ASTNode *node);

#endif /* AST_H */
