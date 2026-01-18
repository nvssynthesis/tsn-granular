//
// Created by Nicholas Solem on 1/17/26.
//

// #include "TimbreSpace/TimbrePointTypes.h"
// #include "TimbreSpace/TimbreSpaceTriangulation.h"

#include <cassert>
#include <iostream>
#include <vector>
#include <set>
#include <cmath>
#include "delaunator.hpp"
#include "TimbreSpace/TimbrePointTypes.h"
#include "TimbreSpace/TimbreSpaceTriangulation.h"
#include <catch2/catch_test_macros.hpp>

using namespace nvs::timbrespace;
using Point2D = Timbre2DPoint;

delaunator::Delaunator buildDelaunator(const std::vector<Point2D>& points) {
    std::vector<double> coords;
    coords.reserve(points.size() * 2);
    for (const auto& p : points) {
        coords.push_back(p.x());
        coords.push_back(p.y());
    }
    return delaunator::Delaunator(coords);
}

TEST_CASE("Helper function tests", "[helpers]") {
    // Simple 2x2 grid: 4 points, 2 triangles
    std::vector<Point2D> points = {
        Point2D(0.0f, 0.0f),  // vertex 0
        Point2D(1.0f, 0.0f),  // vertex 1
        Point2D(0.0f, 1.0f),  // vertex 2
        Point2D(1.0f, 1.0f)   // vertex 3
    };
    auto d = buildDelaunator(points);

    SECTION("getPointFromVertex") {
        auto p0 = getPointFromVertex(d, 0);
        REQUIRE(p0.x() == 0.0f);
        REQUIRE(p0.y() == 0.0f);

        auto p3 = getPointFromVertex(d, 3);
        REQUIRE(p3.x() == 1.0f);
        REQUIRE(p3.y() == 1.0f);
    }

    SECTION("getVertexIndex") {
        Point2D p(1.0f, 0.0f);
        auto idx = getVertexIndex(d, p);
        REQUIRE(idx == 1);

        Point2D pNotFound(5.0f, 5.0f);
        REQUIRE(getVertexIndex(d, pNotFound) == SIZE_MAX);
    }

    SECTION("findHalfedge") {
        // Get first triangle's vertices
        size_t v0 = d.triangles[0];
        size_t v1 = d.triangles[1];

        // Should find the halfedge from v0 to v1 in triangle 0
        auto he = findHalfedge(d, 0, v0, v1);
        REQUIRE(he != SIZE_MAX);
        REQUIRE(d.triangles[he] == v0);

        // Should not find edge from v0 to v0
        REQUIRE(findHalfedge(d, 0, v0, v0) == SIZE_MAX);
    }

    SECTION("getThirdVertex") {
        size_t v0 = d.triangles[0];
        size_t v1 = d.triangles[1];
        size_t v2 = d.triangles[2];

        auto third = getThirdVertex(d, 0, v0, v1);
        REQUIRE(third == v2);

        // Invalid vertices should return SIZE_MAX
        REQUIRE(getThirdVertex(d, 0, 99, 100) == SIZE_MAX);
    }
}

TEST_CASE("hybridWalk basic tests", "[hybridWalk]") {
    // 3x3 grid
    std::vector<Point2D> points;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            points.emplace_back(static_cast<float>(x), static_cast<float>(y));
        }
    }
    auto d = buildDelaunator(points);

    SECTION("point at vertex - early exit") {
        // Point at vertex 0 (0,0), starting from triangle containing it
        Point2D atVertex(0.0f, 0.0f);

        // Find a triangle containing vertex 0
        size_t startTri = 0;
        for (size_t tri = 0; tri < d.triangles.size() / 3; ++tri) {
            size_t t0 = tri * 3;
            if (d.triangles[t0] == 0 || d.triangles[t0+1] == 0 || d.triangles[t0+2] == 0) {
                startTri = tri;
                break;
            }
        }

        auto result = hybridWalk(d, startTri, atVertex);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == startTri);
    }

    SECTION("point in center of grid") {
        // Point (1,1) is at center, should work from any starting triangle
        Point2D center(1.0f, 1.0f);

        auto result = hybridWalk(d, 0, center);
        REQUIRE(result.has_value());

        // Verify the result triangle contains the point
        auto tri = TrianglePoints::create(d, *result);
        REQUIRE(pointInTriangle(center, tri.p0, tri.p1, tri.p2));
    }

    SECTION("point slightly off-center - EXPECTED TO FAIL without stochastic walk") {
        // This point is NOT on a vertex or edge, so it needs the final
        // remembering_stochastic_walk to converge properly
        Point2D offCenter(1.3f, 1.3f);

        auto result = hybridWalk(d, 0, offCenter);
        REQUIRE(result.has_value());

        // This SHOULD fail because we're just returning currentTri
        // without the final stochastic walk refinement
        auto tri = TrianglePoints::create(d, *result);
        INFO("Result triangle: " << tri.p0.x() << "," << tri.p0.y() << " | "
             << tri.p1.x() << "," << tri.p1.y() << " | "
             << tri.p2.x() << "," << tri.p2.y());
        REQUIRE(pointInTriangle(offCenter, tri.p0, tri.p1, tri.p2));
    }

    SECTION("point outside convex hull") {
        Point2D outside(10.0f, 10.0f);

        auto result = hybridWalk(d, 0, outside);
        // Should return nullopt when point is outside
        REQUIRE_FALSE(result.has_value());
    }
}