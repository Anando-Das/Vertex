#include "semantic.h"
#include <stdarg.h>
#include <stdio.h>

typedef struct {
    SymbolTable *symtab;
    int error_count;
} SemanticContext;

static void semantic_error(SemanticContext *ctx, int line, const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "Semantic Error at line %d: ", line);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    ctx->error_count++;
}

static int is_numeric(DataType type) {
    return (type == TYPE_INT || type == TYPE_FLOAT);
}

/*
 * Checks whether a value of `src_type` can be assigned to a variable of `dst_type`.
 * Allows exact type matches and safe numeric widening (int -> float).
 * Rejects narrowing (float -> int) and any conversion involving bool.
 */
static int is_assignable(DataType dst_type, DataType src_type) {
    if (dst_type == src_type) return 1;
    if (dst_type == TYPE_FLOAT && src_type == TYPE_INT) return 1;
    return 0;
}

static SymbolEntry *resolve_identifier(SemanticContext *ctx, const char *name, int line) {
    SymbolEntry *entry = symtab_lookup(ctx->symtab, name);
    if (entry) {
        return entry;
    }

    SymbolEntry *out_of_scope = symtab_lookup_out_of_scope(ctx->symtab, name);
    if (out_of_scope) {
        semantic_error(ctx, line,
                       "Scope violation - variable '%s' (declared at line %d in inner scope level %d) "
                       "is not visible outside its block",
                       name, out_of_scope->line_declared, out_of_scope->scope_level);
    } else {
        semantic_error(ctx, line, "Undeclared variable '%s'", name);
    }
    return NULL;
}

static DataType analyze_expr(SemanticContext *ctx, ASTNode *node);
static void analyze_stmt(SemanticContext *ctx, ASTNode *node);
static void analyze_stmt_list(SemanticContext *ctx, ASTNode *head);

static DataType analyze_binary_op(SemanticContext *ctx, ASTNode *node) {
    DataType left_type = analyze_expr(ctx, node->data.binary_op.left);
    DataType right_type = analyze_expr(ctx, node->data.binary_op.right);
    BinaryOp op = node->data.binary_op.op;
    const char *op_str = ast_binary_op_to_str(op);

    /* Suppress cascading errors if an operand already failed semantic checks */
    if (left_type == TYPE_ERROR || right_type == TYPE_ERROR) {
        node->eval_type = TYPE_ERROR;
        return TYPE_ERROR;
    }

    switch (op) {
        case OP_ADD_AST:
        case OP_SUB_AST:
        case OP_MUL_AST:
        case OP_DIV_AST:
            if (!is_numeric(left_type) || !is_numeric(right_type)) {
                semantic_error(ctx, node->line,
                               "Invalid expression - arithmetic operator '%s' requires numeric "
                               "(int/float) operands, got '%s' and '%s'",
                               op_str,
                               ast_data_type_to_str(left_type),
                               ast_data_type_to_str(right_type));
                node->eval_type = TYPE_ERROR;
                return TYPE_ERROR;
            }
            node->eval_type = (left_type == TYPE_FLOAT || right_type == TYPE_FLOAT)
                                  ? TYPE_FLOAT
                                  : TYPE_INT;
            return node->eval_type;

        case OP_MOD_AST:
            if (left_type != TYPE_INT || right_type != TYPE_INT) {
                semantic_error(ctx, node->line,
                               "Invalid expression - modulo operator '%%' requires 'int' operands, "
                               "got '%s' and '%s'",
                               ast_data_type_to_str(left_type),
                               ast_data_type_to_str(right_type));
                node->eval_type = TYPE_ERROR;
                return TYPE_ERROR;
            }
            node->eval_type = TYPE_INT;
            return TYPE_INT;

        case OP_LT_AST:
        case OP_GT_AST:
        case OP_LE_AST:
        case OP_GE_AST:
            if (!is_numeric(left_type) || !is_numeric(right_type)) {
                semantic_error(ctx, node->line,
                               "Invalid expression - relational operator '%s' requires numeric "
                               "(int/float) operands, got '%s' and '%s'",
                               op_str,
                               ast_data_type_to_str(left_type),
                               ast_data_type_to_str(right_type));
                node->eval_type = TYPE_ERROR;
                return TYPE_ERROR;
            }
            node->eval_type = TYPE_BOOL;
            return TYPE_BOOL;

        case OP_EQ_AST:
        case OP_NEQ_AST:
            if (!((is_numeric(left_type) && is_numeric(right_type)) ||
                  (left_type == TYPE_BOOL && right_type == TYPE_BOOL))) {
                semantic_error(ctx, node->line,
                               "Type mismatch - operator '%s' cannot compare '%s' with '%s'",
                               op_str,
                               ast_data_type_to_str(left_type),
                               ast_data_type_to_str(right_type));
                node->eval_type = TYPE_ERROR;
                return TYPE_ERROR;
            }
            node->eval_type = TYPE_BOOL;
            return TYPE_BOOL;

        case OP_AND_AST:
        case OP_OR_AST:
            if (left_type != TYPE_BOOL || right_type != TYPE_BOOL) {
                semantic_error(ctx, node->line,
                               "Invalid expression - logical operator '%s' requires 'bool' operands, "
                               "got '%s' and '%s'",
                               op_str,
                               ast_data_type_to_str(left_type),
                               ast_data_type_to_str(right_type));
                node->eval_type = TYPE_ERROR;
                return TYPE_ERROR;
            }
            node->eval_type = TYPE_BOOL;
            return TYPE_BOOL;
    }

    node->eval_type = TYPE_ERROR;
    return TYPE_ERROR;
}

