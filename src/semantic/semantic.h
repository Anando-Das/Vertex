#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../ast/ast.h"
#include "../symbol_table/symbol_table.h"

/*
 * Performs semantic analysis over the AST:
 *   - Populates and validates the scoped symbol table
 *   - Checks undeclared variables, redeclarations, and scope violations
 *   - Type-checks expressions, assignments, declarations, and control conditions
 *   - Annotates expression AST nodes with their inferred eval_type
 *
 * Returns the total number of semantic errors detected (0 means valid).
 */
int semantic_analyze(ASTNode *root, SymbolTable *symtab);

#endif /* SEMANTIC_H */
