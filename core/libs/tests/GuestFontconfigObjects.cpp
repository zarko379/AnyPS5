#include "prx/libfontconfig/libfontconfig.hpp"
#include "prx/libfontconfig/include/Sets.hpp"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iterator>
#include <limits>

static void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "Fontconfig object check failed at line %d\n", line);
    std::abort();
}

#define CHECK(value) Require((value), __LINE__)

static const FcChar8* Text(const char* text) {
    return reinterpret_cast<const FcChar8*>(text);
}

static void Languages() {
    FcLangSetDestroy(nullptr);
    auto* first = FcLangSetCreate();
    auto* second = FcLangSetCreate();
    CHECK(first && second);
    CHECK(FcLangSetHasLang(first, Text("en")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("EN-us")) == 1);
    CHECK(FcLangSetAdd(first, Text("en-US")) == 1);
    CHECK(FcLangSetHasLang(first, Text("en-US")) == FcLangEqual);
    CHECK(FcLangSetHasLang(first, Text("en")) == FcLangDifferentTerritory);
    CHECK(FcLangSetHasLang(first, Text("en-GB")) == FcLangDifferentTerritory);
    CHECK(FcLangSetHasLang(first, Text("eng")) == FcLangDifferentLang);
    CHECK(FcLangSetHasLang(second, Text("en-US")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("EN")) == 1);
    CHECK(FcLangSetHasLang(first, Text("en")) == FcLangEqual);
    CHECK(FcLangSetHasLang(first, Text("en_US")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("zh-cn")) == 1);
    CHECK(FcLangSetHasLang(first, Text("zh-tw")) == FcLangDifferentTerritory);
    CHECK(FcLangSetAdd(first, Text("x-private-long-tag")) == 1);
    CHECK(FcLangSetHasLang(first, Text("X-PRIVATE-LONG-TAG")) == FcLangEqual);
    CHECK(FcLangSetHasLang(first, Text("x-another")) == FcLangDifferentTerritory);
    CHECK(FcLangSetAdd(first, Text("und")) == 1);
    CHECK(FcLangSetHasLang(first, Text("und")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("und-Latn")) == 1);
    CHECK(FcLangSetHasLang(first, Text("und-latn")) == FcLangEqual);
    CHECK(FcLangSetAdd(first, Text("")) == 1);
    CHECK(FcLangSetHasLang(first, Text("")) == FcLangEqual);
    FcLangSetDestroy(first);
    CHECK(FcLangSetAdd(second, Text("fr")) == 1);
    CHECK(FcLangSetHasLang(second, Text("FR-ca")) == FcLangDifferentTerritory);
    FcLangSetDestroy(second);
}