static DataType analyze_unary_op(SemanticContext *ctx, ASTNode *node) {
    DataType operand_type = analyze_expr(ctx, node->data.unary_op.operand);
    UnaryOp op = node->data.unary_op.op;

    if (operand_type == TYPE_ERROR) {
        node->eval_type = TYPE_ERROR;
        return TYPE_ERROR;
    }

    switch (op) {
        case OP_NEG_AST:
            if (!is_numeric(operand_type)) {
                semantic_error(ctx, node->line,
                               "Invalid expression - unary '-' requires a numeric (int/float) "
                               "operand, got '%s'",
                               ast_data_type_to_str(operand_type));
                node->eval_type = TYPE_ERROR;
                return TYPE_ERROR;
            }
            node->eval_type = operand_type;
            return operand_type;

        case OP_NOT_AST:
            if (operand_type != TYPE_BOOL) {
                semantic_error(ctx, node->line,
                               "Invalid expression - logical '!' requires a 'bool' operand, "
                               "got '%s'",
                               ast_data_type_to_str(operand_type));
                node->eval_type = TYPE_ERROR;
                return TYPE_ERROR;
            }
            node->eval_type = TYPE_BOOL;
            return TYPE_BOOL;
    }

    node->eval_type = TYPE_ERROR;
    return TYPE_ERROR;
}

static DataType analyze_expr(SemanticContext *ctx, ASTNode *node) {
    if (!node) return TYPE_ERROR;

    switch (node->type) {
        case AST_INT_LITERAL:
            node->eval_type = TYPE_INT;
            return TYPE_INT;

        case AST_FLOAT_LITERAL:
            node->eval_type = TYPE_FLOAT;
            return TYPE_FLOAT;

        case AST_BOOL_LITERAL:
            node->eval_type = TYPE_BOOL;
            return TYPE_BOOL;

        case AST_IDENTIFIER: {
            SymbolEntry *entry = resolve_identifier(ctx, node->data.ident.name, node->line);
            if (!entry) {
                node->eval_type = TYPE_ERROR;
                return TYPE_ERROR;
            }
            node->eval_type = entry->type;
            return entry->type;
        }

        case AST_BINARY_OP:
            return analyze_binary_op(ctx, node);

        case AST_UNARY_OP:
            return analyze_unary_op(ctx, node);

        default:
            semantic_error(ctx, node->line, "Unexpected non-expression AST node in expression");
            node->eval_type = TYPE_ERROR;
            return TYPE_ERROR;
    }
}

static void analyze_branch_with_scope(SemanticContext *ctx, ASTNode *branch) {
    if (!branch) return;
    if (branch->type == AST_BLOCK) {
        /* AST_BLOCK manages its own scope entry and exit */
        analyze_stmt(ctx, branch);
    } else {
        /* Single-statement if/while body still gets its own isolated scope */
        symtab_enter_scope(ctx->symtab);
        analyze_stmt(ctx, branch);
        symtab_exit_scope(ctx->symtab);
    }
}

