//
// Created by schuemann on 21.02.24.
//

#include "octo_compiler.hpp"

namespace octo
{

std::unordered_map<std::string_view, TokenId> lexerTokenMap = {
#define TEXT_TOKEN_MAP(NAME, TEXT) {TEXT, TokenId::NAME},
    TOKEN_LIST(TEXT_TOKEN_MAP)
#undef TEXT_TOKEN_MAP
};

Token::Token(int line, int pos)
    : type{Type::END_OF_FILE}
    , line{line}
    , pos{pos}
{
}

Token::Token(int n)
    : type{Type::NUMBER}
    , line{0}
    , pos{0}
{
    num_value = n;
}

Token::Token(const Token& other)
    : type{other.type}
    , tid(other.tid)
    , line{other.line}
    , pos{other.pos}
{
    if (type == Type::NUMBER) {
        num_value = other.num_value;
    }
    else {
        str_value = other.str_value;
    }
}

char* Token::formatValue(char* d) const
{
    switch (type) {
        case Type::END_OF_FILE:
            snprintf(d, 255, "<end of file>");
            break;
        case Type::STRING:
            snprintf(d, 255, "'%s'", std::string(str_value).c_str());
            break;
        case Type::NUMBER:
            snprintf(d, 255, "%d", (int)num_value);
            break;
        default:
            snprintf(d, 255, "unknown type");
            break;
    }
    return d;
}


Program::~Program()
{
}

/**
 *
 *  Tokenization
 *
 **/

std::string_view Program::safeStringStringView(std::string&& name)
{
    auto iter = stringTable.find(name);
    if (iter != stringTable.end())
        return *iter;
    return *stringTable.emplace(std::move(name)).first;
}

std::string_view Program::safeStringStringView(char* name)
{
    return safeStringStringView({name, std::strlen(name)});
}

int Program::is_end() const
{
    return tokens.empty() && source >= sourceEnd;
}

char Program::next_char()
{
    if(source >= sourceEnd)
        return 0;
    char c = source[0];
    if (c == '\n')
        source_line++, source_pos = 0;
    else
        source_pos++;
    source++;
    return c;
}

char Program::peek_char() const
{
    return source >= sourceEnd ? '\0' : source[0];
}

void Program::skip_whitespace()
{
    while (true) {
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

void Program::fetch_token()
{
    if (is_end()) {
        is_error = 1;
        error = "Unexpected EOF.";
        return;
    }
    if (is_error)
        return;
    tokens.emplace_back(source_line, source_pos);
    auto& t = tokens.back();
    std::string strBuffer;
    const auto* start = source;
    size_t index = 0;
    if (source[0] == '"') {
        next_char();
        while (true) {
            char c = next_char();
            if (c == '\0') {
                is_error = 1;
                error = "Missing a closing \" in a string literal.";
                error_line = source_line, error_pos = source_pos;
                return;
            }
            if (c == '"') {
                next_char();
                break;
            }
            if (c == '\\') {
                start = nullptr;
                char ec = next_char();
                if (ec == '\0') {
                    is_error = 1;
                    error = "Missing a closing \" in a string literal.";
                    error_line = source_line, error_pos = source_pos;
                    return;
                }
                if (ec == 't')
                    strBuffer.push_back('\t');
                else if (ec == 'n')
                    strBuffer.push_back('\n');
                else if (ec == 'r')
                    strBuffer.push_back('\r');
                else if (ec == 'v')
                    strBuffer.push_back('\v');
                else if (ec == '0')
                    strBuffer.push_back('\0');
                else if (ec == '\\')
                    strBuffer.push_back('\\');
                else if (ec == '"')
                    strBuffer.push_back('"');
                else {
                    is_error = 1;
                    error = fmt::format("Unrecognized escape character '{}' in a string literal.", ec);
                    error_line = source_line, error_pos = source_pos - 1;
                    return;
                }
            }
            else {
                strBuffer.push_back(c);
            }
        }
        t.type = Token::Type::STRING;
        t.tid = TokenId::STRING_LITERAL;
        t.str_value = start ? std::string_view(start + 1, strBuffer.length()) : safeStringStringView(std::move(strBuffer));
    }
    else {
        // string or number
        while (true) {
            char c = next_char();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '#' || c == '\0')
                break;
            ++index;
        }
        if(std::isdigit(*start) || (*start == '-' && std::isdigit(start[1]))) {
            double float_val = 0;
            int64_t temp = 0;
            t.type = Token::Type::STRING;
            auto fcr = fast_float::from_chars(start, start + index, float_val);
            if (fcr.ptr == start + index && fcr.ec == std::errc{}) {
                t.type = Token::Type::NUMBER, t.num_value = float_val;
            }
            else if (index > 2 && start[0] == '0' && start[1] == 'b') {
                auto [ptr, ec] = std::from_chars(start + 2, start + index, temp, 2);
                if(ptr == start + index && ec == std::errc{})
                    t.type = Token::Type::NUMBER, t.num_value = static_cast<double>(temp);
            }
            else if (index > 2 && start[0] == '0' && start[1] == 'x') {
                auto [ptr, ec] = std::from_chars(start + 2, start + index, temp, 16);
                if(ptr == start + index && ec == std::errc{})
                    t.type = Token::Type::NUMBER, t.num_value = static_cast<double>(temp);
            }
            else if (index > 3 && start[0] == '-' && start[1] == '0' && start[2] == 'b') {
                auto [ptr, ec] = std::from_chars(start + 3, start + index, temp, 2);
                if(ptr == start + index && ec == std::errc{})
                    t.type = Token::Type::NUMBER, t.num_value = -static_cast<double>(temp);
            }
            else if (index > 3 && start[0] == '-' && start[1] == '0' && start[2] == 'x') {
                auto [ptr, ec] = std::from_chars(start + 3, start + index, temp, 16);
                if(ptr == start + index && ec == std::errc{})
                    t.type = Token::Type::NUMBER, t.num_value = -static_cast<double>(temp);
            }
            if(t.type != Token::Type::NUMBER) {
                is_error = true;
                error = fmt::format("Expected a valid number, but found '{}'.", std::string_view(start, index));
                error_line = source_line, error_pos = source_pos - index;
            }
        }
        else {
            t.type = Token::Type::STRING, t.str_value = safeStringStringView({start, index});
            auto iter = lexerTokenMap.find(t.str_value);
            if (iter != lexerTokenMap.end())
                t.tid = iter->second;
        }
        // this is handy for debugging internal errors:
        // if (t->type==Token::Type::STRING) printf("RAW TOKEN: %p %s\n", (void*)t->str_value, t->str_value);
        // if (t->type==Token::Type::NUMBER) printf("RAW TOKEN: %f\n", t->num_value);
    }
    skip_whitespace();
}

Token Program::next()
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

Token Program::peek()
{
    if (tokens.empty())
        fetch_token();
    if (is_error)
        return {source_line, source_pos};
    return tokens.front();
}

bool Program::peek_match(const std::string_view& name, int index)
{
    while (!is_error && !is_end() && tokens.size() < index + 1)
        fetch_token();
    if (is_end() || is_error)
        return false;
    return tokens[index].type == Token::Type::STRING && tokens[index].str_value == name;
}

inline bool Program::match(const std::string_view& name)
{
    if (peek_match(name, 0)) {
        tokens.pop_front();
        return true;
    }
    return false;
}

void Program::eat()
{
    tokens.pop_front();
}

/**
 *
 *  Parsing
 *
 **/


bool Program::is_reserved(std::string_view name)
{
    return lexerTokenMap.count(name);
}

bool Program::check_name(std::string_view name, const char* kind)
{
    if (is_error)
        return false;
    if (strncmp("OCTO_", name.data(), 5) == 0 || is_reserved(name)) {
        is_error = 1, error = fmt::format("The name '{}' is reserved and cannot be used for a {}.", name, kind);
        return false;
    }
    return true;
}

std::string_view Program::string()
{
    if (is_error)
        return "";
    auto t = next();
    if (t.type != Token::Type::STRING) {
        is_error = 1, error = fmt::format("Expected a string, got {}.", (int)t.num_value);
        return "";
    }
    return t.str_value;
}

std::string_view Program::identifier(const char* kind)
{
    if (is_error)
        return "";
    auto t = next();
    if (t.type != Token::Type::STRING) {
        is_error = 1, error = fmt::format("Expected a name for a {}, got {}.", kind, (int)t.num_value);
        return "";
    }
    if (!check_name(t.str_value, kind))
        return "";
    return t.str_value;
}

void Program::expect(std::string_view name)
{
    if (is_error)
        return;
    auto t = next();
    if (t.type != Token::Type::STRING || t.str_value != name) {
        char d[256];
        is_error = 1, error = fmt::format("Expected {}, got {}.", name, t.formatValue(d));
    }
}

bool Program::is_register(std::string_view name)
{
    if (aliases.count(name))
        return true;
    if (name.length() != 2)
        return false;
    if (name[0] != 'v' && name[0] != 'V')
        return false;
    return isxdigit(name[1]);
}

bool Program::peek_is_register()
{
    auto t = peek();
    return t.type == Token::Type::STRING && is_register(t.str_value);
}

int Program::register_or_alias()
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type != Token::Type::STRING || !is_register(t.str_value)) {
        char d[256];
        is_error = 1, error = fmt::format("Expected register, got {}.", t.formatValue(d));
        return 0;
    }
    auto iter = aliases.find(t.str_value);
    if (iter != aliases.end())
        return iter->second;
    char c = static_cast<char>(std::tolower(t.str_value[1]));
    return isdigit(c) ? c - '0' : 10 + (c - 'a');
}

