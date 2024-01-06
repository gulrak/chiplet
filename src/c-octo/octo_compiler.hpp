/**
 *
 *  octo_compiler.hpp
 *
 *  a compiler for Octo CHIP-8 assembly language,
 *  suitable for embedding in other tools and environments.
 *  depends only upon the C standard library.
 *
 *  the public interface is octo_compile_str(char*);
 *  the result will contain a 64k ROM image in the
 *  'rom' field of the returned octo_program.
 *  octo_free_program can clean up the entire structure
 *  when a consumer is finished using it.
 *
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2020, John Earnest
 *
 *  C++ conversion:
 *  Copyright (c) 2023, Steffen Schümann
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 **/

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <deque>
#include <unordered_map>
#include <string_view>
#include <vector>

/**
 *
 *  Fundamental Data Structures
 *
 **/

#define OCTO_LIST_BLOCK_SIZE 16
#define OCTO_INTERN_MAX (64 * 1024)
#define OCTO_ERR_MAX 4096
#define OCTO_DESTRUCTOR(x) ((void (*)(void*))x)

typedef struct
{
    int count, space;
    void** data;
} octo_list;
void octo_list_init(octo_list* list)
{
    list->count = 0;
    list->space = OCTO_LIST_BLOCK_SIZE;
    list->data = (void**)malloc(sizeof(void*) * OCTO_LIST_BLOCK_SIZE);
}
void octo_list_destroy(octo_list* list, void items(void*))
{
    if (items)
        for (int z = 0; z < list->count; z++)
            items(list->data[z]);
    free(list->data);
}
void octo_list_grow(octo_list* list)
{
    if (list->count < list->space)
        return;
    list->data = (void**)realloc(list->data, (sizeof(void*)) * (list->space += OCTO_LIST_BLOCK_SIZE));
}
void octo_list_append(octo_list* list, void* item)
{
    octo_list_grow(list);
    list->data[list->count++] = item;
}
void octo_list_insert(octo_list* list, void* item, int index)
{
    octo_list_grow(list);
    for (int z = list->count; z > index; z--)
        list->data[z] = list->data[z - 1];
    list->data[index] = item;
    list->count++;
}
void* octo_list_remove(octo_list* list, int index)
{
    void* ret = list->data[index];
    for (int z = index; z < list->count - 1; z++)
        list->data[z] = list->data[z + 1];
    list->count--, list->data[list->count] = NULL;  // paranoia
    return ret;
}
void* octo_list_get(octo_list* list, int index)
{
    return list->data[index];
}
void octo_list_set(octo_list* list, int index, void* value)
{
    list->data[index] = value;
}

// I could just use lists directly, but this abstraction clarifies intent:
typedef struct
{
    octo_list values;
} octo_stack;
void octo_stack_init(octo_stack* stack)
{
    octo_list_init(&stack->values);
}
void octo_stack_destroy(octo_stack* stack, void items(void*))
{
    octo_list_destroy(&stack->values, items);
}
void octo_stack_push(octo_stack* stack, void* item)
{
    octo_list_append(&stack->values, item);
}
void* octo_stack_pop(octo_stack* stack)
{
    return octo_list_remove(&stack->values, stack->values.count - 1);
}
int octo_stack_is_empty(octo_stack* stack)
{
    return stack->values.count < 1;
}

// linear access should be good enough for a start;
// octo programs rarely have more than a few thousand constants:
typedef struct
{
    octo_list keys, values;
} octo_map;
void octo_map_init(octo_map* map)
{
    octo_list_init(&map->keys);
    octo_list_init(&map->values);
}
void octo_map_destroy(octo_map* map, void items(void*))
{
    octo_list_destroy(&map->keys, NULL);
    octo_list_destroy(&map->values, items);
}
void* octo_map_get(octo_map* map, char* key)
{
    for (int z = 0; z < map->keys.count; z++) {
        if (octo_list_get(&map->keys, z) == key)
            return octo_list_get(&map->values, z);
    }
    return NULL;
}
void* octo_map_remove(octo_map* map, char* key)
{
    for (int z = 0; z < map->keys.count; z++) {
        if (octo_list_get(&map->keys, z) == key) {
            void* prev = octo_list_get(&map->values, z);
            octo_list_remove(&map->keys, z);
            octo_list_remove(&map->values, z);
            return prev;
        }
    }
    return NULL;
}
void* octo_map_set(octo_map* map, char* key, void* value)
{
    for (int z = 0; z < map->keys.count; z++) {
        if (octo_list_get(&map->keys, z) == key) {
            void* prev = octo_list_get(&map->values, z);
            octo_list_set(&map->values, z, value);
            return prev;
        }
    }
    octo_list_append(&map->keys, key);
    octo_list_append(&map->values, value);
    return NULL;
}

/**
 *
 *  Compiler State
 *
 **/

#define OCTO_TOK_STR 0
#define OCTO_TOK_NUM 1
#define OCTO_TOK_EOF 2
class OctoToken
{
public:
    OctoToken() = delete;
    OctoToken(int line, int pos);
    OctoToken(int n);
    OctoToken(const OctoToken& other);
    char* format_value(char* d);

    int type;
    int line;
    int pos;
    union
    {
        char* str_value;
        double num_value;
    };
};

OctoToken::OctoToken(int line, int pos)
: type{OCTO_TOK_EOF}
, line{line}
, pos{pos}
{
    str_value = "";
}

OctoToken::OctoToken(int n)
: type{OCTO_TOK_NUM}
, line{0}
, pos{0}
{
    num_value = n;
}

OctoToken::OctoToken(const OctoToken& other)
: type{other.type}
, line{other.line}
, pos{other.pos}
{
    if(type == OCTO_TOK_NUM) {
        num_value = other.num_value;
    }
    else {
        str_value = other.str_value;
    }
}

char* OctoToken::format_value(char* d)
{
    switch(type) {
        case OCTO_TOK_EOF:
            snprintf(d, 255, "<end of file>");
            break;
        case OCTO_TOK_STR:
            snprintf(d, 255, "'%s'", str_value);
            break;
        case OCTO_TOK_NUM:
            snprintf(d, 255, "%d", (int)num_value);
            break;
        default:
            snprintf(d, 255, "unknown type");
            break;
    }
    return d;
}

typedef struct
{
    double value;
    char is_mutable;
} octo_const;
typedef struct
{
    int value;
} octo_reg;
typedef struct
{
    int value;
    char size;
} octo_pref;
typedef struct
{
    int line, pos;
    octo_list addrs;
} octo_proto;

struct OctoMacro
{
    int calls{};
    std::vector<std::string_view> args;
    std::vector<OctoToken> body;
};

struct OctoSMode
{
    int calls;
    char values[256];
    std::array<std::unique_ptr<OctoMacro>,256> modes;
};

typedef struct
{
    int addr, line, pos;
    char* type;
} octo_flow;
typedef struct
{
    int type, base, len;
    char* format;
} octo_mon;

