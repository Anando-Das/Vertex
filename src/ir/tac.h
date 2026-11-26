#ifndef TAC_H
#define TAC_H

#include "../ast/ast.h"

/* Kinds of Three Address Code (TAC) instructions */
typedef enum {
    TAC_ASSIGN,    /* result = arg1 */
    TAC_BINARY,    /* result = arg1 op arg2 */
    TAC_UNARY,     /* result = op arg1 */
    TAC_LABEL,     /* result: */
    TAC_GOTO,      /* goto result */
    TAC_IF_FALSE,  /* ifFalse arg1 goto result */
    TAC_PRINT      /* print arg1 */
} TACOp;

/* Single linear TAC instruction */
typedef struct TACInstruction {
    TACOp kind;
    char *op;      /* Operator string for TAC_BINARY / TAC_UNARY */
    char *arg1;
    char *arg2;
    char *result;  /* Target variable/temp or jump label */
    struct TACInstruction *next;
} TACInstruction;

/* List of generated TAC instructions and generator counters */
typedef struct TACList {
    TACInstruction *head;
    TACInstruction *tail;
    int temp_count;
    int label_count;
    int instr_count;
} TACList;

/* Generate Three Address Code from a validated AST */
TACList *tac_generate(const ASTNode *root);

/* Print the generated TAC instructions */
void tac_print(const TACList *list);

/* Free all memory owned by a TACList */
void tac_free(TACList *list);

#endif /* TAC_H */
