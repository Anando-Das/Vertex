#include <stdio.h>
#include <stdlib.h>
#include "ast/ast.h"
#include "parser/parser.tab.h"
#include "symbol_table/symbol_table.h"
#include "semantic/semantic.h"
#include "ir/tac.h"

extern FILE *yyin;
extern int yyparse(void);
extern ASTNode *ast_root;
extern int lexical_errors;
extern int syntax_errors;

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [source_file]\n", prog_name);
    fprintf(stderr, "If no source_file is given, reads from standard input.\n");
}

int main(int argc, char **argv) {
    const char *input_name = "<stdin>";

    if (argc > 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2) {
        input_name = argv[1];
        yyin = fopen(input_name, "r");
        if (!yyin) {
            fprintf(stderr, "Error: Could not open input file '%s'\n", input_name);
            return EXIT_FAILURE;
        }
    } else {
        yyin = stdin;
    }

    /* Phases 1 & 2 & 3: Lexical Analysis, Syntax Analysis, AST Construction */
    int parse_result = yyparse();

    if (argc == 2 && yyin) {
        fclose(yyin);
    }

    if (parse_result != 0 || lexical_errors > 0 || syntax_errors > 0) {
        fprintf(stderr,
                "\nCompilation failed for '%s': %d lexical error(s), %d syntax error(s). "
                "Semantic analysis and TAC generation skipped.\n",
                input_name, lexical_errors, syntax_errors);
        ast_free(ast_root);
        return EXIT_FAILURE;
    }

    /* Phase 4: Symbol Table & Semantic Analysis */
    SymbolTable *symtab = symtab_create();
    int semantic_errors = semantic_analyze(ast_root, symtab);

    if (semantic_errors > 0) {
        fprintf(stderr,
                "\nSemantic analysis failed for '%s' with %d error(s). "
                "TAC generation skipped.\n",
                input_name, semantic_errors);
        symtab_destroy(symtab);
        ast_free(ast_root);
        return EXIT_FAILURE;
    }

    /* Phase 5: Intermediate Code Generation (Three Address Code) */
    TACList *tac = tac_generate(ast_root);

    printf("=== Abstract Syntax Tree (%s) ===\n", input_name);
    ast_print(ast_root, 0);

    printf("\n=== Semantic Analysis ===\n");
    printf("Status: PASSED (0 semantic errors)\n\n");
    symtab_print(symtab);

    printf("\n=== Three Address Code (TAC) ===\n");
    tac_print(tac);

    tac_free(tac);
    symtab_destroy(symtab);
    ast_free(ast_root);
    return EXIT_SUCCESS;
}
