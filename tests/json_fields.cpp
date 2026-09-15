// JSON output shape: fixed prefix (ts/level/event), native typing for every
// kv() overload, string escaping, and a strict validator over tricky input.
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include "test_util.hpp"

// A small strict JSON validator (RFC 8259 subset: no leading zeros check, but
// full structure, escapes, and number grammar). Good enough to catch a stray
// quote, comma, or control character in a log line.
namespace {

struct Parser {
    const std::string& s;
    std::size_t i = 0;
    bool ok = true;

    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    }
    bool eat(char c) {
        if (i < s.size() && s[i] == c) {
            ++i;
            return true;
        }
        return false;
    }
    void fail() { ok = false; }

    void string() {
        if (!eat('"')) return fail();
        while (i < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[i++]);
            if (c == '"') return;
            if (c < 0x20) return fail();  // raw control character
            if (c == '\\') {
                if (i >= s.size()) return fail();
                char e = s[i++];
                if (e == 'u') {
                    for (int k = 0; k < 4; ++k) {
                        if (i >= s.size() || !std::isxdigit(static_cast<unsigned char>(s[i++])))
                            return fail();
                    }
                } else if (std::string("\"\\/bfnrt").find(e) == std::string::npos) {
                    return fail();
                }
            }
        }
        fail();  // unterminated
    }
    void number() {
        eat('-');
        if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) return fail();
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        if (eat('.')) {
            if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) return fail();
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) return fail();
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        }
    }
    void literal(const char* lit) {
        for (const char* p = lit; *p; ++p)
            if (!eat(*p)) return fail();
    }
    void value() {
        ws();
        if (i >= s.size()) return fail();
        char c = s[i];
        if (c == '{')
            object();
        else if (c == '[')
            array();
        else if (c == '"')
            string();
        else if (c == 't')
            literal("true");
        else if (c == 'f')
            literal("false");
        else if (c == 'n')
            literal("null");
        else
            number();
        ws();
    }
    void object() {
        eat('{');
        ws();
        if (eat('}')) return;
        for (;;) {
            ws();
            string();
            if (!ok) return;
            ws();
            if (!eat(':')) return fail();
            value();
            if (!ok) return;
            if (eat(',')) continue;
            if (eat('}')) return;
            return fail();
        }
    }
    void array() {
        eat('[');
        ws();
        if (eat(']')) return;
        for (;;) {
            value();
            if (!ok) return;
            if (eat(',')) continue;
            if (eat(']')) return;
            return fail();
        }
    }
};

bool valid_json(const std::string& text) {
    Parser p{text};
    p.value();
    return p.ok && p.i == text.size();
}

}  // namespace

