# Celestra ✨

Celestra is a small, dynamically-typed scripting language built from scratch
with **Flex** (lexer) and **Bison** (parser), backed by a hand-written
tree-walking interpreter in C. Celestra programs use the `.cel` file
extension.

```
let name = "World";
print("Hello, " + name + "!");
```

This repository is my submission for the *Build Your Own Programming
Language* assignment.

---

## 1. How it's built

Celestra is implemented as a classic three-stage pipeline:

```
 source.cel --> [Flex lexer]  --> tokens --> [Bison parser] --> AST --> [C interpreter] --> output
                lexer.l                      parser.y            ast.c/.h   interpreter.c/.h
```

* **`src/lexer.l`** – A Flex specification that turns raw source text into
  tokens: keywords (`let`, `if`, `while`, `for`, `def`, `print`, `input`,
  ...), operators, identifiers, numbers, and string literals (with escape
  sequences like `\n` and `\"`). Comments start with `#` and run to the end
  of the line.
* **`src/parser.y`** – A Bison grammar that consumes those tokens and
  builds an **Abstract Syntax Tree (AST)**, rather than evaluating on the
  fly. Operator precedence and associativity (e.g. `*` binds tighter than
  `+`, comparisons bind looser than arithmetic, unary minus binds tightest)
  are declared with Bison's `%left` / `%right` / `%nonassoc` directives.
* **`src/ast.h` / `src/ast.c`** – A minimal, generic `Node` struct
  (a handful of child pointers `a/b/c/d`, a linked `NodeList` for
  statement/argument/parameter lists) with constructor functions for every
  language construct, avoiding a giant tagged union.
* **`src/interpreter.h` / `src/interpreter.c`** – A tree-walking evaluator.
  Variables live in a chain of scopes (`Env` structs, one per block/loop
  iteration/function call), so `let` inside an `if`/`while`/`for` block is
  properly local to that block, and function calls get their own fresh
  scope. Control flow (`break`, `continue`, `return`) is implemented as a
  `Signal` value that statement execution returns and propagates upward,
  rather than using `goto`/`setjmp`.
* **`src/parser.y`** also contains `main()`, which opens the `.cel` file
  given on the command line, runs `yyparse()` to build the AST, and then
  calls `interpret()` to execute it.

### Build process

```bash
cd src
make            # runs bison -d, flex, then gcc, producing ./celestra
./celestra ../examples/hello.cel
```

Full details (including dependencies) are in **`Language_Manual.txt`**.

---

## 2. Language features

| Requirement           | Celestra support                                                        |
|------------------------|--------------------------------------------------------------------|
| Custom identity         | Language name **Celestra**, file extension **`.cel`**                |
| Variables                | `let x = 5;`, reassignment `x = x + 1;`                          |
| Arithmetic operators       | `+  -  *  /  %`                                                |
| Logical / comparison ops     | `>  <  >=  <=  ==  !=  and  or  not`                          |
| Control flow                   | `if` / `else`, `while`, C-style `for`, `break`, `continue`  |
| I/O                              | `print(...)` (multiple, comma-separated args) and `input(var)` |
| **Bonus**                          | See below |

**Bonus features beyond the core requirements:**
* **User-defined functions** with `def` / `return`, including recursion (`factorial(5)` in `examples/functions.cel`).
* **Arrays (lists)** — `let nums = [1, 2, 3];`, read with `nums[0]`, and array
  built-ins `length()`, `push()`, `pop()`, `set()`. Arrays are reference
  types (like Python/JS lists) — passing one to a function or assigning
  it to another variable shares the same underlying data, demonstrated
  in `examples/arrays_and_builtins.cel`.
* **A small built-in standard library**: `length()`, `push()`, `pop()`,
  `set()` for arrays; `upper()`, `lower()` for strings; `sqrt()`, `pow()`,
  `abs()`, `round()`, `random()` for math; `typeof()` for inspecting a
  value's runtime type.
* **Compound assignment operators**: `+=`, `-=`, `*=`, `/=`.
* **String indexing** — `word[0]` returns a single character, same
  syntax as array indexing.
* Block-level lexical scoping, string concatenation with `+`, `#`
  comments, string comparison, and automatic numeric coercion for
  `input()`.

See `Language_Manual.txt` for the full syntax guide, and `examples/*.cel`
for runnable programs that exercise every feature above.

---

## 3. Design decisions & challenges

* **AST-based instead of syntax-directed evaluation.** Early on I
  considered evaluating expressions directly inside the Bison actions
  (the "calculator" style you see in Flex/Bison tutorials). I switched to
  building an explicit AST first because control flow (`if`/`while`/`for`)
  and functions need to *not* execute their bodies immediately during
  parsing — a function body has to be stored and only run later, possibly
  many times, possibly recursively. Building a small generic `Node` type
  once and reusing it everywhere kept `ast.h` short instead of needing a
  separate struct per statement kind.