static void analyze_stmt(SemanticContext *ctx, ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            analyze_stmt_list(ctx, node->data.block.statements);
            break;

        case AST_BLOCK:
            symtab_enter_scope(ctx->symtab);
            analyze_stmt_list(ctx, node->data.block.statements);
            symtab_exit_scope(ctx->symtab);
            break;

        case AST_VAR_DECL: {
            const char *name = node->data.var_decl.name;
            DataType declared_type = node->data.var_decl.var_type;

            /* Analyze optional initializer first (before binding name in current scope) */
            if (node->data.var_decl.init_expr) {
                DataType init_type = analyze_expr(ctx, node->data.var_decl.init_expr);
                if (init_type != TYPE_ERROR && !is_assignable(declared_type, init_type)) {
                    semantic_error(ctx, node->line,
                                   "Type mismatch - cannot initialize '%s' variable '%s' with "
                                   "expression of type '%s'",
                                   ast_data_type_to_str(declared_type),
                                   name,
                                   ast_data_type_to_str(init_type));
                }
            }

            SymbolEntry *existing = symtab_lookup_current_scope(ctx->symtab, name);
            if (existing) {
                semantic_error(ctx, node->line,
                               "Redeclaration of variable '%s' in the same scope "
                               "(previously declared at line %d)",
                               name, existing->line_declared);
            } else {
                symtab_insert(ctx->symtab, name, declared_type, node->line);
            }
            node->eval_type = declared_type;
            break;
        }

        case AST_ASSIGN: {
            const char *name = node->data.assign.name;
            SymbolEntry *entry = resolve_identifier(ctx, name, node->line);
            DataType rhs_type = analyze_expr(ctx, node->data.assign.value);

            if (!entry) {
                node->eval_type = TYPE_ERROR;
                break;
            }

            if (rhs_type != TYPE_ERROR && !is_assignable(entry->type, rhs_type)) {
                semantic_error(ctx, node->line,
                               "Invalid assignment - cannot assign expression of type '%s' to "
                               "variable '%s' of type '%s'",
                               ast_data_type_to_str(rhs_type),
                               name,
                               ast_data_type_to_str(entry->type));
                node->eval_type = TYPE_ERROR;
            } else {
                node->eval_type = entry->type;
            }
            break;
        }

        case AST_IF: {
            DataType cond_type = analyze_expr(ctx, node->data.if_stmt.condition);
            if (cond_type != TYPE_ERROR && cond_type != TYPE_BOOL) {
                semantic_error(ctx, node->data.if_stmt.condition->line,
                               "Type mismatch - 'if' condition must be of type 'bool', got '%s'",
                               ast_data_type_to_str(cond_type));
            }
            analyze_branch_with_scope(ctx, node->data.if_stmt.then_branch);
            if (node->data.if_stmt.else_branch) {
                analyze_branch_with_scope(ctx, node->data.if_stmt.else_branch);
            }
            break;
        }

        case AST_WHILE: {
            DataType cond_type = analyze_expr(ctx, node->data.while_stmt.condition);
            if (cond_type != TYPE_ERROR && cond_type != TYPE_BOOL) {
                semantic_error(ctx, node->data.while_stmt.condition->line,
                               "Type mismatch - 'while' condition must be of type 'bool', got '%s'",
                               ast_data_type_to_str(cond_type));
            }
            analyze_branch_with_scope(ctx, node->data.while_stmt.body);
            break;
        }

        case AST_PRINT: {
            DataType expr_type = analyze_expr(ctx, node->data.print_stmt.expr);
            node->eval_type = expr_type;
            break;
        }

        default:
            analyze_expr(ctx, node);
            break;
    }
}

static void analyze_stmt_list(SemanticContext *ctx, ASTNode *head) {
    for (ASTNode *curr = head; curr != NULL; curr = curr->next) {
        analyze_stmt(ctx, curr);
    }
}

int semantic_analyze(ASTNode *root, SymbolTable *symtab) {
    if (!root || !symtab) return 0;

    SemanticContext ctx;
    ctx.symtab = symtab;
    ctx.error_count = 0;

    analyze_stmt(&ctx, root);
    return ctx.error_count;
}
