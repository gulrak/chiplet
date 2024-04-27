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
    , inMacro{other.inMacro}
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

Token::Group Token::groupId() const
{
    if(type == Type::NUMBER)
        return Group::NUMBER;
    if(type == Type::STRING) {
        if(tid != TokenId::TOK_UNKNOWN) {
            if(tid <= TokenId::GREATER_EQUAL)
                return Group::OPERATOR;
            if(tid <= TokenId::PRE_ASM)
                return Group::PREPROCESSOR;
            if(tid <= TokenId::POINTER24)
                return Group::DIRECTIVE;
            if((str_value.length() == 1 && (str_value[0] == 'i' || str_value[0] == 'I')) || (str_value.length() == 2 && (str_value[0] == 'v' || str_value[0] == 'V') && isxdigit(str_value[1])))
                return Group::REGISTER;
            if(tid == TokenId::STRING_LITERAL)
                return Group::STRING;
        }
    }
    return Group::IDENTIFIER;
}

Lexer::Lexer(std::string_view text, bool emitComments)
: _emitComment(emitComments)
{
    _source = text.data();
    _sourceRoot = text.data();
    _sourceEnd = _source + text.length();
    _sourceLine = 0;
    _sourcePos = 0;
    _isError = 0;
    _error[0] = '\0';
    _errorLine = 0;
    _errorPos = 0;
}

char Lexer::nextChar()
{
    if(_source >= _sourceEnd)
        return 0;
    char c = _source[0];
    if (c == '\n')
        ++_sourceLine, _sourcePos = 0;
    else
        ++_sourcePos;
    ++_source;
    return c;
}

char Lexer::peekChar() const
{
    return _source >= _sourceEnd ? '\0' : _source[0];
}

static FilePos extractFilePos(std::string_view info)
{
    int depth{0};
    int line{0};
    const auto* start = info.data() + 7;  // 7 für '#@line['
    auto result = std::from_chars(start, info.data() + info.size(), depth);
    if (result.ptr == start || *result.ptr != ',')
        return {};
    start = result.ptr + 1;
    result = std::from_chars(start, info.data() + info.size(), line);
    if (result.ptr == start || *result.ptr != ',')
        return {};
    return {std::string(result.ptr + 1, (info.data() + info.size()) - result.ptr - 1), depth, line};
}

void Lexer::skipWhitespace()
{
    while (true) {
        char c = peekChar();
        if (c == '#' && !_emitComment) {  // line comments
            c = nextChar();
            if (c == '@') {
                auto start = _source;
                while (peekChar() != ']' && _source < _sourceEnd)
                    c = nextChar();
                if (c == ']') {
                    auto ep = extractFilePos({start, size_t(_source - start)});
                    /*
                    if (!filePosStack.empty())
                        filePosStack.top().line = fileLine;
                    if (ep.line) {
                        while (!filePosStack.empty() && filePosStack.top().depth > ep.depth)
                            filePosStack.pop();
                        if (filePosStack.empty() || filePosStack.top().depth < ep.depth) {
                            filePosStack.push(ep);
                        }
                        else {
                            filePosStack.top() = ep;
                        }
                        fileLine = ep.line - 1;
                    }
                     */
                }
            }
            while (true) {
                char cc = peekChar();
                if (cc == '\0' || cc == '\n')
                    break;
                nextChar();
            }
        }
        else if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            break;
        nextChar();
    }
}


void Lexer::scanNextToken(octo::Token& t)
{
    std::string strBuffer;
    const auto* start = _source;
    size_t index = 0;
    if (_source[0] == '"') {
        nextChar();
        while (true) {
            char c = nextChar();
            if (c == '\0') {
                _isError = 1;
                _error = "Missing a closing \" in a string literal.";
                _errorLine = _sourceLine, _errorPos = _sourcePos;
                return;
            }
            if (c == '"') {
                nextChar();
                break;
            }
            if (c == '\\') {
                start = nullptr;
                char ec = nextChar();
                if (ec == '\0') {
                    _isError = 1;
                    _error = "Missing a closing \" in a string literal.";
                    _errorLine = _sourceLine, _errorPos = _sourcePos;
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
                    _isError = 1;
                    _error = fmt::format("Unrecognized escape character '{}' in a string literal.", ec);
                    _errorLine = _sourceLine, _errorPos = _sourcePos - 1;
                    return;
                }
            }
            else {
                strBuffer.push_back(c);
            }
        }
        t.type = Token::Type::STRING;
        t.tid = TokenId::STRING_LITERAL;
        if(start) {
            t.str_value = std::string_view(start + 1, strBuffer.length());
        }
        else {
            t.str_container = std::move(strBuffer);
            t.str_value = t.str_container;
        }
    }
    else if(_emitComment && _source[0] == '#') {
        while (true) {
            char c = nextChar();
            if (c == '\r' || c == '\n' || c == '\0')
                break;
            ++index;
        }
        t.type = Token::Type::COMMENT;
        t.str_value = {start, index};
    }
    else {
        // string or number
        while (true) {
            char c = nextChar();
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
#ifdef OCTO_STRICT_PARSING
            if(t.type != Token::Type::NUMBER) {
                _isError = true;
                error = fmt::format("Expected a valid number, but found '{}'.", std::string_view(start, index));
                error_line = source_line, error_pos = static_cast<int>(source_pos - index);
            }
#else
            if(t.type != Token::Type::NUMBER) {
                t.type = Token::Type::STRING;
                t.str_value = {start, index};
                auto iter = lexerTokenMap.find(t.str_value);
                if (iter != lexerTokenMap.end())
                    t.tid = iter->second;
                else
                    t.tid = TokenId::TOK_UNKNOWN;
            }
#endif
        }
        else {
            t.type = Token::Type::STRING;
            t.str_value = {start, index};
            auto iter = lexerTokenMap.find(t.str_value);
            if (iter != lexerTokenMap.end())
                t.tid = iter->second;
            else
                t.tid = TokenId::TOK_UNKNOWN;
        }
        // this is handy for debugging internal errors:
        // if (t->type==Token::Type::STRING) printf("RAW TOKEN: %p %s\n", (void*)t->str_value, t->str_value);
        // if (t->type==Token::Type::NUMBER) printf("RAW TOKEN: %f\n", t->num_value);
    }
    skipWhitespace();
}

