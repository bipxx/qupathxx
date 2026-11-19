// SPDX-License-Identifier: AGPL-3.0-only
// Header-only transport regression; no provider module, image data or corpus.
#include "../../plugins/format-common/json_tree.hpp"
#include "../../plugins/format-common/annotation_json.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace json = qpx_native_json;
namespace old = qpx_annotation_json;
namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Any attempt to grow past the explicit test arena fails rather than finding a
// heap fallback. Input strings belong to the test, outside the parser arena.
class RejectingUpstream final : public std::pmr::memory_resource {
public:
    std::size_t attempted_allocations = 0;
private:
    void* do_allocate(std::size_t, std::size_t) override {
        ++attempted_allocations;
        throw std::bad_alloc();
    }
    void do_deallocate(void*, std::size_t, std::size_t) override {}
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
};
struct Arena {
    std::vector<std::byte> bytes;
    RejectingUpstream upstream;
    std::pmr::monotonic_buffer_resource memory;
    explicit Arena(std::size_t size = 256u * 1024)
        : bytes(size), memory(bytes.data(), bytes.size(), &upstream) {}
};

template<class F> void refuses(F&& action, qpx_reader_status_v1 expected = QPX_READER_IO_ERROR,
    std::string_view message = {}) {
    std::array<char, 256> storage{};
    qpx_reader_text_v1 error{storage.data(), storage.size(), 0};
    const auto status = qpx_binary::boundary(&error, [&]() -> qpx_reader_status_v1 {
        action();
        return QPX_READER_OK;
    });
    check(status == expected, "JSON refusal used an unexpected native status");
    check(error.data == storage.data() && error.capacity == storage.size() &&
        error.size && error.size <= storage.size(), "JSON refusal lost bounded error text");
    check(message.empty() || std::string_view(storage.data(), std::size_t(error.size)).find(message) != std::string_view::npos,
        "JSON refusal did not exercise the intended boundary");
}

const std::string properties_literal = R"({ "\u0069d" : 184467440737095516160,
 "negative" : -184467440737095516160,
 "precise" : 0.12345678901234567890123456789,
 "exponents" : [1.2300e+004, -4.56E-0007, -0.0, 1e9999],
 "nested" : [{"escaped\u0020key":"\u03bc\/\uD834\uDD1E\n"}, true, null]
})";

