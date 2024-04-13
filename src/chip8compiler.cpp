#include <chiplet/chip8compiler.hpp>
//#include <emulation/utility.hpp>

#include <chiplet/sha1.hpp>
#include <iostream>
#include <vector>

#include "octo_compiler.hpp"

namespace emu {

class Chip8Compiler::Private
{
public:
    std::unique_ptr<octo::Program> _program{};
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


bool Chip8Compiler::compile(std::string_view text, int startAddress)
{
    if (_impl->_program) {
        _impl->_program.reset();
    }
    if(text.length() >= 3 && text[0] == (char)0xef && text[1] == (char)0xbb && text[2] == (char)0xbf)
        text.remove_prefix(3); // skip BOM

    _impl->_program = std::make_unique<octo::Program>(text, startAddress);
    if (!_impl->_program->compile()) {
        _impl->_errorMessage = "ERROR (" + std::to_string(_impl->_program->errorLine()) + ":" + std::to_string(_impl->_program->errorPos()) + "): " + _impl->_program->errorMessage();
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
    return _impl->_program->errorLine();
}

int Chip8Compiler::errorCol() const
{
    return _impl->_program->errorPos();
}

bool Chip8Compiler::isError() const
{
    return !_impl->_program || _impl->_program->isError();
}

const std::string& Chip8Compiler::errorMessage() const
{
    return _impl->_errorMessage;
}

size_t Chip8Compiler::numSourceLines() const
{
    return _impl->_program->numSourceLines();
}

uint32_t Chip8Compiler::codeSize() const
{
    return _impl->_program && !_impl->_program->isError() ? _impl->_program->codeSize() : 0;
}

const uint8_t* Chip8Compiler::code() const
{
    return reinterpret_cast<const uint8_t*>(_impl->_program->data());
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
    if(addr <= _impl->_program->lastAddressUsed() && _impl->_program->breakpointInfo(addr)) {
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
    for(uint32_t addr = 0; addr <= _impl->_program->lastAddressUsed(); ++addr) {
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
    for (size_t addr = 0; addr <= _impl->_program->lastAddressUsed(); ++addr) {
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
