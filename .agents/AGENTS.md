# AGENTS.md — SExprTk Implementation Guide

## Context: The MetaTk Ecosystem

MetaTk is a suite of metaprogramming tools grounded in Software Language Engineering (SLE).
Its members:

| Library  | Role                                              |
|----------|---------------------------------------------------|
| DSLtk    | Embedded DSL construction kit (the father)        |
| Parzek   | Parser combinator library                         |
| EkippX   | Equipment/toolchain extension layer               |
| SExprTk  | **This library** — S-Expression substrate         |

SExprTk is built *using* DSLtk for its embedded DSLs. Do not reimplement DSL machinery
from scratch; delegate to DSLtk APIs.

All these are in directories of the same name.

There are several other programs and scripts in `SExprTk` library that we'll discuss after SExprTk itself.

---

## Hard Constraints

- **Single header**: everything lives in `SExprTk/SExprTk.hpp`. No `.cpp` translation units.
- **C++20**: use concepts, ranges, and coroutines where they clarify intent.
- **No exceptions by default**: use `std::expected<T, Error>` throughout. Provide an opt-in exception mode via `SEXPRTK_EXCEPTIONS`.
- **

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│                     SExprTk/SExprTk.hpp                    │
│                                                     │
│  ┌──────────┐  ┌──────────┐  ┌────────────────┐    │
│  │  Lexer   │→ │  Parser  │→ │  SExpr (value) │    │
│  └──────────┘  └──────────┘  └────────┬───────┘    │
│                                        │            │
│          ┌─────────────────────────────┤            │
│          ↓             ↓               ↓            │
│  ┌──────────────┐ ┌─────────┐ ┌──────────────┐     │
│  │ LazyStream   │ │   AST   │ │  Emitter     │     │
│  │ (coroutine)  │ │+Visitor │ │ (pretty-prt) │     │
│  └──────────────┘ └─────────┘ └──────────────┘     │
│                                                     │
│  ┌──────────────────────────────────────────────┐   │
│  │  Pipeline / DAG  (analyzers + transformers)  │   │
│  └──────────────────────────────────────────────┘   │
│                                                     │
│  ┌──────────────┐  ┌──────────────────────────┐     │
│  │  MMap I/O    │  │  Semantics Layer         │     │
│  │  (read/write)│  │  .esema (Enriched)       │     │
│  └──────────────┘  │  .ssema (Schematic)      │     │
│                    └──────────────────────────┘     │
└─────────────────────────────────────────────────────┘
```
---

## Core Value Type

`SExpr` is the central type. Model it as a tagged union:

```cpp
struct SExpr {
    using Atom = std::string;
    using List = std::vector<SExpr>;
    std::variant<Atom, List> value;
};
```
All capabilities operate on or produce `SExpr` values.

---

## Capability Implementation Guide

### 1. Pretty-Printer (Emitter)

- Configurable indent width and max line width.
- Inline short lists; break long ones vertically.
- Entry point: `std::string emit(const SExpr&, EmitOptions = {})`.

### 2. Parser → User-Specified Data Structure

- Parse into `SExpr` first (canonical form), then provide a conversion layer:
  ```cpp
  template<typename T, typename Converter>
  std::expected<T, ParseError> parse_as(std::strin
