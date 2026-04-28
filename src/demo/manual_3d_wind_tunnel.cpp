#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr float kPi = 3.1415926535f;
constexpr int kPanelWidth = 350;
constexpr int kControlMargin = 18;

enum ControlId {
    kPresetCombo = 1001,
    kScaleSlider,
    kViewCombo,
    kAnimateCheck,
    kJitterCheck,
    kFreezeCheck,
    kLinearCheck,
    kRailsCheck,
    kParticlesCheck,
    kResetButton
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vertex {
    Vec3 p;
    uint32_t color = 0xffffffffu;
};

struct ScreenVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    uint32_t color = 0xffffffffu;
    bool valid = false;
};

enum class ViewMode {
    Color,
    Luma,
    Depth,
    EdgeEnergy,
    HistoryWeight,
    RejectionRisk
};

enum class QualityPreset {
    Native,
    UltraQuality,
    Quality,
    Balanced,
    Performance,
    UltraPerformance,
    Custom
};

struct Camera {
    Vec3 pos {0.0f, 1.55f, -5.8f};
    float yaw = 0.0f;
    float pitch = -0.08f;
};

struct AppState {
    HWND hwnd = nullptr;
    HWND preset_label = nullptr;
    HWND scale_label = nullptr;
    HWND view_label = nullptr;
    HWND preset_combo = nullptr;
    HWND scale_slider = nullptr;
    HWND view_combo = nullptr;
    HWND animate_check = nullptr;
    HWND jitter_check = nullptr;
    HWND freeze_check = nullptr;
    HWND linear_check = nullptr;
    HWND rails_check = nullptr;
    HWND particles_check = nullptr;
    HWND reset_button = nullptr;
    int client_w = 1280;
    int client_h = 800;
    int viewport_w = 930;
    int viewport_h = 800;
    int render_w = 854;
    int render_h = 533;
    int frame = 0;
    int jitter_length = 16;
    bool running = true;
    bool animate = true;
    bool jitter = true;
    bool freeze = false;
    bool linear = true;
    bool rails = true;
    bool particles = true;
    bool overlay = true;
    bool mouse_look = false;
    POINT last_mouse {};
    float custom_scale = 0.667f;
    QualityPreset preset = QualityPreset::Quality;
    ViewMode view = ViewMode::Color;
    Camera camera;
    std::vector<uint32_t> color;
    std::vector<uint32_t> debug_color;
    std::vector<float> depth;
    std::vector<uint32_t> display;
};

void SyncControls(const AppState& app);

uint8_t R(uint32_t c) { return static_cast<uint8_t>((c >> 16) & 0xff); }
uint8_t G(uint32_t c) { return static_cast<uint8_t>((c >> 8) & 0xff); }
uint8_t B(uint32_t c) { return static_cast<uint8_t>(c & 0xff); }

uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return 0xff000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

uint32_t ScaleColor(uint32_t c, float s) {
    return Color(static_cast<uint8_t>(std::clamp(R(c) * s, 0.0f, 255.0f)),
                 static_cast<uint8_t>(std::clamp(G(c) * s, 0.0f, 255.0f)),
                 static_cast<uint8_t>(std::clamp(B(c) * s, 0.0f, 255.0f)));
}

float Luma(uint32_t c) {
    return (0.2126f * R(c) + 0.7152f * G(c) + 0.0722f * B(c)) / 255.0f;
}

float Halton(int index, int base) {
    float f = 1.0f;
    float r = 0.0f;
    int i = index;
    while (i > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(i % base);
        i /= base;
    }
    return r;
}

const char* ToString(QualityPreset preset) {
    switch (preset) {
        case QualityPreset::Native: return "Native";
        case QualityPreset::UltraQuality: return "Ultra Quality";
        case QualityPreset::Quality: return "Quality";
        case QualityPreset::Balanced: return "Balanced";
        case QualityPreset::Performance: return "Performance";
        case QualityPreset::UltraPerformance: return "Ultra Performance";
        case QualityPreset::Custom: return "Custom";
    }
    return "Unknown";
}

const char* ToString(ViewMode view) {
    switch (view) {
        case ViewMode::Color: return "Color";
        case ViewMode::Luma: return "Luma";
        case ViewMode::Depth: return "Depth";
        case ViewMode::EdgeEnergy: return "Edge energy";
        case ViewMode::HistoryWeight: return "History weight";
        case ViewMode::RejectionRisk: return "Rejection risk";
    }
    return "Unknown";
}

