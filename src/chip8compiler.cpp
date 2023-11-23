#include <chiplet/chip8compiler.hpp>
//#include <emulation/utility.hpp>

#include <chiplet/sha1.hpp>
#include <iostream>
#include <vector>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma GCC diagnostic ignored "-Wenum-compare"
#pragma GCC diagnostic ignored "-Wwritable-strings"
#pragma GCC diagnostic ignored "-Wwrite-strings"
#if __clang__
#pragma GCC diagnostic ignored "-Wenum-compare-conditional"
#endif
#endif  // __GNUC__

//extern "C" {
#include "c-octo/octo_compiler.hpp"
//}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

namespace emu {

class Chip8Compiler::Private
{
public:
    std::unique_ptr<OctoProgram> _program{};
    std::string _sha1hex;
    std::string _errorMessage;
    std::vector<std::pair<uint32_t, uint32_t>> _lineCoverage;
};

Chip8Compiler::Chip8Compiler()
    : _impl(new Private)
{
    _impl->_program = nullptr;
}

Chip8Compiler::~Chip8Compiler()
{
    _impl->_program.reset();
}

bool Chip8Compiler::compile(std::string str, int startAddress)
{
    return compile(str.data(), str.data() + str.size() + 1, startAddress);
}

bool Chip8Compiler::compile(const char* start, const char* end, int startAddress)
{
    if (_impl->_program) {
        _impl->_program.reset();
    }
    if(end - start >= 3 && *start == (char)0xef && *(start+1) == (char)0xbb && *(start+2) == (char)0xbf)
        start += 3; // skip BOM

    // make a malloc based copy that c-octo will own and free on oct_free_program
    char* source = (char*)malloc(end - start);
    memcpy(source, start, end - start);
    _impl->_program = std::make_unique<OctoProgram>(source, startAddress);
    if (!_impl->_program->compile()) {
        _impl->_errorMessage = "ERROR (" + std::to_string(_impl->_program->errorLine()) + ":" + std::to_string(_impl->_program->errorPos() + 1) + "): " + _impl->_program->errorMessage();
        //std::cerr << _impl->_errorMessage << std::endl;
    }
    else {
        updateHash(); //calculateSha1Hex(code(), codeSize());
        _impl->_errorMessage = "No errors.";
        //std::clog << "compiled successfully." << std::endl;
    }
    return !_impl->_program->isError();
}

std::string Chip8Compiler::rawErrorMessage() const
{
    if(!_impl->_program)
        return "unknown error";
    if(_impl->_program->isError())
        return _impl->_program->errorMessage();
    return "";
}

int Chip8Compiler::errorLine() const
{
    return _impl->_program ? _impl->_program->errorLine() + 1 : 0;
}

int Chip8Compiler::errorCol() const
{
    return _impl->_program ? _impl->_program->errorPos() + 1 : 0;
}

bool Chip8Compiler::isError() const
{
    return !_impl->_program || _impl->_program->isError();
}

const std::string& Chip8Compiler::errorMessage() const
{
    return _impl->_errorMessage;
}

uint32_t Chip8Compiler::codeSize() const
{
    return _impl->_program && !_impl->_program->isError() ? _impl->_program->romLength() - _impl->_program->romStartAddress() : 0;
}

const uint8_t* Chip8Compiler::code() const
{
    return reinterpret_cast<const uint8_t*>(_impl->_program->data() + _impl->_program->romStartAddress());
}

const std::string& Chip8Compiler::sha1Hex() const
{
    return _impl->_sha1hex;
}

std::pair<uint32_t, uint32_t> Chip8Compiler::addrForLine(uint32_t line) const
{
    return line < _impl->_lineCoverage.size() && !isError() ? _impl->_lineCoverage[line] : std::make_pair(0xFFFFFFFFu, 0xFFFFFFFFu);
}

uint32_t Chip8Compiler::lineForAddr(uint32_t addr) const
{
    return _impl->_program->lineForAddress(addr);
}

const char* Chip8Compiler::breakpointForAddr(uint32_t addr) const
{
    if(addr < OCTO_RAM_MAX && _impl->_program->breakpointInfo(addr)) {
        return _impl->_program->breakpointInfo(addr);
    }
    return nullptr;
}

void Chip8Compiler::updateHash()
{
    char hex[SHA1_HEX_SIZE];
    char bpName[1024];
    sha1 sum;
    sum.add(code(), codeSize());
    for(uint32_t addr = 0; addr < OCTO_RAM_MAX; ++addr) {
        if(_impl->_program->breakpointInfo(addr)) {
            auto l = std::snprintf(bpName, 1023, "%04x:%s", addr, _impl->_program->breakpointInfo(addr));
            sum.add(bpName, l);
        }
    }
    sum.finalize();
    sum.print_hex(hex);
    _impl->_sha1hex = hex;
}

void Chip8Compiler::updateLineCoverage()
{
    _impl->_lineCoverage.clear();
    _impl->_lineCoverage.resize(_impl->_program->numSourceLines());
    if (!_impl->_program)
        return;
    for (size_t addr = 0; addr < OCTO_RAM_MAX; ++addr) {
        auto line = _impl->_program->lineForAddress(addr);
        if (line < _impl->_lineCoverage.size()) {
            auto& range = _impl->_lineCoverage.at(line);
            if (range.first > addr || range.first == 0xffffffff)
                range.first = addr;
            if (range.second < addr || range.second == 0xffffffff)
                range.second = addr;
        }
    }
}

}
