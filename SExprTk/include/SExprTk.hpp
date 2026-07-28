#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "SExprTk-XASEvent.h"

namespace sexprtk {

struct Atom;
struct Cell;
struct List;
struct Cartable;
struct Iterator;
struct Source;
struct Serializer;
struct Package;
struct PackageManifest;
struct XASEvent;
struct XASEventDispatcher;
struct CartableDispatcher;
struct Analyzer;
struct Transformer;
class Kernel;
class KernelSemanticizer;
class LuaKernel;
class LuaKernelSemanticizer;
class S7Kernel;
class S7KernelSemanticizer;
class SExprTk;

enum class NodeKind : std::uint8_t {
    Nil, Bool, Integer, Float, String, Symbol, List
};

inline std::string_view to_string(NodeKind k) {
    switch (k) {
    case NodeKind::Nil:     return "nil";
    case NodeKind::Bool:    return "bool";
    case NodeKind::Integer: return "integer";
    case NodeKind::Float:   return "float";
    case NodeKind::String:  return "string";
    case NodeKind::Symbol:  return "symbol";
    case NodeKind::List:    return "list";
    }
    return "unknown";
}

struct List {
    std::vector<Cell> cells {};

    void push(Cell c);
    void pop();
    bool empty() const { return cells.empty(); }
    std::size_t size() const { return cells.size(); }
    Cell& front() { return cells.front(); }
    const Cell& front() const { return cells.front(); }
    Cell& back() { return cells.back(); }
    const Cell& back() const { return cells.back(); }
    Cell& operator[](std::size_t i) { return cells[i]; }
    const Cell& operator[](std::size_t i) const { return cells[i]; }
};

struct Atom {
    using ListPtr = std::shared_ptr<List>;
    using Value = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, ListPtr>;

    Value value {nullptr};
    NodeKind kind {NodeKind::Nil};

    Atom() = default;
    Atom(std::nullptr_t) : value(nullptr), kind(NodeKind::Nil) {}
    Atom(bool v) : value(v), kind(NodeKind::Bool) {}
    Atom(std::int64_t v) : value(v), kind(NodeKind::Integer) {}
    Atom(double v) : value(v), kind(NodeKind::Float) {}
    Atom(std::string v, NodeKind k = NodeKind::String) : value(std::move(v)), kind(k) {}
    Atom(ListPtr v) : value(std::move(v)), kind(NodeKind::List) {}

    bool is_nil()    const { return kind == NodeKind::Nil; }
    bool is_bool()   const { return kind == NodeKind::Bool; }
    bool is_int()    const { return kind == NodeKind::Integer; }
    bool is_float()  const { return kind == NodeKind::Float; }
    bool is_string() const { return kind == NodeKind::String; }
    bool is_symbol() const { return kind == NodeKind::Symbol; }
    bool is_list()   const { return kind == NodeKind::List; }

    bool truthy() const {
        if (is_nil()) return false;
        if (is_bool()) return std::get<bool>(value);
        return true;
    }

    std::int64_t as_int() const {
        if (is_int()) return std::get<std::int64_t>(value);
        if (is_float()) return static_cast<std::int64_t>(std::get<double>(value));
        throw std::runtime_error("as_int on non-numeric atom");
    }

    double as_float() const {
        if (is_float()) return std::get<double>(value);
        if (is_int()) return static_cast<double>(std::get<std::int64_t>(value));
        throw std::runtime_error("as_float on non-numeric atom");
    }

    const std::string& as_string() const {
        if (is_string() || is_symbol()) return std::get<std::string>(value);
        throw std::runtime_error("as_string on non-string atom");
    }

    const List& as_list() const {
        if (is_list()) return *std::get<ListPtr>(value);
        throw std::runtime_error("as_list on non-list atom");
    }

    List& as_list() {
        if (is_list()) return *std::get<ListPtr>(value);
        throw std::runtime_error("as_list on non-list atom");
    }
};

struct Cell {
    Atom head {};
    std::vector<Cell> tail {};

    Cell() = default;
    Cell(Atom h) : head(std::move(h)) {}
    Cell(Atom h, std::vector<Cell> t) : head(std::move(h)), tail(std::move(t)) {}

    bool is_atom() const { return tail.empty() && !head.is_list(); }
    bool is_pair() const { return !tail.empty() && !head.is_list(); }
    bool is_list_cell() const { return head.is_list(); }