int Program::value_range(int n, int mask)
{
    if (mask == 0xF && (n < 0 || n > mask))
        is_error = 1, error = fmt::format("Argument {} does not fit in 4 bits- must be in range [0,15].", n);
    if (mask == 0xFF && (n < -128 || n > mask))
        is_error = 1, error = fmt::format("Argument {} does not fit in a byte- must be in range [-128,255].", n);
    if (mask == 0xFFF && (n < 0 || n > mask))
        is_error = 1, error = fmt::format("Argument {} does not fit in 12 bits.", n);
    if (mask == 0xFFFF && (n < 0 || n > mask))
        is_error = 1, error = fmt::format("Argument {} does not fit in 16 bits.", n);
    if (mask == 0xFFFFFF && (n < 0 || n > mask))
        is_error = 1, error = fmt::format("Argument {} does not fit in 24 bits.", n);
    return n & mask;
}

void Program::value_fail(const std::string_view& w, const std::string_view& n, bool undef)
{
    if (is_error)
        return;
    if (is_register(n))
        is_error = 1, error = fmt::format("Expected {} value, but found the register {}.", w, n);
    else if (is_reserved(n))
        is_error = 1, error = fmt::format("Expected {} value, but found the keyword '{}'. Missing a token?", w, n);
    else if (undef)
        is_error = 1, error = fmt::format("Expected {} value, but found the undefined name '{}'.", w, n);
}

int Program::value_4bit()
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return value_range((int)t.num_value, 0xF);
    }
    auto& n = t.str_value;
    auto iter = constants.find(n);
    if (iter != constants.end())
        return value_range((int)iter->second.value, 0xF);
    return value_fail("a 4-bit", n, true), 0;
}

int Program::value_8bit()
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return value_range((int)t.num_value, 0xFF);
    }
    auto& n = t.str_value;
    auto iter = constants.find(t.str_value);
    if (iter != constants.end())
        return value_range((int)iter->second.value, 0xFF);
    return value_fail("an 8-bit", t.str_value, true), 0;
}

