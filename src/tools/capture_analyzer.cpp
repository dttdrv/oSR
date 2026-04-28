#include "debug/capture_analysis.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: osr_capture_analyzer <capture-frame-directory>\n";
        return 2;
    }
    const auto analysis = osr::debug::AnalyzeCaptureFrame(std::filesystem::path(argv[1]));
    std::cout << osr::debug::SummarizeCaptureAnalysis(analysis) << "\n";
    return analysis.ok ? 0 : 1;
}