static void Patterns() {
    FcPatternDestroy(nullptr);
    CHECK(FcPatternDuplicate(nullptr) == nullptr);
    auto* pattern = FcPatternCreate();
    auto* empty = FcPatternCreate();
    CHECK(pattern && empty);
    CHECK(FcPatternEqual(pattern, pattern) == 1);
    CHECK(FcPatternEqual(pattern, empty) == 0);
    CHECK(FcPatternEqual(nullptr, nullptr) == 1);
    FcValue value{};
    value.type = FcTypeRange;
    value.u.i = 123;
    CHECK(FcPatternGet(nullptr, "family", 0, &value) == FcResultNoMatch);
    CHECK(FcPatternGet(pattern, "missing", 0, &value) == FcResultNoMatch);
    CHECK(value.type == FcTypeRange && value.u.i == 123);
    char object[] = "family";
    FcChar8 original[] = "Example";
    CHECK(FcPatternAddString(pattern, object, original));
    object[0] = 'X';
    original[0] = 'Y';
    FcChar8* string = nullptr;
    CHECK(FcPatternGetString(pattern, "family", 0, &string) == FcResultMatch);
    CHECK(std::strcmp(reinterpret_cast<const char*>(string), "Example") == 0);
    const auto* savedString = string;
    CHECK(FcPatternAddString(pattern, "family", Text("Second")));
    CHECK(std::strcmp(reinterpret_cast<const char*>(savedString), "Example") == 0);
    CHECK(FcPatternGetString(pattern, "family", 1, &string) == FcResultMatch);
    CHECK(std::strcmp(reinterpret_cast<const char*>(string), "Second") == 0);
    CHECK(FcPatternGet(pattern, "family", -1, &value) == FcResultNoId);
    CHECK(FcPatternGet(pattern, "family", INT_MAX, &value) == FcResultNoId);
    CHECK(value.type == FcTypeRange && value.u.i == 123);
    int integer = 0x12345678;
    CHECK(FcPatternGetInteger(pattern, "family", 0, &integer) == FcResultTypeMismatch);
    CHECK(integer == 0x12345678);
    CHECK(FcPatternAddInteger(pattern, "family", 4) == 0);
    CHECK(FcPatternAddString(pattern, "family", nullptr) == 0);
    CHECK(FcPatternAddMatrix(pattern, "matrix", nullptr) == 0);
    CHECK(FcPatternAddInteger(pattern, "slant", INT_MIN));
    CHECK(FcPatternAddDouble(pattern, "slant", -3.75));
    CHECK(FcPatternGetInteger(pattern, "slant", 0, &integer) == FcResultMatch && integer == INT_MIN);
    CHECK(FcPatternGetInteger(pattern, "slant", 1, &integer) == FcResultTypeMismatch);
    CHECK(integer == INT_MIN);
    CHECK(FcPatternAddDouble(pattern, "size", 2147483647.75));
    CHECK(FcPatternGetInteger(pattern, "size", 0, &integer) == FcResultTypeMismatch);
    CHECK(integer == INT_MIN);
    CHECK(FcPatternAddBool(pattern, "antialias", 7));
    CHECK(FcPatternGetBool(pattern, "antialias", 0, &integer) == FcResultMatch && integer == 7);
    CHECK(FcPatternAddInteger(pattern, "antialias", 1) == 0);
    CHECK(FcPatternAddString(pattern, "lang", Text("en")));
    CHECK(FcPatternAddBool(pattern, "lang", 1) == 0);
    CHECK(FcPatternAddInteger(pattern, "custom", 42));
    CHECK(FcPatternAddString(pattern, "custom", Text("mixed")));
    CHECK(FcPatternGet(pattern, "custom", 0, &value) == FcResultMatch && value.type == FcTypeInteger && value.u.i == 42);
    CHECK(FcPatternGet(pattern, "custom", 1, &value) == FcResultMatch && value.type == FcTypeString);
    CHECK(FcPatternAddString(pattern, "Family", Text("case-sensitive object name")));
    CHECK(FcPatternAddInteger(pattern, "Family", 5));
    FcMatrix input{1, 2, 3, 4};
    CHECK(FcPatternAddMatrix(pattern, "matrix", &input));
    input.xx = 99;
    FcMatrix* matrix = nullptr;
    CHECK(FcPatternGetMatrix(pattern, "matrix", 0, &matrix) == FcResultMatch);
    CHECK(matrix->xx == 1 && matrix->xy == 2 && matrix->yx == 3 && matrix->yy == 4);
    CHECK(FcPatternAddMatrix(pattern, "matrix", matrix));
    CHECK(matrix->xx == 1);
    auto* copy = FcPatternDuplicate(pattern);
    CHECK(copy && copy != pattern && FcPatternEqual(pattern, copy));
    FcChar8* copyString = nullptr;
    CHECK(FcPatternGetString(copy, "family", 0, &copyString) == FcResultMatch);
    CHECK(copyString != savedString);
    CHECK(FcPatternGetMatrix(copy, "matrix", 0, &matrix) == FcResultMatch);
    matrix->xx = 9;
    CHECK(!FcPatternEqual(pattern, copy));
    matrix->xx = 1;
    CHECK(FcPatternEqual(pattern, copy));
    CHECK(!FcPatternRemove(pattern, "family", -1));
    CHECK(!FcPatternRemove(pattern, "family", 2));
    CHECK(FcPatternRemove(pattern, "family", 0));
    CHECK(FcPatternGetString(pattern, "family", 0, &string) == FcResultMatch);
    CHECK(std::strcmp(reinterpret_cast<const char*>(string), "Second") == 0);
    CHECK(FcPatternRemove(pattern, "family", 0));
    CHECK(FcPatternGet(pattern, "family", 0, &value) == FcResultNoMatch);
    CHECK(!FcPatternDel(pattern, "family"));
    CHECK(FcPatternDel(pattern, "custom"));
    CHECK(!FcPatternDel(pattern, "custom"));
    FcPatternReference(copy);
    FcPatternDestroy(copy);
    CHECK(FcPatternGetInteger(copy, "slant", 0, &integer) == FcResultMatch && integer == INT_MIN);
    CHECK(FcPatternGet(copy, "absent", 0, nullptr) == FcResultNoMatch);
    CHECK(FcPatternGet(copy, "slant", 2, nullptr) == FcResultNoId);
    CHECK(FcPatternGetString(copy, "slant", 0, nullptr) == FcResultTypeMismatch);
    FcPatternDestroy(copy);
    FcPatternDestroy(pattern);
    FcPatternDestroy(empty);
}