float PresetScale(const AppState& app) {
    switch (app.preset) {
        case QualityPreset::Native: return 1.0f;
        case QualityPreset::UltraQuality: return 0.77f;
        case QualityPreset::Quality: return 0.667f;
        case QualityPreset::Balanced: return 0.58f;
        case QualityPreset::Performance: return 0.5f;
        case QualityPreset::UltraPerformance: return 0.333f;
        case QualityPreset::Custom: return app.custom_scale;
    }
    return app.custom_scale;
}

void SetPreset(AppState& app, QualityPreset preset) {
    app.preset = preset;
    if (preset != QualityPreset::Custom) {
        app.custom_scale = PresetScale(app);
    }
    SyncControls(app);
}

Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 Normalize(Vec3 v) {
    const float len = std::sqrt(std::max(0.000001f, Dot(v, v)));
    return {v.x / len, v.y / len, v.z / len};
}

Vec3 Forward(const Camera& c) {
    const float cp = std::cos(c.pitch);
    return {std::sin(c.yaw) * cp, std::sin(c.pitch), std::cos(c.yaw) * cp};
}

Vec3 Right(const Camera& c) {
    return {std::cos(c.yaw), 0.0f, -std::sin(c.yaw)};
}

Vec3 RotateY(Vec3 p, float a) {
    const float c = std::cos(a);
    const float s = std::sin(a);
    return {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
}

Vec3 CameraSpace(const Camera& cam, Vec3 world) {
    Vec3 p = Sub(world, cam.pos);
    const float cy = std::cos(-cam.yaw);
    const float sy = std::sin(-cam.yaw);
    const float x = p.x * cy + p.z * sy;
    const float z = -p.x * sy + p.z * cy;
    p = {x, p.y, z};

    const float cp = std::cos(-cam.pitch);
    const float sp = std::sin(-cam.pitch);
    return {p.x, p.y * cp - p.z * sp, p.y * sp + p.z * cp};
}

ScreenVertex Project(const AppState& app, const Vertex& v, float jitter_x, float jitter_y) {
    const Vec3 p = CameraSpace(app.camera, v.p);
    if (p.z <= 0.08f) {
        return {};
    }

    const float f = 1.0f / std::tan(60.0f * kPi / 360.0f);
    const float aspect = static_cast<float>(app.render_w) / static_cast<float>(std::max(1, app.render_h));
    const float ndc_x = (p.x * f / aspect) / p.z;
    const float ndc_y = (p.y * f) / p.z;
    return {
        (ndc_x * 0.5f + 0.5f) * app.render_w + jitter_x,
        (0.5f - ndc_y * 0.5f) * app.render_h + jitter_y,
        p.z,
        v.color,
        true
    };
}

void Clear(AppState& app) {
    app.color.assign(static_cast<size_t>(app.render_w) * app.render_h, Color(18, 20, 26));
    app.depth.assign(static_cast<size_t>(app.render_w) * app.render_h, 1.0e9f);
}

void PutPixel(AppState& app, int x, int y, float z, uint32_t c) {
    if (x < 0 || y < 0 || x >= app.render_w || y >= app.render_h) {
        return;
    }
    const size_t idx = static_cast<size_t>(y) * app.render_w + x;
    if (z < app.depth[idx]) {
        app.depth[idx] = z;
        app.color[idx] = c;
    }
}

void DrawLine(AppState& app, ScreenVertex a, ScreenVertex b, uint32_t c) {
    if (!a.valid || !b.valid) {
        return;
    }
    const int steps = std::max(std::abs(static_cast<int>(b.x - a.x)), std::abs(static_cast<int>(b.y - a.y)));
    if (steps <= 0) {
        return;
    }
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const int x = static_cast<int>(a.x + (b.x - a.x) * t);
        const int y = static_cast<int>(a.y + (b.y - a.y) * t);
        const float z = a.z + (b.z - a.z) * t;
        PutPixel(app, x, y, z, c);
    }
}

