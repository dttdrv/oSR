#include "core/frame_context.h"
#include "debug/capture_pack.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

bool Contains(const std::filesystem::path& path, const std::string& needle) {
    std::ifstream in(path);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text.find(needle) != std::string::npos;
}

osr::core::ResourceDesc Resource(osr::core::ResourceKind kind, uint64_t id) {
    return {kind, nullptr, id, {1280, 800}, 28, osr::core::ToString(kind)};
}

osr::core::FrameContext Frame() {
    osr::core::FrameContext frame;
    frame.frame_id = 1;
    frame.source_api = "test_api";
    frame.render_size = {1280, 800};
    frame.display_size = {1920, 1200};
    frame.jitter_offset = {0.25f, -0.25f};
    frame.motion_vector_scale = {1280.0f, 800.0f};
    frame.motion_vector_space = osr::core::MotionVectorSpace::Pixel;
    frame.color_space = osr::core::ColorSpace::LinearSdr;
    frame.color_input = Resource(osr::core::ResourceKind::ColorInput, 1);
    frame.color_output = Resource(osr::core::ResourceKind::ColorOutput, 2);
    frame.depth = Resource(osr::core::ResourceKind::Depth, 3);
    frame.motion_vectors = Resource(osr::core::ResourceKind::MotionVectors, 4);
    frame.notes.push_back("quoted \"note\"\nnext");
    return frame;
}

} // namespace

int main() {
    using namespace osr::debug;

    if (JsonEscape("\"\\\n\t\r\x01") != "\\\"\\\\\\n\\t\\r\\u0001") {
        return Fail("JsonEscape did not escape required characters");
    }
    if (CsvEscape("plain") != "plain") {
        return Fail("CsvEscape changed plain value");
    }
    if (CsvEscape("a,b") != "\"a,b\"") {
        return Fail("CsvEscape did not quote comma");
    }
    if (CsvEscape("a\"b") != "\"a\"\"b\"") {
        return Fail("CsvEscape did not double quote");
    }
    if (CsvEscape("a\nb") != "\"a\nb\"") {
        return Fail("CsvEscape did not quote newline");
    }
    const uint8_t hash_data[] = {1, 2, 3, 4};
    if (HashBytes(hash_data, sizeof(hash_data)) != HashBytes(hash_data, sizeof(hash_data))) {
        return Fail("HashBytes must be deterministic");
    }

    CapturePackConfig config;
    config.root = "build/manual/capture_pack_tests";
    config.run_name = "unit";
    config.overwrite_existing = true;
    std::filesystem::remove_all(config.root);

    CapturePackWriter writer;
    if (!writer.BeginSession(config)) {
        return Fail("BeginSession failed");
    }

    auto frame = Frame();
    if (!writer.WriteSessionManifest(frame, "unit --flag \"value\"")) {
        return Fail("WriteSessionManifest failed");
    }

    HarnessFrameRow row;
    row.frame_id = frame.frame_id;
    row.render_size = frame.render_size;
    row.display_size = frame.display_size;
    row.jitter = frame.jitter_offset;
    row.motion_vector_scale = frame.motion_vector_scale;
    row.validation_warnings = 1;
    if (!writer.WriteFrameRow(row)) {
        return Fail("WriteFrameRow failed");
    }

    HarnessMetricRow metrics;
    metrics.frame_id = frame.frame_id;
    metrics.text_contrast = 0.42;
    if (!writer.WriteMetricRow(metrics)) {
        return Fail("WriteMetricRow failed");
    }

    osr::core::ValidationReport report;
    report.messages.push_back({osr::core::ValidationSeverity::Warning, "warn_code", "message \"body\""});
    if (!writer.WriteValidationWarnings(frame.frame_id, report)) {
        return Fail("WriteValidationWarnings failed");
    }
    if (!writer.WriteFrameContextJson(frame)) {
        return Fail("WriteFrameContextJson failed");
    }

    const auto root = writer.SessionPath();
    if (!Contains(root / "session.json", "\"schema\": \"osr.capture.session.v1\"")) {
        return Fail("session manifest missing schema");
    }
    if (!Contains(root / "frames.csv", "frame_id,scenario_time_ms,render_w")) {
        return Fail("frames.csv missing header");
    }
    if (!Contains(root / "metrics.csv", "1,0,0,0,0,0,0.42,0,0")) {
        return Fail("metrics.csv missing row");
    }
    if (!Contains(root / "warnings.jsonl", "\"code\":\"warn_code\"")) {
        return Fail("warnings.jsonl missing warning");
    }
    if (!Contains(root / "bookmarks.jsonl", "")) {
        return Fail("bookmarks.jsonl missing");
    }
    if (!Contains(root / "frame_000001" / "frame_context.json", "quoted \\\"note\\\"\\nnext")) {
        return Fail("frame_context.json missing escaped note");
    }

    CapturePackConfig duplicate = config;
    duplicate.overwrite_existing = false;
    CapturePackWriter duplicate_writer;
    if (duplicate_writer.BeginSession(duplicate)) {
        return Fail("BeginSession should fail when output exists and overwrite is false");
    }

    return 0;
}