int Program::value_12bit()
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return value_range((int)t.num_value, 0xFFF);
    }
    auto& n = t.str_value;
    int proto_line = t.line, proto_pos = t.pos;
    auto iter = constants.find(n);
    if (iter != constants.end())
        return value_range((int)iter->second.value, 0xFFF);
    value_fail("a 12-bit", n, false);
    if (is_error)
        return 0;
    if (!check_name(n, "label"))
        return 0;
    addProtoRef(n, proto_line, proto_pos, here, 12);
    return 0;
}

int Program::value_16bit(int can_forward_ref, int offset)
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return value_range((int)t.num_value, 0xFFFF);
    }
    auto& n = t.str_value;
    int proto_line = t.line, proto_pos = t.pos;
    auto iter = constants.find(n);
    if (iter != constants.end())
        return value_range((int)iter->second.value, 0xFFFF);
    value_fail("a 16-bit", n, false);
    if (is_error)
        return 0;
    if (!check_name(n, "label"))
        return 0;
    if (!can_forward_ref) {
        is_error = 1, error = fmt::format("The reference to '{}' may not be forward-declared.", n);
        return 0;
    }
    addProtoRef(n, proto_line, proto_pos, here + offset, 16);
    return 0;
}

int Program::value_24bit(int can_forward_ref, int offset)
{
    if (is_error)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return value_range((int)t.num_value, 0xFFFFFF);
    }
    auto& n = t.str_value;
    int proto_line = t.line, proto_pos = t.pos;
    auto iter = constants.find(n);
    if (iter != constants.end())
        return value_range((int)iter->second.value, 0xFFFFFF);
    value_fail("a 24-bit", n, false);
    if (is_error)
        return 0;
    if (!check_name(n, "label"))
        return 0;
    if (!can_forward_ref) {
        is_error = 1, error = fmt::format("The reference to '{}' may not be forward-declared.", n);
        return 0;
    }
    addProtoRef(n, proto_line, proto_pos, here + offset, 24);
    return 0;
}

void Program::addProtoRef(std::string_view name, int line, int pos, int where, int8_t size)
{
    auto iter = protos.find(name);
    if (iter == protos.end()) {
        iter = protos.emplace(name, Prototype{line, pos}).first;
    }
    iter->second.addrs.push_back({where, size});
}

Constant Program::value_constant()
{
    auto t = next();
    if (is_error)
        return {0, false};
    if (t.type == Token::Type::NUMBER) {
        return {static_cast<double>((int)t.num_value), false};
    }
    auto& n = t.str_value;
    auto iter = constants.find(n);
    if (iter != constants.end())
        return {iter->second.value, false};
    if (protos.count(n))
        is_error = 1, error = fmt::format("A constant reference to '{}' may not be forward-declared.", n);
    value_fail("a constant", n, true);
    return {0, false};
}

void Program::macro_body(const std::string_view& desc, const std::string_view& name, Macro& m)
{
    if (is_error)
        return;
    expect("{");
    if (is_error) {
        error = fmt::format("Expected '{{' for definition of {} '{}'.", desc, name);
        return;
    }
    int depth = 1;
    while (!is_end()) {
        auto t = peek();
        if (t.type == Token::Type::STRING && t.str_value == "{")
            depth++;
        if (t.type == Token::Type::STRING && t.str_value == "}")
            depth--;
        if (depth == 0)
            break;
        m.body.push_back(next());
    }
    expect("}");
    if (is_error)
        error = fmt::format("Expected '}}' for definition of {} '{}'.", desc, name);
}

//-----------------------------------------------------------
// Compile-time Calculation
//-----------------------------------------------------------

double Program::calc_terminal(std::string_view name)
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
    if (t.type == Token::Type::NUMBER) {
        double r = t.num_value;
        return r;
    }
    auto& n = t.str_value;
    if (protos.count(n)) {
        is_error = 1, error = fmt::format("Cannot use forward declaration '{}' when calculating constant '{}'.", n, name);
        return 0;
    }
    auto iter = constants.find(n);
    if (iter != constants.end())
        return iter->second.value;
    if (n != "(") {
        is_error = 1, error = fmt::format("Found undefined name '{}' when calculating constant '{}'.", n, name);
        return 0;
    }
    double r = calc_expr(name);
    expect(")");
    return r;
}

