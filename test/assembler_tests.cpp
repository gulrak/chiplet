//
// Created by Steffen Schümann on 20.02.24.
//
#include <doctest/doctest.h>

#include <numeric>
#include <sstream>

#include "../src/octo/assembler.hpp"
#include "../src/octo/preprocessor.hpp"

using Data = std::vector<uint8_t>;

namespace doctest {
template<> struct
StringMaker<Data>
{
    static String convert(const Data& vec)
    {
        std::ostringstream oss;
        oss << "[";
        if(!vec.empty())
            oss << std::accumulate(std::next(vec.begin()), vec.end(), fmt::format("0x{:02x}", vec[0]), [](const std::string& a, uint8_t b) { return a + "," + fmt::format("0x{:02x}", b); });
        oss << "]";
        return oss.str().c_str();
    }
};
} // namespace doctest

void compileTest(const std::string& source, const std::vector<uint8_t>& result)
{
    auto comp = std::make_unique<octo::Assembler>(source, 0x200);
    CHECK(comp->compile());
    CHECK(!comp->isError());
    CHECK_EQ(comp->codeSize(), result.size());
    CHECK_EQ(result, std::vector<uint8_t>(comp->data(), comp->data() + comp->codeSize()));
}

TEST_SUITE("Assembler")
{
    TEST_CASE("octo lexer preserves flexible identifiers")
    {
        octo::Lexer lex{"foo-bar foo.bar %scratch [v0] 8x5"};
        octo::Token token{0, 0};

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "foo-bar");
        CHECK_EQ(token.tid, octo::TokenId::TOK_UNKNOWN);

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "foo.bar");

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "%scratch");

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "[v0]");

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "8x5");
    }

    TEST_CASE("octo lexer classifies numbers and keywords")
    {
        octo::Lexer lex{": main va := -0x3A 0b1010 loop again"};
        octo::Token token{0, 0};

        lex.scanNextToken(token);
        CHECK_EQ(token.tid, octo::TokenId::COLON);
        CHECK_EQ(token.type, octo::Token::Type::DIRECTIVE);

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "main");

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "va");

        lex.scanNextToken(token);
        CHECK_EQ(token.tid, octo::TokenId::ASSIGN);
        CHECK_EQ(token.type, octo::Token::Type::OPERATOR);

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::NUMBER);
        CHECK_EQ(static_cast<int>(token.numValue), -0x3A);

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::NUMBER);
        CHECK_EQ(static_cast<int>(token.numValue), 0b1010);

        lex.scanNextToken(token);
        CHECK_EQ(token.tid, octo::TokenId::LOOP);
        CHECK_EQ(token.type, octo::Token::Type::KEYWORD);

        lex.scanNextToken(token);
        CHECK_EQ(token.tid, octo::TokenId::AGAIN);
        CHECK_EQ(token.type, octo::Token::Type::KEYWORD);
    }

    TEST_CASE("octo lexer optionally preserves trivia")
    {
        octo::Lexer lex{"# comment\n  : main", {.preprocessorMode = true}};
        octo::Token token{0, 0};

        lex.scanNextToken(token);
        CHECK_EQ(token.tid, octo::TokenId::COLON);
        CHECK_EQ(token.prefix, "# comment\n  ");
        CHECK_EQ(token.rawValue, ":");
        CHECK_EQ(token.line, 1);
        CHECK_EQ(token.pos, 2);
    }

    TEST_CASE("octo preprocessor scanner classifies preprocessor directives")
    {
        octo::Preprocessor preprocessor;
        auto result = preprocessor.scan(":include \"font.8o\"\n: main");

        REQUIRE_GE(result.tokens.size(), 5);
        CHECK_EQ(result.tokens[0].type, octo::Token::Type::PREPROCESSOR);
        CHECK_EQ(result.tokens[0].strValue, ":include");
        CHECK_EQ(result.tokens[1].type, octo::Token::Type::STRING);
        CHECK_EQ(result.tokens[1].strValue, "font.8o");
        CHECK_EQ(result.tokens[2].tid, octo::TokenId::COLON);
        CHECK_EQ(result.tokens[2].type, octo::Token::Type::DIRECTIVE);
        CHECK_EQ(result.tokens[2].prefix, "\n");
        CHECK_EQ(result.tokens[3].type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(result.tokens[3].strValue, "main");
    }

    TEST_CASE("minimal")
    {
        compileTest(R"(: main)",{});
    }

    TEST_CASE("tiny")
    {
        compileTest(R"(
# a line comment

: main
	va := 0xBC
	loop again
)",
                    {0x6a, 0xbc, 0x12, 0x02});
    }

    TEST_CASE("negative literals")
    {
        compileTest(R"(
: main
	-23
	-0x3A
	-0b1
)",
                    {0xe9 ,0xc6, 0xff});
    }
}
