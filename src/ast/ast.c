#include "ast.h"
#include <stdlib.h>
#include <string.h>

static char *ast_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (!copy) {
        fprintf(stderr, "Fatal Error: Out of memory allocating string\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, s, len);
    return copy;
}

static ASTNode *ast_alloc_node(ASTNodeType type, int line) {
    ASTNode *node = (ASTNode *)calloc(1, sizeof(ASTNode));
    if (!node) {
        fprintf(stderr, "Fatal Error: Out of memory allocating AST node\n");
        exit(EXIT_FAILURE);
    }
    node->type = type;
    node->line = line;
    node->eval_type = TYPE_UNKNOWN;
    node->next = NULL;
    return node;
}

ASTNode *ast_create_program(ASTNode *statements, int line) {
    ASTNode *node = ast_alloc_node(AST_PROGRAM, line);
    node->data.block.statements = statements;
    return node;
}

ASTNode *ast_create_block(ASTNode *statements, int line) {
    ASTNode *node = ast_alloc_node(AST_BLOCK, line);
    node->data.block.statements = statements;
    return node;
}

ASTNode *ast_create_var_decl(DataType var_type, const char *name, ASTNode *init_expr, int line) {
    ASTNode *node = ast_alloc_node(AST_VAR_DECL, line);
    node->data.var_decl.var_type = var_type;
    node->data.var_decl.name = ast_strdup(name);
    node->data.var_decl.init_expr = init_expr;
    return node;
}

ASTNode *ast_create_assign(const char *name, ASTNode *value, int line) {
    ASTNode *node = ast_alloc_node(AST_ASSIGN, line);
    node->data.assign.name = ast_strdup(name);
    node->data.assign.value = value;
    return node;
}

ASTNode *ast_create_if(ASTNode *condition, ASTNode *then_branch, ASTNode *else_branch, int line) {
    ASTNode *node = ast_alloc_node(AST_IF, line);
    node->data.if_stmt.condition = condition;
    node->data.if_stmt.then_branch = then_branch;
    node->data.if_stmt.else_branch = else_branch;
    return node;
}

ASTNode *ast_create_while(ASTNode *condition, ASTNode *body, int line) {
    ASTNode *node = ast_alloc_node(AST_WHILE, line);
    node->data.while_stmt.condition = condition;
    node->data.while_stmt.body = body;
    return node;
}

ASTNode *ast_create_print(ASTNode *expr, int line) {
    ASTNode *node = ast_alloc_node(AST_PRINT, line);
    node->data.print_stmt.expr = expr;
    return node;
}

ASTNode *ast_create_binary_op(BinaryOp op, ASTNode *left, ASTNode *right, int line) {
    ASTNode *node = ast_alloc_node(AST_BINARY_OP, line);
    node->data.binary_op.op = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    return node;
}

ASTNode *ast_create_unary_op(UnaryOp op, ASTNode *operand, int line) {
    ASTNode *node = ast_alloc_node(AST_UNARY_OP, line);
    node->data.unary_op.op = op;
    node->data.unary_op.operand = operand;
    return node;
}

ASTNode *ast_create_int_literal(int value, int line) {
    ASTNode *node = ast_alloc_node(AST_INT_LITERAL, line);
    node->data.int_lit.value = value;
    node->eval_type = TYPE_INT;
    return node;
}

ASTNode *ast_create_float_literal(double value, int line) {
    ASTNode *node = ast_alloc_node(AST_FLOAT_LITERAL, line);
    node->data.float_lit.value = value;
    node->eval_type = TYPE_FLOAT;
    return node;
}

ASTNode *ast_create_bool_literal(int value, int line) {
    ASTNode *node = ast_alloc_node(AST_BOOL_LITERAL, line);
    node->data.bool_lit.value = value ? 1 : 0;
    node->eval_type = TYPE_BOOL;
    return node;
}

ASTNode *ast_create_identifier(const char *name, int line) {
    ASTNode *node = ast_alloc_node(AST_IDENTIFIER, line);
    node->data.ident.name = ast_strdup(name);
    return node;
}

ASTNode *ast_append_stmt(ASTNode *list, ASTNode *stmt) {
    if (!stmt) return list;
    if (!list) return stmt;
    ASTNode *curr = list;
    while (curr->next) {
        curr = curr->next;
    }
    curr->next = stmt;
    return list;
}

const char *ast_data_type_to_str(DataType type) {
    switch (type) {
        case TYPE_INT:     return "int";
        case TYPE_FLOAT:   return "float";
        case TYPE_BOOL:    return "bool";
        case TYPE_ERROR:   return "<error>";
        case TYPE_UNKNOWN:
        default:           return "<unknown>";
    }
}

const char *ast_binary_op_to_str(BinaryOp op) {
    switch (op) {
        case OP_ADD_AST: return "+";
        case OP_SUB_AST: return "-";
        case OP_MUL_AST: return "*";
        case OP_DIV_AST: return "/";
        case OP_MOD_AST: return "%";
        case OP_LT_AST:  return "<";
        case OP_GT_AST:  return ">";
        case OP_LE_AST:  return "<=";
        case OP_GE_AST:  return ">=";
        case OP_EQ_AST:  return "==";
        case OP_NEQ_AST: return "!=";
        case OP_AND_AST: return "&&";
        case OP_OR_AST:  return "||";
        default:         return "?";
    }
}

const char *ast_unary_op_to_str(UnaryOp op) {
    switch (op) {
        case OP_NEG_AST: return "-";
        case OP_NOT_AST: return "!";
        default:         return "?";
    }
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

static void ast_print_stmt_list(const ASTNode *head, int indent) {
    const ASTNode *curr = head;
    while (curr) {
        ast_print(curr, indent);
        curr = curr->next;
    }
}

void ast_print(const ASTNode *node, int indent) {
    if (!node) return;

    print_indent(indent);

    switch (node->type) {
        case AST_PROGRAM:
            printf("Program (line %d)\n", node->line);
            ast_print_stmt_list(node->data.block.statements, indent + 1);
            break;

        case AST_BLOCK:
            printf("Block (line %d)\n", node->line);
            ast_print_stmt_list(node->data.block.statements, indent + 1);
            break;

        case AST_VAR_DECL:
            printf("VarDecl (type: %s, name: %s, line %d)\n",
                   ast_data_type_to_str(node->data.var_decl.var_type),
                   node->data.var_decl.name,
                   node->line);
            if (node->data.var_decl.init_expr) {
                print_indent(indent + 1);
                printf("Init:\n");
                ast_print(node->data.var_decl.init_expr, indent + 2);
            }
            break;

        case AST_ASSIGN:
            printf("Assign (target: %s, line %d)\n",
                   node->data.assign.name,
                   node->line);
            ast_print(node->data.assign.value, indent + 1);
            break;

        case AST_IF:
            printf("%s (line %d)\n",
                   node->data.if_stmt.else_branch ? "IfElse" : "If",
                   node->line);
            print_indent(indent + 1);
            printf("Condition:\n");
            ast_print(node->data.if_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("Then:\n");
            ast_print(node->data.if_stmt.then_branch, indent + 2);
            if (node->data.if_stmt.else_branch) {
                print_indent(indent + 1);
                printf("Else:\n");
                ast_print(node->data.if_stmt.else_branch, indent + 2);
            }
            break;

        case AST_WHILE:
            printf("While (line %d)\n", node->line);
            print_indent(indent + 1);
            printf("Condition:\n");
            ast_print(node->data.while_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("Body:\n");
            ast_print(node->data.while_stmt.body, indent + 2);
            break;

        case AST_PRINT:
            printf("Print (line %d)\n", node->line);
            ast_print(node->data.print_stmt.expr, indent + 1);
            break;

        case AST_BINARY_OP:
            if (node->eval_type != TYPE_UNKNOWN && node->eval_type != TYPE_ERROR) {
                printf("BinaryOp '%s' [%s] (line %d)\n",
                       ast_binary_op_to_str(node->data.binary_op.op),
                       ast_data_type_to_str(node->eval_type),
                       node->line);
            } else {
                printf("BinaryOp '%s' (line %d)\n",
                       ast_binary_op_to_str(node->data.binary_op.op),
                       node->line);
            }
            ast_print(node->data.binary_op.left, indent + 1);
            ast_print(node->data.binary_op.right, indent + 1);
            break;

        case AST_UNARY_OP:
            if (node->eval_type != TYPE_UNKNOWN && node->eval_type != TYPE_ERROR) {
                printf("UnaryOp '%s' [%s] (line %d)\n",
                       ast_unary_op_to_str(node->data.unary_op.op),
                       ast_data_type_to_str(node->eval_type),
                       node->line);
            } else {
                printf("UnaryOp '%s' (line %d)\n",
                       ast_unary_op_to_str(node->data.unary_op.op),
                       node->line);
            }
            ast_print(node->data.unary_op.operand, indent + 1);
            break;

        case AST_INT_LITERAL:
            printf("IntLiteral: %d (line %d)\n",
                   node->data.int_lit.value,
                   node->line);
            break;

        case AST_FLOAT_LITERAL:
            printf("FloatLiteral: %g (line %d)\n",
                   node->data.float_lit.value,
                   node->line);
            break;

        case AST_BOOL_LITERAL:
            printf("BoolLiteral: %s (line %d)\n",
                   node->data.bool_lit.value ? "true" : "false",
                   node->line);
            break;

        case AST_IDENTIFIER:
            if (node->eval_type != TYPE_UNKNOWN && node->eval_type != TYPE_ERROR) {
                printf("Identifier: %s [%s] (line %d)\n",
                       node->data.ident.name,
                       ast_data_type_to_str(node->eval_type),
                       node->line);
            } else {
                printf("Identifier: %s (line %d)\n",
                       node->data.ident.name,
                       node->line);
            }
            break;
    }
}

void ast_free(ASTNode *node) {
    while (node) {
        ASTNode *next = node->next;
        switch (node->type) {
            case AST_PROGRAM:
            case AST_BLOCK:
                ast_free(node->data.block.statements);
                break;

            case AST_VAR_DECL:
                free(node->data.var_decl.name);
                ast_free(node->data.var_decl.init_expr);
                break;

            case AST_ASSIGN:
                free(node->data.assign.name);
                ast_free(node->data.assign.value);
                break;

            case AST_IF:
                ast_free(node->data.if_stmt.condition);
                ast_free(node->data.if_stmt.then_branch);
                ast_free(node->data.if_stmt.else_branch);
                break;

            case AST_WHILE:
                ast_free(node->data.while_stmt.condition);
                ast_free(node->data.while_stmt.body);
                break;

            case AST_PRINT:
                ast_free(node->data.print_stmt.expr);
                break;

            case AST_BINARY_OP:
                ast_free(node->data.binary_op.left);
                ast_free(node->data.binary_op.right);
                break;

            case AST_UNARY_OP:
                ast_free(node->data.unary_op.operand);
                break;

            case AST_IDENTIFIER:
                free(node->data.ident.name);
                break;

            case AST_INT_LITERAL:
            case AST_FLOAT_LITERAL:
            case AST_BOOL_LITERAL:
                break;
        }
        free(node);
        node = next;
    }
}