static void IntegerGetters() {
    auto* pattern = FcPatternCreate();
    CHECK(pattern);
    const int integers[] = {INT_MIN, -1, 0, 1, INT_MAX};
    for (int i = 0; i < static_cast<int>(std::size(integers)); ++i) {
        CHECK(FcPatternAddInteger(pattern, "integer", integers[i]));
        int output = 0x12345678;
        CHECK(FcPatternGetInteger(pattern, "integer", i, &output) == FcResultMatch);
        CHECK(output == integers[i]);
    }
    const double doubles[] = {
        0.0, -0.0, 42.0, -3.75, 2147483647.75, -2147483649.0,
        std::numeric_limits<double>::max(), std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()
    };
    for (int i = 0; i < static_cast<int>(std::size(doubles)); ++i) {
        CHECK(FcPatternAddDouble(pattern, "double", doubles[i]));
        FcValue value{};
        CHECK(FcPatternGet(pattern, "double", i, &value) == FcResultMatch);
        CHECK(value.type == FcTypeDouble);
        int output = 0x12345678;
        CHECK(FcPatternGetInteger(pattern, "double", i, &output) == FcResultTypeMismatch);
        CHECK(output == 0x12345678);
    }
    int output = 0x12345678;
    CHECK(FcPatternGetInteger(pattern, "missing", 0, &output) == FcResultNoMatch);
    CHECK(output == 0x12345678);
    CHECK(FcPatternGetInteger(pattern, "double", -1, &output) == FcResultNoId);
    CHECK(output == 0x12345678);
    CHECK(FcPatternGetInteger(pattern, "double", static_cast<int>(std::size(doubles)), &output) == FcResultNoId);
    CHECK(output == 0x12345678);
    FcPatternDestroy(pattern);
}