void DrawTriangle(AppState& app, ScreenVertex a, ScreenVertex b, ScreenVertex c, uint32_t color) {
    if (!a.valid || !b.valid || !c.valid) {
        return;
    }

    const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::abs(area) < 0.0001f) {
        return;
    }

    const int min_x = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
    const int max_x = std::min(app.render_w - 1, static_cast<int>(std::ceil(std::max({a.x, b.x, c.x}))));
    const int min_y = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
    const int max_y = std::min(app.render_h - 1, static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float w0 = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) / area;
            const float w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) / area;
            const float w2 = 1.0f - w0 - w1;
            if (w0 >= -0.0001f && w1 >= -0.0001f && w2 >= -0.0001f) {
                const float z = a.z * w0 + b.z * w1 + c.z * w2;
                PutPixel(app, x, y, z, color);
            }
        }
    }

    DrawLine(app, a, b, ScaleColor(color, 0.45f));
    DrawLine(app, b, c, ScaleColor(color, 0.45f));
    DrawLine(app, c, a, ScaleColor(color, 0.45f));
}

void DrawCube(AppState& app, Vec3 center, Vec3 scale, float yaw, uint32_t color, float jitter_x, float jitter_y) {
    static constexpr std::array<Vec3, 8> corners {{
        {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
        {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
    }};
    static constexpr std::array<int, 36> idx {{
        0,1,2, 0,2,3, 1,5,6, 1,6,2, 5,4,7, 5,7,6,
        4,0,3, 4,3,7, 3,2,6, 3,6,7, 4,5,1, 4,1,0
    }};

    std::array<Vertex, 8> verts {};
    for (size_t i = 0; i < corners.size(); ++i) {
        Vec3 p {corners[i].x * scale.x, corners[i].y * scale.y, corners[i].z * scale.z};
        p = RotateY(p, yaw);
        verts[i] = {Add(center, p), color};
    }
    for (size_t i = 0; i < idx.size(); i += 3) {
        DrawTriangle(app,
                     Project(app, verts[idx[i]], jitter_x, jitter_y),
                     Project(app, verts[idx[i + 1]], jitter_x, jitter_y),
                     Project(app, verts[idx[i + 2]], jitter_x, jitter_y),
                     color);
    }
}

void DrawWorldLine(AppState& app, Vec3 a, Vec3 b, uint32_t color, float jitter_x, float jitter_y) {
    DrawLine(app, Project(app, {a, color}, jitter_x, jitter_y), Project(app, {b, color}, jitter_x, jitter_y), color);
}

void DrawScene(AppState& app) {
    Clear(app);

    const int jitter_index = (app.frame % std::max(1, app.jitter_length)) + 1;
    const float jitter_x = app.jitter ? Halton(jitter_index, 2) - 0.5f : 0.0f;
    const float jitter_y = app.jitter ? Halton(jitter_index, 3) - 0.5f : 0.0f;
    const float t = app.animate ? static_cast<float>(app.frame) * 0.016f : 0.0f;

    for (int i = -12; i <= 12; ++i) {
        const uint32_t c = (i == 0) ? Color(78, 112, 132) : Color(45, 49, 58);
        DrawWorldLine(app, {-12.0f, 0.0f, static_cast<float>(i)}, {12.0f, 0.0f, static_cast<float>(i)}, c, jitter_x, jitter_y);
        DrawWorldLine(app, {static_cast<float>(i), 0.0f, -12.0f}, {static_cast<float>(i), 0.0f, 12.0f}, c, jitter_x, jitter_y);
    }

    DrawCube(app, {-1.5f, 1.0f, 2.5f}, {0.75f, 1.0f, 0.75f}, t * 0.8f, Color(170, 196, 177), jitter_x, jitter_y);
    DrawCube(app, {1.35f, 0.55f, 3.2f + std::sin(t) * 0.65f}, {0.45f, 0.55f, 0.45f}, -t * 1.2f, Color(119, 171, 197), jitter_x, jitter_y);
    DrawCube(app, {0.0f, 0.12f, 5.4f}, {2.0f, 0.12f, 0.18f}, 0.0f, Color(96, 87, 73), jitter_x, jitter_y);

    if (app.rails) {
        for (int i = -4; i <= 4; ++i) {
            DrawCube(app, {static_cast<float>(i) * 0.36f, 0.8f, 1.25f}, {0.022f, 0.8f, 0.022f}, 0.0f, Color(220, 206, 147), jitter_x, jitter_y);
        }
        DrawCube(app, {0.0f, 1.52f, 1.25f}, {1.7f, 0.024f, 0.024f}, 0.0f, Color(220, 206, 147), jitter_x, jitter_y);
    }

    if (app.particles) {
        for (int i = 0; i < 24; ++i) {
            const float fi = static_cast<float>(i);
            const float x = std::sin(t * 1.7f + fi * 2.1f) * 1.55f;
            const float y = 0.8f + std::fmod(fi * 0.37f + t * 0.45f, 1.45f);
            const float z = 1.5f + std::cos(t + fi) * 0.55f;
            DrawCube(app, {x, y, z}, {0.035f, 0.035f, 0.035f}, 0.0f, Color(239, 110, 78), jitter_x, jitter_y);
        }
    }
}

void BuildDebugView(AppState& app) {
    app.debug_color = app.color;
    if (app.view == ViewMode::Color) {
        return;
    }

    auto local_edge = [&](int x, int y, float l) {
        const int xr = std::min(app.render_w - 1, x + 1);
        const int yd = std::min(app.render_h - 1, y + 1);
        const size_t idx = static_cast<size_t>(y) * app.render_w + x;
        const float depth = app.depth[idx];
        const float dx = std::abs(l - Luma(app.color[static_cast<size_t>(y) * app.render_w + xr]));
        const float dy = std::abs(l - Luma(app.color[static_cast<size_t>(yd) * app.render_w + x]));
        const float depth_dx = std::abs(depth - app.depth[static_cast<size_t>(y) * app.render_w + xr]) / std::max(1.0f, depth);
        const float depth_dy = std::abs(depth - app.depth[static_cast<size_t>(yd) * app.render_w + x]) / std::max(1.0f, depth);
        return std::clamp((dx + dy) * 5.0f + (depth_dx + depth_dy) * 1.75f, 0.0f, 1.0f);
    };

    for (int y = 0; y < app.render_h; ++y) {
        for (int x = 0; x < app.render_w; ++x) {
            const size_t idx = static_cast<size_t>(y) * app.render_w + x;
            const float l = Luma(app.color[idx]);
            if (app.view == ViewMode::Luma) {
                const auto v = static_cast<uint8_t>(std::clamp(l * 255.0f, 0.0f, 255.0f));
                app.debug_color[idx] = Color(v, v, v);
                continue;
            }
            if (app.view == ViewMode::Depth) {
                const float d = app.depth[idx] > 1000000.0f ? 0.0f : std::clamp(1.0f - app.depth[idx] / 12.0f, 0.0f, 1.0f);
                app.debug_color[idx] = Color(static_cast<uint8_t>(d * 208.0f),
                                             static_cast<uint8_t>(d * 224.0f),
                                             static_cast<uint8_t>(d * 238.0f));
                continue;
            }
            const float edge = local_edge(x, y, l);
            if (app.view == ViewMode::HistoryWeight) {
                const float trust = std::clamp(1.0f - edge, 0.0f, 1.0f);
                app.debug_color[idx] = Color(static_cast<uint8_t>(trust * 118.0f),
                                             static_cast<uint8_t>(trust * 190.0f),
                                             static_cast<uint8_t>(trust * 164.0f));
            } else if (app.view == ViewMode::RejectionRisk) {
                app.debug_color[idx] = Color(static_cast<uint8_t>(edge * 226.0f),
                                             static_cast<uint8_t>(edge * 118.0f),
                                             static_cast<uint8_t>(edge * 80.0f));
            } else {
                app.debug_color[idx] = Color(static_cast<uint8_t>(edge * 255.0f),
                                             static_cast<uint8_t>(edge * 208.0f),
                                             static_cast<uint8_t>(edge * 108.0f));
            }
        }
    }
}

uint32_t SampleNearest(const std::vector<uint32_t>& src, int w, int h, float u, float v) {
    const int x = std::clamp(static_cast<int>(u * w), 0, w - 1);
    const int y = std::clamp(static_cast<int>(v * h), 0, h - 1);
    return src[static_cast<size_t>(y) * w + x];
}

uint32_t SampleLinear(const std::vector<uint32_t>& src, int w, int h, float u, float v) {
    const float fx = std::clamp(u * w - 0.5f, 0.0f, static_cast<float>(w - 1));
    const float fy = std::clamp(v * h - 0.5f, 0.0f, static_cast<float>(h - 1));
    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);
    const int x1 = std::min(w - 1, x0 + 1);
    const int y1 = std::min(h - 1, y0 + 1);
    const float tx = fx - x0;
    const float ty = fy - y0;
    const auto c00 = src[static_cast<size_t>(y0) * w + x0];
    const auto c10 = src[static_cast<size_t>(y0) * w + x1];
    const auto c01 = src[static_cast<size_t>(y1) * w + x0];
    const auto c11 = src[static_cast<size_t>(y1) * w + x1];

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    const float r0 = lerp(R(c00), R(c10), tx);
    const float r1 = lerp(R(c01), R(c11), tx);
    const float g0 = lerp(G(c00), G(c10), tx);
    const float g1 = lerp(G(c01), G(c11), tx);
    const float b0 = lerp(B(c00), B(c10), tx);
    const float b1 = lerp(B(c01), B(c11), tx);
    return Color(static_cast<uint8_t>(lerp(r0, r1, ty)),
                 static_cast<uint8_t>(lerp(g0, g1, ty)),
                 static_cast<uint8_t>(lerp(b0, b1, ty)));
}

void BuildDisplay(AppState& app) {
    app.display.resize(static_cast<size_t>(app.viewport_w) * app.viewport_h);
    const auto& src = app.debug_color;
    for (int y = 0; y < app.viewport_h; ++y) {
        for (int x = 0; x < app.viewport_w; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(std::max(1, app.viewport_w));
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(std::max(1, app.viewport_h));
            app.display[static_cast<size_t>(y) * app.viewport_w + x] =
                app.linear ? SampleLinear(src, app.render_w, app.render_h, u, v) : SampleNearest(src, app.render_w, app.render_h, u, v);
        }
    }
}

void ResizeRenderTarget(AppState& app) {
    const float scale = std::clamp(PresetScale(app), 0.333f, 1.0f);
    app.viewport_w = std::max(1, app.client_w - kPanelWidth);
    app.viewport_h = std::max(1, app.client_h);
    app.render_w = std::max(1, static_cast<int>(std::round(app.viewport_w * scale)));
    app.render_h = std::max(1, static_cast<int>(std::round(app.viewport_h * scale)));
}

void DrawTextLine(HDC dc, int& y, const char* text) {
    TextOutA(dc, 14, y, text, static_cast<int>(std::strlen(text)));
    y += 18;
}

void PaintOverlay(HDC dc, const AppState& app) {
    if (!app.overlay) {
        return;
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(222, 228, 235));
    HFONT font = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             FIXED_PITCH | FF_MODERN, "Cascadia Mono");
    HFONT old_font = static_cast<HFONT>(SelectObject(dc, font));

    RECT panel {8, 8, 560, 238};
    HBRUSH brush = CreateSolidBrush(RGB(22, 25, 31));
    FillRect(dc, &panel, brush);
    DeleteObject(brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(55, 62, 74));
    HPEN old_pen = static_cast<HPEN>(SelectObject(dc, pen));
    SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, panel.left, panel.top, panel.right, panel.bottom);
    SelectObject(dc, old_pen);
    DeleteObject(pen);

    char line[256] {};
    int y = 18;
    DrawTextLine(dc, y, "oSR native 3D wind tunnel");
    std::snprintf(line, sizeof(line), "viewport %dx%d  internal %dx%d  scale %.1f%%  preset %s",
                  app.viewport_w, app.viewport_h, app.render_w, app.render_h, PresetScale(app) * 100.0f, ToString(app.preset));
    DrawTextLine(dc, y, line);
    std::snprintf(line, sizeof(line), "view %s  upsample %s  frame %d  jitter %s/%d  freeze %s",
                  ToString(app.view), app.linear ? "linear" : "nearest", app.frame,
                  app.jitter ? "on" : "off", app.jitter_length, app.freeze ? "yes" : "no");
    DrawTextLine(dc, y, line);
    std::snprintf(line, sizeof(line), "camera pos %.2f %.2f %.2f  yaw %.2f  pitch %.2f",
                  app.camera.pos.x, app.camera.pos.y, app.camera.pos.z, app.camera.yaw, app.camera.pitch);
    DrawTextLine(dc, y, line);
    DrawTextLine(dc, y, "Move: WASD + Q/E    Look: arrows or hold left mouse");
    DrawTextLine(dc, y, "Presets: 1-6    Scale: -/=    View: V    Jitter: J/K");
    DrawTextLine(dc, y, "Toggle: L linear, F freeze, P particles, T rails, R reset, H help");
    DrawTextLine(dc, y, "Read edges on rails/cubes; use Edge energy + freeze to compare shimmer.");

    SelectObject(dc, old_font);
    DeleteObject(font);
}