    const Cell& car() const { return head.is_list() ? std::get<Atom::ListPtr>(head.value)->cells.front() : *this; }
    std::vector<Cell> cdr() const { return tail; }
};

inline void List::push(Cell c) { cells.push_back(std::move(c)); }
inline void List::pop() { if (!cells.empty()) cells.pop_back(); }

struct Source {
    std::string name {};
    std::string text {};

    static Source from_string(std::string input, std::string name = "<memory>") {
        return {std::move(name), std::move(input)};
    }

    static Source from_file(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("cannot open source file: " + path.string());
        std::ostringstream ss;
        ss << in.rdbuf();
        return {path.string(), ss.str()};
    }
};

struct Serializer {
    static std::string escape(std::string_view text) {
        std::ostringstream out;
        for (char ch : text) {
            switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"':  out << "\\\""; break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            default:   out << ch;       break;
            }
        }
        return out.str();
    }

    static std::string unescape(std::string_view text) {
        std::ostringstream out;
        for (std::size_t i = 0; i < text.size(); ++i) {
            char ch = text[i];
            if (ch == '\\' && i + 1 < text.size()) {
                char esc = text[++i];
                switch (esc) {
                case 'n': out << '\n'; break;
                case 'r': out << '\r'; break;
                case 't': out << '\t'; break;
                case '\\': out << '\\'; break;
                case '"': out << '"'; break;
                default:  out << esc; break;
                }
            } else {
                out << ch;
            }
        }
        return out.str();
    }

    static std::string to_string(const Atom& atom);
    static std::string to_string(const Cell& cell);
    static std::string to_string(const List& list);
    static std::string to_json(const Atom& atom);
    static std::string to_json(const Cell& cell);
    static std::string to_json(const List& list);
    static std::string to_toml(const PackageManifest& manifest);
};

struct XASEvent {
    sexprtk_xas::EventType type {sexprtk_xas::EventType::BeginList};
    std::uint64_t sequence {0};
    std::string payload {};
    std::string source {};
};

struct Cartable {
    List root {};
    std::map<std::string, std::string> metadata {};
    std::vector<XASEvent> events {};
    std::vector<std::string> errors {};

    std::string to_string() const { return Serializer::to_string(root); }
    std::string to_json() const { return Serializer::to_json(root); }
    bool ok() const { return errors.empty(); }
};

struct Iterator {
    const List* list {nullptr};
    std::size_t index {0};

    explicit Iterator(const List& l) : list(&l) {}

    bool done() const { return !list || index >= list->cells.size(); }
    const Cell& operator*() const { return list->cells[index]; }
    const Cell* operator->() const { return &list->cells[index]; }
    Iterator& operator++() { if (!done()) ++index; return *this; }
    Iterator begin() const { return Iterator(*list); }
    Iterator end() const { Iterator it(*list); it.index = list ? list->cells.size() : 0; return it; }
    bool operator==(const Iterator& o) const { return list == o.list && index == o.index; }
    bool operator!=(const Iterator& o) const { return !(*this == o); }
};

struct LazyStream {
    std::string buffer {};
    std::size_t pos {0};
    explicit LazyStream(std::string s = {}) : buffer(std::move(s)) {}

    bool empty() const { return pos >= buffer.size(); }
    std::optional<char> peek() const { if (pos < buffer.size()) return buffer[pos]; return std::nullopt; }
    std::optional<char> take() { if (pos < buffer.size()) return buffer[pos++]; return std::nullopt; }
    void append(std::string s) { buffer += std::move(s); }
};

struct PackageManifest {
    std::string name {"SExprTk"};
    std::string version {"0.1.0"};
    std::string entry {"main.sx"};
    std::map<std::string, std::string> fields {};

    std::string to_toml() const { return Serializer::to_toml(*this); }

    static PackageManifest from_toml(std::string_view text) {
        PackageManifest m;
        std::istringstream in{std::string(text)};
        std::string line;
        while (std::getline(in, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            auto trim = [](std::string& s) {
                std::size_t a = 0;
                while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
                std::size_t b = s.size();
                while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
                s = s.substr(a, b - a);
            };
            trim(key); trim(val);
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
            if (key == "name") m.name = val;
            else if (key == "version") m.version = val;
            else if (key == "entry") m.entry = val;
            else m.fields[key] = val;
        }
        return m;
    }
};

struct Package {
    PackageManifest manifest {};
    Cartable cartable {};
    std::map<std::string, std::string> metadata {};

