# Vertex — Mini Programming Language Compiler

A complete compiler front-end with intermediate Three Address Code (TAC) generation and an interactive web-based IDE for a statically typed mini programming language, built in C using **Flex**, **Bison**, and **GCC** (with a **Python/Flask** web frontend) for the **Compiler Construction Lab** (Department of Computer Science and Engineering, Metropolitan University, Bangladesh).

---

## Project Overview

Vertex implements a full 5-phase compiler front-end pipeline:

```text
Source Code (.mc)
      │
      ▼
Lexical Analyzer (Flex)          ──► Token Stream (line-tracked, lexical error detection)
      │
      ▼
Syntax Analyzer (Bison)          ──► Unambiguous CFG parsing with error recovery
      │
      ▼
Abstract Syntax Tree (AST)       ──► Structured tree with line numbers & indented printer
      │
      ▼
Semantic Analyzer & Symbol Table ──► Nested scope tracking & static type checking
      │
      ▼
Intermediate Code Generator      ──► Linear Three Address Code (TAC)
```

TAC is generated **only** when lexical, syntax, and semantic analysis all pass with zero errors.

---

## Language Specification

- **Data Types**: `int`, `float`, `bool` (`true`, `false`)
- **Statements**:
  - Variable declarations (`int x;`, optional initializer `int x = 5;`)
  - Assignments (`x = 5;`)
  - Conditionals (`if (cond) ...`, `if (cond) ... else ...`)
  - Loops (`while (cond) ...`)
  - Output (`print expr;`)
  - Nested lexical blocks (`{ ... }`)
- **Operators** (lowest to highest precedence):
  1. Logical OR: `||` (left-associative)
  2. Logical AND: `&&` (left-associative)
  3. Equality: `==`, `!=` (left-associative)
  4. Relational: `<`, `>`, `<=`, `>=` (left-associative)
  5. Additive: `+`, `-` (left-associative)
  6. Multiplicative: `*`, `/`, `%` (left-associative)
  7. Unary: `-`, `!` (right-associative)
- **Delimiters**: `{`, `}`, `(`, `)`, `;`
- **Comments**: single-line (`// ...`) and block (`/* ... */`, with unterminated comment detection)

---

## Directory Structure

```text
.
├── docs/                                # Project report, formal CFG, and design docs
├── examples/
│   ├── sample.mc                        # Lab manual Section 5.5 sample program
│   ├── sample.out                       # Expected compiler output
│   ├── valid_program.mc                 # Non-trivial program using all language features
│   └── valid_program.out                # Expected compiler output
├── gui/
│   ├── app.py                           # Flask backend bridging the C compiler executable
│   ├── requirements.txt                 # Python dependencies for Vertex Web IDE
│   └── templates/
│       └── index.html                     # Vertex interactive web IDE interface
├── src/
│   ├── main.c                           # Front-end compiler driver
│   ├── lexer/
│   │   └── lexer.l                      # Flex lexical analyzer
│   ├── parser/
│   │   └── parser.y                     # Bison unambiguous CFG & AST builder
│   ├── ast/
│   │   ├── ast.h                        # AST node definitions & type enums
│   │   └── ast.c                        # AST constructors, indented printer, cleanup
│   ├── symbol_table/
│   │   ├── symbol_table.h               # Scoped symbol table interface
│   │   └── symbol_table.c               # Nested scope management & table printer
│   ├── semantic/
│   │   ├── semantic.h                   # Semantic analyzer interface
│   │   └── semantic.c                   # Scope & type checker with error diagnostics
│   └── ir/
│       ├── tac.h                        # Three Address Code IR definitions
│       └── tac.c                        # Linear TAC generator & printer
├── tests/
│   ├── valid_basic.mc / .out            # Basic types, comments, operator precedence
│   ├── valid_nested_and_ops.mc / .out   # Nested blocks, dangling-else, logical/unary ops
│   ├── valid_tac_manual.mc / .out       # Lab manual Section 4.6 TAC illustration
│   ├── lexical_error.mc / .out          # Invalid token & malformed literal errors
│   ├── syntax_error.mc / .out           # Syntax errors & Bison error recovery
│   └── semantic_errors/
│       ├── 01_undeclared_variable.mc / .out
│       ├── 02_redeclaration.mc / .out
│       ├── 03_scope_violation.mc / .out
│       ├── 04_type_mismatch.mc / .out
│       ├── 05_invalid_assignment.mc / .out
│       └── 06_invalid_expression.mc / .out
├── .gitignore
├── Makefile
└── README.md
```

