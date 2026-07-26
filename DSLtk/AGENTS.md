Here’s a cleaned‑up version of the document with consistent markdown structure and proper fenced code blocks.

```markdown
# Additions to `DSLtk.hpp`: PEG Support

This document specifies **new additive features** for `DSLtk.hpp`.

These changes are strictly layered on top of the existing API:

- the **old ABI remains intact**;
- the **old API remains valid**;
- existing applications using `DSLtk.hpp` **do not need to change**;
- all implementation must remain inside **`DSLtk.hpp`**;
- no new file is introduced.

## Goals

Add two new PEG‑oriented facilities:

1. **PEG‑based pattern matching**
2. **PEG‑based parser combinators**

These are built on top of the existing pattern‑matching and parser‑combinator infrastructure already present in `DSLtk.hpp`.

---

## 1. PEG Definitions

A PEG grammar is represented by a runtime object, while its rules are declared through template‑based expressions.

### Creating a PEG Definition

```cpp
auto peg = dsl::create_peg_definition(); // type: dsl::PEGDefinition
```

A PEG definition contains a set of rules, channels, and optional inheritance metadata.  
A PEG definition may also derive from an existing definition:

```cpp
auto peg2 = dsl::derive_peg_definition(peg);
```

Internally, a PEG definition may carry a `parent` reference. By default, this parent is `nullptr`.  
This allows grammars to be extended without modifying the base grammar.

---

## 2. PEG Rules

Rules are added to a PEG definition via `add_rule`.

### Example

```cpp
auto peg = dsl::create_peg_definition();

auto ws = peg.add_rule<"[ \r\t\n]+">();

auto ident = peg.add_rule<"[a-zA-Z_][a-zA-Z0-9_]*">(
    [](dsl::PEGMatch& m)
    {
        if (m.value == "foobar")
            std::cout << "Won!" << std::endl;
    });

auto number = peg.add_rule<"[0-9]+">(
    [](dsl::PEGMatch& m)
    {
        if (m.value == "666")
            std::cout << "Mark of the best! MR. BEST!!" << std::endl;
    });

ws.channel = dsl::PEGIgnoreChannel;
```

Each `add_rule` call returns an object of type:

```cpp
dsl::PEGRule
```

A `PEGRule` is mutable after creation; its attributes may be adjusted later.

---

## 3. Mutable Rule Properties

`dsl::PEGRule` supports post‑declaration customization.

### 3.1 Semantic Action

The semantic action may be replaced after the rule is created:

```cpp
ident.semantic_action = [](dsl::PEGMatch& m)
{
    /* ... */
};
```

The semantic action is invoked when the rule successfully matches, unless the rule has special control flags that alter this behavior.

### 3.2 Channels

Each rule belongs to a channel.

Channels are used for:

1. suppressing non‑semantic tokens such as whitespace;
2. grouping matches by category;
3. filtering pattern‑matching behavior;
4. enabling multi‑stream token interpretation without changing the grammar.

A built‑in channel is provided:

```cpp
dsl::PEGIgnoreChannel   // logical name: "@IGNORE"
```

This is typically used for whitespace, comments, separators, and similar ignorable text.

#### Creating a Custom Channel

Custom channels must begin with `@`.

```cpp
auto all_ones_channel = dsl::new_peg_channel("@ALL_ONES");

auto all_ones = peg.add_rule<"1+">(
    [](dsl::PEGMatch& m)
    {
        std::cout << "all ones: " << m.value << std::endl;
    });

all_ones.channel = all_ones_channel;
```

> Note: the rule property is assigned on the rule object, not on the PEG definition.

#### Channel Naming Rule

All custom channel names are required to be prefixed with `@`.

**Valid examples**

- `@IGNORE`
- `@COMMENTS`
- `@ALL_ONES`
- `@METADATA`

**Invalid examples**

- `IGNORE`
- `comments`
- `AllOnes`

### 3.3 Negated / Must‑Fail Rules

A rule may be marked as a **must‑fail rule**.

```cpp
foo.must_fail = true;
foo.if_succeed = another_rule;
```

This gives PEG‑style negative‑look behavior.

#### Semantics

If `foo.must_fail == true`:

- the rule succeeds only when its underlying pattern **fails**;
- if the pattern succeeds, the negated rule fails;
- if `if_succeed` is specified and the pattern succeeds, `if_succeed` is attempted next;
- semantic‑action execution for negated rules is optional and implementation‑defined unless explicitly enabled.

#### Recommended Interpretation

- `must_fail = false` → normal rule
- `must_fail = true`  → negated predicate (succeeds when pattern fails, consumes no input)
- `if_succeed = some_rule` → dispatch to `some_rule` after a successful original match

---

## 4. `dsl::PEGMatch`

Semantic actions receive a `dsl::PEGMatch&`.

### Minimum fields

```cpp
struct PEGMatch
{
    std::string_view value;     // matched text
    std::size_t       begin;     // start offset
    std::size_t       end;       // end offset
    dsl::PEGRule*    rule;      // matched rule
    dsl::PEGChannel  channel;   // resolved channel
    void*            user_data; // optional extension point
};
```

### Recommended additional fields

```cpp
std::size_t line;
std::size_t column;
std::size_t length;
bool        ignored;
```

These help with diagnostics and parser‑combinator integration.

---

## 5. Parsing with a PEG Definition

Once rules are registered, text may be parsed directly.

### Example

```cpp
auto txt = R"(
Foo bar;
666;
12;
1;
)";