* **Scoping.** The trickiest part was deciding how `let` should interact
  with blocks. I settled on: every `{ ... }` block, every loop iteration,
  and every function call gets its own `Env` whose parent is the
  enclosing scope, so `let x` inside an `if` doesn't leak out, but reading
  an outer variable from inside a nested block still works via the parent
  chain. Function calls deliberately parent onto the *global* scope, not
  the caller's scope — this keeps Celestra's function semantics simple (no
  accidental closures) and avoids a class of confusing bugs where a
  function could read stale local variables from whoever happened to call
  it.
* **`break` / `continue` / `return` as a propagated signal.** Instead of
  using C's `goto` or `setjmp`/`longjmp`, `exec_stmt` returns a small
  `Signal` enum. A block stops executing further statements as soon as a
  non-`SIG_NONE` signal appears, and loops interpret `SIG_BREAK` /
  `SIG_CONTINUE` themselves while letting `SIG_RETURN` keep propagating
  upward until it reaches the function call that started it. This was the
  single trickiest piece of control-flow logic to get right — my first
  version let `continue` accidentally skip the loop's update step in
  `for` loops, which I caught by writing `control_flow.cel` and testing
  loop output line-by-line.
* **Forward-referenced functions.** Since Celestra executes top-level
  statements in order, a function defined near the bottom of a file
  couldn't be called from code above it (or from another function defined
  above it) if functions were only registered as execution reached their
  `def`. I fixed this with a pre-pass in `interpret()` that registers
  every top-level `def` before running any code, so definition order at
  the top level doesn't matter and mutual recursion works.
* **Grammar conflicts.** The very first grammar (allowing a bare `if
  (cond) stmt` without requiring braces) produced the classic
  dangling-`else` shift/reduce conflict. Rather than fight Bison's default
  resolution, I simplified the grammar so `if`/`while`/`for` bodies must
  be `{ ... }` blocks. This removed the ambiguity entirely and, as a side
  benefit, made every Celestra program more readable.
* **Arrays as reference types.** Once I added arrays, I had to decide
  whether assigning `let b = a;` (where `a` is an array) should copy the
  array or share it. I chose to share it (arrays are heap-allocated
  `ArrayObj` structs, and a `Value` just holds a pointer to one) — the
  same behaviour as Python and JavaScript lists. This is what makes
  `push(arr, x)` inside a function actually mutate the caller's array,
  which is what most programmers expect and is essential for the
  `arrays_and_builtins.cel` example.
* **Built-ins vs. a `[]`-assignment grammar rule.** I initially considered
  adding `arr[0] = 5;` as its own grammar rule, but that creates a
  shift/reduce ambiguity with a plain identifier followed by `[` (the
  parser can't immediately tell, one token of lookahead in, whether it's
  about to see an index-read expression or the start of an
  index-assignment statement). Rather than fight the grammar, I kept
  indexing (`arr[i]`) as a pure expression and added a `set(arr, i, v)`
  built-in function for writes — this sidesteps the ambiguity entirely
  since it reuses the already-unambiguous function-call syntax.
* **Numbers-as-strings from `input()`.** Since everything typed at the
  terminal arrives as text, `input(x)` tries to parse the line as a
  number with `strtod` and only falls back to storing it as a string if
  the whole line isn't a valid number — so `input(age); print(age + 1);`
  works without the user needing an explicit cast.

### Known limitations (things I'd tackle with more time)
* No arrays/lists or user-defined data structures.
* No garbage collection — string `Value`s are `strdup`'d and leaked at
  program end (acceptable for short-lived scripts, not for long-running
  ones).
* Functions can't close over local variables from an enclosing function
  (by design, see above) — only over the global scope.
* Error messages report the *current* lexer line at the time of the error
  rather than the exact line the offending statement started on.

---

## 4. Repository layout

```
celestra-lang/
├── README.md              <- you are here
├── Language_Manual.txt    <- build instructions + full syntax guide
├── src/
│   ├── lexer.l             <- Flex lexer
│   ├── parser.y             <- Bison grammar + main()
│   ├── ast.h / ast.c          <- AST node definitions & constructors
│   ├── interpreter.h / .c        <- tree-walking evaluator
│   └── Makefile                    <- builds ./celestra
└── examples/
    ├── hello.cel            <- minimal "Hello, World!"
    ├── variables.cel         <- variables + all operators
    ├── control_flow.cel       <- if/else, while, for, break/continue, FizzBuzz
    ├── functions.cel            <- def/return, recursion (factorial)
    ├── calculator.cel             <- input() driven calculator
    └── arrays_and_builtins.cel      <- arrays, +=, and the built-in library
```