octo_const* octo_make_const(double v, char m)
{
    octo_const* r = (octo_const*)calloc(1, sizeof(octo_const));
    r->value = v, r->is_mutable = m;
    return r;
}
octo_reg* octo_make_reg(int v)
{
    octo_reg* r = (octo_reg*)calloc(1, sizeof(octo_reg));
    r->value = v;
    return r;
}
octo_pref* octo_make_pref(int a, char l)
{
    octo_pref* r = (octo_pref*)calloc(1, sizeof(octo_pref));
    r->value = a;
    r->size = l;
    return r;
}
octo_proto* octo_make_proto(int l, int p)
{
    octo_proto* r = (octo_proto*)calloc(1, sizeof(octo_proto));
    octo_list_init(&r->addrs);
    r->line = l, r->pos = p;
    return r;
}
octo_flow* octo_make_flow(int a, int l, int p, char* t)
{
    octo_flow* r = (octo_flow*)calloc(1, sizeof(octo_flow));
    r->addr = a, r->line = l, r->pos = p, r->type = t;
    return r;
}
octo_mon* octo_make_mon()
{
    octo_mon* r = (octo_mon*)calloc(1, sizeof(octo_mon));
    return r;
}
void octo_free_const(octo_const* x)
{
    free(x);
}
void octo_free_reg(octo_reg* x)
{
    free(x);
}
void octo_free_pref(octo_pref* x)
{
    free(x);
}
void octo_free_proto(octo_proto* x)
{
    octo_list_destroy(&x->addrs, OCTO_DESTRUCTOR(octo_free_pref));
    free(x);
}
void octo_free_flow(octo_flow* x)
{
    free(x);
}
void octo_free_mon(octo_mon* x)
{
    free(x);
}

class OctoProgram
{
public:
    static constexpr int RAM_MAX  = 16 * 1024 * 1024;
    static constexpr int RAM_MASK = 16 * 1024 * 1024 - 1;

    OctoProgram() = delete;
    OctoProgram(char* text, int startAddress);
    ~OctoProgram();
    bool compile();
    bool isError() const { return is_error; }
    int errorLine() const { return error_line; }
    int errorPos() const { return error_pos; }
    [[nodiscard]] std::string errorMessage() const { return error; }
    int romLength() const { return length; }
    int romStartAddress() const { return startAddress; }
    const char* data() const { return rom.data(); }
    int numSourceLines() const { return source_line; }
    const char* breakpointInfo(uint32_t addr) const
    {
        if (is_error || addr > rom.size())
            return nullptr;
        auto iter = breakpoints.find(addr);
        return iter == breakpoints.end() ? nullptr : iter->second;
    }
    int lineForAddress(uint32_t addr) const { return !is_error && addr < romLineMap.size() ? romLineMap[addr] : 0xFFFFFFFF; }

private:
    double sign(double x)
    {
        return (0.0 < x) - (x < 0.0);
    }

    double max(double x, double y)
    {
        return x < y ? y : x;
    }
    double min(double x, double y)
    {
        return x < y ? x : y;
    }
    char* counted(char* name, int length);
    char* counted(char* name);
    int is_end() const;
    char next_char();
    char peek_char() const;
    void skip_whitespace();
    void fetch_token();
    OctoToken next();
    OctoToken peek();
    int peek_match(char* name, int index);
    int match(char* name);
    int check_name(char* name, char* kind);
    char* string();
    char* identifier(char* kind);
    void expect(char* name);
    int is_register(char* name);
    int peek_is_register();
    int register_or_alias();
    int value_range(int n, int mask);
    void value_fail(char* w, char* n, int undef);
    int value_4bit();
    int value_8bit();
    int value_12bit();
    int value_16bit(int can_forward_ref, int offset);
    int value_24bit(int can_forward_ref, int offset);
    octo_const* value_constant();
    void macro_body(char* desc, char* name, OctoMacro& m);
    double calc_expr(char* name);
    double calc_terminal(char* name);
    double calculated(char* name);
    void append(char byte);
    void instruction(char a, char b);
    void immediate(char op, int nnn);
    void jump(int addr, int dest);
    void pseudo_conditional(int reg, int sub, int comp);
    void conditional(int negated);
    void resolve_label(int offset);
    void compile_statement();

    static int is_reserved(char* name);

    // string interning table
    size_t strings_used;
    char strings[OCTO_INTERN_MAX]{};

    // tokenizer
    char* source;
    char* source_root;
    int source_line;
    int source_pos;
    std::deque<OctoToken> tokens;

    // compiler
    char has_main{};  // do we need a trampoline for 'main'?
    int startAddress{};
    int here{};
    int length{};
    std::vector<char> rom{};
    std::vector<char> used{};
    std::vector<uint32_t> romLineMap{};
    octo_map constants{};    // name -> octo_const
    octo_map aliases{};      // name -> octo_reg
    octo_map protos{};       // name -> octo_proto
    std::unordered_map<std::string_view,OctoMacro> macros; // name -> octo_macro
    std::unordered_map<std::string_view,OctoSMode> stringmodes; // name -> octo_smode
    octo_stack loops{};      // [octo_flow]
    octo_stack branches{};   // [octo_flow]
    octo_stack whiles{};     // [octo_flow], value=-1 indicates a marker

    // debugging
    std::unordered_map<uint32_t,char*> breakpoints{};
    octo_map monitors{};  // name -> octo_mon

    // error reporting
    char is_error{};
    char error[OCTO_ERR_MAX]{};
    int error_line{};
    int error_pos{};
};

OctoProgram::~OctoProgram()
{
    free(source_root);
    octo_map_destroy(&constants, OCTO_DESTRUCTOR(octo_free_const));
    octo_map_destroy(&aliases, OCTO_DESTRUCTOR(octo_free_reg));
    octo_map_destroy(&protos, OCTO_DESTRUCTOR(octo_free_proto));
    octo_stack_destroy(&loops, OCTO_DESTRUCTOR(octo_free_flow));
    octo_stack_destroy(&branches, OCTO_DESTRUCTOR(octo_free_flow));
    octo_stack_destroy(&whiles, OCTO_DESTRUCTOR(octo_free_flow));
    octo_map_destroy(&monitors, OCTO_DESTRUCTOR(octo_free_mon));
}

/**
 *
 *  Tokenization
 *
 **/

int octo_interned_len(char* name)
{
    return (name[-2] << 8) | name[-1];
}
char* OctoProgram::counted(char* name, int length)
{
    size_t index = 2;
    while (index < strings_used) {
        if (memcmp(name, strings + index, length + 1) == 0)
            return strings + index;
        index += octo_interned_len(strings + index) + 3;  // [ \0 , len-lo , len-hi ]
    }
    if (strings_used + length + 1 >= OCTO_INTERN_MAX) {
        return is_error = 1, snprintf(error, OCTO_ERR_MAX, "Internal Error: exhausted string interning table."), (char*)"";
    }
    memcpy(strings + index, name, length);
    strings[index - 2] = 0xFF & (length >> 8);
    strings[index - 1] = 0xFF & length;
    strings_used += length + 3;
    return strings + index;
}

char* OctoProgram::counted(char* name)
{
    return counted(name, strlen(name));
}

int OctoProgram::is_end() const
{
    return tokens.empty() && source[0] == '\0';
}

char OctoProgram::next_char()
{
    char c = source[0];
    if (c == '\0')
        return '\0';
    if (c == '\n')
        source_line++, source_pos = 0;
    else
        source_pos++;
    source++;
    return c;
}

char OctoProgram::peek_char() const
{
    return source[0] == '\0' ? '\0' : source[0];
}

void OctoProgram::skip_whitespace()
{
    while (1) {
        char c = peek_char();
        if (c == '#') {  // line comments
            next_char();
            while (true) {
                char cc = peek_char();
                if (cc == '\0' || cc == '\n')
                    break;
                next_char();
            }
        }
        else if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            break;
        next_char();
    }
}