    std::string to_toml() const {
        std::ostringstream out;
        out << manifest.to_toml();
        for (const auto& [k, v] : metadata) out << k << " = \"" << Serializer::escape(v) << "\"\n";
        return out.str();
    }
};

struct XASEventDispatcher {
    std::function<void(const XASEvent&)> sink {};
    std::vector<XASEvent> buffered {};

    void emit(const XASEvent& event) {
        buffered.push_back(event);
        if (sink) sink(buffered.back());
    }

    void clear() { buffered.clear(); }
    std::size_t size() const { return buffered.size(); }
    const XASEvent& front() const { return buffered.front(); }
    const XASEvent& back() const { return buffered.back(); }
};

struct CartableDispatcher {
    XASEventDispatcher* dispatcher {nullptr};
    std::uint64_t seq {0};

    std::uint64_t begin_list(std::string payload = {}) {
        std::uint64_t s = ++seq;
        if (dispatcher) dispatcher->emit({sexprtk_xas::EventType::BeginList, s, std::move(payload)});
        return s;
    }
    std::uint64_t atom(std::string payload = {}) {
        std::uint64_t s = ++seq;
        if (dispatcher) dispatcher->emit({sexprtk_xas::EventType::Atom, s, std::move(payload)});
        return s;
    }
    std::uint64_t end_list(std::string payload = {}) {
        std::uint64_t s = ++seq;
        if (dispatcher) dispatcher->emit({sexprtk_xas::EventType::EndList, s, std::move(payload)});
        return s;
    }
};

struct Analyzer {
    static std::size_t count_atoms(const List& list);
    static std::size_t count_lists(const List& list);
    static std::size_t depth(const List& list);
    static bool has_symbol(const List& list, std::string_view sym);
private:
    static std::size_t count_atoms(const std::vector<Cell>& tail);
    static std::size_t count_lists(const std::vector<Cell>& tail);
    static std::size_t depth(const std::vector<Cell>& tail);
    static bool has_symbol(const std::vector<Cell>& tail, std::string_view sym);
};

struct Transformer {
    static List flatten(const List& list);
    static List quote(const List& list);
    static std::vector<Cell> map(const std::vector<Cell>& cells, std::function<Cell(const Cell&)> fn);
    static std::vector<Cell> filter(const std::vector<Cell>& cells, std::function<bool(const Cell&)> fn);
};

class KernelSemanticizer {
public:
    virtual ~KernelSemanticizer() = default;
    virtual std::string semanticize(const Cartable& cartable) const = 0;
    virtual std::string name() const = 0;
};

class Kernel {
public:
    using Fn = std::function<std::string(const Cartable&)>;

    explicit Kernel(Fn fn = {}) : fn_(std::move(fn)) {}
    virtual ~Kernel() = default;

    virtual std::string run(const Cartable& c) const { return fn_ ? fn_(c) : c.to_string(); }
    virtual std::string name() const { return "native"; }

private:
    Fn fn_ {};
};

class LuaKernel : public Kernel {
public:
    LuaKernel() : Kernel([](const Cartable& c) { return "[Lua] " + c.to_string(); }) {}
    std::string name() const override { return "lua"; }
};

class S7Kernel : public Kernel {
public:
    S7Kernel() : Kernel([](const Cartable& c) { return "[S7] " + c.to_string(); }) {}
    std::string name() const override { return "s7"; }
};

class LuaKernelSemanticizer : public KernelSemanticizer {
public:
    std::string semanticize(const Cartable& cartable) const override {
        return "[LuaSemanticizer] " + cartable.to_string();
    }
    std::string name() const override { return "lua-semanticizer"; }
};

class S7KernelSemanticizer : public KernelSemanticizer {
public:
    std::string semanticize(const Cartable& cartable) const override {
        return "[S7Semanticizer] " + cartable.to_string();
    }
    std::string name() const override { return "s7-semanticizer"; }
};

class SExprTk {
public:
    explicit SExprTk(std::string name = "SExprTk") : name_(std::move(name)) {}

    Cartable parse(const Source& source, XASEventDispatcher* dispatcher = nullptr) const {
        Parser p(source, dispatcher);
        Cartable c;
        c.root = p.parse_list();
        c.events = std::move(p.events);
        c.errors = std::move(p.errors);
        c.metadata["source"] = source.name;
        return c;
    }

    std::string run(const Source& source) const {
        return parse(source).to_string();
    }

