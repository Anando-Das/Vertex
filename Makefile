CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
FLEX    = flex
BISON   = bison

TARGET  = compiler

LEX_SRC   = src/lexer/lexer.l
LEX_GEN   = src/lexer/lex.yy.c

YACC_SRC  = src/parser/parser.y
YACC_C    = src/parser/parser.tab.c
YACC_H    = src/parser/parser.tab.h

OBJS      = src/main.o \
            src/ast/ast.o \
            src/symbol_table/symbol_table.o \
            src/semantic/semantic.o \
            src/ir/tac.o \
            src/parser/parser.tab.o \
            src/lexer/lex.yy.o

.PHONY: all clean test

all: $(TARGET)

$(YACC_C) $(YACC_H): $(YACC_SRC) src/ast/ast.h
	$(BISON) -d -o $(YACC_C) $(YACC_SRC)

$(LEX_GEN): $(LEX_SRC) $(YACC_H) src/ast/ast.h
	$(FLEX) -o $(LEX_GEN) $(LEX_SRC)

src/main.o: src/main.c src/ast/ast.h src/symbol_table/symbol_table.h src/semantic/semantic.h src/ir/tac.h $(YACC_H)
	$(CC) $(CFLAGS) -c -o $@ src/main.c

src/ast/ast.o: src/ast/ast.c src/ast/ast.h
	$(CC) $(CFLAGS) -c -o $@ src/ast/ast.c

src/symbol_table/symbol_table.o: src/symbol_table/symbol_table.c src/symbol_table/symbol_table.h src/ast/ast.h
	$(CC) $(CFLAGS) -c -o $@ src/symbol_table/symbol_table.c

src/semantic/semantic.o: src/semantic/semantic.c src/semantic/semantic.h src/symbol_table/symbol_table.h src/ast/ast.h
	$(CC) $(CFLAGS) -c -o $@ src/semantic/semantic.c

src/ir/tac.o: src/ir/tac.c src/ir/tac.h src/ast/ast.h
	$(CC) $(CFLAGS) -c -o $@ src/ir/tac.c

src/parser/parser.tab.o: $(YACC_C) $(YACC_H) src/ast/ast.h
	$(CC) $(CFLAGS) -c -o $@ $(YACC_C)

src/lexer/lex.yy.o: $(LEX_GEN) $(YACC_H) src/ast/ast.h
	$(CC) $(CFLAGS) -Wno-sign-compare -c -o $@ $(LEX_GEN)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

test: $(TARGET)
	@echo "=== Running Example Programs ==="
	./$(TARGET) examples/valid_program.mc
	./$(TARGET) examples/sample.mc
	@echo ""
	@echo "=== Running Valid Tests ==="
	./$(TARGET) tests/valid_tac_manual.mc
	./$(TARGET) tests/valid_basic.mc
	./$(TARGET) tests/valid_nested_and_ops.mc
	@echo ""
	@echo "=== Running Lexical Error Test (expected failure) ==="
	-./$(TARGET) tests/lexical_error.mc
	@echo ""
	@echo "=== Running Syntax Error Test (expected failure) ==="
	-./$(TARGET) tests/syntax_error.mc
	@echo ""
	@echo "=== Running Semantic Error Tests (expected failures) ==="
	-./$(TARGET) tests/semantic_errors/01_undeclared_variable.mc
	-./$(TARGET) tests/semantic_errors/02_redeclaration.mc
	-./$(TARGET) tests/semantic_errors/03_scope_violation.mc
	-./$(TARGET) tests/semantic_errors/04_type_mismatch.mc
	-./$(TARGET) tests/semantic_errors/05_invalid_assignment.mc
	-./$(TARGET) tests/semantic_errors/06_invalid_expression.mc

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJS) $(LEX_GEN) $(YACC_C) $(YACC_H) src/parser/*.output