void OctoProgram::fetch_token()
{
    if (is_end()) {
        is_error = 1;
        snprintf(error, OCTO_ERR_MAX, "Unexpected EOF.");
        return;
    }
    if (is_error)
        return;
    tokens.emplace_back(source_line, source_pos);
    auto& t = tokens.back();
    char str_buffer[4096];
    int index = 0;
    if (source[0] == '"') {
        next_char();
        while (1) {
            char c = next_char();
            if (c == '\0') {
                is_error = 1;
                snprintf(error, OCTO_ERR_MAX, "Missing a closing \" in a string literal.");
                error_line = source_line, error_pos = source_pos;
                return;
            }
            if (c == '"') {
                next_char();
                break;
            }
            if (c == '\\') {
                char ec = next_char();
                if (ec == '\0') {
                    is_error = 1;
                    snprintf(error, OCTO_ERR_MAX, "Missing a closing \" in a string literal.");
                    error_line = source_line, error_pos = source_pos;
                    return;
                }
                if (ec == 't')
                    str_buffer[index++] = '\t';
                else if (ec == 'n')
                    str_buffer[index++] = '\n';
                else if (ec == 'r')
                    str_buffer[index++] = '\r';
                else if (ec == 'v')
                    str_buffer[index++] = '\v';
                else if (ec == '0')
                    str_buffer[index++] = '\0';
                else if (ec == '\\')
                    str_buffer[index++] = '\\';
                else if (ec == '"')
                    str_buffer[index++] = '"';
                else {
                    is_error = 1;
                    snprintf(error, OCTO_ERR_MAX, "Unrecognized escape character '%c' in a string literal.", ec);
                    error_line = source_line, error_pos = source_pos - 1;
                    return;
                }
            }
            else {
                str_buffer[index++] = c;
            }
            if (index >= 4095) {
                is_error = 1;
                snprintf(error, OCTO_ERR_MAX, "String literals must be < 4096 characters long.");
                error_line = source_line, error_pos = source_pos;
            }
        }
        str_buffer[index++] = '\0';
        t.type = OCTO_TOK_STR, t.str_value = counted(str_buffer, index - 1);
    }
    else {
        // string or number
        while (1) {
            char c = next_char();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '#' || c == '\0')
                break;
            str_buffer[index++] = c;
        }
        str_buffer[index++] = '\0';
        char* float_end;
        double float_val = strtod(str_buffer, &float_end);
        if (float_end[0] == '\0') {
            t.type = OCTO_TOK_NUM, t.num_value = float_val;
        }
        else if (str_buffer[0] == '0' && str_buffer[1] == 'b') {
            t.type = OCTO_TOK_NUM, t.num_value = std::strtol(str_buffer + 2, nullptr, 2);
        }
        else if (str_buffer[0] == '0' && str_buffer[1] == 'x') {
            t.type = OCTO_TOK_NUM, t.num_value = std::strtol(str_buffer + 2, nullptr, 16);
        }
        else if (str_buffer[0] == '-' && str_buffer[1] == '0' && str_buffer[2] == 'b') {
            t.type = OCTO_TOK_NUM, t.num_value = -std::strtol(str_buffer + 3, nullptr, 2);
        }
        else if (str_buffer[0] == '-' && str_buffer[1] == '0' && str_buffer[2] == 'x') {
            t.type = OCTO_TOK_NUM, t.num_value = -std::strtol(str_buffer + 3, nullptr, 16);
        }
        else {
            t.type = OCTO_TOK_STR, t.str_value = counted(str_buffer);
        }
        // this is handy for debugging internal errors:
        // if (t->type==OCTO_TOK_STR) printf("RAW TOKEN: %p %s\n", (void*)t->str_value, t->str_value);
        // if (t->type==OCTO_TOK_NUM) printf("RAW TOKEN: %f\n", t->num_value);
    }
    skip_whitespace();
}

OctoToken OctoProgram::next()
{
    if (tokens.empty())
        fetch_token();
    if (is_error)
        return {source_line, source_pos};
    auto t = tokens.front();
    tokens.pop_front();
    error_line = t.line, error_pos = t.pos;
    return t;
}

OctoToken OctoProgram::peek()
{
    if (tokens.empty())
        fetch_token();
    if (is_error)
        return {source_line, source_pos};
    return tokens.front();
}

int OctoProgram::peek_match(char* name, int index)
{
    while (!is_error && !is_end() && tokens.size() < index + 1)
        fetch_token();
    if (is_end() || is_error)
        return 0;
    return tokens[index].type == OCTO_TOK_STR && strcmp(tokens[index].str_value, name) == 0;
}

int OctoProgram::match(char* name)
{
    if (peek_match(name, 0)) {
        tokens.pop_front();
        return 1;
    }
    return 0;
}

/**
 *
 *  Parsing
 *
 **/

char* octo_reserved_words[] = {
    ":=",          "|=",     "&=",     "^=",        "-=",        "=-",    "+=",      ">>=",         "<<=",    "==",     "!=",     "<",     ">",           "<=",      ">=",          "key",       "-key",
    "hex",         "bighex", "random", "delay",     ":",         ":next", ":unpack", ":breakpoint", ":proto", ":alias", ":const", ":org",  ";",           "return",  "clear",       "bcd",       "save",
    "load",        "buzzer", "if",     "then",      "begin",     "else",  "end",     "jump",        "jump0",  "native", "sprite", "loop",  "while",       "again",   "scroll-down", "scroll-up", "scroll-right",
    "scroll-left", "lores",  "hires",  "loadflags", "saveflags", "i",     "audio",   "plane",       ":macro", ":calc",  ":byte",  ":call", ":stringmode", ":assert", ":monitor",    ":pointer",
    ":pointer16", ":pointer24", "pitch",
};
int OctoProgram::is_reserved(char* name)
{
    for (size_t z = 0; z < sizeof(octo_reserved_words) / sizeof(char*); z++)
        if (strcmp(name, octo_reserved_words[z]) == 0)
            return 1;
    return 0;
}

int OctoProgram::check_name(char* name, char* kind)
{
    if (is_error)
        return 0;
    if (strncmp("OCTO_", name, 5) == 0 || is_reserved(name)) {
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "The name '%s' is reserved and cannot be used for a %s.", name, kind);
        return 0;
    }
    return 1;
}

char* OctoProgram::string()
{
    if (is_error)
        return "";
    auto t = next();
    if (t.type != OCTO_TOK_STR) {
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Expected a string, got %d.", (int)t.num_value);
        return "";
    }
    char* n = t.str_value;
    return n;
}

char* OctoProgram::identifier(char* kind)
{
    if (is_error)
        return "";
    auto t = next();
    if (t.type != OCTO_TOK_STR) {
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Expected a name for a %s, got %d.", kind, (int)t.num_value);
        return "";
    }
    char* n = t.str_value;
    if (!check_name(n, kind))
        return "";
    return n;
}

void OctoProgram::expect(char* name)
{
    if (is_error)
        return;
    auto t = next();
    if (t.type != OCTO_TOK_STR || strcmp(t.str_value, name) != 0) {
        char d[256];
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Expected %s, got %s.", name, t.format_value(d));
    }
}

