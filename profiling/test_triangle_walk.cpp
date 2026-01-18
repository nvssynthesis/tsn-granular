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