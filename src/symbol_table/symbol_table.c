#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *sym_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (!copy) {
        fprintf(stderr, "Fatal Error: Out of memory in symbol table\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, s, len);
    return copy;
}

static Scope *scope_create(int level, int id, Scope *parent) {
    Scope *scope = (Scope *)calloc(1, sizeof(Scope));
    if (!scope) {
        fprintf(stderr, "Fatal Error: Out of memory creating scope\n");
        exit(EXIT_FAILURE);
    }
    scope->scope_level = level;
    scope->scope_id = id;
    scope->is_active = 1;
    scope->parent = parent;
    scope->all_next = NULL;
    scope->symbols = NULL;
    return scope;
}

SymbolTable *symtab_create(void) {
    SymbolTable *st = (SymbolTable *)calloc(1, sizeof(SymbolTable));
    if (!st) {
        fprintf(stderr, "Fatal Error: Out of memory creating symbol table\n");
        exit(EXIT_FAILURE);
    }
    Scope *global = scope_create(0, 0, NULL);
    st->current_scope = global;
    st->all_scopes_head = global;
    st->all_scopes_tail = global;
    st->next_scope_id = 1;
    return st;
}

void symtab_destroy(SymbolTable *st) {
    if (!st) return;
    Scope *scope = st->all_scopes_head;
    while (scope) {
        Scope *next_scope = scope->all_next;
        SymbolEntry *entry = scope->symbols;
        while (entry) {
            SymbolEntry *next_entry = entry->next;
            free(entry->name);
            free(entry);
            entry = next_entry;
        }
        free(scope);
        scope = next_scope;
    }
    free(st);
}

void symtab_enter_scope(SymbolTable *st) {
    if (!st || !st->current_scope) return;
    int new_level = st->current_scope->scope_level + 1;
    int new_id = st->next_scope_id++;
    Scope *child = scope_create(new_level, new_id, st->current_scope);

    st->all_scopes_tail->all_next = child;
    st->all_scopes_tail = child;
    st->current_scope = child;
}

void symtab_exit_scope(SymbolTable *st) {
    if (!st || !st->current_scope || !st->current_scope->parent) {
        return;
    }
    st->current_scope->is_active = 0;
    st->current_scope = st->current_scope->parent;
}

int symtab_current_level(const SymbolTable *st) {
    if (!st || !st->current_scope) return -1;
    return st->current_scope->scope_level;
}

SymbolEntry *symtab_lookup_current_scope(const SymbolTable *st, const char *name) {
    if (!st || !st->current_scope || !name) return NULL;
    for (SymbolEntry *curr = st->current_scope->symbols; curr != NULL; curr = curr->next) {
        if (strcmp(curr->name, name) == 0) {
            return curr;
        }
    }
    return NULL;
}

SymbolEntry *symtab_lookup(const SymbolTable *st, const char *name) {
    if (!st || !name) return NULL;
    for (Scope *scope = st->current_scope; scope != NULL; scope = scope->parent) {
        for (SymbolEntry *curr = scope->symbols; curr != NULL; curr = curr->next) {
            if (strcmp(curr->name, name) == 0) {
                return curr;
            }
        }
    }
    return NULL;
}

/*
 * Searches previously exited (inactive) scopes to distinguish a scope violation
 * (variable existed in an inner block that already closed) from a completely
 * undeclared variable.
 */
SymbolEntry *symtab_lookup_out_of_scope(const SymbolTable *st, const char *name) {
    if (!st || !name) return NULL;
    SymbolEntry *match = NULL;
    for (Scope *scope = st->all_scopes_head; scope != NULL; scope = scope->all_next) {
        if (scope->is_active) continue;
        for (SymbolEntry *curr = scope->symbols; curr != NULL; curr = curr->next) {
            if (strcmp(curr->name, name) == 0) {
                match = curr; /* Keep the most recently declared matching symbol */
            }
        }
    }
    return match;
}

SymbolEntry *symtab_insert(SymbolTable *st, const char *name, DataType type, int line_declared) {
    if (!st || !st->current_scope || !name) return NULL;

    /* Disallow duplicate declarations in the exact same scope */
    if (symtab_lookup_current_scope(st, name) != NULL) {
        return NULL;
    }

    SymbolEntry *entry = (SymbolEntry *)calloc(1, sizeof(SymbolEntry));
    if (!entry) {
        fprintf(stderr, "Fatal Error: Out of memory allocating symbol entry\n");
        exit(EXIT_FAILURE);
    }

    entry->name = sym_strdup(name);
    entry->type = type;
    entry->scope_level = st->current_scope->scope_level;
    entry->scope_id = st->current_scope->scope_id;
    entry->line_declared = line_declared;
    entry->next = NULL;

    /* Append at end to preserve declaration order when printing */
    if (!st->current_scope->symbols) {
        st->current_scope->symbols = entry;
    } else {
        SymbolEntry *tail = st->current_scope->symbols;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = entry;
    }

    return entry;
}

void symtab_print(const SymbolTable *st) {
    if (!st) return;

    printf("=== Symbol Table ===\n");
    printf("%-16s %-8s %-12s %-10s %-14s\n",
           "Name", "Type", "Scope Level", "Scope ID", "Line Declared");
    printf("----------------------------------------------------------------\n");

    int count = 0;
    for (Scope *scope = st->all_scopes_head; scope != NULL; scope = scope->all_next) {
        for (SymbolEntry *entry = scope->symbols; entry != NULL; entry = entry->next) {
            printf("%-16s %-8s %-12d %-10d %-14d\n",
                   entry->name,
                   ast_data_type_to_str(entry->type),
                   entry->scope_level,
                   entry->scope_id,
                   entry->line_declared);
            count++;
        }
    }

    if (count == 0) {
        printf("(no symbols declared)\n");
    }
}
