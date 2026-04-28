#include "debug/capture_analysis.h"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: osr_capture_analyzer <capture-frame-directory> [--gate]\n";
        return 2;
    }
    const bool gate_enabled = argc == 3 && std::string(argv[2]) == "--gate";
    if (argc == 3 && !gate_enabled) {
        std::cerr << "unknown option: " << argv[2] << "\n";
        return 2;
    }
    const auto analysis = osr::debug::AnalyzeCaptureFrame(std::filesystem::path(argv[1]));
    std::cout << osr::debug::SummarizeCaptureAnalysis(analysis) << "\n";
    if (!analysis.ok) {
        return 1;
    }
    if (gate_enabled) {
        const auto gate = osr::debug::EvaluateCaptureAnalysisGate(analysis);
        std::cout << "capture_analysis_gate=" << (gate.passed ? "ok" : "FAILED")
                  << " reason=" << gate.reason << "\n";
        return gate.passed ? 0 : 3;
    }
    return 0;
}