int OctoProgram::is_register(char* name)
{
    if (octo_map_get(&aliases, name) != nullptr)
        return 1;
    if (octo_interned_len(name) != 2)
        return 0;
    if (name[0] != 'v' && name[0] != 'V')
        return 0;
    return isxdigit(name[1]);
}

int OctoProgram::peek_is_register()
{
    auto t = peek();
    return t.type == OCTO_TOK_STR && is_register(t.str_value);
}

int OctoProgram::register_or_alias()
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type != OCTO_TOK_STR || !is_register(t.str_value)) {
        char d[256];
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Expected register, got %s.", t.format_value(d));
        return 0;
    }
    auto* a = (octo_reg*)octo_map_get(&aliases, t.str_value);
    if (a != nullptr)
        return a->value;
    char c = tolower(t.str_value[1]);
    return isdigit(c) ? c - '0' : 10 + (c - 'a');
}

int OctoProgram::value_range(int n, int mask)
{
    if (mask == 0xF && (n < 0 || n > mask))
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Argument %d does not fit in 4 bits- must be in range [0,15].", n);
    if (mask == 0xFF && (n < -128 || n > mask))
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Argument %d does not fit in a byte- must be in range [-128,255].", n);
    if (mask == 0xFFF && (n < 0 || n > mask))
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Argument %d does not fit in 12 bits.", n);
    if (mask == 0xFFFF && (n < 0 || n > mask))
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Argument %d does not fit in 16 bits.", n);
    if (mask == 0xFFFFFF && (n < 0 || n > mask))
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Argument %d does not fit in 24 bits.", n);
    return n & mask;
}

void OctoProgram::value_fail(char* w, char* n, int undef)
{
    if (is_error)
        return;
    if (is_register(n))
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Expected %s value, but found the register %s.", w, n);
    else if (is_reserved(n))
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Expected %s value, but found the keyword '%s'. Missing a token?", w, n);
    else if (undef)
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Expected %s value, but found the undefined name '%s'.", w, n);
}

int OctoProgram::value_4bit()
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type == OCTO_TOK_NUM) {
        int n = t.num_value;
        return value_range(n, 0xF);
    }
    char* n = t.str_value;
    auto* c = (octo_const*)octo_map_get(&constants, n);
    if (c != nullptr)
        return value_range(c->value, 0xF);
    return value_fail("a 4-bit", n, 1), 0;
}

int OctoProgram::value_8bit()
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type == OCTO_TOK_NUM) {
        int n = t.num_value;
        return value_range(n, 0xFF);
    }
    char* n = t.str_value;
    auto* c = (octo_const*)octo_map_get(&constants, n);
    if (c != nullptr)
        return value_range(c->value, 0xFF);
    return value_fail("an 8-bit", n, 1), 0;
}

int OctoProgram::value_12bit()
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type == OCTO_TOK_NUM) {
        int n = t.num_value;
        return value_range(n, 0xFFF);
    }
    char* n = t.str_value;
    int proto_line = t.line, proto_pos = t.pos;
    auto* c = (octo_const*)octo_map_get(&constants, n);
    if (c != nullptr)
        return value_range(c->value, 0xFFF);
    value_fail("a 12-bit", n, 0);
    if (is_error)
        return 0;
    if (!check_name(n, "label"))
        return 0;
    auto* pr = (octo_proto*)octo_map_get(&protos, n);
    if (pr == nullptr) {
        octo_map_set(&protos, n, pr = octo_make_proto(proto_line, proto_pos));
    }
    octo_list_append(&pr->addrs, octo_make_pref(here, 12));
    return 0;
}

int OctoProgram::value_16bit(int can_forward_ref, int offset)
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type == OCTO_TOK_NUM) {
        int n = t.num_value;
        return value_range(n, 0xFFFF);
    }
    char* n = t.str_value;
    int proto_line = t.line, proto_pos = t.pos;
    auto* c = (octo_const*)octo_map_get(&constants, n);
    if (c != nullptr)
        return value_range(c->value, 0xFFFF);
    value_fail("a 16-bit", n, 0);
    if (is_error)
        return 0;
    if (!check_name(n, "label"))
        return 0;
    if (!can_forward_ref) {
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "The reference to '%s' may not be forward-declared.", n);
        return 0;
    }
    auto* pr = (octo_proto*)octo_map_get(&protos, n);
    if (pr == nullptr) {
        octo_map_set(&protos, n, pr = octo_make_proto(proto_line, proto_pos));
    }
    octo_list_append(&pr->addrs, octo_make_pref(here + offset, 16));
    return 0;
}

int OctoProgram::value_24bit(int can_forward_ref, int offset)
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type == OCTO_TOK_NUM) {
        int n = t.num_value;
        return value_range(n, 0xFFFFFF);
    }
    char* n = t.str_value;
    int proto_line = t.line, proto_pos = t.pos;
    auto* c = (octo_const*)octo_map_get(&constants, n);
    if (c != nullptr)
        return value_range(c->value, 0xFFFFFF);
    value_fail("a 24-bit", n, 0);
    if (is_error)
        return 0;
    if (!check_name(n, "label"))
        return 0;
    if (!can_forward_ref) {
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "The reference to '%s' may not be forward-declared.", n);
        return 0;
    }
    auto* pr = (octo_proto*)octo_map_get(&protos, n);
    if (pr == nullptr) {
        octo_map_set(&protos, n, pr = octo_make_proto(proto_line, proto_pos));
    }
    octo_list_append(&pr->addrs, octo_make_pref(here + offset, 24));
    return 0;
}

octo_const* OctoProgram::value_constant()
{
    auto t = next();
    if (is_error)
        return octo_make_const(0, 0);
    if (t.type == OCTO_TOK_NUM) {
        int n = t.num_value;
        return octo_make_const(n, 0);
    }
    char* n = t.str_value;
    auto* c = (octo_const*)octo_map_get(&constants, n);
    if (c != nullptr)
        return octo_make_const(c->value, 0);
    if (octo_map_get(&protos, n) != nullptr)
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "A constant reference to '%s' may not be forward-declared.", n);
    return value_fail("a constant", n, 1), octo_make_const(0, 0);
}

void OctoProgram::macro_body(char* desc, char* name, OctoMacro& m)
{
    if (is_error)
        return;
    expect("{");
    if (is_error) {
        snprintf(error, OCTO_ERR_MAX, "Expected '{' for definition of %s '%s'.", desc, name);
        return;
    }
    int depth = 1;
    while (!is_end()) {
        auto t = peek();
        if (t.type == OCTO_TOK_STR && strcmp(t.str_value, "{") == 0)
            depth++;
        if (t.type == OCTO_TOK_STR && strcmp(t.str_value, "}") == 0)
            depth--;
        if (depth == 0)
            break;
        m.body.push_back(next());
    }
    expect("}");
    if (is_error)
        snprintf(error, OCTO_ERR_MAX, "Expected '}' for definition of %s '%s'.", desc, name);
}

/**
 *
 *  Compile-time Calculation
 *
 **/

