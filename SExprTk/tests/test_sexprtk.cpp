#define CATCH_CONFIG_MAIN
#include <catch_amalgamated.hpp>

#include "SExprTk.hpp"

using namespace sexprtk;

TEST_CASE("parse atoms and round-trip", "[parse]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(alpha 12 \"beta\" #t)"));
    REQUIRE(cartable.root.cells.size() == 1);
    REQUIRE(cartable.ok());
    CHECK(Serializer::to_string(cartable.root) == "((alpha 12 \"beta\" #t))");
}

TEST_CASE("parse nested lists and numbers", "[parse]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a (b 1 2.5) (c (d)))"));
    REQUIRE(cartable.root.cells.size() == 1);
    CHECK(cartable.root.cells[0].head.as_list().size() == 3);
    CHECK(Serializer::to_string(cartable.root) == "((a (b 1 2.5) (c (d))))");
}

TEST_CASE("comments and whitespace are ignored", "[parse]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a ; comment\n b)"));
    REQUIRE(cartable.ok());
    CHECK(Serializer::to_string(cartable.root) == "((a b))");
}

TEST_CASE("quotes are expanded", "[parse]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a 'b)"));
    REQUIRE(cartable.ok());
    CHECK(Serializer::to_string(cartable.root) == "((a (quote b)))");
}

TEST_CASE("atoms expose correct kinds", "[atom]") {
    CHECK(Atom(true).is_bool());
    CHECK(Atom(std::int64_t{42}).is_int());
    CHECK(Atom(3.14).is_float());
    CHECK(Atom(std::string("x")).is_string());
    CHECK(Atom(std::string("y"), NodeKind::Symbol).is_symbol());
    CHECK(Atom(std::make_shared<List>()).is_list());
    CHECK(Atom{}.is_nil());
}

TEST_CASE("analyzer counts", "[analysis]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a (b 1 2) (c))"));
    CHECK(Analyzer::count_atoms(cartable.root) == 5);
    CHECK(Analyzer::count_lists(cartable.root) == 3);
    CHECK(Analyzer::depth(cartable.root) == 5);
    CHECK(Analyzer::has_symbol(cartable.root, "b"));
    CHECK_FALSE(Analyzer::has_symbol(cartable.root, "z"));
}

TEST_CASE("serializer escapes strings", "[serialize]") {
    Atom a{std::string("line\nbreak")};
    CHECK(Serializer::to_string(a) == "\"line\\nbreak\"");
}

TEST_CASE("json output", "[json]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(x 1 \"y\")"));
    auto json = cartable.to_json();
    CHECK(json.find("\"x\"") != std::string::npos);
    CHECK(json.find("1") != std::string::npos);
    CHECK(json.find("\"y\"") != std::string::npos);
}

TEST_CASE("emit xas events", "[xas]") {
    XASEventDispatcher disp;
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(x y)"), &disp);
    REQUIRE(cartable.events.size() >= 3);
    CHECK(disp.buffered.size() == cartable.events.size());
    CHECK(sexprtk_xas::to_string(disp.front().type) == "begin");
}

TEST_CASE("xas event round-trip", "[xas]") {
    sexprtk_xas::EventWire e{7, sexprtk_xas::EventType::Atom, "payload"};
    auto encoded = e.encode();
    auto decoded = sexprtk_xas::EventWire::decode(encoded);
    CHECK(decoded.sequence == 7);
    CHECK(decoded.type == sexprtk_xas::EventType::Atom);
    CHECK(decoded.payload == "payload");
}

TEST_CASE("datagram frame round-trip", "[xas]") {
    sexprtk_xas::EventWire e{42, sexprtk_xas::EventType::BeginList, "("};
    auto frame = sexprtk_xas::DatagramFrame::from_event(e);
    auto decoded = frame.to_event();
    CHECK(decoded.sequence == 42);
    CHECK(decoded.type == sexprtk_xas::EventType::BeginList);
    CHECK(decoded.payload == "(");
}

TEST_CASE("package manifest serializes", "[package]") {
    PackageManifest manifest;
    manifest.name = "demo";
    manifest.fields["mode"] = "test";
    auto toml = manifest.to_toml();
    CHECK(toml.find("name = \"demo\"") != std::string::npos);
    CHECK(toml.find("mode = \"test\"") != std::string::npos);

    auto parsed = PackageManifest::from_toml(toml);
    CHECK(parsed.name == "demo");
    CHECK(parsed.fields["mode"] == "test");
}

TEST_CASE("transformer flatten", "[transform]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a (b c) d)"));
    auto flat = Transformer::flatten(cartable.root);
    CHECK(flat.size() == 4);
}

TEST_CASE("iterator traversal", "[iterator]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a b c)"));
    Iterator it(cartable.root);
    std::size_t n = 0;
    while (!it.done()) { ++it; ++n; }
    CHECK(n == 1);
}

TEST_CASE("kernel dispatch", "[kernel]") {
    SExprTk rt;
    auto source = Source::from_string("(kernel demo)");
    CHECK(rt.run(source, LuaKernel{}) == "[Lua] ((kernel demo))");
    CHECK(rt.run(source, S7Kernel{}) == "[S7] ((kernel demo))");
    CHECK(rt.run(source, LuaKernelSemanticizer{}) == "[LuaSemanticizer] ((kernel demo))");
    CHECK(rt.run(source, S7KernelSemanticizer{}) == "[S7Semanticizer] ((kernel demo))");
}

TEST_CASE("lazy stream", "[stream]") {
    LazyStream ls("hello");
    CHECK(*ls.peek() == 'h');
    CHECK(*ls.take() == 'h');
    CHECK(*ls.take() == 'e');
    ls.append("world");
    CHECK(ls.buffer == "helloworld");
}

TEST_CASE("unterminated list produces error", "[error]") {
    SExprTk rt;
    auto cartable = rt.parse(Source::from_string("(a (b"));
    CHECK_FALSE(cartable.ok());
    CHECK(!cartable.errors.empty());
}

TEST_CASE("package manifest from_toml round-trip", "[package]") {
    std::string toml = "name = \"pkg\"\nversion = \"1.0.0\"\nentry = \"main.sx\"\n";
    auto m = PackageManifest::from_toml(toml);
    CHECK(m.name == "pkg");
    CHECK(m.version == "1.0.0");
    CHECK(m.entry == "main.sx");
}
