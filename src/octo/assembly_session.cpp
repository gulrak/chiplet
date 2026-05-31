#include "assembly_session.hpp"
//#include <emulation/utility.hpp>

#include <chiplet/sha1.hpp>
#include <iostream>
#include <vector>

#include "assembler.hpp"

namespace octo {

class AssemblySession::Private
{
public:
    std::unique_ptr<octo::Assembler> _assembler{};
    Sha1::Digest _sha1;
    std::string _errorMessage;
    std::vector<std::pair<uint32_t, uint32_t>> _lineCoverage;
};

AssemblySession::AssemblySession()
    : _impl(new Private)
{
    _impl->_assembler = nullptr;
}

AssemblySession::~AssemblySession()
{
    _impl->_assembler.reset();
}


bool AssemblySession::compile(std::string_view text, int startAddress)
{
    if (_impl->_assembler) {
        _impl->_assembler.reset();
    }
    if(text.length() >= 3 && text[0] == (char)0xef && text[1] == (char)0xbb && text[2] == (char)0xbf)
        text.remove_prefix(3); // skip BOM

    _impl->_assembler = std::make_unique<octo::Assembler>(text, startAddress);
    if (!_impl->_assembler->compile()) {
        _impl->_errorMessage = "ERROR (" + std::to_string(_impl->_assembler->errorLine()) + ":" + std::to_string(_impl->_assembler->errorPos()) + "): " + _impl->_assembler->errorMessage();
        //std::cerr << _impl->_errorMessage << std::endl;
    }
    else {
        updateHash(); //calculateSha1Hex(code(), codeSize());
        _impl->_errorMessage = "No errors.";
        //std::clog << "compiled successfully." << std::endl;
    }
    return !_impl->_assembler->isError();
}

std::string AssemblySession::rawErrorMessage() const
{
    if(!_impl->_assembler)
        return "unknown error";
    if(_impl->_assembler->isError())
        return _impl->_assembler->errorMessage();
    return "";
}

int AssemblySession::errorLine() const
{
    return _impl->_assembler->errorLine();
}

int AssemblySession::errorCol() const
{
    return _impl->_assembler->errorPos();
}

bool AssemblySession::isError() const
{
    return !_impl->_assembler || _impl->_assembler->isError();
}

const std::string& AssemblySession::errorMessage() const
{
    return _impl->_errorMessage;
}

size_t AssemblySession::numSourceLines() const
{
    return _impl->_assembler->numSourceLines();
}

uint32_t AssemblySession::codeSize() const
{
    return _impl->_assembler && !_impl->_assembler->isError() ? _impl->_assembler->codeSize() : 0;
}

const uint8_t* AssemblySession::code() const
{
    return reinterpret_cast<const uint8_t*>(_impl->_assembler->data());
}

const Sha1::Digest& AssemblySession::sha1() const
{
    return _impl->_sha1;
}

std::pair<uint32_t, uint32_t> AssemblySession::addrForLine(uint32_t line) const
{
    return line < _impl->_lineCoverage.size() && !isError() ? _impl->_lineCoverage[line] : std::make_pair(0xFFFFFFFFu, 0xFFFFFFFFu);
}

uint32_t AssemblySession::lineForAddr(uint32_t addr) const
{
    return _impl->_assembler->lineForAddress(addr);
}

std::string_view AssemblySession::breakpointForAddr(uint32_t addr) const
{
    if(addr <= _impl->_assembler->lastAddressUsed() && !_impl->_assembler->breakpointInfo(addr).empty()) {
        return _impl->_assembler->breakpointInfo(addr);
    }
    return "";
}

void AssemblySession::updateHash()
{
    char hex[SHA1_HEX_SIZE];
    char bpName[1024];
    Sha1 sum;
    sum.add(code(), codeSize());
    for(uint32_t addr = 0; addr <= _impl->_assembler->lastAddressUsed(); ++addr) {
        if(!_impl->_assembler->breakpointInfo(addr).empty()) {
            auto l = fmt::format_to_n(bpName, 1023, "{:04x}:{}", addr, _impl->_assembler->breakpointInfo(addr)).size;
            sum.add(bpName, l);
        }
    }
    sum.finalize();
    _impl->_sha1 = Sha1::Digest(sum);
}

void AssemblySession::updateLineCoverage()
{
    _impl->_lineCoverage.clear();
    _impl->_lineCoverage.resize(_impl->_assembler->numSourceLines());
    if (!_impl->_assembler)
        return;
    for (size_t addr = 0; addr <= _impl->_assembler->lastAddressUsed(); ++addr) {
        auto line = _impl->_assembler->lineForAddress(addr);
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
