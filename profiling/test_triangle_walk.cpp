//
// Created by Nicholas Solem on 1/17/26.
//

// #include "TimbreSpace/TimbrePointTypes.h"
// #include "TimbreSpace/TimbreSpaceTriangulation.h"

#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>
#include "delaunator.hpp"


struct Point2D {
    double x, y;
    Point2D(double x_, double y_) : x(x_), y(y_) {}
};

delaunator::Delaunator buildDelaunator(const std::vector<Point2D>& points) {
    std::vector<double> coords;
    coords.reserve(points.size() * 2);
    for (const auto& p : points) {
        coords.push_back(p.x);
        coords.push_back(p.y);
    }
    return delaunator::Delaunator(coords);
}

Point2D getPoint(const delaunator::Delaunator& d, size_t idx) {
    return {d.coords[2 * idx], d.coords[2 * idx + 1]};
}

// barycentric method
bool pointInTriangle(const Point2D& p, const Point2D& a, const Point2D& b, const Point2D& c) {
    auto sign = [](const Point2D& p1, const Point2D& p2, const Point2D& p3) {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };

    double d1 = sign(p, a, b);
    double d2 = sign(p, b, c);
    double d3 = sign(p, c, a);

    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

// PLACEHOLDER: Triangle walking implementation
// Returns triangle index (i / 3 where i is the index into d.triangles)
std::optional<size_t> walkToTriangle(const delaunator::Delaunator& d,
                                     size_t startTriIdx,
                                     const Point2D& target) {
    // TODO: Implement actual walking algorithm
    // For now, this is a stub that will be replaced
    return std::nullopt;
}

// simple 3-point triangle (degenerate case)
void test_single_triangle() {
    std::cout << "Test 1: Single triangle..." << std::endl;

    std::vector<Point2D> points = {
        {0.0, 0.0},
        {1.0, 0.0},
        {0.5, 1.0}
    };

    auto d = buildDelaunator(points);

    // point inside the triangle
    Point2D inside(0.5, 0.3);

    // with only one triangle, any start point should work
    auto result = walkToTriangle(d, 0, inside);

    // for single triangle, should find it (when implemented)
    // for now, just verify the triangulation worked
    assert(d.triangles.size() == 3);
    std::cout << "  ✓ Single triangle test setup complete" << std::endl;
}

// simple grid of points
void test_grid_points() {
    std::cout << "Test 2: Grid of points..." << std::endl;

    // create 3x3 grid
    std::vector<Point2D> points;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            points.emplace_back(x, y);
        }
    }

    const auto d = buildDelaunator(points);

    // test point near center
    const Point2D center(1.0, 1.0);

    // try walking from different starting triangles
    for (size_t startTri = 0; startTri < d.triangles.size() / 3; ++startTri) {
        auto result = walkToTriangle(d, startTri, center);
        // when implemented, should find the containing triangle
    }

    assert(!d.triangles.empty());
    std::cout << "  ✓ Grid triangulation has " << d.triangles.size() / 3 << " triangles" << std::endl;
}

// point on edge (boundary case)
void test_point_on_edge() {
    std::cout << "Test 3: Point on edge..." << std::endl;

    std::vector<Point2D> points = {
        {0.0, 0.0},
        {2.0, 0.0},
        {1.0, 2.0},
        {1.0, 0.0}  // point on edge between first two
    };

    const auto d = buildDelaunator(points);

    const Point2D onEdge(1.0, 0.0);

    // should handle edge case gracefully
    auto result = walkToTriangle(d, 0, onEdge);

    std::cout << "  ✓ Edge case setup complete" << std::endl;
}

// point outside convex hull
void test_point_outside_hull() {
    std::cout << "Test 4: Point outside convex hull..." << std::endl;

    std::vector<Point2D> points = {
        {0.0, 0.0},
        {1.0, 0.0},
        {1.0, 1.0},
        {0.0, 1.0}
    };

    const auto d = buildDelaunator(points);

    const Point2D outside(5.0, 5.0);  // Far outside the unit square

    auto result = walkToTriangle(d, 0, outside);

    // should return nullopt when outside hull
    // assert(!result.has_value()); // Uncomment when implemented

    std::cout << "  ✓ Outside hull test setup complete" << std::endl;
}

// random points - walk from multiple starts should give same result
void test_consistency() {
    std::cout << "Test 5: Consistency across different start points..." << std::endl;

    // create random-ish points
    const std::vector<Point2D> points = {
        {0.2, 0.3}, {0.8, 0.1}, {0.5, 0.9},
        {0.1, 0.7}, {0.9, 0.6}, {0.4, 0.4},
        {0.7, 0.8}, {0.3, 0.2}
    };

    const auto d = buildDelaunator(points);

    const Point2D target(0.5, 0.5);

    // walk from different starting triangles
    std::optional<size_t> firstResult;

    for (size_t startTri = 0; startTri < d.triangles.size() / 3; ++startTri) {
        auto result = walkToTriangle(d, startTri, target);

        if (result.has_value()) {
            if (!firstResult.has_value()) {
                firstResult = result;
            } else {
                // all walks should end at the same triangle
                // assert(result.value() == firstResult.value()); // Uncomment when implemented
            }
        }
    }

    std::cout << "  ✓ Consistency test setup complete" << std::endl;
}

// performance test - many queries
void test_performance() {
    std::cout << "Test 6: Performance comparison..." << std::endl;

    // Create larger point set
    std::vector<Point2D> points;
    for (int i = 0; i < 100; ++i) {
        double angle = 2.0 * M_PI * i / 100.0;
        double radius = 1.0 + 0.3 * std::sin(5.0 * angle);  // Varying radius
        points.emplace_back(radius * std::cos(angle), radius * std::sin(angle));
    }

    const auto d = buildDelaunator(points);

    std::cout << "  Triangulation has " << d.triangles.size() / 3 << " triangles" << std::endl;

    // test points
    std::vector<Point2D> testPoints = {
        {0.0, 0.0}, {0.5, 0.5}, {-0.3, 0.4}, {0.2, -0.6}
    };

    // TODO: Time the walking algorithm vs brute force
    std::cout << "  ✓ Performance test setup complete" << std::endl;
}

int main() {
    std::cout << "=== Triangle Walking Algorithm Tests ===" << std::endl << std::endl;

    test_single_triangle();
    test_grid_points();
    test_point_on_edge();
    test_point_outside_hull();
    test_consistency();
    test_performance();

    std::cout << std::endl << "All test cases set up successfully!" << std::endl;
    std::cout << "Now implement the walkToTriangle function to make them pass." << std::endl;

    return 0;
}