static void Equality() {
    const struct { const char* first; const char* second; bool equal; } pairs[] = {
        {"Example", "eXAMPLE", true},
        {"Stra\xc3\x9f" "e", "STRASSE", true},
        {"\xce\xa3\xcf\x82\xcf\x83", "\xcf\x83\xcf\x83\xcf\x83", true},
        {"\xe2\x84\xaa", "k", true},
        {"\xc4\xb0", "i\xcc\x87", true},
        {"\xef\xac\x83", "ffi", true},
        {"\xc4\xb1", "i", false},
        {"\xe1\xb2\x88", "", true},
        {"\xe1\xb2\x88", "\xea\x99\x8b", false},
        {"\xc1\x81", "a", true},
        {"\xff" "A", "\xff" "a", true},
        {"\xc3", "\xc3", true},
        {"a b", "ab", false}
    };
    for (const auto& pair : pairs) {
        auto* first = FcPatternCreate();
        auto* second = FcPatternCreate();
        CHECK(first && second);
        CHECK(FcPatternAddString(first, "family", Text(pair.first)));
        CHECK(FcPatternAddString(second, "family", Text(pair.second)));
        CHECK((FcPatternEqual(first, second) != 0) == pair.equal);
        CHECK((FcPatternEqual(second, first) != 0) == pair.equal);
        FcPatternDestroy(first);
        FcPatternDestroy(second);
    }
    auto* first = FcPatternCreate();
    auto* second = FcPatternCreate();
    CHECK(first && second);
    CHECK(FcPatternAddInteger(first, "custom", 42));
    CHECK(FcPatternAddDouble(second, "custom", 42));
    CHECK(FcPatternEqual(first, second));
    CHECK(FcPatternAddDouble(first, "nan", std::numeric_limits<double>::quiet_NaN()));
    CHECK(FcPatternAddDouble(second, "nan", std::numeric_limits<double>::quiet_NaN()));
    CHECK(!FcPatternEqual(first, second));
    CHECK(FcPatternEqual(first, first));
    FcPatternDestroy(first);
    FcPatternDestroy(second);
}

static void FontSets() {
    auto* pattern = FcPatternCreate();
    auto* set = FcFontSetCreate();
    CHECK(pattern && set);
    CHECK(FcPatternAddInteger(pattern, "weight", 80));
    CHECK(FcFontSetAdd(set, pattern));
    CHECK(set->nfont == 1);
    CHECK(set->fonts[0] != pattern);
    CHECK(FcPatternEqual(set->fonts[0], pattern));
    CHECK(FcPatternDel(pattern, "weight"));
    CHECK(FcPatternAddInteger(pattern, "weight", 200));
    CHECK(FcFontSetAdd(set, pattern));
    CHECK(set->nfont == 2);
    CHECK(set->fonts[1] != pattern && set->fonts[1] != set->fonts[0]);
    FcPatternDestroy(pattern);
    int weight = 0;
    CHECK(FcPatternGetInteger(set->fonts[0], "weight", 0, &weight) == FcResultMatch);
    CHECK(weight == 80);
    CHECK(FcPatternGetInteger(set->fonts[1], "weight", 0, &weight) == FcResultMatch);
    CHECK(weight == 200);
    FcFontSetDestroy(set);
}