double OctoProgram::calc_terminal(char* name)
{
    // NUMBER | CONSTANT | LABEL | VREGISTER | '(' expression ')'
    if (peek_is_register())
        return register_or_alias();
    if (match("PI"))
        return 3.141592653589793;
    if (match("E"))
        return 2.718281828459045;
    if (match("HERE"))
        return here;
    auto t = next();
    if (t.type == OCTO_TOK_NUM) {
        double r = t.num_value;
        return r;
    }
    char* n = t.str_value;
    if (octo_map_get(&protos, n) != nullptr) {
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Cannot use forward declaration '%s' when calculating constant '%s'.", n, name);
        return 0;
    }
    auto* c = (octo_const*)octo_map_get(&constants, n);
    if (c != nullptr)
        return c->value;
    if (strcmp(n, "(") != 0) {
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Found undefined name '%s' when calculating constant '%s'.", n, name);
        return 0;
    }
    double r = calc_expr(name);
    expect(")");
    return r;
}

double OctoProgram::calc_expr(char* name)
{
    // UNARY expression
    if (match("strlen"))
        return octo_interned_len(string());
    if (match("-"))
        return -calc_expr(name);
    if (match("~"))
        return ~((int)calc_expr(name));
    if (match("!"))
        return !((int)calc_expr(name));
    if (match("sin"))
        return sin(calc_expr(name));
    if (match("cos"))
        return cos(calc_expr(name));
    if (match("tan"))
        return tan(calc_expr(name));
    if (match("exp"))
        return exp(calc_expr(name));
    if (match("log"))
        return log(calc_expr(name));
    if (match("abs"))
        return fabs(calc_expr(name));
    if (match("sqrt"))
        return sqrt(calc_expr(name));
    if (match("sign"))
        return sign(calc_expr(name));
    if (match("ceil"))
        return ceil(calc_expr(name));
    if (match("floor"))
        return floor(calc_expr(name));
    if (match("@")) {
        auto addr = (int)calc_expr(name);
        return addr >= 0 && addr < rom.size() ? 0xFF & rom[addr] : 0;
    }

    // expression BINARY expression
    double r = calc_terminal(name);
    if (match("-"))
        return r - calc_expr(name);
    if (match("+"))
        return r + calc_expr(name);
    if (match("*"))
        return r * calc_expr(name);
    if (match("/"))
        return r / calc_expr(name);
    if (match("%"))
        return ((int)r) % ((int)calc_expr(name));
    if (match("&"))
        return ((int)r) & ((int)calc_expr(name));
    if (match("|"))
        return ((int)r) | ((int)calc_expr(name));
    if (match("^"))
        return ((int)r) ^ ((int)calc_expr(name));
    if (match("<<"))
        return ((int)r) << ((int)calc_expr(name));
    if (match(">>"))
        return ((int)r) >> ((int)calc_expr(name));
    if (match("pow"))
        return pow(r, calc_expr(name));
    if (match("min"))
        return min(r, calc_expr(name));
    if (match("max"))
        return max(r, calc_expr(name));
    if (match("<"))
        return r < calc_expr(name);
    if (match(">"))
        return r > calc_expr(name);
    if (match("<="))
        return r <= calc_expr(name);
    if (match(">="))
        return r >= calc_expr(name);
    if (match("=="))
        return r == calc_expr(name);
    if (match("!="))
        return r != calc_expr(name);
    // terminal
    return r;
}

double OctoProgram::calculated(char* name)
{
    expect("{");
    double r = calc_expr(name);
    expect("}");
    return r;
}

/**
 *
 *  ROM construction
 *
 **/
void OctoProgram::append(char byte)
{
    if (is_error)
        return;
    if (here >= RAM_MAX) {
        is_error = 1;
        snprintf(error, OCTO_ERR_MAX, "Supported ROM space is full (16MB).");
        return;
    }
    if(here >= rom.size()) {
        if(rom.size() < 1024*1024) {
            rom.resize(1024*1024, 0);
            used.resize(1024 * 1024, 0);
            romLineMap.resize(1024 * 1024, 0xFFFFFFFF);
        }
        else if(rom.size() < RAM_MAX/2) {
            rom.resize(RAM_MAX/2, 0);
            used.resize(RAM_MAX/2, 0);
            romLineMap.resize(RAM_MAX/2, 0xFFFFFFFF);
        }
        else if(rom.size() < RAM_MAX) {
            rom.resize(RAM_MAX, 0);
            used.resize(RAM_MAX, 0);
            romLineMap.resize(RAM_MAX, 0xFFFFFFFF);
        }
    }
    if (here > startAddress && used[here]) {
        is_error = 1;
        snprintf(error, OCTO_ERR_MAX, "Data overlap. Address 0x%0X has already been defined.", here);
        return;
    }
    romLineMap[here] = source_line;
    rom[here] = byte, used[here] = 1, here++;
    if(here > length)
        length = here;
}

void OctoProgram::instruction(char a, char b)
{
    append(a), append(b);
}

void OctoProgram::immediate(char op, int nnn)
{
    instruction(op | ((nnn >> 8) & 0xF), (nnn & 0xFF));
}

void OctoProgram::jump(int addr, int dest)
{
    if (is_error)
        return;
    rom[addr] = (0x10 | ((dest >> 8) & 0xF)), used[addr] = 1;
    rom[addr + 1] = (dest & 0xFF), used[addr + 1] = 1;
}

/**
 *
 *  The Compiler proper
 *
 **/
void OctoProgram::pseudo_conditional(int reg, int sub, int comp)
{
    if (peek_is_register())
        instruction(0x8F, register_or_alias() << 4);
    else
        instruction(0x6F, value_8bit());
    instruction(0x8F, (reg << 4) | sub);
    instruction(comp, 0);
}

void OctoProgram::conditional(int negated)
{
    int reg = register_or_alias();
    auto t = peek();
    char d[256];
    t.format_value(d);
    if (is_error)
        return;
    char* n = string();

#define octo_ca(pos, neg) (strcmp(n,negated?(neg):(pos))==0)

    if (octo_ca("==", "!=")) {
        if (peek_is_register())
            instruction(0x90 | reg, register_or_alias() << 4);
        else
            instruction( 0x40 | reg, value_8bit());
    }
    else if (octo_ca("!=", "==")) {
        if (peek_is_register())
            instruction(0x50 | reg, register_or_alias() << 4);
        else
            instruction(0x30 | reg, value_8bit());
    }
    else if (octo_ca("key", "-key"))
        instruction(0xE0 | reg, 0xA1);
    else if (octo_ca("-key", "key"))
        instruction(0xE0 | reg, 0x9E);
    else if (octo_ca(">", "<="))
        pseudo_conditional(reg, 0x5, 0x4F);
    else if (octo_ca("<", ">="))
        pseudo_conditional(reg, 0x7, 0x4F);
    else if (octo_ca(">=", "<"))
        pseudo_conditional(reg, 0x7, 0x3F);
    else if (octo_ca("<=", ">"))
        pseudo_conditional(reg, 0x5, 0x3F);
    else {
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "Expected conditional operator, got %s.", d);
    }
}