void exact_raw_spans_and_lifetime() {
    std::string owned_properties, owned_number, owned_unicode;
    {
        // Tree borrows this exact allocation. Neither input nor arena may be
        // moved, changed or destroyed while Tree/raw views are consumed.
        const auto root_literal = std::string("{\"properties\" : ") + properties_literal +
            ", \"bounded\":18446744073709551615}";
        const auto input = std::string(" \r\n") + root_literal + "\t\n";
        Arena arena;
        const json::Tree tree(input, &arena.memory, {});
        check(tree.input.data() == input.data() && tree.input.size() == input.size(), "Tree copied or replaced borrowed input");
        check(tree.root().raw == root_literal && tree.root().raw.data() == input.data() + 3,
            "Root lexical span changed whitespace or input ownership");
        const auto& properties = tree.get(tree.root(), "properties");
        check(properties.raw == properties_literal &&
            properties.raw.data() == input.data() + input.find(properties_literal),
            "Nested properties were reformatted, rounded or detached from input");
        check(tree.get(properties, "id").raw == "184467440737095516160" &&
            tree.get(properties, "id").key == "id", "Escaped key/large unsigned raw value changed");
        check(tree.get(properties, "negative").raw == "-184467440737095516160", "Large negative raw value changed");
        check(tree.get(properties, "precise").raw == "0.12345678901234567890123456789", "Long decimal was rounded");
        const auto& exponents = tree.array(tree.get(properties, "exponents"));
        check(exponents.size() == 4 && tree.nodes[exponents[0]].raw == "1.2300e+004" &&
            tree.nodes[exponents[1]].raw == "-4.56E-0007" && tree.nodes[exponents[2]].raw == "-0.0" &&
            tree.nodes[exponents[3]].raw == "1e9999", "Numeric exponent/sign spelling changed");
        const auto zero = json::real(tree.nodes[exponents[2]]);
        check(zero == 0 && std::signbit(zero), "Explicit finite conversion lost negative zero");
        refuses([&] { json::integer(tree.get(properties, "id")); }, QPX_READER_IO_ERROR, "unsigned integer");
        refuses([&] { json::integer(tree.get(properties, "negative")); }, QPX_READER_IO_ERROR, "unsigned integer");
        refuses([&] { json::real(tree.nodes[exponents[3]]); }, QPX_READER_IO_ERROR, "finite binary64");
        check(json::integer(tree.get(tree.root(), "bounded")) == UINT64_MAX, "Exact UInt64 maximum conversion changed");
        const auto& nested = tree.array(tree.get(properties, "nested"));
        const auto& escaped = tree.get(tree.nodes[nested[0]], "escaped key");
        const std::string expected_unicode = "\xCE\xBC/\xF0\x9D\x84\x9E\n";
        check(escaped.raw == R"("\u03bc\/\uD834\uDD1E\n")" && json::string(escaped) == std::string_view(expected_unicode),
            "Escaped Unicode/key decoding changed the exact raw string span");
        check(tree.nodes[nested[1]].kind == json::Kind::Boolean && tree.nodes[nested[1]].raw == "true" &&
            tree.nodes[nested[2]].kind == json::Kind::Null && tree.nodes[nested[2]].raw == "null",
            "Boolean/null raw spans changed");
        check(tree.nodes.get_allocator().resource() == &arena.memory, "Tree nodes escaped the caller arena");
        for (const auto& node : tree.nodes) {
            check(node.key.get_allocator().resource() == &arena.memory &&
                node.value.get_allocator().resource() == &arena.memory &&
                node.children.get_allocator().resource() == &arena.memory, "Tree child/text storage escaped the caller arena");
            const auto at = std::uintptr_t(node.raw.data()), begin = std::uintptr_t(input.data());
            check(at >= begin && at - begin <= input.size() && node.raw.size() <= input.size() - (at - begin),
                "A raw token points outside its live input");
        }
        check(arena.upstream.attempted_allocations == 0, "Ordinary parse exceeded its fixed arena");
        owned_properties.assign(properties.raw.data(), properties.raw.size());
        owned_number.assign(tree.get(properties, "id").raw.data(), tree.get(properties, "id").raw.size());
        owned_unicode.assign(json::string(escaped));
        // Tree dies before both its arena and input. Only explicit owned copies
        // below survive; no dangling Tree or raw string_view is ever accessed.
    }
    check(owned_properties == properties_literal && owned_number == "184467440737095516160" &&
        owned_unicode == "\xCE\xBC/\xF0\x9D\x84\x9E\n", "Owned metadata copy did not survive borrowed-input retirement");
}

std::string object(std::size_t count) {
    std::string input = "{";
    for (std::size_t i = 0; i < count; ++i) {
        if (i) input += ',';
        input += "\"k" + std::to_string(i) + "\":0";
    }
    return input + '}';
}
std::string array(std::size_t count) {
    std::string input = "[";
    for (std::size_t i = 0; i < count; ++i) {
        if (i) input += ',';
        input += '0';
    }
    return input + ']';
}
std::string nested(unsigned depth) { return std::string(depth, '[') + "0" + std::string(depth, ']'); }