---

## Prerequisites

- **GCC** (C11 compatible)
- **Flex**
- **Bison**
- **GNU Make**
- **Python 3.8+** & **Flask** *(optional, for running the Vertex Web IDE)*

*(Recommended environment: Linux/Ubuntu, WSL, or MSYS2 UCRT64 on Windows.)*

---

## Build & Execution Instructions

### Build the Compiler

```bash
make
```

This generates the Flex lexer and Bison parser and compiles everything into the `./compiler` executable (`compiler.exe` on Windows) with zero warnings.

### Run on a Source File

```bash
./compiler examples/valid_program.mc
```

Or run on any test file (or pipe source code via `stdin`):

```bash
./compiler examples/sample.mc
./compiler tests/lexical_error.mc
./compiler tests/syntax_error.mc
./compiler tests/semantic_errors/04_type_mismatch.mc
```

On a valid program, `./compiler <file>` prints:
1. The indented **Abstract Syntax Tree (AST)** with line numbers and inferred types
2. **Semantic Analysis** status and the complete **Symbol Table**
3. The generated **Three Address Code (TAC)**

On an invalid program, `./compiler <file>` prints detailed errors with line numbers, skips later phases (no TAC is generated), and exits with a non-zero status code.

### Run the Vertex Web IDE (Bonus GUI)

After building `./compiler` with `make`, start the interactive web frontend:

```bash
pip install -r gui/requirements.txt
python gui/app.py
```

Open `http://127.0.0.1:5000` in your browser to write code, load built-in examples, and inspect the AST, Symbol Table, TAC, and error diagnostics visually.

### Run the Full Test Suite

```bash
make test
```

### Clean Build Artifacts

```bash
make clean
```

---

## Test Suite Summary

| Category | Test File | Expected Result | Output File |
| :--- | :--- | :--- | :--- |
| Valid (Comprehensive) | `examples/valid_program.mc` | Compiles cleanly to TAC | `examples/valid_program.out` |
| Valid (Manual Sample) | `examples/sample.mc` | Compiles cleanly to TAC | `examples/sample.out` |
| Valid (Manual TAC) | `tests/valid_tac_manual.mc` | Compiles cleanly to TAC | `tests/valid_tac_manual.out` |
| Valid (Basic & Prec.) | `tests/valid_basic.mc` | Compiles cleanly to TAC | `tests/valid_basic.out` |
| Valid (Nested & Ops) | `tests/valid_nested_and_ops.mc` | Compiles cleanly to TAC | `tests/valid_nested_and_ops.out` |
| Lexical Errors | `tests/lexical_error.mc` | Detects invalid tokens with lines | `tests/lexical_error.out` |
| Syntax Errors | `tests/syntax_error.mc` | Detects syntax errors & recovers | `tests/syntax_error.out` |
| Undeclared Variable | `tests/semantic_errors/01_undeclared_variable.mc` | Reports undeclared variables | `tests/semantic_errors/01_undeclared_variable.out` |
| Redeclaration | `tests/semantic_errors/02_redeclaration.mc` | Reports same-scope redeclarations | `tests/semantic_errors/02_redeclaration.out` |
| Scope Violation | `tests/semantic_errors/03_scope_violation.mc` | Reports out-of-scope accesses | `tests/semantic_errors/03_scope_violation.out` |
| Type Mismatch | `tests/semantic_errors/04_type_mismatch.mc` | Reports type mismatches | `tests/semantic_errors/04_type_mismatch.out` |
| Invalid Assignment | `tests/semantic_errors/05_invalid_assignment.mc` | Reports incompatible assignments | `tests/semantic_errors/05_invalid_assignment.out` |
| Invalid Expression | `tests/semantic_errors/06_invalid_expression.mc` | Reports invalid operator operands | `tests/semantic_errors/06_invalid_expression.out` |

---

## Team Members

| Name | Student ID | Role / Modules | GitHub Username |
| :--- | :--- | :--- | :--- |
| Anando Das Utso | 232-115-124 | Lexer, Parser, AST, Vertex Web IDE (GUI) | `@Anando-Das` |
| Priom Dhar | 232-115-144 | Symbol Table, Semantic Analysis, TAC Generation, Testing, Docs | `@priomdhar` |