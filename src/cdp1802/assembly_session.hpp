#pragma once

#include "assembler.hpp"

#include <memory>
#include <string>

namespace cdp1802 {

class AssemblySession
{
public:
    bool compile(std::string_view text, AssemblerOptions options = {});
    bool isError() const;
    const std::string& errorMessage() const;
    int errorLine() const;
    int errorColumn() const;
    const std::vector<AssemblySegment>& segments() const;
    const std::vector<uint8_t>& contiguousData() const;
    const std::string& listing() const;
    uint16_t startAddress() const;

private:
    std::unique_ptr<Assembler> _assembler;
    std::string _empty;
};

} // namespace cdp1802
