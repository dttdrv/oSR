#include "debug/validation.h"

#include <sstream>

namespace osr::debug {

std::string SummarizeValidation(const core::ValidationReport& report) {
    uint32_t infos = 0;
    uint32_t warnings = 0;
    uint32_t errors = 0;

    for (const auto& message : report.messages) {
        switch (message.severity) {
        case core::ValidationSeverity::Info: ++infos; break;
        case core::ValidationSeverity::Warning: ++warnings; break;
        case core::ValidationSeverity::Error: ++errors; break;
        }
    }

    std::ostringstream out;
    out << "infos=" << infos << " warnings=" << warnings << " errors=" << errors;
    return out.str();
}

} // namespace osr::debug