static void CharSets() {
    auto* set = FcCharSetCreate();
    auto* pattern = FcPatternCreate();
    CHECK(set && pattern);
    CHECK(FcCharSetAddChar(set, 'a'));
    CHECK(FcPatternAddCharSet(pattern, "charset", nullptr) == 0);
    CHECK(FcPatternAddCharSet(pattern, "family", set) == 0);
    CHECK(FcPatternAddCharSet(pattern, "lang", set) == 0);
    CHECK(set->references == 1);
    CHECK(FcPatternAddCharSet(pattern, "charset", set));
    CHECK(FcPatternAddCharSet(pattern, "custom", set));
    CHECK(set->references == 3);
    FcCharSet* found = nullptr;
    CHECK(FcPatternGetCharSet(pattern, "charset", 0, &found) == FcResultMatch && found == set);
    found = nullptr;
    CHECK(FcPatternGetCharSet(pattern, "custom", 0, &found) == FcResultMatch && found == set);
    found = nullptr;
    CHECK(FcPatternGetCharSet(pattern, "charset", 1, &found) == FcResultNoId && !found);
    CHECK(FcPatternGetCharSet(pattern, "missing", 0, &found) == FcResultNoMatch && !found);
    CHECK(FcPatternGetCharSet(nullptr, "charset", 0, &found) == FcResultNoMatch && !found);
    FcValue value{};
    CHECK(FcPatternGet(pattern, "charset", 0, &value) == FcResultMatch);
    CHECK(value.type == FcTypeCharSet && value.u.c == set);
    FcChar8* string = nullptr;
    CHECK(FcPatternGetString(pattern, "charset", 0, &string) == FcResultTypeMismatch && !string);
    CHECK(FcPatternAddString(pattern, "family", Text("Example")));
    CHECK(FcPatternGetCharSet(pattern, "family", 0, &found) == FcResultTypeMismatch && !found);
    FcCharSetDestroy(set);
    CHECK(set->references == 2);
    CHECK(FcPatternGetCharSet(pattern, "charset", 0, &found) == FcResultMatch && found == set);
    CHECK(FcCharSetAddChar(found, 'b'));
    auto* copy = FcPatternDuplicate(pattern);
    CHECK(copy && set->references == 4);
    FcCharSet* copied = nullptr;
    CHECK(FcPatternGetCharSet(copy, "charset", 0, &copied) == FcResultMatch && copied == set);
    CHECK(FcPatternEqual(pattern, copy));
    auto* other = FcPatternCreate();
    auto* same = FcCharSetCreate();
    CHECK(other && same);
    CHECK(FcCharSetAddChar(same, 'b') && FcCharSetAddChar(same, 'a'));
    CHECK(FcPatternAddString(other, "family", Text("EXAMPLE")));
    CHECK(FcPatternAddCharSet(other, "charset", same));
    CHECK(FcPatternAddCharSet(other, "custom", same));
    CHECK(FcPatternEqual(pattern, other) && FcPatternEqual(other, pattern));
    CHECK(FcCharSetAddChar(same, 0x10000));
    CHECK(!FcPatternEqual(pattern, other) && !FcPatternEqual(other, pattern));
    CHECK(FcCharSetAddChar(set, 0x10000));
    CHECK(FcPatternEqual(pattern, other));
    CHECK(FcCharSetAddChar(set, 0x10001));
    CHECK(!FcPatternEqual(pattern, other));
    CHECK(FcPatternDel(copy, "custom"));
    CHECK(set->references == 3);
    FcPatternDestroy(copy);
    CHECK(set->references == 2);
    CHECK(FcPatternRemove(pattern, "charset", 0));
    CHECK(set->references == 1);
    FcPatternDestroy(pattern);
    CHECK(same->references == 3);
    FcPatternDestroy(other);
    CHECK(same->references == 1);
    FcCharSetDestroy(same);
}