    std::string run(const Source& source, const Kernel& kernel) const {
        return kernel.run(parse(source));
    }

    std::string run(const Source& source, const KernelSemanticizer& sem) const {
        return sem.semanticize(parse(source));
    }

    Package package(const Source& source, const PackageManifest& manifest = {}) const {
        Package pkg;
        pkg.manifest = manifest;
        pkg.cartable = parse(source);
        return pkg;
    }

    std::string name() const { return name_; }

private:
    struct Parser {
        std::string input;
        std::string source_name;
        std::size_t pos {0};
        XASEventDispatcher* dispatcher {nullptr};
        std::vector<XASEvent> events;
        std::vector<std::string> errors;
        std::uint64_t seq {0};
        std::size_t line {1};
        std::size_t column {1};

        Parser(const Source& source, XASEventDispatcher* d)
            : input(source.text), source_name(source.name), dispatcher(d) {}

        void error(std::string msg) {
            msg += " at " + source_name + ":" + std::to_string(line) + ":" + std::to_string(column);
            errors.push_back(std::move(msg));
        }

        void emit(sexprtk_xas::EventType t, std::string payload = {}) {
            XASEvent e{t, ++seq, std::move(payload), source_name};
            events.push_back(e);
            if (dispatcher) dispatcher->emit(e);
        }

        void advance() {
            if (pos < input.size() && input[pos] == '\n') { ++line; column = 1; }
            else { ++column; }
            ++pos;
        }

        void skip_ws() {
            while (pos < input.size()) {
                unsigned char ch = static_cast<unsigned char>(input[pos]);
                if (std::isspace(ch)) { advance(); continue; }
                if (input[pos] == ';') {
                    emit(sexprtk_xas::EventType::Comment, {});
                    while (pos < input.size() && input[pos] != '\n') advance();
                    continue;
                }
                break;
            }
        }

        bool eof() { skip_ws(); return pos >= input.size(); }

        std::string parse_token() {
            skip_ws();
            if (pos >= input.size()) return {};
            if (input[pos] == '"') {
                advance();
                std::ostringstream out;
                while (pos < input.size()) {
                    char ch = input[pos];
                    if (ch == '"') { advance(); break; }
                    if (ch == '\\' && pos + 1 < input.size()) {
                        advance();
                        char esc = input[pos];
                        advance();
                        switch (esc) {
                        case 'n': out << '\n'; break;
                        case 'r': out << '\r'; break;
                        case 't': out << '\t'; break;
                        case '\\': out << '\\'; break;
                        case '"': out << '"'; break;
                        default:  out << esc; break;
                        }
                    } else {
                        out << ch;
                        advance();
                    }
                }
                return "\"" + out.str() + "\"";
            }
            std::size_t start = pos;
            while (pos < input.size()) {
                char ch = input[pos];
                if (std::isspace(static_cast<unsigned char>(ch)) || ch == '(' || ch == ')' || ch == ';') break;
                advance();
            }
            return input.substr(start, pos - start);
        }

        Atom parse_atom(const std::string& tok) {
            if (tok.empty()) return Atom{};
            if (tok == "nil") return Atom{};
            if (tok == "#t" || tok == "true") return Atom(true);
            if (tok == "#f" || tok == "false") return Atom(false);
            if (tok.front() == '"' && tok.back() == '"' && tok.size() >= 2)
                return Atom(Serializer::unescape(tok.substr(1, tok.size() - 2)), NodeKind::String);
            if (tok.front() == ':') return Atom(std::string(tok), NodeKind::Symbol);

            char* end = nullptr;
            const long long i = std::strtoll(tok.c_str(), &end, 10);
            if (*end == '\0') return Atom(static_cast<std::int64_t>(i));
            char* endf = nullptr;
            const double d = std::strtod(tok.c_str(), &endf);
            if (*endf == '\0' && (tok.find_first_of(".eE") != std::string::npos || tok == "nan" || tok == "inf")) {
                if (tok == "nan") return Atom(std::numeric_limits<double>::quiet_NaN());
                if (tok == "inf") return Atom(std::numeric_limits<double>::infinity());
                return Atom(d);
            }
            return Atom(tok, NodeKind::Symbol);
        }

