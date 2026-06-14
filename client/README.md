# CCO Language

A [Language Server Protocol](https://microsoft.github.io/language-server-protocol/) extension for **CCO** (CNT Configuration Object) — a fully-typed configuration language with templates, inheritance, generics, expressions, and more.

Built on top of [libcco](https://github.com/aplt-cnt/libcco), the reference CCO parser library.

---

*Developed by **APLT CNT Development Team.***

---

## Features

- **Syntax highlighting** — Full TextMate grammar for `.cco` files with accurate tokenization
- **Error reporting** — Real-time diagnostics via the libcco parser (syntax errors, type errors, unexpected tokens)
- **Completions** — `$`-keywords (`typedef`, `enum`, `temp`, `function`, …), type keywords (`String`, `Integer`, `Float`, …), type aliases, enum names, template names
- **Hover information** — Type signatures, enum value documentation, template field descriptions
- **Go to definition** — Jump to `$typedef`, `$enum`, `$temp` declarations
- **Document symbols** — Outline view showing all top-level declarations

## CCO language quick reference

| Syntax | Description |
|---|---|
| `$typedef.Name: Type` | Type alias |
| `$enum.Name = (A, B, C)` | Enumeration |
| `$temp.Name: (field: Type, …)` | Template (class) definition |
| `$temp.Name + Parent: (…)` | Template with inheritance |
| `$temp.<T>Name: (…)` | Generic template |
| `$function.@(params): (body)` | Constructor |
| `$function.@: $default` | Default constructor |
| `$function.#method(params): Type (body)` | Static method |
| `$return<Type>(expr)` | Return statement |
| `$this` | `this` reference |
| `$format(expr)` | Runtime CCO string parsing |
| `#Name(args)` | Template instantiation (positional) |
| `#Name(.field: val, …)` | Template instantiation (named) |
| `#Name:method(args)` | Static method call |
| `$(expr op expr)` | Compound expression with operators |
| `(field: val, …)` | Object/map literal |
| `(val, val, …)` | Array literal |

**Primitive types:** `String`, `Integer`, `Float`, `Boolean`, `Array`, `dyn`, `None`

**Operators:** `+` `-` `*` `/` `==` `!=` `<` `>` `<=` `>=` `&&` `||` `!` `|` (coalesce)

**Comments:** `/* … */`

## LSP protocol support

| Method | Status |
|---|---|
| `initialize` | ✅ Server capabilities |
| `shutdown` | ✅ Graceful exit |
| `textDocument/didOpen` | ✅ Parse + push diagnostics |
| `textDocument/didChange` | ✅ Reparse + push updated diagnostics |
| `textDocument/didClose` | ✅ Release document |
| `textDocument/completion` | ✅ Keywords, types, templates, enums |
| `textDocument/hover` | ✅ Type info, enum values, template signatures |
| `textDocument/definition` | ✅ Jump to `$typedef`, `$enum`, `$temp` |
| `textDocument/documentSymbol` | ✅ Outline of top-level declarations |

## Architecture

```
  VS Code                    LSP Server (cco-lsp.exe)
  ────────                   ───────────────────────
  extension.ts ──stdin/json──► main.c
      │                      ├── lsp.c          (JSON-RPC framing)
      │                      ├── json.c         (JSON parser)
      │                      ├── cco_analyze.c  (↔ libcco)
      │                      └── text_document.c
      ◄──stdout/json───────
```

The LSP server is a native C executable (~200 KB) with zero external dependencies. It communicates with VS Code over stdin/stdout using the LSP `Content-Length` framing protocol. All CCO parsing is delegated to `libcco` via `cco_parse_full()`, and diagnostics are read from `cco_diag_get()/cco_diag_count()`.

## License

GNU General Public License v3.0 — see [LICENSE.txt](LICENSE.txt).
