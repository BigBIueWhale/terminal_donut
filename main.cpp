#include <curses.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstring>

struct Vec3 {
    float x, y, z;
};

static inline Vec3 rotateX(const Vec3& v, float a) {
    float c = std::cos(a), s = std::sin(a);
    return { v.x, c*v.y - s*v.z, s*v.y + c*v.z };
}
static inline Vec3 rotateY(const Vec3& v, float a) {
    float c = std::cos(a), s = std::sin(a);
    return {  c*v.x + s*v.z, v.y, -s*v.x + c*v.z };
}
static inline float dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline Vec3 normalize(Vec3 v) {
    float m = std::sqrt(dot(v, v));
    return (m > 0.0f) ? Vec3{ v.x/m, v.y/m, v.z/m } : Vec3{0,0,0};
}

struct FrameBuffer {
    int w, h;
    std::vector<char> chars;
    std::vector<float> depth; // z-buffer (stores 1/z for perspective)
    FrameBuffer(int W, int H) : w(W), h(H), chars(W*H, ' '), depth(W*H, -1e9f) {}
    void clear() {
        std::fill(chars.begin(), chars.end(), ' ');
        std::fill(depth.begin(), depth.end(), -1e9f);
    }
    void set(int x, int y, float invZ, char c) {
        int idx = y*w + x;
        if (invZ > depth[idx]) { depth[idx] = invZ; chars[idx] = c; }
    }
    char get(int x, int y) const { return chars[y*w + x]; }
};

struct CursesSession {
    CursesSession() {
        initscr();
        cbreak();
        noecho();
        curs_set(0);
        keypad(stdscr, TRUE);
        nodelay(stdscr, TRUE);
        timeout(0);           // non-blocking getch()
        clear();
    }
    ~CursesSession() { endwin(); }
};

static void renderTorus(FrameBuffer& fb, float Ax, float Ay) {
    fb.clear();

    // Virtual-space parameters (hard-coded, independent of terminal size)
    constexpr float R_major = 1.0f;  // center radius
    constexpr float R_minor = 0.5f;  // tube radius
    constexpr float K2 = 3.0f;       // camera distance
    const float K1 = 0.75f * std::min(static_cast<float>(fb.h), static_cast<float>(fb.w) / 2.0f);    // projection scale for virtual buffer
    constexpr float xCellAspect = 2.0f; // character cells are ~twice as tall as they are wide

    const Vec3 light = normalize({ -0.5f, 0.5f, -1.0f });
    static const char* shades = " .:-=+*#%@"; // darkest -> brightest
    const int shadeMax = static_cast<int>(std::strlen(shades) - 1);

    const float dTheta = 0.07f;
    const float dPhi   = 0.02f;

    for (float theta = 0.0f; theta < 2.0f * M_PI; theta += dTheta) {
        float ct = std::cos(theta), st = std::sin(theta);

        // Object-space normal (unit) for torus: proportional to {ct*cp, ct*sp, st}
        for (float phi = 0.0f; phi < 2.0f * M_PI; phi += dPhi) {
            float cp = std::cos(phi), sp = std::sin(phi);

            // Torus point in object space
            float circle = R_major + R_minor * ct;
            Vec3 pObj{ circle * cp, circle * sp, R_minor * st };
            Vec3 nObj{ R_minor * ct * cp, R_minor * ct * sp, R_minor * st }; // proportional; R_minor cancels after normalize

            // Rotate (virtual space)
            Vec3 p = rotateY(rotateX(pObj, Ax), Ay);
            Vec3 n = normalize(rotateY(rotateX(nObj, Ax), Ay));

            // Perspective projection into virtual buffer (keep aspect fix in X)
            float invZ = 1.0f / (K2 + p.z);
            float x_proj = p.x * invZ;
            float y_proj = p.y * invZ;

            int x = static_cast<int>(fb.w * 0.5f + K1 * xCellAspect * x_proj);
            int y = static_cast<int>(fb.h * 0.5f - K1 * y_proj);

            if (x < 0 || x >= fb.w || y < 0 || y >= fb.h) continue;

            // Simple Lambert lighting
            float lum = std::max(0.0f, dot(n, light));
            int shadeIdx = std::clamp(static_cast<int>(lum * shadeMax), 0, shadeMax);
            char ch = shades[shadeIdx];

            fb.set(x, y, invZ, ch);
        }
    }
}

static void blitVirtualToCurses(const FrameBuffer& fb) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // Uniform scale to preserve aspect ratio when mapping virtual buffer to terminal
    float sx = static_cast<float>(cols) / fb.w;
    float sy = static_cast<float>(rows) / fb.h;
    float s = std::max(sx, sy);

    float invS = 1.0f / s;
    int viewW = std::max(1, static_cast<int>(std::round(cols * invS)));
    int viewH = std::max(1, static_cast<int>(std::round(rows * invS)));
    int x0 = (fb.w - viewW) / 2;
    int y0 = (fb.h - viewH) / 2;

    // Fill background
    erase();

    // Nearest-neighbor upscaling
    for (int y = 0; y < rows; ++y) {
        int vy = std::clamp(y0 + static_cast<int>(y * invS), 0, fb.h - 1);
        for (int x = 0; x < cols; ++x) {
            int vx = std::clamp(x0 + static_cast<int>(x * invS), 0, fb.w - 1);
            char c = fb.get(vx, vy);
            mvaddch(y, x, c ? c : ' ');
        }
    }
    refresh();
}

int main() {
    CursesSession term;

    // A slightly wide virtual buffer looks sharper when scaled up
    int initRows, initCols;
    getmaxyx(stdscr, initRows, initCols);
    const int baseW = 160, baseH = 90;
    const float overscan = 1.15f;
    float aspect = (initRows > 0) ? static_cast<float>(initCols) / static_cast<float>(initRows) : 1.0f;
    int fbW = static_cast<int>(baseW * std::max(1.0f, aspect) * overscan);
    int fbH = static_cast<int>(baseH * std::max(1.0f, 1.0f / aspect) * overscan);
    FrameBuffer fb(fbW, fbH);
    int lastRows = initRows, lastCols = initCols;

    using clock = std::chrono::steady_clock;
    auto tPrev = clock::now();

    float Ax = 0.0f;  // rotation around X
    float Ay = 0.0f;  // rotation around Y

    const float spinX = 0.7f;  // rad/s
    const float spinY = 1.1f;  // rad/s

    while (true) {
        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        int curRows, curCols;
        getmaxyx(stdscr, curRows, curCols);
        if (curRows != lastRows || curCols != lastCols) {
            aspect = (curRows > 0) ? static_cast<float>(curCols) / static_cast<float>(curRows) : 1.0f;
            fbW = static_cast<int>(baseW * std::max(1.0f, aspect) * overscan);
            fbH = static_cast<int>(baseH * std::max(1.0f, 1.0f / aspect) * overscan);
            fb = FrameBuffer(fbW, fbH);
            lastRows = curRows; lastCols = curCols;
        }

        auto tNow = clock::now();
        std::chrono::duration<float> dt = tNow - tPrev;
        tPrev = tNow;

        Ax += spinX * dt.count();
        Ay += spinY * dt.count();

        renderTorus(fb, Ax, Ay);
        blitVirtualToCurses(fb);

        // Small delay to keep CPU reasonable
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}
