#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace emu {

struct SourceLocation {
    std::string file;
    int line{};
    int column{};
};

struct CompileResult {
    enum ResultType { eOK, eINFO, eWARNING, eERROR };
    struct Location {
        enum Type { eROOT, eINCLUDED, eINSTANTIATED };
        std::string file;
        int line;
        int column;
        Type type;
    };
    ResultType resultType{eOK};
    std::string errorMessage;
    std::vector<Location> locations;
    std::shared_ptr<nlohmann::json> config;
    void reset()
    {
        resultType = eOK;
        errorMessage.clear();
        locations.clear();
    }
};

}