peg.parse(txt);
```

The parse routine should:

- consume input according to PEG rule ordering;
- invoke semantic actions as matches occur;
- route or skip matches according to channels;
- release temporary parsing resources automatically.

### Return type options

1. `void` – action‑driven parsing  
2. A parse‑result object for diagnostics

#### Recommended Parse Result API

```cpp
auto result = peg.parse(txt);

result.ok();
result.failed();
result.error_offset();
result.error_line();
result.error_column();
result.message();
```

This extends, not replaces, older behavior.

---

## 6. PEG Pattern Matching

PEG definitions should also support stream‑style pattern matching.

### Intended usage

```cpp
auto matcher = dsl::peg_new_matcher(peg, my_text);

while (!matcher.is_spent())
{
    matcher
        << ident([&](dsl::PEGMatch& m) { /* ... */ })
        << number([&](dsl::PEGMatch& m) { /* ... */ })
        << matcher.wildcard([&](dsl::PEGMatch& m) { /* fallback */ });
}

matcher.close();
```

The original sketch used syntax that is not valid C++. The above keeps the same spirit while remaining implementable.

---

## 7. Recommended Matcher API

A fluent API makes PEG‑based pattern matching practical in C++.

### Construction

```cpp
auto matcher = dsl::peg_new_matcher(peg, text);
```

### State Queries

```cpp
matcher.is_spent();
matcher.position();
matcher.close();
```

### Pattern Arms

Rule objects are invocable with a callback:

```cpp
ident([](dsl::PEGMatch& m) { /* ... */ })
number([](dsl::PEGMatch& m) { /* ... */ })
```

These yield lightweight “match‑arm” objects consumable via `operator<<`.

### Wildcard

```cpp
matcher.wildcard([](dsl::PEGMatch& m) { /* ... */ });
```

The wildcard matches when no preceding arm matches at the current position.

### Placeholder Form (optional)

If the library already supports placeholders elsewhere:

```cpp
number(dsl::_)   // capture without explicit callback yet
```

The callback form is the safest baseline API.

---

## 8. Rule Ordering and PEG Semantics

PEGs are **ordered**; this must be preserved.

When several rules are tested at the same position:

- the earliest applicable rule wins;
- once a rule succeeds, later alternatives are not tried at that decision point;
- channels do not alter precedence; they affect handling after a match is recognized.

---

## 9. Uses of Channels

### 9.1 Ignoring Non‑Semantic Input

The built‑in channel `@IGNORE` is used for tokens that should be recognized but not emitted into the semantic stream (e.g., whitespace, comments, separators).

```cpp
auto ws = peg.add_rule<"[ \t\r\n]+">();
ws.channel = dsl::PEGIgnoreChannel;
```

### 9.2 Filtering in Pattern Matching

Channels can act as pattern filters.

```cpp
auto matcher = dsl::peg_new_matcher(peg, text);

