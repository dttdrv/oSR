#include "debug/capture_compare.h"

#include <filesystem>
#include <iostream>
#include <vector>

namespace {

void AddCapturePath(const std::filesystem::path& path, std::vector<osr::debug::CaptureComparisonRow>& rows) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(path, ec)) {
        rows.push_back(osr::debug::LoadCaptureComparisonRow(path));
        return;
    }
    if (!std::filesystem::is_directory(path, ec)) {
        rows.push_back(osr::debug::LoadCaptureComparisonRow(path));
        return;
    }
    if (std::filesystem::exists(path / "capture_analysis.json", ec)) {
        rows.push_back(osr::debug::LoadCaptureComparisonRow(path));
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        if (!entry.is_directory()) {
            continue;
        }
        if (std::filesystem::exists(entry.path() / "capture_analysis.json", ec)) {
            rows.push_back(osr::debug::LoadCaptureComparisonRow(entry.path()));
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: osr_capture_compare <capture-session-dir|capture-root|capture_analysis.json> [...]\n";
        return 2;
    }
    std::vector<osr::debug::CaptureComparisonRow> rows;
    for (int i = 1; i < argc; ++i) {
        AddCapturePath(argv[i], rows);
    }
    rows = osr::debug::RankCaptureComparisons(std::move(rows));
    std::cout << osr::debug::CaptureComparisonCsvHeader() << "\n";
    for (size_t i = 0; i < rows.size(); ++i) {
        std::cout << osr::debug::CaptureComparisonCsvRow(i + 1, rows[i]) << "\n";
    }
    return rows.empty() ? 1 : 0;
}