Program::~Program()
{
}

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

int Program::isEnd() const
{
    return tokens.empty() && _source >= _sourceEnd;
}

void Program::fetchToken()
{
    if (isEnd()) {
        _isError = 1;
        _error = "Unexpected EOF.";
        return;
    }
    if (_isError)
        return;
    tokens.emplace_back(_sourceLine, _sourcePos);
    auto& t = tokens.back();
    scanNextToken(t);
}

Token Program::next()
{
    if (tokens.empty())
        fetchToken();
    if (_isError)
        return {_sourceLine, _sourcePos};
    auto t = tokens.front();
    tokens.pop_front();
    _errorLine = t.line, _errorPos = t.pos;
    return t;
}

Token Program::peek()
{
    if (tokens.empty())
        fetchToken();
    if (_isError)
        return {_sourceLine, _sourcePos};
    return tokens.front();
}

bool Program::peekMatch(const std::string_view& name, int index)
{
    while (!_isError && !isEnd() && tokens.size() < index + 1)
        fetchToken();
    if (isEnd() || _isError)
        return false;
    return tokens[index].type == Token::Type::STRING && tokens[index].str_value == name;
}

inline bool Program::match(const std::string_view& name)
{
    if (peekMatch(name, 0)) {
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


bool Program::isReserved(std::string_view name)
{
    return lexerTokenMap.count(name);
}

bool Program::checkName(std::string_view name, const char* kind)
{
    if (_isError)
        return false;
    if (strncmp("OCTO_", name.data(), 5) == 0 || isReserved(name)) {
        _isError = 1, _error = fmt::format("The name '{}' is reserved and cannot be used for a {}.", name, kind);
        return false;
    }
    return true;
}

std::string_view Program::string()
{
    if (_isError)
        return "";
    auto t = next();
    if (t.type != Token::Type::STRING) {
        _isError = 1, _error = fmt::format("Expected a string, got {}.", (int)t.num_value);
        return "";
    }
    return t.str_value;
}

std::string_view Program::identifier(const char* kind)
{
    if (_isError)
        return "";
    auto t = next();
    if (t.type != Token::Type::STRING) {
        _isError = 1, _error = fmt::format("Expected a name for a {}, got {}.", kind, (int)t.num_value);
        return "";
    }
    if (!checkName(t.str_value, kind))
        return "";
    return t.str_value;
}

void Program::expect(std::string_view name)
{
    if (_isError)
        return;
    auto t = next();
    if (t.type != Token::Type::STRING || t.str_value != name) {
        char d[256];
        _isError = 1, _error = fmt::format("Expected {}, got {}.", name, t.formatValue(d));
    }
}

bool Program::isRegister(std::string_view name)
{
    if (_aliases.count(name))
        return true;
    if (name.length() != 2)
        return false;
    if (name[0] != 'v' && name[0] != 'V')
        return false;
    return isxdigit(name[1]);
}

bool Program::peekIsRegister()
{
    auto t = peek();
    return t.type == Token::Type::STRING && isRegister(t.str_value);
}

int Program::registerOrAlias()
{
    if (_isError)
        return 0;
    auto t = next();
    if (t.type != Token::Type::STRING || !isRegister(t.str_value)) {
        char d[256];
        _isError = 1, _error = fmt::format("Expected register, got {}.", t.formatValue(d));
        return 0;
    }
    auto iter = _aliases.find(t.str_value);
    if (iter != _aliases.end())
        return iter->second;
    char c = static_cast<char>(std::tolower(t.str_value[1]));
    return isdigit(c) ? c - '0' : 10 + (c - 'a');
}

bool Program::isRegisterAlias(std::string_view name) const
{
    return _aliases.find(name) != _aliases.end();
}

int Program::valueRange(int n, int mask)
{
    if (mask == 0xF && (n < 0 || n > mask))
        _isError = 1, _error = fmt::format("Argument {} does not fit in 4 bits- must be in range [0,15].", n);
    if (mask == 0xFF && (n < -128 || n > mask))
        _isError = 1, _error = fmt::format("Argument {} does not fit in a byte- must be in range [-128,255].", n);
    if (mask == 0xFFF && (n < 0 || n > mask))
        _isError = 1, _error = fmt::format("Argument {} does not fit in 12 bits.", n);
    if (mask == 0xFFFF && (n < 0 || n > mask))
        _isError = 1, _error = fmt::format("Argument {} does not fit in 16 bits.", n);
    if (mask == 0xFFFFFF && (n < 0 || n > mask))
        _isError = 1, _error = fmt::format("Argument {} does not fit in 24 bits.", n);
    return n & mask;
}

void Program::valueFail(const std::string_view& w, const std::string_view& n, bool undef)
{
    if (_isError)
        return;
    if (isRegister(n))
        _isError = 1, _error = fmt::format("Expected {} value, but found the register {}.", w, n);
    else if (isReserved(n))
        _isError = 1, _error = fmt::format("Expected {} value, but found the keyword '{}'. Missing a token?", w, n);
    else if (undef)
        _isError = 1, _error = fmt::format("Expected {} value, but found the undefined name '{}'.", w, n);
}

int Program::value4Bit()
{
    if (_isError)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return valueRange((int)t.num_value, 0xF);
    }
    auto& n = t.str_value;
    auto iter = _constants.find(n);
    if (iter != _constants.end())
        return valueRange((int)iter->second.value, 0xF);
    return valueFail("a 4-bit", n, true), 0;
}

int Program::value8Bit()
{
    if (_isError)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return valueRange((int)t.num_value, 0xFF);
    }
    auto& n = t.str_value;
    auto iter = _constants.find(t.str_value);
    if (iter != _constants.end())
        return valueRange((int)iter->second.value, 0xFF);
    return valueFail("an 8-bit", t.str_value, true), 0;
}