matcher.only("@ALL_ONES");
matcher.exclude("@IGNORE");
matcher.include("@DEFAULT").include("@ALL_ONES").exclude("@IGNORE");
```

### 9.3 Multi‑Pass Interpretation

Channels enable a single parse to produce multiple logical views:

- `@IGNORE` – whitespace/comments
- `@DEFAULT` – ordinary syntax
- `@DOCS` – documentation comments
- `@ATTRS` – annotations
- `@ALL_ONES` – special semantic categories

---

## 10. PEG Parser Combinators

PEG‑aware combinators should layer over the existing framework while preserving ordered choice, greedy sequencing, lookahead, etc.

### Core combinators

```cpp
dsl::peg_seq(a, b, c);        // sequence
dsl::peg_choice(a, b, c);     // ordered choice
dsl::peg_opt(a);              // optional
dsl::peg_many(a);             // zero or more
dsl::peg_many1(a);            // one or more
dsl::peg_and(a);              // positive lookahead
dsl::peg_not(a);              // negative lookahead
dsl::peg_rule(r);             // adapt PEGRule into a parser
dsl::peg_channel("@NAME", a); // assign channel to parser result
```

These can be thin wrappers around existing combinator machinery.

---

## 11. Integration with Existing Pattern Matching

- PEG rules should be adaptable into existing matcher arms.
- Existing callbacks and value extractors remain usable.
- Old non‑PEG pattern matching works unchanged.
- PEG matching feels like a natural extension, not a separate subsystem.

---

## 12. Inheritance / Grammar Derivation

Derived PEG definitions inherit rules from their parent definition.

```cpp
auto base    = dsl::create_peg_definition();
auto derived = dsl::derive_peg_definition(base);
```

**Recommended behavior**

- Parent rules stay visible in the derived grammar.
- New rules may be added in the derived grammar.
- Derived rules may shadow parent rules (implementation‑defined).
- Channels remain valid across the inheritance chain.
- Semantic actions can be replaced in the derived grammar without mutating the parent.

---

## 13. Error Handling and Diagnostics

A richer diagnostic API is useful.

### Suggested result type

```cpp
struct PEGParseResult
{
    bool        success;
    std::size_t offset;
    std::size_t line;
    std::size_t column;
    std::string message;
};
```

Usage:

```cpp
auto result = peg.parse(text);
if (!result.success)
    std::cerr << result.message << std::endl;
```

---

## 14. ABI Preservation Strategy

Because the old ABI must remain intact, introduce PEG additions **additively**:

1. Add new types instead of altering existing layouts.
2. Wrap existing parser‑combinator and matcher internals rather than replace them.
3. Keep old overloads untouched.
4. Add new overloads, helper factories, and adapter objects.
5. Prefer inline/template definitions inside `DSLtk.hpp` to avoid link‑time breakage.

All changes stay header‑only within the existing file.

---

## 15. Minimal Proposed Surface API

```cpp
namespace dsl
{
    struct PEGDefinition;
    struct PEGRule;
    struct PEGMatch;
    struct PEGParseResult;
    struct PEGMatcher;
    struct PEGChannel;

    inline constexpr /*...*/ PEGIgnoreChannel = /* @IGNORE */;

    PEGDefinition create_peg_definition();
    PEGDefinition derive_peg_definition(const PEGDefinition& parent);
    PEGChannel   new_peg_channel(std::string_view name);
    PEGMatcher   peg_new_matcher(PEGDefinition& peg, std::string_view text);
}
```

Member‑level operations:

```cpp
peg.add_rule<"...">();
peg.add_rule<"...">(callback);
peg.parse(text);

rule.semantic_action = callback;
rule.channel          = some_channel;
rule.must_fail        = true;
rule.if_succeed       = other_rule;

matcher.is_spent();
matcher.close();
matcher.wildcard(callback);
matcher.only("@NAME");
matcher.include("@NAME");
matcher.exclude("@NAME");
```

---

## 16. Resolution

1. No existing API or ABI is removed or changed incompatibly.  
2. All PEG support is implemented inside `DSLtk.hpp`.  
3. `dsl::PEGDefinition` becomes the root object for PEG grammars.  
4. `dsl::PEGRule` objects are mutable and support semantic actions, channels, and guarded behavior.  
5. Channels are first‑class, supporting `@IGNORE` and user‑defined `@…` channels.  
6. PEG parsing is action‑driven by default, with optional parse‑result diagnostics.  
7. PEG pattern matching is exposed via a fluent matcher interface compatible with C++ syntax.  
8. PEG parser combinators are layered over existing combinator facilities rather than replacing them.  
9. Grammar derivation is supported through parent‑linked PEG definitions.  
10. Rule ordering follows PEG ordered‑choice semantics.  
11. Diagnostics and channel filtering are included as additive features.  
12. Implementation should prefer inline/template composition so the feature remains header‑only and non‑disruptive.

*In short: the PEG subsystem is an additive, header‑only extension to `DSLtk.hpp` that provides grammar definition, parsing, matching, combinators, channels, inheritance, and diagnostics while preserving all existing behavior.*
