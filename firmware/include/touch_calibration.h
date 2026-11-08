#pragma once
// Raw XPT2046 coordinates -> screen pixels via an affine transform:
//   sx = ax*rx + bx*ry + cx
//   sy = ay*rx + by*ry + cy
// Fitted by least squares from >= 3 points, so it handles inverted axes,
// swapped axes and slight panel rotation. Persisted in NVS (survives reflashing).

#include <stdint.h>

namespace touch_cal {

struct Matrix {
    float ax, bx, cx;
    float ay, by, cy;
};

struct Point {
    float x, y;
};

Matrix defaults();   // from measured ranges on the dev unit (docs/hardware.md)

// Fit raw[i] -> screen[i]. Returns false if the points are degenerate.
bool solve(const Point* raw, const Point* screen, int n, Matrix& out);

Point map(const Matrix& m, float rx, float ry);   // unclamped

bool load(Matrix& out);   // false if nothing stored or stored data fails sanity checks
bool save(const Matrix& m);
void erase();

}  // namespace touch_cal