void OctoProgram::resolve_label(int offset)
{
    int target = (here) + offset;
    char* n = identifier("label");
    if (is_error)
        return;
    if (octo_map_get(&constants, n) != nullptr) {
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "The name '%s' has already been defined.", n);
        return;
    }
    if (octo_map_get(&aliases, n) != nullptr) {
        is_error = 1, snprintf(error, OCTO_ERR_MAX, "The name '%s' is already used by an alias.", n);
        return;
    }
    if ((target == startAddress + 2 || target == startAddress) && (strcmp(n, "main") == 0)) {
        has_main = 0, here = target = startAddress;
        rom[startAddress] = 0, used[startAddress] = 0;
        rom[startAddress + 1] = 0, used[startAddress + 1] = 0;
    }
    octo_map_set(&constants, n, octo_make_const(target, 0));
    if (octo_map_get(&protos, n) == nullptr)
        return;

    auto* pr = (octo_proto*)octo_map_remove(&protos, n);
    for (int z = 0; z < pr->addrs.count; z++) {
        auto* pa = (octo_pref*)octo_list_get(&pr->addrs, z);
        if (pa->size == 16 && (rom[pa->value] & 0xF0) == 0x60) {  // :unpack long target
            rom[pa->value + 1] = target >> 8;
            rom[pa->value + 3] = target;
        }
        else if (pa->size == 16) {  // i := long target
            rom[pa->value] = target >> 8;
            rom[pa->value + 1] = target;
        }
        else if (pa->size <= 12 && (target & 0xFFF) != target) {
            is_error = 1, snprintf(error, OCTO_ERR_MAX, "Value 0x%0X for label '%s' does not fit in 12 bits.", target, n);
            break;
        }
        else if (pa->size <= 16 && (target & 0xFFFF) != target) {
            is_error = 1, snprintf(error, OCTO_ERR_MAX, "Value 0x%0X for label '%s' does not fit in 16 bits.", target, n);
            break;
        }
        else if (pa->size <= 24 && (target & 0xFFFFFF) != target) {
            is_error = 1, snprintf(error, OCTO_ERR_MAX, "Value 0x%0X for label '%s' does not fit in 24 bits.", target, n);
            break;
        }
        else if(pa->size == 24) {
            rom[pa->value] = target>>16;
            rom[pa->value + 1] = target >> 8;
            rom[pa->value + 2] = target;
        }
        else if ((rom[pa->value] & 0xF0) == 0x60) {  // :unpack target
            rom[pa->value + 1] = ((rom[pa->value + 1]) & 0xF0) | ((target >> 8) & 0xF);
            rom[pa->value + 3] = target;
        }
        else {
            rom[pa->value] = ((rom[pa->value]) & 0xF0) | ((target >> 8) & 0xF);
            rom[pa->value + 1] = target;
        }
    }
    octo_free_proto(pr);
}