void Render(AppState& app) {
    ResizeRenderTarget(app);
    DrawScene(app);
    BuildDebugView(app);
    BuildDisplay(app);
    if (!app.freeze) {
        ++app.frame;
    }
}

void Present(AppState& app, HDC dc) {
    BITMAPINFO info {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = app.viewport_w;
    info.bmiHeader.biHeight = -app.viewport_h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(dc, 0, 0, app.viewport_w, app.viewport_h, 0, 0, app.viewport_w, app.viewport_h,
                  app.display.data(), &info, DIB_RGB_COLORS, SRCCOPY);
    RECT panel {app.viewport_w, 0, app.client_w, app.client_h};
    HBRUSH brush = CreateSolidBrush(RGB(23, 26, 32));
    FillRect(dc, &panel, brush);
    DeleteObject(brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(58, 65, 77));
    HPEN old_pen = static_cast<HPEN>(SelectObject(dc, pen));
    MoveToEx(dc, app.viewport_w, 0, nullptr);
    LineTo(dc, app.viewport_w, app.client_h);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
    PaintOverlay(dc, app);
}

HWND MakeControl(HWND parent, const char* klass, const char* text, DWORD style, int id) {
    return CreateWindowExA(0, klass, text, WS_CHILD | WS_VISIBLE | style,
                           0, 0, 10, 10, parent, reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),
                           GetModuleHandle(nullptr), nullptr);
}