int Program::value12Bit()
{
    if (_isError)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return valueRange((int)t.num_value, 0xFFF);
    }
    auto& n = t.str_value;
    int protoLine = t.line, protoPos = t.pos;
    auto iter = _constants.find(n);
    if (iter != _constants.end())
        return valueRange((int)iter->second.value, 0xFFF);
    valueFail("a 12-bit", n, false);
    if (_isError)
        return 0;
    if (!checkName(n, "label"))
        return 0;
    addProtoRef(n, protoLine, protoPos, _here, 12);
    return 0;
}

int Program::value16Bit(int can_forward_ref, int offset)
{
    if (_isError)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return valueRange((int)t.num_value, 0xFFFF);
    }
    auto& n = t.str_value;
    int protoLine = t.line, protoPos = t.pos;
    auto iter = _constants.find(n);
    if (iter != _constants.end())
        return valueRange((int)iter->second.value, 0xFFFF);
    valueFail("a 16-bit", n, false);
    if (_isError)
        return 0;
    if (!checkName(n, "label"))
        return 0;
    if (!can_forward_ref) {
        _isError = 1, _error = fmt::format("The reference to '{}' may not be forward-declared.", n);
        return 0;
    }
    addProtoRef(n, protoLine, protoPos, _here + offset, 16);
    return 0;
}

int Program::value24Bit(int can_forward_ref, int offset)
{
    if (_isError)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return valueRange((int)t.num_value, 0xFFFFFF);
    }
    auto& n = t.str_value;
    int protoLine = t.line, protoPos = t.pos;
    auto iter = _constants.find(n);
    if (iter != _constants.end())
        return valueRange((int)iter->second.value, 0xFFFFFF);
    valueFail("a 24-bit", n, false);
    if (_isError)
        return 0;
    if (!checkName(n, "label"))
        return 0;
    if (!can_forward_ref) {
        _isError = 1, _error = fmt::format("The reference to '{}' may not be forward-declared.", n);
        return 0;
    }
    addProtoRef(n, protoLine, protoPos, _here + offset, 24);
    return 0;
}

void Program::addProtoRef(std::string_view name, int line, int pos, int where, int8_t size)
{
    auto iter = _protos.find(name);
    if (iter == _protos.end()) {
        iter = _protos.emplace(name, Prototype{line, pos}).first;
    }
    iter->second.addrs.push_back({where, size});
}

Constant Program::valueConstant()
{
    auto t = next();
    if (_isError)
        return {0, false};
    if (t.type == Token::Type::NUMBER) {
        return {static_cast<double>((int)t.num_value), false};
    }
    auto& n = t.str_value;
    auto iter = _constants.find(n);
    if (iter != _constants.end())
        return {iter->second.value, false};
    if (_protos.count(n))
        _isError = 1, _error = fmt::format("A constant reference to '{}' may not be forward-declared.", n);
    valueFail("a constant", n, true);
    return {0, false};
}

void Program::macroBody(const std::string_view& desc, const std::string_view& name, Macro& m)
{
    if (_isError)
        return;
    expect("{");
    if (_isError) {
        _error = fmt::format("Expected '{{' for definition of {} '{}'.", desc, name);
        return;
    }
    int depth = 1;
    while (!isEnd()) {
        auto t = peek();
        if (t.type == Token::Type::STRING && t.str_value == "{")
            depth++;
        if (t.type == Token::Type::STRING && t.str_value == "}")
            depth--;
        if (depth == 0)
            break;
        m.body.push_back(next());
        m.body.back().inMacro = true;
    }
    expect("}");
    if (_isError)
        _error = fmt::format("Expected '}}' for definition of {} '{}'.", desc, name);
}

//-----------------------------------------------------------
// Compile-time Calculation
//-----------------------------------------------------------