void OctoProgram::compile_statement()
{
    if (is_error)
        return;
    int peek_line = peek().line, peek_pos = peek().pos;
    if (peek_is_register()) {
        int r = register_or_alias();
        if (match(":=")) {
            if (peek_is_register())
                instruction(0x80 | r, (register_or_alias() << 4) | 0x0);
            else if (match("random"))
                instruction(0xC0 | r, value_8bit());
            else if (match("key"))
                instruction(0xF0 | r, 0x0A);
            else if (match("delay"))
                instruction(0xF0 | r, 0x07);
            else
                instruction(0x60 | r, value_8bit());
        }
        else if (match("+=")) {
            if (peek_is_register())
                instruction(0x80 | r, (register_or_alias() << 4) | 0x4);
            else
                instruction(0x70 | r, value_8bit());
        }
        else if (match("-=")) {
            if (peek_is_register())
                instruction(0x80 | r, (register_or_alias() << 4) | 0x5);
            else
                instruction(0x70 | r, 1 + ~value_8bit());
        }
        else if (match("|="))
            instruction(0x80 | r, (register_or_alias() << 4) | 0x1);
        else if (match("&="))
            instruction(0x80 | r, (register_or_alias() << 4) | 0x2);
        else if (match("^="))
            instruction(0x80 | r, (register_or_alias() << 4) | 0x3);
        else if (match("=-"))
            instruction(0x80 | r, (register_or_alias() << 4) | 0x7);
        else if (match(">>="))
            instruction(0x80 | r, (register_or_alias() << 4) | 0x6);
        else if (match("<<="))
            instruction(0x80 | r, (register_or_alias() << 4) | 0xE);
        else {
            auto t = next();
            char d[256];
            if (!is_error)
                is_error = 1, snprintf(error, OCTO_ERR_MAX, "Unrecognized operator %s.", t.format_value(d));
        }
        return;
    }
    if (match(":"))
        resolve_label(0);
    else if (match(":next"))
        resolve_label(1);
    else if (match(":unpack")) {
        int a = 0;
        if (match("long")) {
            a = value_16bit(1, 0);
        }
        else {
            int v = value_4bit();
            a = (v << 12) | value_12bit();
        }
        auto* rh = (octo_reg*)octo_map_get(&aliases, counted("unpack-hi"));
        auto* rl = (octo_reg*)octo_map_get(&aliases, counted("unpack-lo"));
        instruction(0x60 | rh->value, a >> 8);
        instruction(0x60 | rl->value, a);
    }
    else if (match(":breakpoint"))
        breakpoints[here] = string();
    else if (match(":monitor")) {
        char n[256];
        octo_mon* m = octo_make_mon();
        peek().format_value(n);
        if (peek_is_register()) {
            m->type = 0;  // register monitor
            m->base = register_or_alias();
            if (peek().type == OCTO_TOK_NUM)
                m->len = value_4bit(), m->format = nullptr;
            else
                m->len = -1, m->format = string();
        }
        else {
            m->type = 1;  // memory monitor
            m->base = value_16bit(0, 0);
            if (peek().type == OCTO_TOK_NUM)
                m->len = value_16bit(0, 0), m->format = nullptr;
            else
                m->len = -1, m->format = string();
        }
        if (n[strlen(n) - 1] == '\'')
            n[strlen(n) - 1] = '\0';
        char* nn = counted(n[0] == '\'' ? n + 1 : n);
        octo_map_set(&monitors, nn, m);
    }
    else if (match(":assert")) {
        char* message = peek_match("{", 0) ? nullptr : string();
        if (!calculated("assertion")) {
            is_error = 1;
            if (message != nullptr)
                snprintf(error, OCTO_ERR_MAX, "Assertion failed: %s", message);
            else
                snprintf(error, OCTO_ERR_MAX, "Assertion failed.");
        }
    }
    else if (match(":proto"))
        next();  // deprecated
    else if (match(":alias")) {
        char* n = identifier("alias");
        if (octo_map_get(&constants, n) != nullptr) {
            is_error = 1, snprintf(error, OCTO_ERR_MAX, "The name '%s' is already used by a constant.", n);
            return;
        }
        int v = peek_match("{", 0) ? calculated("ANONYMOUS") : register_or_alias();
        if (v < 0 || v > 15) {
            is_error = 1;
            snprintf(error, OCTO_ERR_MAX, "Register index must be in the range [0,F].");
            return;
        }
        auto* prev = (octo_reg*)octo_map_set(&aliases, n, octo_make_reg(v));
        if (prev != nullptr)
            octo_free_reg(prev);
    }
    else if (match(":byte")) {
        append(peek_match("{", 0) ? (int)calculated("ANONYMOUS") : value_8bit());
    }
    else if (match(":pointer") || match(":pointer16")) {
        int a = peek_match("{", 0) ? calculated("ANONYMOUS") : value_16bit(1, 0);
        instruction(a >> 8, a);
    }
    else if (match(":pointer24")) {
        int a = peek_match("{", 0) ? calculated("ANONYMOUS") : value_24bit(1, 0);
        append(a >> 16);
        instruction(a >> 8, a);
    }
    else if (match(":org")) {
        here = (peek_match("{", 0) ? RAM_MASK & (int)calculated("ANONYMOUS") : value_16bit(0, 0));
    }
    else if (match(":call")) {
        immediate(0x20, peek_match("{", 0) ? 0xFFF & (int)calculated("ANONYMOUS") : value_12bit());
    }
    else if (match(":const")) {
        char* n = identifier("constant");
        if (octo_map_get(&constants, n) != nullptr) {
            is_error = 1;
            snprintf(error, OCTO_ERR_MAX, "The name '%s' has already been defined.", n);
            return;
        }
        octo_map_set(&constants, n, value_constant());
    }
    else if (match(":calc")) {
        char* n = identifier("calculated constant");
        auto* prev = (octo_const*)octo_map_get(&constants, n);
        if (prev != nullptr && !prev->is_mutable) {
            is_error = 1, snprintf(error, OCTO_ERR_MAX, "Cannot redefine the name '%s' with :calc.", n);
            return;
        }
        octo_map_set(&constants, n, octo_make_const(calculated(n), 1));
        if (prev != nullptr)
            octo_free_const(prev);
    }
    else if (match(";") || match("return"))
        instruction(0x00, 0xEE);
    else if (match("clear"))
        instruction(0x00, 0xE0);
    else if (match("bcd"))
        instruction(0xF0 | register_or_alias(), 0x33);
    else if (match("delay"))
        expect(":="), instruction(0xF0 | register_or_alias(), 0x15);
    else if (match("buzzer"))
        expect(":="), instruction(0xF0 | register_or_alias(), 0x18);
    else if (match("pitch"))
        expect(":="), instruction(0xF0 | register_or_alias(), 0x3A);
    else if (match("jump0"))
        immediate(0xB0, value_12bit());
    else if (match("jump"))
        immediate(0x10, value_12bit());
    else if (match("native"))
        immediate(0x00, value_12bit());
    else if (match("audio"))
        instruction(0xF0, 0x02);
    else if (match("scroll-down"))
        instruction(0x00, 0xC0 | value_4bit());
    else if (match("scroll-up"))
        instruction(0x00, 0xD0 | value_4bit());
    else if (match("scroll-right"))
        instruction(0x00, 0xFB);
    else if (match("scroll-left"))
        instruction(0x00, 0xFC);
    else if (match("exit"))
        instruction(0x00, 0xFD);
    else if (match("lores"))
        instruction(0x00, 0xFE);
    else if (match("hires"))
        instruction(0x00, 0xFF);
    else if (match("sprite")) {
        int x = register_or_alias(), y = register_or_alias();
        instruction(0xD0 | x, (y << 4) | value_4bit());
    }
    else if (match("plane")) {
        int n = value_4bit();
        if (n > 15)
            is_error = 1, snprintf(error, OCTO_ERR_MAX, "The plane bitmask must be [0,15], was %d.", n);
        instruction(0xF0 | n, 0x01);
    }
    else if (match("saveflags"))
        instruction(0xF0 | register_or_alias(), 0x75);
    else if (match("loadflags"))
        instruction(0xF0 | register_or_alias(), 0x85);
    else if (match("save")) {
        int r = register_or_alias();
        if (match("-"))
            instruction(0x50 | r, (register_or_alias() << 4) | 0x02);
        else
            instruction(0xF0 | r, 0x55);
    }
    else if (match("load")) {
        int r = register_or_alias();
        if (match("-"))
            instruction(0x50 | r, (register_or_alias() << 4) | 0x03);
        else
            instruction(0xF0 | r, 0x65);
    }
    else if (match("i")) {
        if (match(":=")) {
            if (match("long")) {
                int a = value_16bit(1, 2);
                instruction(0xF0, 0x00);
                instruction((a >> 8), a);
            }
            else if (match("hex"))
                instruction(0xF0 | register_or_alias(), 0x29);
            else if (match("bighex"))
                instruction(0xF0 | register_or_alias(), 0x30);
            else
                immediate(0xA0, value_12bit());
        }
        else if (match("+="))
            instruction(0xF0 | register_or_alias(), 0x1E);
        else {
            auto t = next();
            char d[256];
            is_error = 1, snprintf(error, OCTO_ERR_MAX, "%s is not an operator that can target the i register.", t.format_value(d));
        }
    }
    else if (match("if")) {
        int index = (peek_match("key", 1) || peek_match("-key", 1)) ? 2 : 3;
        if (peek_match("then", index)) {
            conditional(0), expect("then");
        }
        else if (peek_match("begin", index)) {
            conditional(1), expect("begin");
            octo_stack_push(&branches, octo_make_flow(here, source_line, source_pos, "begin"));
            instruction(0x00, 0x00);
        }
        else {
            for (int z = 0; z <= index; z++)
                if (!is_end())
                    next();
            is_error = 1;
            snprintf(error, OCTO_ERR_MAX, "Expected 'then' or 'begin'.");
        }
    }
    else if (match("else")) {
        if (octo_stack_is_empty(&branches)) {
            is_error = 1;
            snprintf(error, OCTO_ERR_MAX, "This 'else' does not have a matching 'begin'.");
            return;
        }
        auto* f = (octo_flow*)octo_stack_pop(&branches);
        jump(f->addr, here + 2);
        octo_free_flow(f);
        octo_stack_push(&branches, octo_make_flow(here, peek_line, peek_pos, "else"));
        instruction(0x00, 0x00);
    }
    else if (match("end")) {
        if (octo_stack_is_empty(&branches)) {
            is_error = 1;
            snprintf(error, OCTO_ERR_MAX, "This 'end' does not have a matching 'begin'.");
            return;
        }
        auto* f = (octo_flow*)octo_stack_pop(&branches);
        jump(f->addr, here);
        octo_free_flow(f);
    }
    else if (match("loop")) {
        octo_stack_push(&loops, octo_make_flow(here, peek_line, peek_pos, "loop"));
        octo_stack_push(&whiles, octo_make_flow(-1, peek_line, peek_pos, "loop"));
    }
    else if (match("while")) {
        if (octo_stack_is_empty(&loops)) {
            is_error = 1;
            snprintf(error, OCTO_ERR_MAX, "This 'while' is not within a loop.");
            return;
        }
        conditional(1);
        octo_stack_push(&whiles, octo_make_flow(here, peek_line, peek_pos, "while"));
        immediate(0x10, 0);  // forward jump
    }
    else if (match("again")) {
        if (octo_stack_is_empty(&loops)) {
            is_error = 1;
            snprintf(error, OCTO_ERR_MAX, "This 'again' does not have a matching 'loop'.");
            return;
        }
        auto* f = (octo_flow*)octo_stack_pop(&loops);
        immediate(0x10, f->addr);
        octo_free_flow(f);
        while (1) {
            auto* f = (octo_flow*)octo_stack_pop(&whiles);
            int a = f->addr;
            octo_free_flow(f);
            if (a == -1)
                break;
            jump(a, here);
        }
    }
    else if (match(":macro")) {
        char* n = identifier("macro");
        if(is_error)
            return;
        if (macros.count(n)) {
            is_error = 1, snprintf(error, OCTO_ERR_MAX, "The name '%s' has already been defined.", n);
            return;
        }
        auto& m = macros.emplace(n, OctoMacro()).first->second;
        while (!is_error && !is_end() && !peek_match("{", 0))
            m.args.push_back(identifier("macro argument"));
        macro_body("macro", n, m);
    }
    else if (match(":stringmode")) {
        char* n = identifier("stringmode");
        if (is_error)
            return;
        auto& s = stringmodes.try_emplace(n, OctoSMode()).first->second;
        int alpha_base = source_pos, alpha_quote = peek_char() == '"';
        char* alphabet = string();
        OctoMacro m;  // every stringmode needs its own copy of this
        macro_body("string mode", n, m);
        for (int z = 0; z < octo_interned_len(alphabet); z++) {
            int c = 0xFF & alphabet[z];
            if (s.modes[c]) {
                error_pos = alpha_base + z + (alpha_quote ? 1 : 0);
                is_error = 1, snprintf(error, OCTO_ERR_MAX, "String mode '%s' is already defined for the character '%c'.", n, c);
                break;
            }
            s.values[c] = z;
            auto mm = std::make_unique<OctoMacro>();
            mm->body = m.body;
            s.modes[c] = std::move(mm);
        }
    }
    else {
        auto t = peek();
        if (is_error)
            return;
        if (t.type == OCTO_TOK_NUM) {
            int n = t.num_value;
            next();
            if (n < -128 || n > 255) {
                is_error = 1, snprintf(error, OCTO_ERR_MAX, "Literal value '%d' does not fit in a byte- must be in range [-128,255].", n);
            }
            append(n);
            return;
        }
        char* n = t.type == OCTO_TOK_STR ? t.str_value : (char*)"";
        if (auto iter = macros.find(n); iter != macros.end()) {
            next();
            auto& m = iter->second;
            std::unordered_map<std::string_view,OctoToken> bindings;  // name -> tok
            bindings.emplace(counted("CALLS"), OctoToken(m.calls++));
            for (int z = 0; z < m.args.size(); z++) {
                if (is_end()) {
                    error_line = source_line, error_pos = source_pos;
                    is_error = 1, snprintf(error, OCTO_ERR_MAX, "Not enough arguments for expansion of macro '%s'.", n);
                    break;
                }
                bindings.emplace(m.args[z], next());
            }
            int splice_index = 0;
            for (int z = 0; z < m.body.size(); z++) {
                auto& bt = m.body[z];
                auto argIter = (bt.type == OCTO_TOK_STR ? bindings.find(bt.str_value) : bindings.end());
                tokens.insert(tokens.begin() + z, argIter != bindings.end() ? argIter->second : bt);
            }
        }
        else if (auto iter = stringmodes.find(n); iter != stringmodes.end()) {
            next();
            auto& s = iter->second;
            int text_base = source_pos, text_quote = peek_char() == '"';
            char* text = string();
            int splice_index = 0;
            for (int tz = 0; tz < octo_interned_len(text); tz++) {
                int c = 0xFF & text[tz];
                if (!s.modes[c]) {
                    error_pos = text_base + tz + (text_quote ? 1 : 0);
                    is_error = 1, snprintf(error, OCTO_ERR_MAX, "String mode '%s' is not defined for the character '%c'.", n, c);
                    break;
                }
                std::unordered_map<std::string_view,OctoToken> bindings;  // name -> tok
                bindings.emplace(counted("CALLS"), OctoToken(s.calls++));    // expansion count
                bindings.emplace(counted("CHAR"), OctoToken(c));              // ascii value of current char
                bindings.emplace(counted("INDEX"), OctoToken((int)tz));       // index of char in input string
                bindings.emplace(counted("VALUE"), OctoToken(s.values[c]));  // index of char in class alphabet
                auto& sm = *s.modes[c];
                for (int z = 0; z < sm.body.size(); z++) {
                    auto& bt = sm.body[z];
                    auto argIter = (bt.type == OCTO_TOK_STR ? bindings.find(bt.str_value) : bindings.end());
                    tokens.insert(tokens.begin() + splice_index++, argIter != bindings.end() ? argIter->second : bt);
                }
            }
        }
        else
            immediate(0x20, value_12bit());
    }
}