void limits_and_bounded_pmr() {
    {
        const auto input = object(64);
        Arena arena;
        const json::Tree tree(input, &arena.memory, {});
        check(tree.root().children.size() == 64 && tree.object_member_limit == 64 && tree.node_limit == 1000000 &&
            tree.depth_limit == 32, "Generic three-argument parser defaults changed");
    }
    refuses([] { const auto input = object(65); Arena arena; json::Tree tree(input, &arena.memory, {}); },
        QPX_READER_IO_ERROR, "object member limit");
    {
        const auto input = object(129);
        Arena arena;
        const json::Tree tree(input, &arena.memory, {}, 16384, 16384, 32);
        check(tree.root().children.size() == 129 && tree.object_member_limit == 16384 && tree.node_limit == 16384,
            "Explicit NGFF dictionary/node bounds were not selected");
    }
    refuses([] { const auto input = object(3); Arena arena; json::Tree tree(input, &arena.memory, {}, 2, 100, 32); },
        QPX_READER_IO_ERROR, "object member limit");
    {
        const auto input = array(2);
        Arena arena;
        const json::Tree tree(input, &arena.memory, {}, 64, 3, 32);
        check(tree.nodes.size() == 3, "Exact configurable node boundary was narrowed");
    }
    refuses([] { const auto input = array(3); Arena arena; json::Tree tree(input, &arena.memory, {}, 64, 3, 32); },
        QPX_READER_IO_ERROR, "nesting/node limit");
    {
        const auto input = array(16383);
        Arena arena(16u * 1024 * 1024);
        const json::Tree tree(input, &arena.memory, {}, 16384, 16384, 32);
        check(tree.nodes.size() == 16384 && arena.upstream.attempted_allocations == 0,
            "Explicit16384-node profile failed within its supplied fixed arena");
    }
    refuses([] { const auto input = array(16384); Arena arena(16u * 1024 * 1024); json::Tree tree(input, &arena.memory, {}, 16384, 16384, 32); },
        QPX_READER_IO_ERROR, "nesting/node limit");
    for (const auto depth : {4u, 32u}) {
        const auto input = nested(depth);
        Arena arena;
        const json::Tree tree(input, &arena.memory, {}, 64, 1000000, depth);
        check(tree.nodes.size() == depth + 1, "Exact depth boundary was narrowed");
        refuses([&] { const auto too_deep = nested(depth + 1); Arena rejected; json::Tree value(too_deep, &rejected.memory, {}, 64, 1000000, depth); },
            QPX_READER_IO_ERROR, "nesting/node limit");
    }
    {
        const auto input = nested(32);
        Arena arena;
        const json::Tree tree(input, &arena.memory, {});
        check(tree.nodes.size() == 33, "Default depth32 behavior changed");
    }
    refuses([] { const auto input = nested(33); Arena arena; json::Tree tree(input, &arena.memory, {}); },
        QPX_READER_IO_ERROR, "nesting/node limit");
    Arena exhausted(1);
    const std::string input = R"({"key":"value"})";
    refuses([&] { json::Tree tree(input, &exhausted.memory, {}); }, QPX_READER_INVALID_REQUEST, "work budget exhausted");
    check(exhausted.upstream.attempted_allocations != 0, "PMR exhaustion did not reach the rejecting upstream");
}

