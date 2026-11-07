//---------------------------------------------------------------------------------------
//
// Copyright (c) 2025, Steffen Schümann <s.schuemann@pobox.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//---------------------------------------------------------------------------------------
#include <doctest/doctest.h>

#include <ghc/cli.hpp>

#include <vector>
#include <string>

struct ArgvBuilder {
    std::vector<std::string> storage;
    std::vector<char*> argv;
    ArgvBuilder(std::initializer_list<const char*> args) {
        storage.reserve(args.size());
        argv.reserve(args.size() + 1);
        for (auto s : args) {
            storage.emplace_back(s);
        }
        for (auto& str : storage) {
            argv.push_back(str.data());
        }
        argv.push_back(nullptr);
    }

    [[nodiscard]] int argc() const {
        // We do not count the final null pointer in argv
        return static_cast<int>(argv.size() - 1);
    }

    char** data() {
        return argv.data();
    }
};

ghc::CLI makeCLI(std::initializer_list<const char*> args) {
    ArgvBuilder builder(args);
    return {builder.argc(), builder.data()};
}

TEST_SUITE("<cli>")
{

    TEST_CASE("basic function")
    {
        int64_t intArg = -1;
        std::vector<std::string> positional;
        auto cli = makeCLI({"bin/clitest", "-c", "42", "positional"});
        cli.option({"-c"}, intArg, "Some int argument, default -1");
        cli.positional(positional, "Some positional arguments");
        cli.parse();
        REQUIRE(intArg == 42);
        REQUIRE(positional.size() == 1);
        REQUIRE(positional[0] == "positional");
        std::ostringstream oss;
        cli.usage(oss);
        REQUIRE(oss.str() == R"(USAGE: bin/clitest [options] ...
OPTIONS:

...
    Some positional arguments

)");
    }

}