double Program::calc_expr(std::string_view name)
{
    // UNARY expression
    if (match("strlen"))
        return (double)string().length();
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

double Program::calculated(std::string_view name)
{
    expect("{");
    double r = calc_expr(name);
    expect("}");
    return r;
}

//-----------------------------------------------------------
//  ROM construction
//-----------------------------------------------------------

void Program::append(uint8_t byte)
{
    if (is_error)
        return;
    if (here >= RAM_MAX) {
        is_error = 1;
        error = "Supported ROM space is full (16MB).";
        return;
    }
    if (here >= rom.size()) {
        if (rom.size() < 1024 * 1024) {
            rom.resize(1024 * 1024, 0);
            used.resize(1024 * 1024, 0);
            romLineMap.resize(1024 * 1024, 0xFFFFFFFF);
        }
        else if (rom.size() < RAM_MAX / 2) {
            rom.resize(RAM_MAX / 2, 0);
            used.resize(RAM_MAX / 2, 0);
            romLineMap.resize(RAM_MAX / 2, 0xFFFFFFFF);
        }
        else if (rom.size() < RAM_MAX) {
            rom.resize(RAM_MAX, 0);
            used.resize(RAM_MAX, 0);
            romLineMap.resize(RAM_MAX, 0xFFFFFFFF);
        }
    }
    if (here > startAddress && used[here]) {
        is_error = 1;
        error = fmt::format("Data overlap. Address 0x{:0X} has already been defined.", here);
        return;
    }
    romLineMap[here] = source_line;
    rom[here] = byte, used[here] = 1, here++;
    if (here > length)
        length = here;
}

void Program::instruction(uint8_t a, uint8_t b)
{
    append(a), append(b);
}

void Program::immediate(uint8_t op, int nnn)
{
    instruction(op | ((nnn >> 8) & 0xF), (nnn & 0xFF));
}

void Program::jump(int addr, int dest)
{
    if (is_error)
        return;
    rom[addr] = (0x10 | ((dest >> 8) & 0xF)), used[addr] = 1;
    rom[addr + 1] = (dest & 0xFF), used[addr + 1] = 1;
}

//-----------------------------------------------------------
//  The Compiler proper
//-----------------------------------------------------------

void Program::pseudo_conditional(int reg, int sub, int comp)
{
    if (peek_is_register())
        instruction(0x8F, register_or_alias() << 4);
    else
        instruction(0x6F, value_8bit());
    instruction(0x8F, (reg << 4) | sub);
    instruction(comp, 0);
}

void Program::conditional(int negated)
{
    int reg = register_or_alias();
    auto t = peek();
    char d[256];
    t.formatValue(d);
    if (is_error)
        return;
    auto n = string();

#define octo_ca(pos, neg) (n == (negated ? (neg) : (pos)))

    if (octo_ca("==", "!=")) {
        if (peek_is_register())
            instruction(0x90 | reg, register_or_alias() << 4);
        else
            instruction(0x40 | reg, value_8bit());
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
        is_error = 1, error = fmt::format("Expected conditional operator, got {}.", d);
    }
}

void Program::resolve_label(int offset)
{
    int target = (here) + offset;
    auto n = identifier("label");
    if (is_error)
        return;
    if (constants.count(n)) {
        is_error = 1, error = fmt::format("The name '{}' has already been defined.", n);
        return;
    }
    if (aliases.count(n)) {
        is_error = 1, error = fmt::format("The name '{}' is already used by an alias.", n);
        return;
    }
    if ((target == startAddress + 2 || target == startAddress) && (n == "main")) {
        has_main = 0, here = target = startAddress;
        rom[startAddress] = 0, used[startAddress] = 0;
        rom[startAddress + 1] = 0, used[startAddress + 1] = 0;
    }
    constants.insert_or_assign(n, Constant{static_cast<double>(target), false});
    auto iter = protos.find(n);
    if (iter == protos.end())
        return;

    auto& pr = iter->second;
    for (auto& pa : pr.addrs) {
        if (pa.size == 16 && (rom[pa.value] & 0xF0) == 0x60) {  // :unpack long target
            rom[pa.value + 1] = target >> 8;
            rom[pa.value + 3] = target;
        }
        else if (pa.size == 16) {  // i := long target
            rom[pa.value] = target >> 8;
            rom[pa.value + 1] = target;
        }
        else if (pa.size <= 12 && (target & 0xFFF) != target) {
            is_error = 1, error = fmt::format("Value 0x{:0X} for label '{}' does not fit in 12 bits.", target, n);
            break;
        }
        else if (pa.size <= 16 && (target & 0xFFFF) != target) {
            is_error = 1, error = fmt::format("Value 0x{:0X} for label '{}' does not fit in 16 bits.", target, n);
            break;
        }
        else if (pa.size <= 24 && (target & 0xFFFFFF) != target) {
            is_error = 1, error = fmt::format("Value 0x{:0X} for label '{}' does not fit in 24 bits.", target, n);
            break;
        }
        else if (pa.size == 24) {
            rom[pa.value] = target >> 16;
            rom[pa.value + 1] = target >> 8;
            rom[pa.value + 2] = target;
        }
        else if ((rom[pa.value] & 0xF0) == 0x60) {  // :unpack target
            rom[pa.value + 1] = ((rom[pa.value + 1]) & 0xF0) | ((target >> 8) & 0xF);
            rom[pa.value + 3] = target;
        }
        else {
            rom[pa.value] = ((rom[pa.value]) & 0xF0) | ((target >> 8) & 0xF);
            rom[pa.value + 1] = target;
        }
    }
    protos.erase(n);
}

#if 0
void Program::compile_statement()
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
                is_error = 1, error = fmt::format("Unrecognized operator {}.", t.formatValue(d));
        }
    }
    else if (match(":"))
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
        auto rh = aliases["unpack-hi"];
        auto rl = aliases["unpack-lo"];
        instruction(0x60 | rh, a >> 8);
        instruction(0x60 | rl, a);
    }
    else if (match(":breakpoint"))
        breakpoints[here] = string().data();
    else if (match(":monitor")) {
        char n[256];
        int type, base, len;
        std::string format;
        peek().formatValue(n);
        if (peek_is_register()) {
            type = 0;  // register monitor
            base = register_or_alias();
            if (peek().type == Token::Type::NUMBER)
                len = value_4bit();
            else
                len = -1, format = string();
        }
        else {
            type = 1;  // memory monitor
            base = value_16bit(0, 0);
            if (peek().type == Token::Type::NUMBER)
                len = value_16bit(0, 0);
            else
                len = -1, format = string();
        }
        if (n[strlen(n) - 1] == '\'')
            n[strlen(n) - 1] = '\0';
        auto nn = safeStringStringView(n[0] == '\'' ? n + 1 : n);
        monitors.insert_or_assign(nn, Monitor{type, base, len, format});
    }
    else if (match(":assert")) {
        auto message = peek_match("{", 0) ? std::string_view() : string();
        if (!(int)calculated("assertion")) {
            is_error = 1;
            if (!message.empty())
                error = fmt::format("Assertion failed: {}", message);
            else
                error = "Assertion failed.";
        }
    }
    else if (match(":proto"))
        next();  // deprecated
    else if (match(":alias")) {
        auto n = identifier("alias");
        if (constants.count(n)) {
            is_error = 1, error = fmt::format("The name '{}' is already used by a constant.", n);
            return;
        }
        int v = peek_match("{", 0) ? (int)calculated("ANONYMOUS") : (int)register_or_alias();
        if (v < 0 || v > 15) {
            is_error = 1;
            error = "Register index must be in the range [0,F].";
            return;
        }
        aliases[n] = v;
    }
    else if (match(":byte")) {
        append(peek_match("{", 0) ? (int)calculated("ANONYMOUS") : value_8bit());
    }
    else if (match(":pointer") || match(":pointer16")) {
        int a = peek_match("{", 0) ? (int)calculated("ANONYMOUS") : (int)value_16bit(1, 0);
        instruction(a >> 8, a);
    }
    else if (match(":pointer24")) {
        int a = peek_match("{", 0) ? (int)calculated("ANONYMOUS") : (int)value_24bit(1, 0);
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
        auto n = identifier("constant");
        if (constants.count(n)) {
            is_error = 1;
            error = fmt::format("The name '{}' has already been defined.", n);
            return;
        }
        constants.insert_or_assign(n, value_constant());
    }
    else if (match(":calc")) {
        auto n = identifier("calculated constant");
        auto iter = constants.find(n);
        if (iter != constants.end() && !iter->second.isMutable) {
            is_error = 1, error = fmt::format("Cannot redefine the name '{}' with :calc.", n);
            return;
        }
        constants.insert_or_assign(n, Constant{calculated(n), true});
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
            is_error = 1, error = fmt::format("The plane bitmask must be [0,15], was {}.", n);
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
            is_error = 1, error = fmt::format("{} is not an operator that can target the i register.", t.formatValue(d));
        }
    }
    else if (match("if")) {
        int index = (peek_match("key", 1) || peek_match("-key", 1)) ? 2 : 3;
        if (peek_match("then", index)) {
            conditional(0), expect("then");
        }
        else if (peek_match("begin", index)) {
            conditional(1), expect("begin");
            branches.push({here, source_line, source_pos, "begin"});
            instruction(0x00, 0x00);
        }
        else {
            for (int z = 0; z <= index; z++)
                if (!is_end())
                    next();
            is_error = 1;
            error = "Expected 'then' or 'begin'.";
        }
    }
    else if (match("else")) {
        if (branches.empty()) {
            is_error = 1;
            error = "This 'else' does not have a matching 'begin'.";
            return;
        }
        jump(branches.top().addr, here + 2);
        branches.pop();
        branches.push({here, peek_line, peek_pos, "else"});
        instruction(0x00, 0x00);
    }
    else if (match("end")) {
        if (branches.empty()) {
            is_error = 1;
            error = "This 'end' does not have a matching 'begin'.";
            return;
        }
        jump(branches.top().addr, here);
        branches.pop();
    }
    else if (match("loop")) {
        loops.push({here, peek_line, peek_pos, "loop"});
        whiles.push({-1, peek_line, peek_pos, "loop"});
    }
    else if (match("while")) {
        if (loops.empty()) {
            is_error = 1;
            error = "This 'while' is not within a loop.";
            return;
        }
        conditional(1);
        whiles.push({here, peek_line, peek_pos, "while"});
        immediate(0x10, 0);  // forward jump
    }
    else if (match("again")) {
        if (loops.empty()) {
            is_error = 1;
            error = "This 'again' does not have a matching 'loop'.";
            return;
        }
        immediate(0x10, loops.top().addr);
        loops.pop();
        while (true) {
            // works as loop always pushes a -1 while, but is it needed?
            int a = whiles.top().addr;
            whiles.pop();
            if (a == -1)
                break;
            jump(a, here);
        }
    }
    else if (match(":macro")) {
        auto n = identifier("macro");
        if (is_error)
            return;
        if (macros.count(n)) {
            is_error = 1, error = fmt::format("The name '{}' has already been defined.", n);
            return;
        }
        auto& m = macros.emplace(n, Macro()).first->second;
        while (!is_error && !is_end() && !peek_match("{", 0))
            m.args.push_back(identifier("macro argument"));
        macro_body("macro", n, m);
    }
    else if (match(":stringmode")) {
        auto n = identifier("stringmode");
        if (is_error)
            return;
        auto& s = stringModes.try_emplace(n, StringMode()).first->second;
        int alpha_base = source_pos, alpha_quote = peek_char() == '"';
        auto alphabet = string();
        Macro m;  // every stringmode needs its own copy of this
        macro_body("string mode", n, m);
        for (int z = 0; z < alphabet.length(); z++) {
            int c = 0xFF & alphabet[z];
            if (s.modes[c]) {
                error_pos = alpha_base + z + (alpha_quote ? 1 : 0);
                is_error = 1, error = fmt::format("String mode '{}' is already defined for the character '{:c}'.", n, c);
                break;
            }
            s.values[c] = (char)z;
            auto mm = std::make_unique<Macro>();
            mm->body = m.body;
            s.modes[c] = std::move(mm);
        }
    }
    else {
        auto t = peek();
        if (is_error)
            return;
        if (t.type == Token::Type::NUMBER) {
            int n = (int)t.num_value;
            next();
            if (n < -128 || n > 255) {
                is_error = 1, error = fmt::format("Literal value '{}' does not fit in a byte- must be in range [-128,255].", n);
            }
            append(n);
            return;
        }
        auto n = t.type == Token::Type::STRING ? t.str_value : std::string_view();
        if (auto mi = macros.find(n); mi != macros.end()) {
            next();
            auto& m = mi->second;
            std::unordered_map<std::string_view, Token> bindings;  // name -> tok
            bindings.emplace("CALLS", Token(m.calls++));
            for (auto& arg : m.args) {
                if (is_end()) {
                    error_line = source_line, error_pos = source_pos;
                    is_error = 1, error = fmt::format("Not enough arguments for expansion of macro '{}'.", n);
                    break;
                }
                bindings.emplace(arg, next());
            }
            int splice_index = 0;
            for (int z = 0; z < m.body.size(); z++) {
                auto& bt = m.body[z];
                auto argIter = (bt.type == Token::Type::STRING ? bindings.find(bt.str_value) : bindings.end());
                tokens.insert(tokens.begin() + z, argIter != bindings.end() ? argIter->second : bt);
            }
        }
        else if (auto iter = stringModes.find(n); iter != stringModes.end()) {
            next();
            auto& s = iter->second;
            int text_base = source_pos, text_quote = peek_char() == '"';
            auto text = string();
            int splice_index = 0;
            for (int tz = 0; tz < text.length(); tz++) {
                int c = 0xFF & text[tz];
                if (!s.modes[c]) {
                    error_pos = text_base + tz + (text_quote ? 1 : 0);
                    is_error = 1, error = fmt::format("String mode '{}' is not defined for the character '{:c}'.", n, c);
                    break;
                }
                std::unordered_map<std::string_view, Token> bindings;  // name -> tok
                bindings.emplace("CALLS", Token(s.calls++));           // expansion count
                bindings.emplace("CHAR", Token(c));                    // ascii value of current char
                bindings.emplace("INDEX", Token((int)tz));             // index of char in input string
                bindings.emplace("VALUE", Token(s.values[c]));         // index of char in class alphabet
                auto& sm = *s.modes[c];
                for (auto& bt : sm.body) {
                    auto argIter = (bt.type == Token::Type::STRING ? bindings.find(bt.str_value) : bindings.end());
                    tokens.insert(tokens.begin() + splice_index++, argIter != bindings.end() ? argIter->second : bt);
                }
            }
        }
        else
            immediate(0x20, value_12bit());
    }
}
#else
void Program::compile_statement()
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
                is_error = 1, error = fmt::format("Unrecognized operator {}.", t.formatValue(d));
        }
    }
    else {
        if (!is_error && !is_end() && tokens.empty())
            fetch_token();
        if (is_end() || is_error)
            return;
        switch (tokens.front().tid) {
            case TokenId::COLON:
                eat();
                resolve_label(0);
                break;
            case TokenId::NEXT:
                eat();
                resolve_label(1);
                break;
            case TokenId::UNPACK: {
                eat();
                int a = 0;
                if (match("long")) {
                    a = value_16bit(1, 0);
                }
                else {
                    int v = value_4bit();
                    a = (v << 12) | value_12bit();
                }
                auto rh = aliases["unpack-hi"];
                auto rl = aliases["unpack-lo"];
                instruction(0x60 | rh, a >> 8);
                instruction(0x60 | rl, a);
                break;
            }
            case TokenId::BREAKPOINT:
                eat();
                breakpoints[here] = string().data();
                break;
            case TokenId::MONITOR: {
                eat();
                char n[256];
                int type, base, len;
                std::string format;
                peek().formatValue(n);
                if (peek_is_register()) {
                    type = 0;  // register monitor
                    base = register_or_alias();
                    if (peek().type == Token::Type::NUMBER)
                        len = value_4bit();
                    else
                        len = -1, format = string();
                }
                else {
                    type = 1;  // memory monitor
                    base = value_16bit(0, 0);
                    if (peek().type == Token::Type::NUMBER)
                        len = value_16bit(0, 0);
                    else
                        len = -1, format = string();
                }
                if (n[strlen(n) - 1] == '\'')
                    n[strlen(n) - 1] = '\0';
                auto nn = safeStringStringView(n[0] == '\'' ? n + 1 : n);
                monitors.insert_or_assign(nn, Monitor{type, base, len, format});
                break;
            }
            case TokenId::ASSERT: {
                eat();
                auto message = peek_match("{", 0) ? std::string_view() : string();
                if (!(int)calculated("assertion")) {
                    is_error = 1;
                    if (!message.empty())
                        error = fmt::format("Assertion failed: {}", message);
                    else
                        error = "Assertion failed.";
                }
                break;
            }
            case TokenId::PROTO:
                eat();
                next();  // deprecated
                break;
            case TokenId::ALIAS: {
                eat();
                auto n = identifier("alias");
                if (constants.count(n)) {
                    is_error = 1, error = fmt::format("The name '{}' is already used by a constant.", n);
                    return;
                }
                int v = peek_match("{", 0) ? (int)calculated("ANONYMOUS") : (int)register_or_alias();
                if (v < 0 || v > 15) {
                    is_error = 1;
                    error = "Register index must be in the range [0,F].";
                    return;
                }
                aliases[n] = v;
                break;
            }
            case TokenId::BYTE: {
                eat();
                append(peek_match("{", 0) ? (int)calculated("ANONYMOUS") : value_8bit());
                break;
            }
            case TokenId::POINTER:
            case TokenId::POINTER16: {
                eat();
                int a = peek_match("{", 0) ? (int)calculated("ANONYMOUS") : (int)value_16bit(1, 0);
                instruction(a >> 8, a);
                break;
            }
            case TokenId::POINTER24: {
                eat();
                int a = peek_match("{", 0) ? (int)calculated("ANONYMOUS") : (int)value_24bit(1, 0);
                append(a >> 16);
                instruction(a >> 8, a);
                break;
            }
            case TokenId::ORG: {
                eat();
                here = (peek_match("{", 0) ? RAM_MASK & (int)calculated("ANONYMOUS") : value_16bit(0, 0));
                break;
            }
            case TokenId::CALL: {
                eat();
                immediate(0x20, peek_match("{", 0) ? 0xFFF & (int)calculated("ANONYMOUS") : value_12bit());
                break;
            }
            case TokenId::CONST: {
                eat();
                auto n = identifier("constant");
                if (constants.count(n)) {
                    is_error = 1;
                    error = fmt::format("The name '{}' has already been defined.", n);
                    return;
                }
                constants.insert_or_assign(n, value_constant());
                break;
            }
            case TokenId::CALC: {
                eat();
                auto n = identifier("calculated constant");
                auto iter = constants.find(n);
                if (iter != constants.end() && !iter->second.isMutable) {
                    is_error = 1, error = fmt::format("Cannot redefine the name '{}' with :calc.", n);
                    return;
                }
                constants.insert_or_assign(n, Constant{calculated(n), true});
                break;
            }
            case TokenId::SEMICOLON:
            case TokenId::RETURN:
                eat(), instruction(0x00, 0xEE);
                break;
            case TokenId::CLEAR:
                eat(), instruction(0x00, 0xE0);
                break;
            case TokenId::BCD:
                eat(), instruction(0xF0 | register_or_alias(), 0x33);
                break;
            case TokenId::DELAY:
                eat(), expect(":="), instruction(0xF0 | register_or_alias(), 0x15);
                break;
            case TokenId::BUZZER:
                eat(), expect(":="), instruction(0xF0 | register_or_alias(), 0x18);
                break;
            case TokenId::PITCH:
                eat(), expect(":="), instruction(0xF0 | register_or_alias(), 0x3A);
                break;
            case TokenId::JUMP0:
                eat(), immediate(0xB0, value_12bit());
                break;
            case TokenId::JUMP:
                eat(), immediate(0x10, value_12bit());
                break;
            case TokenId::NATIVE:
                eat(), immediate(0x00, value_12bit());
                break;
            case TokenId::AUDIO:
                eat(), instruction(0xF0, 0x02);
                break;
            case TokenId::SCROLL_DOWN:
                eat(), instruction(0x00, 0xC0 | value_4bit());
                break;
            case TokenId::SCROLL_UP:
                eat(), instruction(0x00, 0xD0 | value_4bit());
                break;
            case TokenId::SCROLL_RIGHT:
                eat(), instruction(0x00, 0xFB);
                break;
            case TokenId::SCROLL_LEFT:
                eat(), instruction(0x00, 0xFC);
                break;
            case TokenId::EXIT:
                eat(), instruction(0x00, 0xFD);
                break;
            case TokenId::LORES:
                eat(), instruction(0x00, 0xFE);
                break;
            case TokenId::HIRES:
                eat(), instruction(0x00, 0xFF);
                break;
            case TokenId::SPRITE: {
                eat();
                int x = register_or_alias(), y = register_or_alias();
                instruction(0xD0 | x, (y << 4) | value_4bit());
                break;
            }
            case TokenId::PLANE: {
                eat();
                int n = value_4bit();
                if (n > 15)
                    is_error = 1, error = fmt::format("The plane bitmask must be [0,15], was {}.", n);
                instruction(0xF0 | n, 0x01);
                break;
            }
            case TokenId::SAVEFLAGS:
                eat(), instruction(0xF0 | register_or_alias(), 0x75);
                break;
            case TokenId::LOADFLAGS:
                eat(), instruction(0xF0 | register_or_alias(), 0x85);
                break;
            case TokenId::SAVE: {
                eat();
                int r = register_or_alias();
                if (match("-"))
                    instruction(0x50 | r, (register_or_alias() << 4) | 0x02);
                else
                    instruction(0xF0 | r, 0x55);
                break;
            }
            case TokenId::LOAD: {
                eat();
                int r = register_or_alias();
                if (match("-"))
                    instruction(0x50 | r, (register_or_alias() << 4) | 0x03);
                else
                    instruction(0xF0 | r, 0x65);
                break;
            }
            case TokenId::I_REG: {
                eat();
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
                    is_error = 1, error = fmt::format("{} is not an operator that can target the i register.", t.formatValue(d));
                }
                break;
            }
            case TokenId::IF: {
                eat();
                int index = (peek_match("key", 1) || peek_match("-key", 1)) ? 2 : 3;
                if (peek_match("then", index)) {
                    conditional(0), expect("then");
                }
                else if (peek_match("begin", index)) {
                    conditional(1), expect("begin");
                    branches.push({here, source_line, source_pos, "begin"});
                    instruction(0x00, 0x00);
                }
                else {
                    for (int z = 0; z <= index; z++)
                        if (!is_end())
                            next();
                    is_error = 1;
                    error = "Expected 'then' or 'begin'.";
                }
                break;
            }
            case TokenId::ELSE: {
                eat();
                if (branches.empty()) {
                    is_error = 1;
                    error = "This 'else' does not have a matching 'begin'.";
                    return;
                }
                jump(branches.top().addr, here + 2);
                branches.pop();
                branches.push({here, peek_line, peek_pos, "else"});
                instruction(0x00, 0x00);
                break;
            }
            case TokenId::END: {
                eat();
                if (branches.empty()) {
                    is_error = 1;
                    error = "This 'end' does not have a matching 'begin'.";
                    return;
                }
                jump(branches.top().addr, here);
                branches.pop();
                break;
            }
            case TokenId::LOOP: {
                eat();
                loops.push({here, peek_line, peek_pos, "loop"});
                whiles.push({-1, peek_line, peek_pos, "loop"});
                break;
            }
            case TokenId::WHILE: {
                eat();
                if (loops.empty()) {
                    is_error = 1;
                    error = "This 'while' is not within a loop.";
                    return;
                }
                conditional(1);
                whiles.push({here, peek_line, peek_pos, "while"});
                immediate(0x10, 0);  // forward jump
                break;
            }
            case TokenId::AGAIN: {
                eat();
                if (loops.empty()) {
                    is_error = 1;
                    error = "This 'again' does not have a matching 'loop'.";
                    return;
                }
                immediate(0x10, loops.top().addr);
                loops.pop();
                while (true) {
                    // works as loop always pushes a -1 while, but is it needed?
                    int a = whiles.top().addr;
                    whiles.pop();
                    if (a == -1)
                        break;
                    jump(a, here);
                }
                break;
            }
            case TokenId::MACRO: {
                eat();
                auto n = identifier("macro");
                if (is_error)
                    return;
                if (macros.count(n)) {
                    is_error = 1, error = fmt::format("The name '{}' has already been defined.", n);
                    return;
                }
                auto& m = macros.emplace(n, Macro()).first->second;
                while (!is_error && !is_end() && !peek_match("{", 0))
                    m.args.push_back(identifier("macro argument"));
                macro_body("macro", n, m);
                break;
            }
            case TokenId::STRINGMODE: {
                eat();
                auto n = identifier("stringmode");
                if (is_error)
                    return;
                auto& s = stringModes.try_emplace(n, StringMode()).first->second;
                int alpha_base = source_pos, alpha_quote = peek_char() == '"';
                auto alphabet = string();
                Macro m;  // every stringmode needs its own copy of this
                macro_body("string mode", n, m);
                for (int z = 0; z < alphabet.length(); z++) {
                    int c = 0xFF & alphabet[z];
                    if (s.modes[c]) {
                        error_pos = alpha_base + z + (alpha_quote ? 1 : 0);
                        is_error = 1, error = fmt::format("String mode '{}' is already defined for the character '{:c}'.", n, c);
                        break;
                    }
                    s.values[c] = (char)z;
                    auto mm = std::make_unique<Macro>();
                    mm->body = m.body;
                    s.modes[c] = std::move(mm);
                }
                break;
            }
            default: {
                auto t = peek();
                if (is_error)
                    return;
                if (t.type == Token::Type::NUMBER) {
                    int n = (int)t.num_value;
                    next();
                    if (n < -128 || n > 255) {
                        is_error = 1, error = fmt::format("Literal value '{}' does not fit in a byte- must be in range [-128,255].", n);
                    }
                    append(n);
                    return;
                }
                auto n = t.type == Token::Type::STRING ? t.str_value : std::string_view();
                if (auto mi = macros.find(n); mi != macros.end()) {
                    next();
                    auto& m = mi->second;
                    std::unordered_map<std::string_view, Token> bindings;  // name -> tok
                    bindings.emplace("CALLS", Token(m.calls++));
                    for (auto& arg : m.args) {
                        if (is_end()) {
                            error_line = source_line, error_pos = source_pos;
                            is_error = 1, error = fmt::format("Not enough arguments for expansion of macro '{}'.", n);
                            break;
                        }
                        bindings.emplace(arg, next());
                    }
                    int splice_index = 0;
                    for (int z = 0; z < m.body.size(); z++) {
                        auto& bt = m.body[z];
                        auto argIter = (bt.type == Token::Type::STRING ? bindings.find(bt.str_value) : bindings.end());
                        tokens.insert(tokens.begin() + z, argIter != bindings.end() ? argIter->second : bt);
                    }
                }
                else if (auto iter = stringModes.find(n); iter != stringModes.end()) {
                    next();
                    auto& s = iter->second;
                    int text_base = source_pos, text_quote = peek_char() == '"';
                    auto text = string();
                    int splice_index = 0;
                    for (int tz = 0; tz < text.length(); tz++) {
                        int c = 0xFF & text[tz];
                        if (!s.modes[c]) {
                            error_pos = text_base + tz + (text_quote ? 1 : 0);
                            is_error = 1, error = fmt::format("String mode '{}' is not defined for the character '{:c}'.", n, c);
                            break;
                        }
                        std::unordered_map<std::string_view, Token> bindings;  // name -> tok
                        bindings.emplace("CALLS", Token(s.calls++));           // expansion count
                        bindings.emplace("CHAR", Token(c));                    // ascii value of current char
                        bindings.emplace("INDEX", Token((int)tz));             // index of char in input string
                        bindings.emplace("VALUE", Token(s.values[c]));         // index of char in class alphabet
                        auto& sm = *s.modes[c];
                        for (auto& bt : sm.body) {
                            auto argIter = (bt.type == Token::Type::STRING ? bindings.find(bt.str_value) : bindings.end());
                            tokens.insert(tokens.begin() + splice_index++, argIter != bindings.end() ? argIter->second : bt);
                        }
                    }
                }
                else
                    immediate(0x20, value_12bit());
            }
        }
    }
}
#endif