void AddComboItem(HWND combo, const char* text) {
    SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
}

void LayoutControls(AppState& app) {
    if (!app.preset_combo) {
        return;
    }

    const int x = app.viewport_w + kControlMargin;
    const int w = std::max(1, kPanelWidth - kControlMargin * 2);
    int y = 18;
    auto place = [&](HWND control, int h) {
        MoveWindow(control, x, y, w, h, TRUE);
        y += h + 8;
    };

    place(app.preset_label, 20);
    place(app.preset_combo, 110);
    y -= 76;
    place(app.scale_label, 20);
    place(app.scale_slider, 34);
    place(app.view_label, 20);
    place(app.view_combo, 86);
    y -= 52;
    place(app.animate_check, 24);
    place(app.jitter_check, 24);
    place(app.freeze_check, 24);
    place(app.linear_check, 24);
    place(app.rails_check, 24);
    place(app.particles_check, 24);
    y += 6;
    place(app.reset_button, 34);
}

void SyncControls(const AppState& app) {
    if (!app.preset_combo) {
        return;
    }
    SendMessageA(app.preset_combo, CB_SETCURSEL, static_cast<WPARAM>(app.preset), 0);
    SendMessageA(app.view_combo, CB_SETCURSEL, static_cast<WPARAM>(app.view), 0);
    SendMessageA(app.scale_slider, TBM_SETPOS, TRUE, static_cast<LPARAM>(std::round(PresetScale(app) * 100.0f)));
    SendMessageA(app.animate_check, BM_SETCHECK, app.animate ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(app.jitter_check, BM_SETCHECK, app.jitter ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(app.freeze_check, BM_SETCHECK, app.freeze ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(app.linear_check, BM_SETCHECK, app.linear ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(app.rails_check, BM_SETCHECK, app.rails ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(app.particles_check, BM_SETCHECK, app.particles ? BST_CHECKED : BST_UNCHECKED, 0);
}

void CreateControls(HWND hwnd, AppState& app) {
    app.viewport_w = std::max(1, app.client_w - kPanelWidth);
    app.viewport_h = std::max(1, app.client_h);

    app.preset_label = MakeControl(hwnd, "STATIC", "Quality preset", 0, 2001);
    app.preset_combo = MakeControl(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, kPresetCombo);
    AddComboItem(app.preset_combo, "Native");
    AddComboItem(app.preset_combo, "Ultra Quality");
    AddComboItem(app.preset_combo, "Quality");
    AddComboItem(app.preset_combo, "Balanced");
    AddComboItem(app.preset_combo, "Performance");
    AddComboItem(app.preset_combo, "Ultra Performance");
    AddComboItem(app.preset_combo, "Custom");

    app.scale_label = MakeControl(hwnd, "STATIC", "Render scale", 0, 2002);
    app.scale_slider = MakeControl(hwnd, TRACKBAR_CLASSA, "", TBS_AUTOTICKS, kScaleSlider);
    SendMessageA(app.scale_slider, TBM_SETRANGE, TRUE, MAKELPARAM(33, 100));
    SendMessageA(app.scale_slider, TBM_SETTICFREQ, 5, 0);

    app.view_label = MakeControl(hwnd, "STATIC", "Debug view", 0, 2003);
    app.view_combo = MakeControl(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST, kViewCombo);
    AddComboItem(app.view_combo, "Color");
    AddComboItem(app.view_combo, "Luma");
    AddComboItem(app.view_combo, "Depth");
    AddComboItem(app.view_combo, "Edge energy");
    AddComboItem(app.view_combo, "History weight");
    AddComboItem(app.view_combo, "Rejection risk");

    app.animate_check = MakeControl(hwnd, "BUTTON", "Animate scene", BS_AUTOCHECKBOX, kAnimateCheck);
    app.jitter_check = MakeControl(hwnd, "BUTTON", "Subpixel jitter", BS_AUTOCHECKBOX, kJitterCheck);
    app.freeze_check = MakeControl(hwnd, "BUTTON", "Freeze frame", BS_AUTOCHECKBOX, kFreezeCheck);
    app.linear_check = MakeControl(hwnd, "BUTTON", "Linear upsample", BS_AUTOCHECKBOX, kLinearCheck);
    app.rails_check = MakeControl(hwnd, "BUTTON", "Thin rails", BS_AUTOCHECKBOX, kRailsCheck);
    app.particles_check = MakeControl(hwnd, "BUTTON", "Particles", BS_AUTOCHECKBOX, kParticlesCheck);
    app.reset_button = MakeControl(hwnd, "BUTTON", "Camera cut / reset", BS_PUSHBUTTON, kResetButton);

    LayoutControls(app);
    SyncControls(app);
}

void StepInput(AppState& app) {
    constexpr float move_speed = 0.075f;
    constexpr float look_speed = 0.026f;
    const Vec3 fwd = Forward(app.camera);
    const Vec3 flat_fwd = Normalize({fwd.x, 0.0f, fwd.z});
    const Vec3 right = Right(app.camera);

    if (GetAsyncKeyState('W') & 0x8000) app.camera.pos = Add(app.camera.pos, Mul(flat_fwd, move_speed));
    if (GetAsyncKeyState('S') & 0x8000) app.camera.pos = Sub(app.camera.pos, Mul(flat_fwd, move_speed));
    if (GetAsyncKeyState('D') & 0x8000) app.camera.pos = Add(app.camera.pos, Mul(right, move_speed));
    if (GetAsyncKeyState('A') & 0x8000) app.camera.pos = Sub(app.camera.pos, Mul(right, move_speed));
    if (GetAsyncKeyState('E') & 0x8000) app.camera.pos.y += move_speed;
    if (GetAsyncKeyState('Q') & 0x8000) app.camera.pos.y -= move_speed;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) app.camera.yaw -= look_speed;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) app.camera.yaw += look_speed;
    if (GetAsyncKeyState(VK_UP) & 0x8000) app.camera.pitch += look_speed;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) app.camera.pitch -= look_speed;
    app.camera.pitch = std::clamp(app.camera.pitch, -1.25f, 1.25f);
}

void CycleView(AppState& app) {
    app.view = static_cast<ViewMode>((static_cast<int>(app.view) + 1) % 6);
    SyncControls(app);
}

void AdjustScale(AppState& app, float delta) {
    app.preset = QualityPreset::Custom;
    app.custom_scale = std::clamp(app.custom_scale + delta, 0.333f, 1.0f);
    SyncControls(app);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* app = reinterpret_cast<AppState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* create = reinterpret_cast<CREATESTRUCT*>(lparam);
            app = reinterpret_cast<AppState*>(create->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            if (app) {
                CreateControls(hwnd, *app);
            }
            return 0;
        }
        case WM_SIZE:
            if (app) {
                app->client_w = std::max(1, static_cast<int>(LOWORD(lparam)));
                app->client_h = std::max(1, static_cast<int>(HIWORD(lparam)));
                app->viewport_w = std::max(1, app->client_w - kPanelWidth);
                app->viewport_h = std::max(1, app->client_h);
                LayoutControls(*app);
            }
            return 0;
        case WM_COMMAND:
            if (!app) return 0;
            switch (LOWORD(wparam)) {
                case kPresetCombo:
                    if (HIWORD(wparam) == CBN_SELCHANGE) {
                        const auto sel = static_cast<int>(SendMessageA(app->preset_combo, CB_GETCURSEL, 0, 0));
                        SetPreset(*app, static_cast<QualityPreset>(std::clamp(sel, 0, 6)));
                    }
                    return 0;
                case kViewCombo:
                    if (HIWORD(wparam) == CBN_SELCHANGE) {
                        const auto sel = static_cast<int>(SendMessageA(app->view_combo, CB_GETCURSEL, 0, 0));
                        app->view = static_cast<ViewMode>(std::clamp(sel, 0, 5));
                    }
                    return 0;
                case kAnimateCheck:
                    app->animate = SendMessageA(app->animate_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    return 0;
                case kJitterCheck:
                    app->jitter = SendMessageA(app->jitter_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    return 0;
                case kFreezeCheck:
                    app->freeze = SendMessageA(app->freeze_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    return 0;
                case kLinearCheck:
                    app->linear = SendMessageA(app->linear_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    return 0;
                case kRailsCheck:
                    app->rails = SendMessageA(app->rails_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    return 0;
                case kParticlesCheck:
                    app->particles = SendMessageA(app->particles_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    return 0;
                case kResetButton:
                    app->frame = 0;
                    app->camera = Camera {};
                    return 0;
            }
            return 0;
        case WM_HSCROLL:
            if (app && reinterpret_cast<HWND>(lparam) == app->scale_slider) {
                app->preset = QualityPreset::Custom;
                app->custom_scale = std::clamp(static_cast<float>(SendMessageA(app->scale_slider, TBM_GETPOS, 0, 0)) / 100.0f, 0.333f, 1.0f);
                SyncControls(*app);
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (app) {
                app->mouse_look = true;
                app->last_mouse.x = GET_X_LPARAM(lparam);
                app->last_mouse.y = GET_Y_LPARAM(lparam);
                SetCapture(hwnd);
            }
            return 0;
        case WM_LBUTTONUP:
            if (app) {
                app->mouse_look = false;
                ReleaseCapture();
            }
            return 0;
        case WM_MOUSEMOVE:
            if (app && app->mouse_look) {
                const POINT p {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                app->camera.yaw += static_cast<float>(p.x - app->last_mouse.x) * 0.004f;
                app->camera.pitch += static_cast<float>(p.y - app->last_mouse.y) * -0.004f;
                app->camera.pitch = std::clamp(app->camera.pitch, -1.25f, 1.25f);
                app->last_mouse = p;
            }
            return 0;
        case WM_KEYDOWN:
            if (!app) return 0;
            switch (wparam) {
                case VK_ESCAPE: PostQuitMessage(0); return 0;
                case '1': SetPreset(*app, QualityPreset::Native); return 0;
                case '2': SetPreset(*app, QualityPreset::UltraQuality); return 0;
                case '3': SetPreset(*app, QualityPreset::Quality); return 0;
                case '4': SetPreset(*app, QualityPreset::Balanced); return 0;
                case '5': SetPreset(*app, QualityPreset::Performance); return 0;
                case '6': SetPreset(*app, QualityPreset::UltraPerformance); return 0;
                case 'V': CycleView(*app); return 0;
                case 'J': app->jitter = !app->jitter; SyncControls(*app); return 0;
                case 'K': app->jitter_length = (app->jitter_length >= 96) ? 8 : app->jitter_length + 8; return 0;
                case 'L': app->linear = !app->linear; SyncControls(*app); return 0;
                case 'F': app->freeze = !app->freeze; SyncControls(*app); return 0;
                case 'P': app->particles = !app->particles; SyncControls(*app); return 0;
                case 'T': app->rails = !app->rails; SyncControls(*app); return 0;
                case 'H': app->overlay = !app->overlay; return 0;
                case 'R': app->frame = 0; app->camera = Camera {}; return 0;
                case VK_OEM_MINUS: AdjustScale(*app, -0.025f); return 0;
                case VK_OEM_PLUS: AdjustScale(*app, 0.025f); return 0;
            }
            return 0;
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_cmd) {
    AppState app;

    INITCOMMONCONTROLSEX controls {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&controls);

    WNDCLASSA wc {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = "OSRManual3DWindTunnel";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassA(&wc);

    RECT rect {0, 0, app.client_w, app.client_h};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    app.hwnd = CreateWindowExA(0, wc.lpszClassName, "oSR Manual 3D Wind Tunnel",
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               rect.right - rect.left, rect.bottom - rect.top,
                               nullptr, nullptr, instance, &app);
    if (!app.hwnd) {
        return 1;
    }
    ShowWindow(app.hwnd, show_cmd);

    MSG msg {};
    while (app.running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                app.running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!app.running) {
            break;
        }

        StepInput(app);
        Render(app);
        HDC dc = GetDCEx(app.hwnd, nullptr, DCX_CACHE | DCX_CLIPCHILDREN);
        Present(app, dc);
        ReleaseDC(app.hwnd, dc);
        Sleep(1);
    }
    return 0;
}