int main() {
    // The validator itself must reject broken input, or it proves nothing.
    CHECK(valid_json("{\"a\":1,\"b\":[true,null,\"x\\n\"]}"));
    CHECK(!valid_json("{\"a\":1,}"));
    CHECK(!valid_json("{\"a\":\"unterminated}"));
    CHECK(!valid_json("{\"a\":\"raw\ttab\"}"));
    CHECK(!valid_json("{\"a\":01x}"));

    std::vector<std::string> lines;
    {
        c_log::Logger log(test::quiet_options());
        log.add_sink<test::CaptureSink>(&lines);

        log.error("db.fail").kv("code", 503).kv("ok", false).kv("ratio", 0.5).kv("note", "a\"b");

        // Every overload.
        log.info("types")
            .kv("i8", static_cast<std::int8_t>(-8))
            .kv("u8", static_cast<std::uint8_t>(200))
            .kv("short", static_cast<short>(-3))
            .kv("ushort", static_cast<unsigned short>(3))
            .kv("long", -1234567890L)
            .kv("ll", std::numeric_limits<long long>::min())
            .kv("ull", std::numeric_limits<unsigned long long>::max())
            .kv("size", static_cast<std::size_t>(99))
            .kv("float", 1.5f)
            .kv("double", -2.25)
            .kv("nan", std::nan(""))
            .kv("inf", std::numeric_limits<double>::infinity())
            .kv("chr", 'z')
            .kv("sv", std::string_view("view"))
            .kv("str", std::string("owned"))
            .kv("cstr", "literal")
            .kv("nullc", static_cast<const char*>(nullptr))
            .kv("null", nullptr)
            .kv("opt_some", std::optional<int>(7))
            .kv("opt_none", std::optional<std::string>())
            .kv_raw("obj", "{\"nested\":[1,2,3]}")
            .kv("t", true)
            .message("free text");

        // Escaping in keys, values and the event name.
        log.warn("ev\"ent\\with\nnewline")
            .kv("k\"ey", "v\\al\tue")
            .kv("ctrl", std::string("a\x01"
                                    "b\x1f"
                                    "c"))
            .kv("utf8", "h\xC3\xA9llo \xE2\x9C\x93");  // raw UTF-8 passes through
    }
    CHECK(lines.size() == 3);
    for (const auto& l : lines) CHECK(valid_json(l));

    const std::string& l = lines[0];
    CHECK(l.rfind("{\"ts\":\"", 0) == 0);  // fixed prefix comes first
    CHECK_CONTAINS(l, "Z\",\"level\":\"error\",\"event\":\"db.fail\"");
    CHECK_CONTAINS(l, "\"code\":503");  // number, unquoted
    CHECK_NOT_CONTAINS(l, "\"code\":\"503\"");
    CHECK_CONTAINS(l, "\"ok\":false");
    CHECK_CONTAINS(l, "\"ratio\":0.5");
    CHECK_CONTAINS(l, "\"note\":\"a\\\"b\"");
    CHECK(l.back() == '}');

    const std::string& t = lines[1];
    CHECK_CONTAINS(t, "\"i8\":-8");
    CHECK_CONTAINS(t, "\"u8\":200");
    CHECK_CONTAINS(t, "\"short\":-3");
    CHECK_CONTAINS(t, "\"ushort\":3");
    CHECK_CONTAINS(t, "\"long\":-1234567890");
    CHECK_CONTAINS(t, "\"ll\":-9223372036854775808");
    CHECK_CONTAINS(t, "\"ull\":18446744073709551615");
    CHECK_CONTAINS(t, "\"size\":99");
    CHECK_CONTAINS(t, "\"float\":1.5");
    CHECK_CONTAINS(t, "\"double\":-2.25");
    CHECK_CONTAINS(t, "\"nan\":null");
    CHECK_CONTAINS(t, "\"inf\":null");
    CHECK_CONTAINS(t, "\"chr\":\"z\"");
    CHECK_CONTAINS(t, "\"sv\":\"view\"");
    CHECK_CONTAINS(t, "\"str\":\"owned\"");
    CHECK_CONTAINS(t, "\"cstr\":\"literal\"");
    CHECK_CONTAINS(t, "\"nullc\":\"\"");
    CHECK_CONTAINS(t, "\"null\":null");
    CHECK_CONTAINS(t, "\"opt_some\":7");
    CHECK_CONTAINS(t, "\"opt_none\":null");
    CHECK_CONTAINS(t, "\"obj\":{\"nested\":[1,2,3]}");
    CHECK_CONTAINS(t, "\"t\":true");
    CHECK_CONTAINS(t, "\"msg\":\"free text\"");

    const std::string& e = lines[2];
    CHECK_CONTAINS(e, "\"event\":\"ev\\\"ent\\\\with\\nnewline\"");
    CHECK_CONTAINS(e, "\"k\\\"ey\":\"v\\\\al\\tue\"");
    CHECK_CONTAINS(e, "\"ctrl\":\"a\\u0001b\\u001fc\"");
    CHECK_CONTAINS(e, "\"utf8\":\"h\xC3\xA9llo \xE2\x9C\x93\"");
    return test::result();
}
