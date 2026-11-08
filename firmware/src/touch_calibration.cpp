#include "touch_calibration.h"

#include <Preferences.h>
#include <math.h>

#include "board_cyd.h"

namespace touch_cal {
namespace {

constexpr const char* NVS_NAMESPACE = "touch";
constexpr const char* NVS_KEY       = "cal";
constexpr uint32_t    MAGIC         = 0x54434131;   // "TCA1" - bump if Matrix layout changes

struct Stored {
    uint32_t magic;
    Matrix   m;
};

double det3(double a, double b, double c,
            double d, double e, double f,
            double g, double h, double i) {
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

// Least-squares fit of s = a*x + b*y + c (normal equations, Cramer's rule).
bool fitAxis(const Point* raw, const float* s, int n, float& a, float& b, float& c) {
    double sxx = 0, sxy = 0, sx = 0, syy = 0, sy = 0, sxs = 0, sys = 0, ss = 0;
    for (int i = 0; i < n; ++i) {
        const double x = raw[i].x, y = raw[i].y, v = s[i];
        sxx += x * x; sxy += x * y; sx += x;
        syy += y * y; sy += y;
        sxs += x * v; sys += y * v; ss += v;
    }
    const double det = det3(sxx, sxy, sx, sxy, syy, sy, sx, sy, n);
    if (fabs(det) < 1e-9 * sxx * syy * n) return false;   // collinear / repeated points
    a = static_cast<float>(det3(sxs, sxy, sx, sys, syy, sy, ss, sy, n) / det);
    b = static_cast<float>(det3(sxx, sxs, sx, sxy, sys, sy, sx, ss, n) / det);
    c = static_cast<float>(det3(sxx, sxy, sxs, sxy, syy, sys, sx, sy, ss) / det);
    return true;
}

bool plausible(const Matrix& m) {
    const float vals[] = {m.ax, m.bx, m.cx, m.ay, m.by, m.cy};
    for (float v : vals) if (!isfinite(v)) return false;
    // The middle of the raw range must land roughly on screen.
    const Point p = map(m, 2000, 2000);
    return p.x > -40 && p.x < board::SCREEN_W + 40 && p.y > -40 && p.y < board::SCREEN_H + 40;
}

}  // namespace

Matrix defaults() {
    // Fallback only (used until a calibration is stored): the first verified
    // 4-point fit on the dev unit (5.1 px centre error). Both axes run in the
    // same direction as the screen; X and Y are not swapped.
    return Matrix{0.09084f, -0.00058f, -14.41f, 0.00067f, 0.06752f, -14.69f};
}

bool solve(const Point* raw, const Point* screen, int n, Matrix& out) {
    if (n < 3) return false;
    float sx[8], sy[8];
    if (n > 8) n = 8;
    for (int i = 0; i < n; ++i) { sx[i] = screen[i].x; sy[i] = screen[i].y; }
    Matrix m{};
    if (!fitAxis(raw, sx, n, m.ax, m.bx, m.cx)) return false;
    if (!fitAxis(raw, sy, n, m.ay, m.by, m.cy)) return false;
    if (!plausible(m)) return false;
    out = m;
    return true;
}

Point map(const Matrix& m, float rx, float ry) {
    return {m.ax * rx + m.bx * ry + m.cx, m.ay * rx + m.by * ry + m.cy};
}

bool load(Matrix& out) {
    Preferences prefs;
    // Opened read-write: a read-only open of a never-created namespace logs an error.
    if (!prefs.begin(NVS_NAMESPACE, false)) return false;
    Stored st{};
    size_t n = 0;
    if (prefs.isKey(NVS_KEY) && prefs.getBytesLength(NVS_KEY) == sizeof(st))
        n = prefs.getBytes(NVS_KEY, &st, sizeof(st));
    prefs.end();
    if (n != sizeof(st) || st.magic != MAGIC || !plausible(st.m)) return false;
    out = st.m;
    return true;
}

bool save(const Matrix& m) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return false;
    const Stored st{MAGIC, m};
    const bool ok = prefs.putBytes(NVS_KEY, &st, sizeof(st)) == sizeof(st);
    prefs.end();
    return ok;
}

void erase() {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) { prefs.remove(NVS_KEY); prefs.end(); }
}

}  // namespace touch_cal
