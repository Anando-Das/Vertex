#include "tac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *tac_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (!copy) {
        fprintf(stderr, "Fatal Error: Out of memory in TAC generator\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, s, len);
    return copy;
}

static char *new_temp(TACList *list) {
    char buf[32];
    list->temp_count++;
    snprintf(buf, sizeof(buf), "t%d", list->temp_count);
    return tac_strdup(buf);
}

static char *new_label(TACList *list) {
    char buf[32];
    list->label_count++;
    snprintf(buf, sizeof(buf), "L%d", list->label_count);
    return tac_strdup(buf);
}

static void emit(TACList *list, TACOp kind, const char *op,
                 const char *arg1, const char *arg2, const char *result) {
    TACInstruction *instr = (TACInstruction *)calloc(1, sizeof(TACInstruction));
    if (!instr) {
        fprintf(stderr, "Fatal Error: Out of memory allocating TAC instruction\n");
        exit(EXIT_FAILURE);
    }
    instr->kind = kind;
    instr->op = tac_strdup(op);
    instr->arg1 = tac_strdup(arg1);
    instr->arg2 = tac_strdup(arg2);
    instr->result = tac_strdup(result);
    instr->next = NULL;

    if (!list->head) {
        list->head = instr;
        list->tail = instr;
    } else {
        list->tail->next = instr;
        list->tail = instr;
    }
    list->instr_count++;
}

static char *gen_expr(TACList *list, const ASTNode *node);
static void gen_stmt(TACList *list, const ASTNode *node);
static void gen_stmt_list(TACList *list, const ASTNode *head);

static char *gen_expr(TACList *list, const ASTNode *node) {
    if (!node) return tac_strdup("0");

    char buf[64];

    switch (node->type) {
        case AST_INT_LITERAL:
            snprintf(buf, sizeof(buf), "%d", node->data.int_lit.value);
            return tac_strdup(buf);

        case AST_FLOAT_LITERAL:
            snprintf(buf, sizeof(buf), "%g", node->data.float_lit.value);
            return tac_strdup(buf);

        case AST_BOOL_LITERAL:
            return tac_strdup(node->data.bool_lit.value ? "true" : "false");

        case AST_IDENTIFIER:
            return tac_strdup(node->data.ident.name);

        case AST_BINARY_OP: {
            char *lhs = gen_expr(list, node->data.binary_op.left);
            char *rhs = gen_expr(list, node->data.binary_op.right);
            char *tmp = new_temp(list);
            const char *op_str = ast_binary_op_to_str(node->data.binary_op.op);

            emit(list, TAC_BINARY, op_str, lhs, rhs, tmp);

            free(lhs);
            free(rhs);
            return tmp;
        }

        case AST_UNARY_OP: {
            char *operand = gen_expr(list, node->data.unary_op.operand);
            char *tmp = new_temp(list);
            const char *op_str = ast_unary_op_to_str(node->data.unary_op.op);

            emit(list, TAC_UNARY, op_str, operand, NULL, tmp);

            free(operand);
            return tmp;
        }

        default:
            return tac_strdup("0");
    }
}

static void gen_stmt(TACList *list, const ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK:
            gen_stmt_list(list, node->data.block.statements);
            break;

        case AST_VAR_DECL:
            if (node->data.var_decl.init_expr) {
                char *rhs = gen_expr(list, node->data.var_decl.init_expr);
                emit(list, TAC_ASSIGN, NULL, rhs, NULL, node->data.var_decl.name);
                free(rhs);
            }
            break;

        case AST_ASSIGN: {
            char *rhs = gen_expr(list, node->data.assign.value);
            emit(list, TAC_ASSIGN, NULL, rhs, NULL, node->data.assign.name);
            free(rhs);
            break;
        }

        case AST_PRINT: {
            char *val = gen_expr(list, node->data.print_stmt.expr);
            emit(list, TAC_PRINT, NULL, val, NULL, NULL);
            free(val);
            break;
        }

        case AST_IF: {
            char *cond = gen_expr(list, node->data.if_stmt.condition);

            if (node->data.if_stmt.else_branch) {
                char *label_else = new_label(list);
                char *label_end = new_label(list);

                emit(list, TAC_IF_FALSE, NULL, cond, NULL, label_else);
                gen_stmt(list, node->data.if_stmt.then_branch);
                emit(list, TAC_GOTO, NULL, NULL, NULL, label_end);
                emit(list, TAC_LABEL, NULL, NULL, NULL, label_else);
                gen_stmt(list, node->data.if_stmt.else_branch);
                emit(list, TAC_LABEL, NULL, NULL, NULL, label_end);

                free(label_else);
                free(label_end);
            } else {
                char *label_end = new_label(list);

                emit(list, TAC_IF_FALSE, NULL, cond, NULL, label_end);
                gen_stmt(list, node->data.if_stmt.then_branch);
                emit(list, TAC_LABEL, NULL, NULL, NULL, label_end);

                free(label_end);
            }

            free(cond);
            break;
        }

        case AST_WHILE: {
            char *label_start = new_label(list);
            char *label_end = new_label(list);

            emit(list, TAC_LABEL, NULL, NULL, NULL, label_start);
            char *cond = gen_expr(list, node->data.while_stmt.condition);
            emit(list, TAC_IF_FALSE, NULL, cond, NULL, label_end);
            gen_stmt(list, node->data.while_stmt.body);
            emit(list, TAC_GOTO, NULL, NULL, NULL, label_start);
            emit(list, TAC_LABEL, NULL, NULL, NULL, label_end);

            free(cond);
            free(label_start);
            free(label_end);
            break;
        }

        default:
            break;
    }
}

static void gen_stmt_list(TACList *list, const ASTNode *head) {
    for (const ASTNode *curr = head; curr != NULL; curr = curr->next) {
        gen_stmt(list, curr);
    }
}

TACList *tac_generate(const ASTNode *root) {
    TACList *list = (TACList *)calloc(1, sizeof(TACList));
    if (!list) {
        fprintf(stderr, "Fatal Error: Out of memory creating TAC list\n");
        exit(EXIT_FAILURE);
    }
    if (root) {
        gen_stmt(list, root);
    }
    return list;
}

void tac_print(const TACList *list) {
    if (!list || !list->head) {
        printf("(no TAC instructions generated)\n");
        return;
    }

    for (const TACInstruction *instr = list->head; instr != NULL; instr = instr->next) {
        switch (instr->kind) {
            case TAC_ASSIGN:
                printf("  %s = %s\n", instr->result, instr->arg1);
                break;

            case TAC_BINARY:
                printf("  %s = %s %s %s\n",
                       instr->result, instr->arg1, instr->op, instr->arg2);
                break;

            case TAC_UNARY:
                printf("  %s = %s%s\n", instr->result, instr->op, instr->arg1);
                break;

            case TAC_LABEL:
                printf("%s:\n", instr->result);
                break;

            case TAC_GOTO:
                printf("  goto %s\n", instr->result);
                break;

            case TAC_IF_FALSE:
                printf("  ifFalse %s goto %s\n", instr->arg1, instr->result);
                break;

            case TAC_PRINT:
                printf("  print %s\n", instr->arg1);
                break;
        }
    }
}

void tac_free(TACList *list) {
    if (!list) return;
    TACInstruction *curr = list->head;
    while (curr) {
        TACInstruction *next = curr->next;
        free(curr->op);
        free(curr->arg1);
        free(curr->arg2);
        free(curr->result);
        free(curr);
        curr = next;
    }
    free(list);
}
