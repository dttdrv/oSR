#include "debug/capture_analysis.h"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: osr_capture_analyzer <capture-frame-directory> [--gate] [--thresholds path]\n";
        return 2;
    }
    bool gate_enabled = false;
    osr::debug::CaptureAnalysisGateThresholds thresholds;
    for (int i = 2; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--gate") {
            gate_enabled = true;
        } else if (option == "--thresholds" && i + 1 < argc) {
            thresholds = osr::debug::LoadCaptureAnalysisGateThresholds(std::filesystem::path(argv[++i]));
        } else {
            std::cerr << "unknown option: " << option << "\n";
            return 2;
        }
    }
    const auto analysis = osr::debug::AnalyzeCaptureFrame(std::filesystem::path(argv[1]));
    std::cout << osr::debug::SummarizeCaptureAnalysis(analysis) << "\n";
    if (!analysis.ok) {
        return 1;
    }
    if (gate_enabled) {
        const auto gate = osr::debug::EvaluateCaptureAnalysisGate(analysis, thresholds);
        std::cout << "capture_analysis_gate=" << (gate.passed ? "ok" : "FAILED")
                  << " reason=" << gate.reason << "\n";
        return gate.passed ? 0 : 3;
    }
    return 0;
}
