#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "../ast/ast.h"

/* Single symbol entry stored in a scope */
typedef struct SymbolEntry {
    char *name;
    DataType type;
    int scope_level;
    int scope_id;
    int line_declared;
    struct SymbolEntry *next;
} SymbolEntry;

/* Represents one lexical scope (global or block) */
typedef struct Scope {
    int scope_level;
    int scope_id;
    int is_active;
    struct Scope *parent;         /* Enclosing scope on the active stack */
    struct Scope *all_next;       /* Flat list of all scopes ever created */
    SymbolEntry *symbols;         /* Linked list of symbols in this scope */
} Scope;

/* Scoped symbol table supporting nested blocks and full scope history */
typedef struct SymbolTable {
    Scope *current_scope;
    Scope *all_scopes_head;
    Scope *all_scopes_tail;
    int next_scope_id;
} SymbolTable;

/* Lifecycle */
SymbolTable *symtab_create(void);
void symtab_destroy(SymbolTable *st);

/* Scope management */
void symtab_enter_scope(SymbolTable *st);
void symtab_exit_scope(SymbolTable *st);
int symtab_current_level(const SymbolTable *st);

/* Symbol operations */
SymbolEntry *symtab_insert(SymbolTable *st, const char *name, DataType type, int line_declared);
SymbolEntry *symtab_lookup(const SymbolTable *st, const char *name);
SymbolEntry *symtab_lookup_current_scope(const SymbolTable *st, const char *name);
SymbolEntry *symtab_lookup_out_of_scope(const SymbolTable *st, const char *name);

/* Diagnostics / display */
void symtab_print(const SymbolTable *st);

#endif /* SYMBOL_TABLE_H */