double Program::calcTerminal(std::string_view name)
{
    // NUMBER | CONSTANT | LABEL | VREGISTER | '(' expression ')'
    if (peekIsRegister())
        return registerOrAlias();
    if (match("PI"))
        return 3.141592653589793;
    if (match("E"))
        return 2.718281828459045;
    if (match("HERE"))
        return _here;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        double r = t.num_value;
        return r;
    }
    auto& n = t.str_value;
    if (_protos.count(n)) {
        _isError = 1, _error = fmt::format("Cannot use forward declaration '{}' when calculating constant '{}'.", n, name);
        return 0;
    }
    auto iter = _constants.find(n);
    if (iter != _constants.end())
        return iter->second.value;
    if (n != "(") {
        _isError = 1, _error = fmt::format("Found undefined name '{}' when calculating constant '{}'.", n, name);
        return 0;
    }
    double r = calcExpr(name);
    expect(")");
    return r;
}

double Program::calcExpr(std::string_view name)
{
    // UNARY expression
    if (match("strlen"))
        return (double)string().length();
    if (match("-"))
        return -calcExpr(name);
    if (match("~"))
        return ~((int)calcExpr(name));
    if (match("!"))
        return !((int)calcExpr(name));
    if (match("sin"))
        return sin(calcExpr(name));
    if (match("cos"))
        return cos(calcExpr(name));
    if (match("tan"))
        return tan(calcExpr(name));
    if (match("exp"))
        return exp(calcExpr(name));
    if (match("log"))
        return log(calcExpr(name));
    if (match("abs"))
        return fabs(calcExpr(name));
    if (match("sqrt"))
        return sqrt(calcExpr(name));
    if (match("sign"))
        return sign(calcExpr(name));
    if (match("ceil"))
        return ceil(calcExpr(name));
    if (match("floor"))
        return floor(calcExpr(name));
    if (match("@")) {
        auto addr = (int)calcExpr(name);
        return addr >= 0 && addr < _rom.size() ? 0xFF & _rom[addr] : 0;
    }

    // expression BINARY expression
    double r = calcTerminal(name);
    if (match("-"))
        return r - calcExpr(name);
    if (match("+"))
        return r + calcExpr(name);
    if (match("*"))
        return r * calcExpr(name);
    if (match("/"))
        return r / calcExpr(name);
    if (match("%"))
        return ((int)r) % ((int)calcExpr(name));
    if (match("&"))
        return ((int)r) & ((int)calcExpr(name));
    if (match("|"))
        return ((int)r) | ((int)calcExpr(name));
    if (match("^"))
        return ((int)r) ^ ((int)calcExpr(name));
    if (match("<<"))
        return ((int)r) << ((int)calcExpr(name));
    if (match(">>"))
        return ((int)r) >> ((int)calcExpr(name));
    if (match("pow"))
        return pow(r, calcExpr(name));
    if (match("min"))
        return min(r, calcExpr(name));
    if (match("max"))
        return max(r, calcExpr(name));
    if (match("<"))
        return r < calcExpr(name);
    if (match(">"))
        return r > calcExpr(name);
    if (match("<="))
        return r <= calcExpr(name);
    if (match(">="))
        return r >= calcExpr(name);
    if (match("=="))
        return r == calcExpr(name);
    if (match("!="))
        return r != calcExpr(name);
    // terminal
    return r;
}

double Program::calculated(std::string_view name)
{
    expect("{");
    double r = calcExpr(name);
    expect("}");
    return r;
}

//-----------------------------------------------------------
//  ROM construction
//-----------------------------------------------------------

void Program::append(uint8_t byte)
{
    if (_isError)
        return;
    if (_here >= RAM_MAX) {
        _isError = 1;
        _error = "Supported ROM space is full (16MB).";
        return;
    }
    if (_here >= _rom.size()) {
        if (_rom.size() < 1024 * 1024) {
            _rom.resize(1024 * 1024, 0);
            _used.resize(1024 * 1024, 0);
            _romLineMap.resize(1024 * 1024, 0xFFFFFFFF);
        }
        else if (_rom.size() < RAM_MAX / 2) {
            _rom.resize(RAM_MAX / 2, 0);
            _used.resize(RAM_MAX / 2, 0);
            _romLineMap.resize(RAM_MAX / 2, 0xFFFFFFFF);
        }
        else if (_rom.size() < RAM_MAX) {
            _rom.resize(RAM_MAX, 0);
            _used.resize(RAM_MAX, 0);
            _romLineMap.resize(RAM_MAX, 0xFFFFFFFF);
        }
    }
    if (_here > _startAddress && _used[_here]) {
        _isError = 1;
        _error = fmt::format("Data overlap. Address 0x{:0X} has already been defined.", _here);
        return;
    }
    _romLineMap[_here] = _tokenStart + 1;
    _rom[_here] = byte, _used[_here] = 1, _here++;
    if (_here > _length)
        _length = _here;
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
    if (_isError)
        return;
    _rom[addr] = (0x10 | ((dest >> 8) & 0xF)), _used[addr] = 1;
    _rom[addr + 1] = (dest & 0xFF), _used[addr + 1] = 1;
}

//-----------------------------------------------------------
//  The Compiler proper
//-----------------------------------------------------------

void Program::pseudoConditional(int reg, int sub, int comp)
{
    if (peekIsRegister())
        instruction(0x8F, registerOrAlias() << 4);
    else
        instruction(0x6F, value8Bit());
    instruction(0x8F, (reg << 4) | sub);
    instruction(comp, 0);
}