static void LangSets() {
    auto* set = FcLangSetCreate();
    auto* pattern = FcPatternCreate();
    CHECK(set && pattern);
    CHECK(FcLangSetAdd(set, Text("en")));
    CHECK(FcPatternAddLangSet(pattern, "lang", nullptr) == 0);
    CHECK(FcPatternAddLangSet(pattern, "family", set) == 0);
    CHECK(FcPatternAddLangSet(pattern, "charset", set) == 0);
    CHECK(FcPatternAddLangSet(pattern, "lang", set));
    CHECK(FcPatternAddString(pattern, "lang", Text("fr")));
    CHECK(FcPatternAddLangSet(pattern, "custom", set));
    FcLangSet* found = nullptr;
    CHECK(FcPatternGetLangSet(pattern, "lang", 0, &found) == FcResultMatch);
    CHECK(found && found != set);
    CHECK(FcLangSetHasLang(found, Text("en")) == FcLangEqual);
    FcLangSet* again = nullptr;
    CHECK(FcPatternGetLangSet(pattern, "lang", 0, &again) == FcResultMatch && again == found);
    FcLangSet* custom = nullptr;
    CHECK(FcPatternGetLangSet(pattern, "custom", 0, &custom) == FcResultMatch && custom != found && custom != set);
    FcLangSet* untouched = nullptr;
    CHECK(FcPatternGetLangSet(pattern, "lang", 1, &untouched) == FcResultTypeMismatch && !untouched);
    CHECK(FcPatternGetLangSet(pattern, "lang", 2, &untouched) == FcResultNoId && !untouched);
    CHECK(FcPatternGetLangSet(pattern, "missing", 0, &untouched) == FcResultNoMatch && !untouched);
    FcValue value{};
    CHECK(FcPatternGet(pattern, "lang", 0, &value) == FcResultMatch && value.type == FcTypeLangSet && value.u.l == found);
    CHECK(FcLangSetAdd(set, Text("de")));
    CHECK(FcLangSetHasLang(found, Text("de")) == FcLangDifferentLang);
    FcLangSetDestroy(set);
    CHECK(FcLangSetHasLang(found, Text("en")) == FcLangEqual);
    auto* copy = FcPatternDuplicate(pattern);
    CHECK(copy && FcPatternEqual(pattern, copy));
    FcLangSet* copied = nullptr;
    CHECK(FcPatternGetLangSet(copy, "lang", 0, &copied) == FcResultMatch && copied != found);
    CHECK(FcLangSetHasLang(copied, Text("en")) == FcLangEqual);
    CHECK(FcLangSetAdd(copied, Text("de")));
    CHECK(!FcPatternEqual(pattern, copy));
    CHECK(FcLangSetHasLang(found, Text("de")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(found, Text("de")));
    CHECK(FcPatternEqual(pattern, copy));
    CHECK(FcLangSetAdd(found, Text("x-private")));
    CHECK(!FcPatternEqual(pattern, copy));
    CHECK(FcLangSetAdd(copied, Text("X-PRIVATE")));
    CHECK(!FcPatternEqual(pattern, copy));
    CHECK(FcLangSetAdd(found, Text("X-PRIVATE")));
    CHECK(FcLangSetAdd(copied, Text("x-private")));
    CHECK(FcPatternEqual(pattern, copy));
    FcPatternDestroy(copy);
    CHECK(FcLangSetHasLang(found, Text("x-private")) == FcLangEqual);
    FcPatternDestroy(pattern);
}

static void FreeTypeFaces() {
    auto* pattern = FcPatternCreate();
    CHECK(pattern);
    void* face = &face;
    CHECK(FcPatternGetFTFace(pattern, "ftface", 0, &face) == FcResultNoMatch && face == &face);
    CHECK(FcPatternGetFTFace(nullptr, "ftface", 0, &face) == FcResultNoMatch && face == &face);
    CHECK(FcPatternAddInteger(pattern, "ftface", 1));
    CHECK(FcPatternAddString(pattern, "ftface", Text("face")));
    CHECK(FcPatternGetFTFace(pattern, "ftface", 0, &face) == FcResultTypeMismatch && face == &face);
    CHECK(FcPatternGetFTFace(pattern, "ftface", 1, &face) == FcResultTypeMismatch && face == &face);
    CHECK(FcPatternGetFTFace(pattern, "ftface", 2, &face) == FcResultNoId && face == &face);
    CHECK(FcPatternGetFTFace(pattern, "ftface", -1, &face) == FcResultNoId && face == &face);
    FcPatternDestroy(pattern);
}

static void Filters() {
    auto* set = FcCharSetCreate();
    auto* languages = FcLangSetCreate();
    auto* pattern = FcPatternCreate();
    CHECK(set && languages && pattern);
    CHECK(FcCharSetAddChar(set, 'a') && FcLangSetAdd(languages, Text("en")));
    CHECK(FcPatternAddString(pattern, "family", Text("First")));
    CHECK(FcPatternAddString(pattern, "family", Text("Second")));
    CHECK(FcPatternAddInteger(pattern, "weight", 80));
    CHECK(FcPatternAddCharSet(pattern, "charset", set));
    CHECK(FcPatternAddLangSet(pattern, "lang", languages));
    const char* names[] = {"family", "missing", "charset", "family", "lang"};
    const FcObjectSet objects{5, static_cast<int>(sizeof(names)), names};
    auto* filtered = FcPatternFilter(pattern, &objects);
    CHECK(filtered && filtered != pattern);
    const char* expected[] = {"First", "Second", "First", "Second"};
    for (int i = 0; i < 4; ++i) {
        FcChar8* string = nullptr;
        CHECK(FcPatternGetString(filtered, "family", i, &string) == FcResultMatch);
        CHECK(std::strcmp(reinterpret_cast<const char*>(string), expected[i]) == 0);
    }
    FcValue value{};
    CHECK(FcPatternGet(filtered, "family", 4, &value) == FcResultNoId);
    CHECK(FcPatternGet(filtered, "weight", 0, &value) == FcResultNoMatch);
    CHECK(FcPatternGet(filtered, "missing", 0, &value) == FcResultNoMatch);
    FcCharSet* charSet = nullptr;
    CHECK(FcPatternGetCharSet(filtered, "charset", 0, &charSet) == FcResultMatch && charSet == set);
    CHECK(set->references == 3);
    FcLangSet* original = nullptr;
    FcLangSet* copied = nullptr;
    CHECK(FcPatternGetLangSet(pattern, "lang", 0, &original) == FcResultMatch);
    CHECK(FcPatternGetLangSet(filtered, "lang", 0, &copied) == FcResultMatch);
    CHECK(copied != original && FcLangSetHasLang(copied, Text("en")) == FcLangEqual);
    CHECK(FcPatternDel(filtered, "family"));
    FcChar8* string = nullptr;
    CHECK(FcPatternGetString(pattern, "family", 1, &string) == FcResultMatch);
    CHECK(std::strcmp(reinterpret_cast<const char*>(string), "Second") == 0);
    FcPatternDestroy(filtered);
    CHECK(set->references == 2);

    const char* subset[] = {"weight"};
    const FcObjectSet single{1, static_cast<int>(sizeof(subset)), subset};
    filtered = FcPatternFilter(pattern, &single);
    int weight = 0;
    CHECK(filtered && FcPatternGetInteger(filtered, "weight", 0, &weight) == FcResultMatch && weight == 80);
    CHECK(FcPatternGet(filtered, "family", 0, &value) == FcResultNoMatch);
    FcPatternDestroy(filtered);

    const FcObjectSet none{0, 0, nullptr};
    filtered = FcPatternFilter(pattern, &none);
    CHECK(filtered && filtered != pattern);
    CHECK(FcPatternGet(filtered, "family", 0, &value) == FcResultNoMatch);
    CHECK(!FcPatternEqual(filtered, pattern));
    FcPatternDestroy(filtered);
    filtered = FcPatternFilter(nullptr, &none);
    CHECK(filtered && FcPatternGet(filtered, "family", 0, &value) == FcResultNoMatch);
    FcPatternDestroy(filtered);
    const FcObjectSet negative{-1, 0, nullptr};
    filtered = FcPatternFilter(nullptr, &negative);
    CHECK(filtered);
    FcPatternDestroy(filtered);

    filtered = FcPatternFilter(pattern, nullptr);
    CHECK(filtered && filtered != pattern && FcPatternEqual(filtered, pattern));
    CHECK(set->references == 3);
    FcPatternDestroy(filtered);
    CHECK(FcPatternFilter(nullptr, nullptr) == nullptr);
    const auto throws = [](FcPattern* source, const FcObjectSet* set) {
        try {
            FcPatternFilter(source, set);
        } catch (const std::exception&) {
            return true;
        }
        return false;
    };
    CHECK(throws(nullptr, &single));
    const char* invalid[] = {"weight", nullptr};
    const FcObjectSet withNull{2, static_cast<int>(sizeof(invalid)), invalid};
    CHECK(throws(pattern, &withNull));

    FcPatternDestroy(pattern);
    CHECK(set->references == 1);
    FcCharSetDestroy(set);
    FcLangSetDestroy(languages);
}

int main() {
    Languages();
    Patterns();
    IntegerGetters();
    Equality();
    FontSets();
    CharSets();
    LangSets();
    FreeTypeFaces();
    Filters();
}
