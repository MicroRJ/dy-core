# Repository Guide

## C Syntax

Always use braces for `if` statements and `for` loops, including single-line
bodies.

Keep parenthesized conditions on one physical line. Never format an `if` like
this:

```c
if (stream->readable &&
    stream->writable)
```

Prefer a long condition on one line. If that becomes unreasonable, simplify
the control flow by splitting the `if` or extract parts of the condition into
clearly named local variables instead of wrapping the parenthesized expression
across lines.

Keep function declarations and definitions, including their parameter lists,
on one physical line. Prefer a long line over splitting a signature across
multiple lines. Apply the same rule to function calls and their argument lists.
As a general rule, keep syntactically connected code on the same line rather
than introducing line breaks inside it.

Prefer explicit, structured control flow over scattered early exits. Use clear
`if`/`else` structure and an obvious exit path instead of spreading `return` or
`break` statements throughout a function or loop.

Keep definition, initialization, and use order explicit. Declare a local when
its value becomes valid, initialize it in the declaration when possible, and
place dependent declarations after the values they depend on. Do not collect
forward declarations at the beginning of a scope and interleave their later
initialization with unrelated work.

Avoid this pattern:

```c
dy_Result result = {0};
dy_Scratch scratch;
char *application;
char *command;
char *working_directory = NULL;
```

Prefer a direct flow from definition to use:

```c
u32 size = calculate_size();
void *data = malloc(sizeof(*data) * size);
```

This applies to structures as well as scalar locals. Fully establish the
fields or values on which an operation depends before performing that
operation. Code should make dependency order visible so a later edit cannot
easily use an uninitialized value or move a required assignment after its use.

## Function Structure

Avoid extracting a single-use helper when its parameter list is large relative
to the work or abstraction it provides. If the caller validates and prepares a
set of values only to pass all of them into one private helper, keep the
operation inline so the preparation, use, and cleanup remain visible together.

Do not split a function merely to separate phases such as validation,
conversion, and the final platform call. A helper should earn its interface
through meaningful reuse or by isolating substantial independent complexity
behind a narrow interface. Passing a large collection of local state through a
one-off function adds noise and obscures data flow. When such a helper does not
meet that bar, inline it at its only call site.

Do not manufacture a narrow interface by packing caller-local state into a
single-use structure. A helper with few parameters is not simpler if it also
requires a bespoke state type, partial-initialization rules, ownership
invariants, and matching initialization and cleanup functions. Judge an
abstraction by the total complexity it removes, not by the length of the caller
or its parameter list.

Prefer inline code when temporary resources exist only to support one nearby
operation and must remain alive until that operation completes. Keeping setup,
the operation, and cleanup in one scope often makes ownership and lifetime more
obvious than wrapping the same machinery in a one-off abstraction.

## Reference Code

Use `src/win32/dy_process.c` as the primary reference for the repository's C
style and formatting. In particular, follow its use of braces, single-line
signatures and conditions, declaration near initialization, explicit phase
ordering, visible resource lifetimes, and centralized cleanup.

Treat it as an example of the governing principles rather than a template to
copy mechanically. Preserve simpler control flow when the code being changed
does not require process-style validation, staged setup, or resource cleanup.