void Program::conditional(int negated)
{
    int reg = registerOrAlias();
    auto t = peek();
    char d[256];
    t.formatValue(d);
    if (_isError)
        return;
    auto n = string();

#define octo_ca(pos, neg) (n == (negated ? (neg) : (pos)))

    if (octo_ca("==", "!=")) {
        if (peekIsRegister())
            instruction(0x90 | reg, registerOrAlias() << 4);
        else
            instruction(0x40 | reg, value8Bit());
    }
    else if (octo_ca("!=", "==")) {
        if (peekIsRegister())
            instruction(0x50 | reg, registerOrAlias() << 4);
        else
            instruction(0x30 | reg, value8Bit());
    }
    else if (octo_ca("key", "-key"))
        instruction(0xE0 | reg, 0xA1);
    else if (octo_ca("-key", "key"))
        instruction(0xE0 | reg, 0x9E);
    else if (octo_ca(">", "<="))
        pseudoConditional(reg, 0x5, 0x4F);
    else if (octo_ca("<", ">="))
        pseudoConditional(reg, 0x7, 0x4F);
    else if (octo_ca(">=", "<"))
        pseudoConditional(reg, 0x7, 0x3F);
    else if (octo_ca("<=", ">"))
        pseudoConditional(reg, 0x5, 0x3F);
    else {
        _isError = 1, _error = fmt::format("Expected conditional operator, got {}.", d);
    }
}

void Program::resolveLabel(int offset)
{
    int target = (_here) + offset;
    auto n = identifier("label");
    if (_isError)
        return;
    if (_constants.count(n)) {
        _isError = 1, _error = fmt::format("The name '{}' has already been defined.", n);
        return;
    }
    if (_aliases.count(n)) {
        _isError = 1, _error = fmt::format("The name '{}' is already used by an alias.", n);
        return;
    }
    if ((target == _startAddress + 2 || target == _startAddress) && (n == "main")) {
        _hasMain = 0, _here = target = _startAddress;
        _rom[_startAddress] = 0, _used[_startAddress] = 0;
        _rom[_startAddress + 1] = 0, _used[_startAddress + 1] = 0;
    }
    _constants.insert_or_assign(n, Constant{static_cast<double>(target), false});
    auto iter = _protos.find(n);
    if (iter == _protos.end())
        return;

    auto& pr = iter->second;
    for (auto& pa : pr.addrs) {
        if (pa.size == 16 && (_rom[pa.value] & 0xF0) == 0x60) {  // :unpack long target
            _rom[pa.value + 1] = target >> 8;
            _rom[pa.value + 3] = target;
        }
        else if (pa.size == 16) {  // i := long target
            _rom[pa.value] = target >> 8;
            _rom[pa.value + 1] = target;
        }
        else if (pa.size <= 12 && (target & 0xFFF) != target) {
            _isError = 1, _error = fmt::format("Value 0x{:0X} for label '{}' does not fit in 12 bits.", target, n);
            break;
        }
        else if (pa.size <= 16 && (target & 0xFFFF) != target) {
            _isError = 1, _error = fmt::format("Value 0x{:0X} for label '{}' does not fit in 16 bits.", target, n);
            break;
        }
        else if (pa.size <= 24 && (target & 0xFFFFFF) != target) {
            _isError = 1, _error = fmt::format("Value 0x{:0X} for label '{}' does not fit in 24 bits.", target, n);
            break;
        }
        else if (pa.size == 24) {
            _rom[pa.value] = target >> 16;
            _rom[pa.value + 1] = target >> 8;
            _rom[pa.value + 2] = target;
        }
        else if ((_rom[pa.value] & 0xF0) == 0x60) {  // :unpack target
            _rom[pa.value + 1] = ((_rom[pa.value + 1]) & 0xF0) | ((target >> 8) & 0xF);
            _rom[pa.value + 3] = target;
        }
        else {
            _rom[pa.value] = ((_rom[pa.value]) & 0xF0) | ((target >> 8) & 0xF);
            _rom[pa.value + 1] = target;
        }
    }
    _protos.erase(n);
}

