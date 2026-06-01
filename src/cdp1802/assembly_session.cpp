#include "assembly_session.hpp"

namespace cdp1802 {

bool AssemblySession::compile(std::string_view text, AssemblerOptions options)
{
    _assembler = std::make_unique<Assembler>(text, options);
    return _assembler->compile();
}

bool AssemblySession::isError() const
{
    return _assembler && _assembler->isError();
}

const std::string& AssemblySession::errorMessage() const
{
    return _assembler ? _assembler->errorMessage() : _empty;
}

int AssemblySession::errorLine() const
{
    return _assembler ? _assembler->errorLine() : 0;
}

int AssemblySession::errorColumn() const
{
    return _assembler ? _assembler->errorColumn() : 0;
}

const std::vector<AssemblySegment>& AssemblySession::segments() const
{
    static const std::vector<AssemblySegment> empty;
    return _assembler ? _assembler->segments() : empty;
}

const std::vector<uint8_t>& AssemblySession::contiguousData() const
{
    static const std::vector<uint8_t> empty;
    return _assembler ? _assembler->contiguousData() : empty;
}

const std::string& AssemblySession::listing() const
{
    return _assembler ? _assembler->listing() : _empty;
}

uint16_t AssemblySession::startAddress() const
{
    return _assembler ? _assembler->startAddress() : 0;
}

} // namespace cdp1802