        Cell parse_cell() {
            skip_ws();
            if (pos >= input.size()) return {};
            if (input[pos] == '(') {
                advance();
                emit(sexprtk_xas::EventType::BeginList, "(");
                Cell c;
                c.head = Atom(std::make_shared<List>());
                auto& list = c.head.as_list();
                while (true) {
                    skip_ws();
                    if (pos >= input.size()) { error("unterminated list"); break; }
                    if (input[pos] == ')') { advance(); break; }
                    list.push(parse_cell());
                }
                emit(sexprtk_xas::EventType::EndList, ")");
                return c;
            }
            if (input[pos] == ')') { error("unexpected ')'"); advance(); return {}; }
            if (input[pos] == '\'' || input[pos] == '`') {
                char quote = input[pos];
                advance();
                Cell quoted = parse_cell();
                Cell wrapper;
                wrapper.head = Atom(std::string(quote == '\'' ? "quote" : "quasiquote"), NodeKind::Symbol);
                wrapper.tail.push_back(quoted);
                return wrapper;
            }
            const auto tok = parse_token();
            emit(sexprtk_xas::EventType::Atom, tok);
            Cell c;
            c.head = parse_atom(tok);
            return c;
        }

        List parse_list() {
            List list;
            while (!eof()) {
                list.push(parse_cell());
            }
            return list;
        }
    };

    std::string name_;
};

inline std::string Serializer::to_string(const Atom& atom) {
    switch (atom.kind) {
    case NodeKind::Nil:     return "nil";
    case NodeKind::Bool:    return std::get<bool>(atom.value) ? "#t" : "#f";
    case NodeKind::Integer: return std::to_string(std::get<std::int64_t>(atom.value));
    case NodeKind::Float: {
        std::ostringstream ss;
        ss << std::get<double>(atom.value);
        return ss.str();
    }
    case NodeKind::String:  return "\"" + escape(std::get<std::string>(atom.value)) + "\"";
    case NodeKind::Symbol:  return std::get<std::string>(atom.value);
    case NodeKind::List:    return to_string(*std::get<Atom::ListPtr>(atom.value));
    }
    return "nil";
}

inline std::string Serializer::to_string(const Cell& cell) {
    if (cell.head.kind == NodeKind::List) return to_string(*std::get<Atom::ListPtr>(cell.head.value));
    if (cell.tail.empty()) return to_string(cell.head);
    std::ostringstream out;
    out << "(" << to_string(cell.head);
    for (const auto& sub : cell.tail) out << " " << to_string(sub);
    out << ")";
    return out.str();
}

inline std::string Serializer::to_string(const List& list) {
    std::ostringstream out;
    out << "(";
    for (std::size_t i = 0; i < list.cells.size(); ++i) {
        if (i) out << " ";
        out << to_string(list.cells[i]);
    }
    out << ")";
    return out.str();
}

inline std::string Serializer::to_json(const Atom& atom) {
    switch (atom.kind) {
    case NodeKind::Nil:     return "null";
    case NodeKind::Bool:    return std::get<bool>(atom.value) ? "true" : "false";
    case NodeKind::Integer: return std::to_string(std::get<std::int64_t>(atom.value));
    case NodeKind::Float: {
        std::ostringstream ss;
        ss << std::get<double>(atom.value);
        return ss.str();
    }
    case NodeKind::String:  return "\"" + escape(std::get<std::string>(atom.value)) + "\"";
    case NodeKind::Symbol:  return "\"" + escape(std::get<std::string>(atom.value)) + "\"";
    case NodeKind::List:    return to_json(*std::get<Atom::ListPtr>(atom.value));
    }
    return "null";
}

inline std::string Serializer::to_json(const Cell& cell) {
    if (cell.head.kind == NodeKind::List) return to_json(*std::get<Atom::ListPtr>(cell.head.value));
    if (cell.tail.empty()) return to_json(cell.head);
    std::ostringstream out;
    out << "{";
    out << "\"head\":" << to_json(cell.head);
    out << ",\"tail\":[";
    for (std::size_t i = 0; i < cell.tail.size(); ++i) {
        if (i) out << ",";
        out << to_json(cell.tail[i]);
    }
    out << "]}";
    return out.str();
}

inline std::string Serializer::to_json(const List& list) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < list.cells.size(); ++i) {
        if (i) out << ",";
        out << to_json(list.cells[i]);
    }
    out << "]";
    return out.str();
}