void lexical_and_syntax_controls() {
    for (const auto& input : std::vector<std::string>{
        R"({"a":1,"a":2})", R"({"a":1,"\u0061":2})", R"({"outer":{"x":1,"x":2}})",
        R"([01])", R"([-])", R"([1.])", R"([1e])", R"([1e+])", R"([1,])", R"({"a":})",
        R"({"a" 1})", R"({"a":1,})", R"({} {})", "truex", "null null", "", " ",
        "NaN", "Infinity", "-Infinity", "0x1", ".1", "+1",
        R"("\uD800")", R"("\uDC00")", R"("\uD800\u1234")", R"("\u0000")",
        std::string("\"unescaped\ncontrol\""), std::string("\"\xc0\xaf\"", 4)}) {
        refuses([&] { Arena arena; json::Tree tree(input, &arena.memory, {}); });
    }
    {
        const std::string input = R"({"a":1,"\\u0061":2})";
        Arena arena;
        const json::Tree tree(input, &arena.memory, {});
        check(json::integer(tree.get(tree.root(), "a")) == 1 &&
            json::integer(tree.get(tree.root(), R"(\u0061)")) == 2, "Escaped backslash key was decoded twice");
    }
    const auto unicode_duplicate = std::string(R"({"\u03bc":1,")") + "\xCE\xBC" + R"(":2})";
    refuses([&] { Arena arena; json::Tree tree(unicode_duplicate, &arena.memory, {}); },
        QPX_READER_IO_ERROR, "Duplicate JSON object key");
    {
        const auto input = std::string("{\"") + std::string(256, 'k') + "\":0}";
        Arena arena;
        const json::Tree tree(input, &arena.memory, {});
        check(tree.nodes[tree.root().children[0]].key.size() == 256, "Default256-byte key boundary changed");
    }
    refuses([] { const auto input = std::string("{\"") + std::string(257, 'k') + "\":0}"; Arena arena; json::Tree tree(input, &arena.memory, {}); },
        QPX_READER_IO_ERROR, "key exceeds native bound");
    {
        const std::string input(1024, '1');
        Arena arena;
        const json::Tree tree(input, &arena.memory, {});
        check(tree.root().raw == input, "Default1024-byte lexical number was rounded or rejected");
    }
    refuses([] { const std::string input(1025, '1'); Arena arena; json::Tree tree(input, &arena.memory, {}); },
        QPX_READER_IO_ERROR, "number exceeds lexical bound");
    {
        const auto input = '"' + std::string(1024 * 1024, 'x') + '"';
        Arena arena(16u * 1024 * 1024);
        const json::Tree tree(input, &arena.memory, {});
        check(json::string(tree.root()).size() == 1024 * 1024 && tree.root().raw == input,
            "Default1MiB string bound changed");
    }
    refuses([] { const auto input = '"' + std::string(1024 * 1024 + 1, 'x') + '"'; Arena arena(16u * 1024 * 1024); json::Tree tree(input, &arena.memory, {}); },
        QPX_READER_IO_ERROR, "string exceeds native bound");
}

struct Cancellation {
    unsigned calls = 0, stop_at = 1;
    static int32_t QPX_READER_CALL callback(void* value) {
        auto& state = *static_cast<Cancellation*>(value);
        return ++state.calls >= state.stop_at;
    }
};
void cancellation() {
    const auto input = array(256);
    for (const auto stop_at : {1u, 30u}) {
        Arena arena;
        Cancellation state{0, stop_at};
        refuses([&] { json::Tree tree(input, &arena.memory, {Cancellation::callback, &state}); },
            QPX_READER_CANCELLED, "cancelled");
        check(state.calls == stop_at, "Parser did not stop at the first cancellation report");
    }
}

void annotation_compatibility() {
    static_assert(std::is_same_v<old::Tree, json::Tree> && std::is_same_v<old::Node, json::Node> &&
        std::is_same_v<old::Kind, json::Kind>, "Annotation parser aliases changed");
    static_assert(std::is_same_v<old::Row, qpx_annotation_source::Row> &&
        std::is_same_v<old::Facts, qpx_annotation_source::Facts>, "Annotation source aliases changed");
    Arena arena;
    old::Row row(&arena.memory);
    old::Facts facts(&arena.memory);
    {
        const auto input = std::string("{\"properties\":") + properties_literal +
            ",\"id\":18446744073709551615}";
        const old::Tree tree(input, &arena.memory, {});
        const auto& properties = tree.get(tree.root(), "properties");
        old::fact(row.facts, "original.properties", properties.raw);
        old::number(facts, "exact.id", old::integer(tree.get(tree.root(), "id")));
        row.id = "authored-row";
        check(tree.object_member_limit == 64 && tree.node_limit == 1000000 && tree.depth_limit == 32,
            "Annotation three-argument defaults changed during extraction");
    }
    check(row.id == "authored-row" && row.facts.size() == 1 &&
        std::string_view(row.facts[0].second) == std::string_view(properties_literal) &&
        facts.size() == 1 && facts[0].second == "18446744073709551615",
        "Annotation Row/Facts lost owned source text after Tree/input retirement");
    {
        const auto input = object(64);
        const old::Tree tree(input, &arena.memory, {});
        check(tree.root().children.size() == 64, "Old annotation64-member object was rejected");
    }
    refuses([] { const auto input = object(65); Arena arena; old::Tree tree(input, &arena.memory, {}); },
        QPX_READER_IO_ERROR, "object member limit");
}

}

int main() {
    try {
        exact_raw_spans_and_lifetime();
        limits_and_bounded_pmr();
        lexical_and_syntax_controls();
        cancellation();
        annotation_compatibility();
        std::cout << "{\"passed\":true,\"raw_properties_exact\":true,\"generic_limits\":[64,1000000,32],"
                     "\"ngff_limits\":[16384,16384,32],\"annotation_compatibility\":true}\n";
        return 0;
    } catch (const qpx_binary::Failure& error) {
        std::cerr << error.message << " (native status " << error.status << ")\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