Program::Program(std::string_view text, int startAddress)
{
    source = text.data();
    source_root = text.data();
    sourceEnd = source + text.length();
    source_line = 0;
    source_pos = 0;
    has_main = 1;
    here = startAddress;
    this->startAddress = startAddress;
    length = 0;
    rom.resize(65536, 0);
    used.resize(65536, 0);
    romLineMap.resize(65536, 0xFFFFFFFF);
    is_error = 0;
    error[0] = '\0';
    error_line = 0;
    error_pos = 0;
    if ((unsigned char)source[0] == 0xEF && (unsigned char)source[1] == 0xBB && (unsigned char)source[2] == 0xBF)
        source += 3;  // UTF-8 BOM
    skip_whitespace();

#define octo_kc(l, n) constants.emplace(("OCTO_KEY_" l), Constant{n, 0})
    octo_kc("1", 0x1), octo_kc("2", 0x2), octo_kc("3", 0x3), octo_kc("4", 0xC), octo_kc("Q", 0x4), octo_kc("W", 0x5), octo_kc("E", 0x6), octo_kc("R", 0xD), octo_kc("A", 0x7), octo_kc("S", 0x8), octo_kc("D", 0x9), octo_kc("F", 0xE), octo_kc("Z", 0xA),
        octo_kc("X", 0x0), octo_kc("C", 0xB), octo_kc("V", 0xF);

    aliases["unpack-hi"] = 0;
    aliases["unpack-lo"] = 1;
}

bool Program::compile()
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
        auto iter = constants.find("main");
        if (iter == constants.end())
            return is_error = 1, error = "This program is missing a 'main' label.", false;
        jump(startAddress, (int)iter->second.value);
    }
    if (!protos.empty()) {
        auto& pr = protos.begin()->second;
        error_line = pr.line, error_pos = pr.pos;
        is_error = 1;
        error = fmt::format("Undefined forward reference: {}", protos.begin()->first);
        return false;
    }
    if (!loops.empty()) {
        is_error = 1;
        error = "This 'loop' does not have a matching 'again'.";
        error_line = loops.top().line, error_pos = loops.top().pos;
        return false;
    }
    if (!branches.empty()) {
        is_error = 1;
        error = fmt::format("This '{}' does not have a matching 'end'.", branches.top().type);
        error_line = branches.top().line, error_pos = branches.top().pos;
        return false;
    }
    return true;  // success!
}


}