inline std::string Serializer::to_toml(const PackageManifest& manifest) {
    std::ostringstream out;
    out << "name = \"" << escape(manifest.name) << "\"\n";
    out << "version = \"" << escape(manifest.version) << "\"\n";
    out << "entry = \"" << escape(manifest.entry) << "\"\n";
    for (const auto& [k, v] : manifest.fields) {
        out << k << " = \"" << escape(v) << "\"\n";
    }
    return out.str();
}

inline std::size_t Analyzer::count_atoms(const List& list) {
    std::size_t n = 0;
    for (const auto& cell : list.cells) {
        if (cell.head.kind != NodeKind::List) ++n;
        else n += count_atoms(cell.head.as_list());
        n += count_atoms(cell.tail);
    }
    return n;
}

inline std::size_t Analyzer::count_atoms(const std::vector<Cell>& tail) {
    std::size_t n = 0;
    for (const auto& c : tail) {
        if (c.head.kind != NodeKind::List) ++n;
        else n += count_atoms(c.head.as_list());
        n += count_atoms(c.tail);
    }
    return n;
}

inline std::size_t Analyzer::count_lists(const List& list) {
    std::size_t n = 0;
    for (const auto& cell : list.cells) {
        if (cell.head.kind == NodeKind::List) { ++n; n += count_lists(cell.head.as_list()); }
        n += count_lists(cell.tail);
    }
    return n;
}

inline std::size_t Analyzer::count_lists(const std::vector<Cell>& tail) {
    std::size_t n = 0;
    for (const auto& c : tail) {
        if (c.head.kind == NodeKind::List) { ++n; n += count_lists(c.head.as_list()); }
        n += count_lists(c.tail);
    }
    return n;
}

inline std::size_t Analyzer::depth(const List& list) {
    std::size_t d = 1;
    for (const auto& cell : list.cells) {
        std::size_t local = 1;
        if (cell.head.kind == NodeKind::List) local += depth(cell.head.as_list()) + 1;
        if (!cell.tail.empty()) local += depth(cell.tail) + 1;
        d = std::max(d, local);
    }
    return d;
}

inline std::size_t Analyzer::depth(const std::vector<Cell>& tail) {
    std::size_t d = 1;
    for (const auto& c : tail) {
        std::size_t local = 1;
        if (c.head.kind == NodeKind::List) local += depth(c.head.as_list()) + 1;
        if (!c.tail.empty()) local += depth(c.tail) + 1;
        d = std::max(d, local);
    }
    return d;
}

inline bool Analyzer::has_symbol(const List& list, std::string_view sym) {
    for (const auto& cell : list.cells) {
        if (cell.head.is_symbol() && cell.head.as_string() == sym) return true;
        if (cell.head.is_list() && has_symbol(cell.head.as_list(), sym)) return true;
        if (has_symbol(cell.tail, sym)) return true;
    }
    return false;
}

inline bool Analyzer::has_symbol(const std::vector<Cell>& tail, std::string_view sym) {
    for (const auto& c : tail) {
        if (c.head.is_symbol() && c.head.as_string() == sym) return true;
        if (c.head.is_list() && has_symbol(c.head.as_list(), sym)) return true;
        if (has_symbol(c.tail, sym)) return true;
    }
    return false;
}

inline List Transformer::flatten(const List& list) {
    List out;
    for (const auto& c : list.cells) {
        if (c.head.is_list()) {
            auto inner = flatten(c.head.as_list());
            for (auto& x : inner.cells) out.push(std::move(x));
        } else {
            out.push(c);
        }
        if (!c.tail.empty()) {
            auto tail_cells = c.tail;
            auto tail = flatten(List{tail_cells});
            for (auto& x : tail.cells) out.push(std::move(x));
        }
    }
    return out;
}

inline List Transformer::quote(const List& list) {
    List out;
    for (const auto& c : list.cells) {
        if (c.head.is_list()) {
            out.push(Cell(Atom(std::make_shared<List>(quote(c.head.as_list())))));
        } else {
            out.push(c);
        }
    }
    return out;
}

inline std::vector<Cell> Transformer::map(const std::vector<Cell>& cells, std::function<Cell(const Cell&)> fn) {
    std::vector<Cell> out;
    out.reserve(cells.size());
    for (const auto& c : cells) out.push_back(fn(c));
    return out;
}

inline std::vector<Cell> Transformer::filter(const std::vector<Cell>& cells, std::function<bool(const Cell&)> fn) {
    std::vector<Cell> out;
    for (const auto& c : cells) if (fn(c)) out.push_back(c);
    return out;
}

} // namespace sexprtk
