# CCO Language Server & VS Code Extension

A [Language Server Protocol](https://microsoft.github.io/language-server-protocol/) implementation for **CCO** (CNT Configuration Object) — a fully-typed configuration language with templates, inheritance, generics, expressions, and more.

Built on top of [libcco](https://github.com/aplt-cnt/libcco), the reference CCO parser library.

## Features

- **Syntax highlighting** — TextMate grammar for `.cco` files
- **Error reporting** — Real-time diagnostics via `libcco` parser (syntax errors, type errors)
- **Completions** — `$`-keywords, type keywords (`String`, `Integer`, …), type aliases, enum names, template names
- **Hover information** — Type signatures, enum values, template fields
- **Go to definition** — Jump to `$typedef`, `$enum`, `$temp` declarations
- **Document symbols** — Outline view showing all top-level declarations

## Prerequisites

| Dependency | Version | Notes |
|---|---|---|
| [libcco](https://github.com/aplt-cnt/libcco) | ≥ 0.1.0 | Cloned as sibling of this project |
| CMake | ≥ 3.15 | Build system |
| C compiler | C11 | MSVC, GCC, or Clang |
| Node.js | ≥ 18 | TypeScript compilation, `vsce` |
| npm | ≥ 9 | Package management |
| Visual Studio Code | ≥ 1.75 | Extension host |

## Quick start

```bash
# 1. Ensure libcco is present as a sibling directory
#    ../../libcco/  ← must have CMakeLists.txt

# 2. Build everything
make

# 3. Run integration tests
make test

# 4. Package VSIX
make package

# 5. Install to VS Code
make install
```

## Build targets

| Command | Result |
|---|---|
| `make` | Build server + compile TypeScript extension |
| `make server` | Build C language server only |
| `make extension` | Compile TypeScript only |
| `make package` | Build everything + create `.vsix` |
| `make test` | Run LSP integration tests |
| `make install` | Package + `code --install-extension` |
| `make clean` | Remove build artefacts |
| `make help` | Print available targets |

## Manual build steps

```bash
# Build libcco
cd ../libcco && cmake -B target && cmake --build target --config Debug

# Build language server
cd ../libcco-language-server
cmake -B target && cmake --build target --config Debug

# Compile extension
cd client && npm install && npm run compile

# Package VSIX
cd client && npx vsce package

# Install
code --install-extension client/cco-language-support-0.1.0.vsix
```

## Project structure

```
libcco-language-server/
├── CMakeLists.txt                  # Build system (links libcco)
├── Makefile                        # Convenience targets
├── server/                         # LSP server (C)
│   ├── main.c                      # Entry point, stdin/stdout LSP loop
│   ├── lsp.h / lsp.c               # LSP protocol (JSON-RPC 2.0, Content-Length framing)
│   ├── json.h / json.c             # Zero-dependency JSON parser/serializer
│   ├── cco_analyze.h / cco_analyze.c  # CCO analysis layer (diagnostics, completions, hover, …)
│   └── text_document.h / text_document.c  # Document buffer with line offset index
├── client/                         # VS Code extension (TypeScript)
│   ├── src/extension.ts            # Activation, LSP client setup
│   ├── syntaxes/cco.tmLanguage.json  # TextMate grammar
│   ├── language-configuration.json   # Comment/bracket config
│   ├── server/cco-lsp.exe          # Bundled LSP server binary
│   ├── package.json                # Extension manifest
│   └── tsconfig.json
└── target/                         # CMake output directory
    └── Debug/cco-lsp.exe           # Compiled language server
```

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

GNU General Public License v3.0 — see [LICENSE.txt](client/LICENSE.txt).