OctoProgram::OctoProgram(char* text, int startAddress)
{
    strings_used = 0;
    memset(strings, '\0', OCTO_INTERN_MAX);
    source = text;
    source_root = text;
    source_line = 0;
    source_pos = 0;
    has_main = 1;
    here = startAddress;
    this->startAddress = startAddress;
    length = 0;
    rom.resize(65536, 0);
    used.resize(65536, 0);
    romLineMap.resize(65536, 0xFFFFFFFF);
    octo_map_init(&constants);
    octo_map_init(&aliases);
    octo_map_init(&protos);
    octo_stack_init(&loops);
    octo_stack_init(&branches);
    octo_stack_init(&whiles);
    octo_map_init(&monitors);
    is_error = 0;
    error[0] = '\0';
    error_line = 0;
    error_pos = 0;
    if ((unsigned char)source[0] == 0xEF && (unsigned char)source[1] == 0xBB && (unsigned char)source[2] == 0xBF)
        source += 3;  // UTF-8 BOM
    skip_whitespace();

#define octo_kc(l, n) (octo_map_set(&constants, counted(("OCTO_KEY_" l)), octo_make_const(n, 0)))
    octo_kc("1", 0x1), octo_kc("2", 0x2), octo_kc("3", 0x3), octo_kc("4", 0xC), octo_kc("Q", 0x4), octo_kc("W", 0x5), octo_kc("E", 0x6), octo_kc("R", 0xD), octo_kc("A", 0x7), octo_kc("S", 0x8), octo_kc("D", 0x9), octo_kc("F", 0xE), octo_kc("Z", 0xA),
        octo_kc("X", 0x0), octo_kc("C", 0xB), octo_kc("V", 0xF);

    octo_map_set(&aliases, counted("unpack-hi"), octo_make_reg(0));
    octo_map_set(&aliases, counted("unpack-lo"), octo_make_reg(1));
}

bool OctoProgram::compile()
{
    instruction(0x00, 0x00);  // reserve a jump slot for main
    while (!is_end() && !is_error) {
        error_line = source_line;
        error_pos = source_pos;
        compile_statement();
    }
    if (is_error)
        return false;
    while (length > startAddress && !used[length - 1])
        length--;
    error_line = source_line, error_pos = source_pos;

    if (has_main) {
        auto* c = (octo_const*)octo_map_get(&constants, counted("main"));
        if (c == nullptr)
            return is_error = 1, snprintf(error, OCTO_ERR_MAX, "This program is missing a 'main' label."), false;
        jump(startAddress, c->value);
    }
    if (protos.keys.count > 0) {
        auto* pr = (octo_proto*)octo_list_get(&protos.values, 0);
        error_line = pr->line, error_pos = pr->pos;
        is_error = 1;
        snprintf(error, OCTO_ERR_MAX, "Undefined forward reference: %s", (char*)octo_list_get(&protos.keys, 0));
        return false;
    }
    if (!octo_stack_is_empty(&loops)) {
        auto* f = (octo_flow*)octo_stack_pop(&loops);
        is_error = 1;
        snprintf(error, OCTO_ERR_MAX, "This 'loop' does not have a matching 'again'.");
        error_line = f->line, error_pos = f->pos;
        octo_free_flow(f);
        return false;
    }
    if (!octo_stack_is_empty(&branches)) {
        auto* f = (octo_flow*)octo_stack_pop(&branches);
        is_error = 1;
        snprintf(error, OCTO_ERR_MAX, "This '%s' does not have a matching 'end'.", f->type);
        error_line = f->line, error_pos = f->pos;
        octo_free_flow(f);
        return false;
    }
    return true; // success!
}
