#include "StringHelpers.h"
#include <fmt/core.h>

namespace nvs {


template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 6)
{
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return std::move(out).str();
}

std::string str(const timbrespace::Timbre2DPoint &p) {
    return std::string("(") +
        to_string_with_precision(p.x(), 2) +
            std::string(", ") +
                to_string_with_precision(p.y(), 2) +
                    std::string(")");
}
std::string str(const TrianglePoints &tri) {
    return std::string("(") +
        str(tri.p0) + std::string(", ") +
            str(tri.p1) + std::string(", ") +
                str(tri.p2) +
                    std::string(")");
}

void printTriangles(const delaunator::Delaunator& d) {
    fmt::print("All triangles in triangulation: \n");
    for (size_t t = 0; t < d.triangles.size(); t += 3) {
        const auto p0 = timbrespace::Timbre2DPoint(d.coords[d.triangles[t + 0] * 2], d.coords[d.triangles[t + 0] * 2 + 1]);
        const auto p1 = timbrespace::Timbre2DPoint(d.coords[d.triangles[t + 1] * 2], d.coords[d.triangles[t + 1] * 2 + 1]);
        const auto p2 = timbrespace::Timbre2DPoint(d.coords[d.triangles[t + 2] * 2], d.coords[d.triangles[t + 2] * 2 + 1]);
        fmt::print("({}, {}, {})\n", str(p0), str(p1), str(p2));
    }
}
void printTriangleIdx(size_t idx) {
    fmt::print("{}\n", idx);
}
void printLine(const timbrespace::Timbre2DPoint &p, const timbrespace::Timbre2DPoint &q) {
    fmt::print("{},{}\n", str(p), str(q));
}


} // namespace nvs