void Program::compileStatement()
{
    if (_isError)
        return;
    if(!peek().inMacro)
        _tokenStart = peek().line;
    int peek_line = peek().line, peek_pos = peek().pos;
    if (peekIsRegister()) {
        int r = registerOrAlias();
        if (match(":=")) {
            if (peekIsRegister())
                instruction(0x80 | r, (registerOrAlias() << 4) | 0x0);
            else if (match("random"))
                instruction(0xC0 | r, value8Bit());
            else if (match("key"))
                instruction(0xF0 | r, 0x0A);
            else if (match("delay"))
                instruction(0xF0 | r, 0x07);
            else
                instruction(0x60 | r, value8Bit());
        }
        else if (match("+=")) {
            if (peekIsRegister())
                instruction(0x80 | r, (registerOrAlias() << 4) | 0x4);
            else
                instruction(0x70 | r, value8Bit());
        }
        else if (match("-=")) {
            if (peekIsRegister())
                instruction(0x80 | r, (registerOrAlias() << 4) | 0x5);
            else
                instruction(0x70 | r, 1 + ~value8Bit());
        }
        else if (match("|="))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0x1);
        else if (match("&="))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0x2);
        else if (match("^="))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0x3);
        else if (match("=-"))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0x7);
        else if (match(">>="))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0x6);
        else if (match("<<="))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0xE);
        else {
            auto t = next();
            char d[256];
            if (!_isError)
                _isError = 1, _error = fmt::format("Unrecognized operator {}.", t.formatValue(d));
        }
    }
    else {
        if (!_isError && !isEnd() && tokens.empty())
            fetchToken();
        if (isEnd() || _isError)
            return;
        switch (tokens.front().tid) {
            case TokenId::COLON:
                eat();
                resolveLabel(0);
                break;
            case TokenId::NEXT:
                eat();
                resolveLabel(1);
                break;
            case TokenId::UNPACK: {
                eat();
                int a = 0;
                if (match("long")) {
                    a = value16Bit(1, 0);
                }
                else {
                    int v = value4Bit();
                    a = (v << 12) | value12Bit();
                }
                auto rh = _aliases["unpack-hi"];
                auto rl = _aliases["unpack-lo"];
                instruction(0x60 | rh, a >> 8);
                instruction(0x60 | rl, a);
                break;
            }
            case TokenId::BREAKPOINT:
                eat();
                _breakpoints[_here] = string().data();
                break;
            case TokenId::MONITOR: {
                eat();
                char n[256];
                int type, base, len;
                std::string format;
                peek().formatValue(n);
                if (peekIsRegister()) {
                    type = 0;  // register monitor
                    base = registerOrAlias();
                    if (peek().type == Token::Type::NUMBER)
                        len = value4Bit();
                    else
                        len = -1, format = string();
                }
                else {
                    type = 1;  // memory monitor
                    base = value16Bit(0, 0);
                    if (peek().type == Token::Type::NUMBER)
                        len = value16Bit(0, 0);
                    else
                        len = -1, format = string();
                }
                if (n[strlen(n) - 1] == '\'')
                    n[strlen(n) - 1] = '\0';
                auto nn = safeStringStringView(n[0] == '\'' ? n + 1 : n);
                _monitors.insert_or_assign(nn, Monitor{type, base, len, format});
                break;
            }
            case TokenId::ASSERT: {
                eat();
                auto message = peekMatch("{", 0) ? std::string_view() : string();
                if (!(int)calculated("assertion")) {
                    _isError = 1;
                    if (!message.empty())
                        _error = fmt::format("Assertion failed: {}", message);
                    else
                        _error = "Assertion failed.";
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
                if (_constants.count(n)) {
                    _isError = 1, _error = fmt::format("The name '{}' is already used by a constant.", n);
                    return;
                }
                int v = peekMatch("{", 0) ? (int)calculated("ANONYMOUS") : (int)registerOrAlias();
                if (v < 0 || v > 15) {
                    _isError = 1;
                    _error = "Register index must be in the range [0,F].";
                    return;
                }
                _aliases[n] = v;
                break;
            }
            case TokenId::BYTE: {
                eat();
                append(peekMatch("{", 0) ? (int)calculated("ANONYMOUS") : value8Bit());
                break;
            }
            case TokenId::POINTER:
            case TokenId::POINTER16: {
                eat();
                int a = peekMatch("{", 0) ? (int)calculated("ANONYMOUS") : (int)value16Bit(1, 0);
                instruction(a >> 8, a);
                break;
            }
            case TokenId::POINTER24: {
                eat();
                int a = peekMatch("{", 0) ? (int)calculated("ANONYMOUS") : (int)value24Bit(1, 0);
                append(a >> 16);
                instruction(a >> 8, a);
                break;
            }
            case TokenId::ORG: {
                eat();
                _here = (peekMatch("{", 0) ? RAM_MASK & (int)calculated("ANONYMOUS") : value16Bit(0, 0));
                break;
            }
            case TokenId::CALL: {
                eat();
                immediate(0x20, peekMatch("{", 0) ? 0xFFF & (int)calculated("ANONYMOUS") : value12Bit());
                break;
            }
            case TokenId::CONST: {
                eat();
                auto n = identifier("constant");
                if (_constants.count(n)) {
                    _isError = 1;
                    _error = fmt::format("The name '{}' has already been defined.", n);
                    return;
                }
                _constants.insert_or_assign(n, valueConstant());
                break;
            }
            case TokenId::CALC: {
                eat();
                auto n = identifier("calculated constant");
                auto iter = _constants.find(n);
                if (iter != _constants.end() && !iter->second.isMutable) {
                    _isError = 1, _error = fmt::format("Cannot redefine the name '{}' with :calc.", n);
                    return;
                }
                _constants.insert_or_assign(n, Constant{calculated(n), true});
                break;
            }
            case TokenId::PRE_ASM:
            case TokenId::PRE_CONFIG:
            case TokenId::PRE_DUMP_OPTIONS:
            case TokenId::PRE_ELSE:
            case TokenId::PRE_END:
            case TokenId::PRE_IF:
            case TokenId::PRE_INCLUDE:
            case TokenId::PRE_SEGMENT:
            case TokenId::PRE_UNLESS:
                _isError = 1, _error = fmt::format("Found preprocessor directive '{}' during assembly.", tokens.front().str_value);
                eat();
                break;
            case TokenId::SEMICOLON:
            case TokenId::RETURN:
                eat(), instruction(0x00, 0xEE);
                break;
            case TokenId::CLEAR:
                eat(), instruction(0x00, 0xE0);
                break;
            case TokenId::BCD:
                eat(), instruction(0xF0 | registerOrAlias(), 0x33);
                break;
            case TokenId::DELAY:
                eat(), expect(":="), instruction(0xF0 | registerOrAlias(), 0x15);
                break;
            case TokenId::BUZZER:
                eat(), expect(":="), instruction(0xF0 | registerOrAlias(), 0x18);
                break;
            case TokenId::PITCH:
                eat(), expect(":="), instruction(0xF0 | registerOrAlias(), 0x3A);
                break;
            case TokenId::JUMP0:
                eat(), immediate(0xB0, value12Bit());
                break;
            case TokenId::JUMP:
                eat(), immediate(0x10, value12Bit());
                break;
            case TokenId::NATIVE:
                eat(), immediate(0x00, value12Bit());
                break;
            case TokenId::AUDIO:
                eat(), instruction(0xF0, 0x02);
                break;
            case TokenId::SCROLL_DOWN:
                eat(), instruction(0x00, 0xC0 | value4Bit());
                break;
            case TokenId::SCROLL_UP:
                eat(), instruction(0x00, 0xD0 | value4Bit());
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
                int x = registerOrAlias(), y = registerOrAlias();
                instruction(0xD0 | x, (y << 4) | value4Bit());
                break;
            }
            case TokenId::PLANE: {
                eat();
                int n = value4Bit();
                if (n > 15)
                    _isError = 1, _error = fmt::format("The plane bitmask must be [0,15], was {}.", n);
                instruction(0xF0 | n, 0x01);
                break;
            }
            case TokenId::SAVEFLAGS:
                eat(), instruction(0xF0 | registerOrAlias(), 0x75);
                break;
            case TokenId::LOADFLAGS:
                eat(), instruction(0xF0 | registerOrAlias(), 0x85);
                break;
            case TokenId::SAVE: {
                eat();
                int r = registerOrAlias();
                if (match("-"))
                    instruction(0x50 | r, (registerOrAlias() << 4) | 0x02);
                else
                    instruction(0xF0 | r, 0x55);
                break;
            }
            case TokenId::LOAD: {
                eat();
                int r = registerOrAlias();
                if (match("-"))
                    instruction(0x50 | r, (registerOrAlias() << 4) | 0x03);
                else
                    instruction(0xF0 | r, 0x65);
                break;
            }
            case TokenId::I_REG: {
                eat();
                if (match(":=")) {
                    if (match("long")) {
                        int a = value16Bit(1, 2);
                        instruction(0xF0, 0x00);
                        instruction((a >> 8), a);
                    }
                    else if (match("hex"))
                        instruction(0xF0 | registerOrAlias(), 0x29);
                    else if (match("bighex"))
                        instruction(0xF0 | registerOrAlias(), 0x30);
                    else
                        immediate(0xA0, value12Bit());
                }
                else if (match("+="))
                    instruction(0xF0 | registerOrAlias(), 0x1E);
                else {
                    auto t = next();
                    char d[256];
                    _isError = 1, _error = fmt::format("{} is not an operator that can target the i register.", t.formatValue(d));
                }
                break;
            }
            case TokenId::IF: {
                eat();
                int index = (peekMatch("key", 1) || peekMatch("-key", 1)) ? 2 : 3;
                if (peekMatch("then", index)) {
                    conditional(0), expect("then");
                }
                else if (peekMatch("begin", index)) {
                    conditional(1), expect("begin");
                    _branches.push({_here, _sourceLine, _sourcePos, "begin"});
                    instruction(0x00, 0x00);
                }
                else {
                    for (int z = 0; z <= index; z++)
                        if (!isEnd())
                            next();
                    _isError = 1;
                    _error = "Expected 'then' or 'begin'.";
                }
                break;
            }
            case TokenId::ELSE: {
                eat();
                if (_branches.empty()) {
                    _isError = 1;
                    _error = "This 'else' does not have a matching 'begin'.";
                    return;
                }
                jump(_branches.top().addr, _here + 2);
                _branches.pop();
                _branches.push({_here, peek_line, peek_pos, "else"});
                instruction(0x00, 0x00);
                break;
            }
            case TokenId::END: {
                eat();
                if (_branches.empty()) {
                    _isError = 1;
                    _error = "This 'end' does not have a matching 'begin'.";
                    return;
                }
                jump(_branches.top().addr, _here);
                _branches.pop();
                break;
            }
            case TokenId::LOOP: {
                eat();
                _loops.push({_here, peek_line, peek_pos, "loop"});
                _whiles.push({-1, peek_line, peek_pos, "loop"});
                break;
            }
            case TokenId::WHILE: {
                eat();
                if (_loops.empty()) {
                    _isError = 1;
                    _error = "This 'while' is not within a loop.";
                    return;
                }
                conditional(1);
                _whiles.push({_here, peek_line, peek_pos, "while"});
                immediate(0x10, 0);  // forward jump
                break;
            }
            case TokenId::AGAIN: {
                eat();
                if (_loops.empty()) {
                    _isError = 1;
                    _error = "This 'again' does not have a matching 'loop'.";
                    return;
                }
                immediate(0x10, _loops.top().addr);
                _loops.pop();
                while (true) {
                    // works as loop always pushes a -1 while, but is it needed?
                    int a = _whiles.top().addr;
                    _whiles.pop();
                    if (a == -1)
                        break;
                    jump(a, _here);
                }
                break;
            }
            case TokenId::MACRO: {
                eat();
                auto n = identifier("macro");
                if (_isError)
                    return;
                if (_macros.count(n)) {
                    _isError = 1, _error = fmt::format("The name '{}' has already been defined.", n);
                    return;
                }
                auto& m = _macros.emplace(n, Macro()).first->second;
                while (!_isError && !isEnd() && !peekMatch("{", 0))
                    m.args.push_back(identifier("macro argument"));
                macroBody("macro", n, m);
                break;
            }
            case TokenId::STRINGMODE: {
                eat();
                auto n = identifier("stringmode");
                if (_isError)
                    return;
                auto& s = _stringModes.try_emplace(n, StringMode()).first->second;
                int alpha_base = _sourcePos, alpha_quote = peekChar() == '"';
                auto alphabet = string();
                Macro m;  // every stringmode needs its own copy of this
                macroBody("string mode", n, m);
                for (int z = 0; z < alphabet.length(); z++) {
                    int c = 0xFF & alphabet[z];
                    if (s.modes[c]) {
                        _errorPos = alpha_base + z + (alpha_quote ? 1 : 0);
                        _isError = 1, _error = fmt::format("String mode '{}' is already defined for the character '{:c}'.", n, c);
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
                if (_isError)
                    return;
                if (t.type == Token::Type::NUMBER) {
                    int n = (int)t.num_value;
                    next();
                    if (n < -128 || n > 255) {
                        _isError = 1, _error = fmt::format("Literal value '{}' does not fit in a byte- must be in range [-128,255].", n);
                    }
                    append(n);
                    return;
                }
                auto n = t.type == Token::Type::STRING ? t.str_value : std::string_view();
                if (auto mi = _macros.find(n); mi != _macros.end()) {
                    next();
                    auto& m = mi->second;
                    std::unordered_map<std::string_view, Token> bindings;  // name -> tok
                    bindings.emplace("CALLS", Token(m.calls++));
                    for (auto& arg : m.args) {
                        if (isEnd()) {
                            _errorLine = _sourceLine, _errorPos = _sourcePos;
                            _isError = 1, _error = fmt::format("Not enough arguments for expansion of macro '{}'.", n);
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
                else if (auto iter = _stringModes.find(n); iter != _stringModes.end()) {
                    next();
                    auto& s = iter->second;
                    int text_base = _sourcePos, text_quote = peekChar() == '"';
                    auto text = string();
                    int splice_index = 0;
                    for (int tz = 0; tz < text.length(); tz++) {
                        int c = 0xFF & text[tz];
                        if (!s.modes[c]) {
                            _errorPos = text_base + tz + (text_quote ? 1 : 0);
                            _isError = 1, _error = fmt::format("String mode '{}' is not defined for the character '{:c}'.", n, c);
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
                    immediate(0x20, value12Bit());
            }
        }
    }
}

Program::Program(std::string_view text, int startAddress)
: Lexer(text)
{
    _hasMain = 1;
    _here = startAddress;
    _startAddress = startAddress;
    _length = 0;
    _rom.resize(65536, 0);
    _used.resize(65536, 0);
    _romLineMap.resize(65536, 0xFFFFFFFF);

    if ((unsigned char)_source[0] == 0xEF && (unsigned char)_source[1] == 0xBB && (unsigned char)_source[2] == 0xBF)
        _source += 3;  // UTF-8 BOM
    skipWhitespace();

#define octo_kc(l, n) _constants.emplace(("OCTO_KEY_" l), Constant{n, 0})
    octo_kc("1", 0x1), octo_kc("2", 0x2), octo_kc("3", 0x3), octo_kc("4", 0xC), octo_kc("Q", 0x4), octo_kc("W", 0x5), octo_kc("E", 0x6), octo_kc("R", 0xD), octo_kc("A", 0x7), octo_kc("S", 0x8), octo_kc("D", 0x9), octo_kc("F", 0xE), octo_kc("Z", 0xA),
        octo_kc("X", 0x0), octo_kc("C", 0xB), octo_kc("V", 0xF);

    _aliases["unpack-hi"] = 0;
    _aliases["unpack-lo"] = 1;
}

bool Program::compile()
{
    instruction(0x00, 0x00);  // reserve a jump slot for main
    while (!isEnd() && !_isError) {
        _errorLine = _sourceLine;
        _errorPos = _sourcePos;
        compileStatement();
    }
    if (_isError)
        return false;
    while (_length > _startAddress && !_used[_length - 1])
        _length--;
    _errorLine = _sourceLine, _errorPos = _sourcePos;

    if (_hasMain) {
        auto iter = _constants.find("main");
        if (iter == _constants.end())
            return _isError = 1, _error = "This program is missing a 'main' label.", false;
        jump(_startAddress, (int)iter->second.value);
    }
    if (!_protos.empty()) {
        auto& pr = _protos.begin()->second;
        _errorLine = pr.line, _errorPos = pr.pos;
        _isError = 1;
        _error = fmt::format("Undefined forward reference: {}", _protos.begin()->first);
        return false;
    }
    if (!_loops.empty()) {
        _isError = 1;
        _error = "This 'loop' does not have a matching 'again'.";
        _errorLine = _loops.top().line, _errorPos = _loops.top().pos;
        return false;
    }
    if (!_branches.empty()) {
        _isError = 1;
        _error = fmt::format("This '{}' does not have a matching 'end'.", _branches.top().type);
        _errorLine = _branches.top().line, _errorPos = _branches.top().pos;
        return false;
    }
    return true;  // success!
}


}