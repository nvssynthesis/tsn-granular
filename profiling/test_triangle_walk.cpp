//
// Created by Nicholas Solem on 1/17/26.
//

// #include "TimbreSpace/TimbrePointTypes.h"
// #include "TimbreSpace/TimbreSpaceTriangulation.h"

#include <cassert>
#include <iostream>
#include <vector>
#include <set>
#include <unordered_set>
#include <cmath>
#include "delaunator.hpp"
#include "TimbreSpace/TimbrePointTypes.h"
#include "TimbreSpace/TimbreSpaceTriangulation.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

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

TEST_CASE("Delaunator triangle orientation is clockwise", "[orientation]") {
    std::vector<Point2D> largePoints;
    srand(423);
    for (int i = 0; i < 100; ++i) {
        float x = static_cast<float>(rand()) / RAND_MAX * 10.0f;
        float y = static_cast<float>(rand()) / RAND_MAX * 10.0f;
        largePoints.emplace_back(x, y);
    }
    auto d = buildDelaunator(largePoints);
    for (size_t i = 0; i < d.triangles.size(); i += 3) {
        const auto triPoints = TrianglePoints::create(d, d.triangles[i]);
        assert(triPoints != std::nullopt);
        const auto o = orientation(triPoints->p0, triPoints->p1, triPoints->p2);
        REQUIRE(o == Orientation_e::CW);
    }
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

TEST_CASE("pointInTriangle", "pointInTriangle") {
    std::vector<Point2D> points = {
        Point2D(0.0f, 0.0f),  // vertex 0
        Point2D(1.0f, 0.0f),  // vertex 1
        Point2D(0.0f, 1.0f),  // vertex 2
    };


    Point2D pointOnEdge = Point2D(0.5f, 0.0f);

    REQUIRE(pointInTriangle(pointOnEdge, points[0], points[1], points[2]));
}

TEST_CASE("getEdgeVertices tests", "[getEdgeVertices]") {
    // Simple triangle
    std::vector<Point2D> points = {
        Point2D(0.0f, 0.0f),  // vertex 0
        Point2D(1.0f, 0.0f),  // vertex 1
        Point2D(0.0f, 1.0f)   // vertex 2
    };
    auto d = buildDelaunator(points);

    SECTION("get all three edges of triangle 0") {
        size_t v0 = d.triangles[0];
        size_t v1 = d.triangles[1];
        size_t v2 = d.triangles[2];

        // Edge 0: from vertex 0 to vertex 1
        auto [e0_v1, e0_v2] = getEdgeVertices(d, 0, 0);
        REQUIRE(e0_v1 == v0);
        REQUIRE(e0_v2 == v1);

        // Edge 1: from vertex 1 to vertex 2
        auto [e1_v1, e1_v2] = getEdgeVertices(d, 0, 1);
        REQUIRE(e1_v1 == v1);
        REQUIRE(e1_v2 == v2);

        // Edge 2: from vertex 2 to vertex 0 (wraps around)
        auto [e2_v1, e2_v2] = getEdgeVertices(d, 0, 2);
        REQUIRE(e2_v1 == v2);
        REQUIRE(e2_v2 == v0);
    }

    SECTION("edge vertices are different") {
        auto [v1, v2] = getEdgeVertices(d, 0, 0);
        REQUIRE(v1 != v2);
    }

    SECTION("multiple triangles") {
        // If we have multiple triangles, each should give different edge vertices
        if (d.triangles.size() / 3 >= 2) {
            auto [tri0_v1, tri0_v2] = getEdgeVertices(d, 0, 0);
            auto [tri1_v1, tri1_v2] = getEdgeVertices(d, 1, 0);

            // The edges should be different (unless they share that specific edge)
            bool different = (tri0_v1 != tri1_v1) || (tri0_v2 != tri1_v2);
            REQUIRE(different);
        }
    }
    SECTION("large random triangulation - edge vertices form valid edges") {
    // Generate 100 random points
    std::vector<Point2D> largePoints;
    srand(42); // Fixed seed for reproducibility
    for (int i = 0; i < 100; ++i) {
        float x = static_cast<float>(rand()) / RAND_MAX * 10.0f;
        float y = static_cast<float>(rand()) / RAND_MAX * 10.0f;
        largePoints.emplace_back(x, y);
    }
    auto largeD = buildDelaunator(largePoints);

    // Check all edges of all triangles
    size_t numTriangles = largeD.triangles.size() / 3;
    for (size_t tri = 0; tri < numTriangles; ++tri) {
        for (size_t edgeIdx = 0; edgeIdx < 3; ++edgeIdx) {
            auto [v1, v2] = getEdgeVertices(largeD, tri, edgeIdx);

            // Vertices should be different
            REQUIRE(v1 != v2);

            // Vertices should be valid indices
            REQUIRE(v1 < largePoints.size());
            REQUIRE(v2 < largePoints.size());
        }
    }
}

    SECTION("large random triangulation - edge consistency") {
        // Generate 200 random points
        std::vector<Point2D> largePoints;
        srand(123);
        for (int i = 0; i < 200; ++i) {
            float x = static_cast<float>(rand()) / RAND_MAX * 20.0f;
            float y = static_cast<float>(rand()) / RAND_MAX * 20.0f;
            largePoints.emplace_back(x, y);
        }
        auto largeD = buildDelaunator(largePoints);

        size_t numTriangles = largeD.triangles.size() / 3;

        // For each triangle, verify that the three edges use exactly the three vertices
        for (size_t tri = 0; tri < numTriangles; ++tri) {
            auto [e0_v1, e0_v2] = getEdgeVertices(largeD, tri, 0);
            auto [e1_v1, e1_v2] = getEdgeVertices(largeD, tri, 1);
            auto [e2_v1, e2_v2] = getEdgeVertices(largeD, tri, 2);

            // Collect all 6 vertex references (each vertex appears twice)
            std::unordered_set<size_t> vertices;
            vertices.insert(e0_v1);
            vertices.insert(e0_v2);
            vertices.insert(e1_v1);
            vertices.insert(e1_v2);
            vertices.insert(e2_v1);
            vertices.insert(e2_v2);

            // Should have exactly 3 unique vertices
            REQUIRE(vertices.size() == 3);
        }
    }
}
TEST_CASE("pointOnOtherSide tests", "[pointOnOtherSide]") {
    // Simple triangle: (0,0), (1,0), (0,1)
    std::vector<Point2D> points = {
        Point2D(0.0f, 0.0f),  // vertex 0
        Point2D(1.0f, 0.0f),  // vertex 1
        Point2D(0.0f, 1.0f)   // vertex 2
    };
    auto d = buildDelaunator(points);

    SECTION("point outside triangle is on other side of at least one edge") {
        Point2D outside(2.0f, 2.0f);  // Clearly outside the triangle

        // Should be on "other side" of at least one edge
        bool onOtherSide = pointOnOtherSide(d, 0, 0, outside) ||
                          pointOnOtherSide(d, 0, 1, outside) ||
                          pointOnOtherSide(d, 0, 2, outside);

        REQUIRE(onOtherSide);
    }

    SECTION("point inside triangle is not on other side of any edge") {
        Point2D inside(0.2f, 0.2f);  // Inside the triangle

        // Should NOT be on "other side" of any edge
        REQUIRE_FALSE(pointOnOtherSide(d, 0, 0, inside));
        REQUIRE_FALSE(pointOnOtherSide(d, 0, 1, inside));
        REQUIRE_FALSE(pointOnOtherSide(d, 0, 2, inside));
    }

    SECTION("point on edge is considered other side") {
        const auto edge = getEdgeVertices(d, 0, 0);
        const auto v0 = getPointFromVertex(d, edge.first);
        const auto v1 = getPointFromVertex(d, edge.second);

        Point2D pointOnEdge = (v1 + v0) * 0.5;  // On edge 0 (from vertex 0 to vertex 1)

        // Point on the edge should be considered "other side" to allow crossing
        REQUIRE(pointOnOtherSide(d, 0, 0,pointOnEdge));
    }
    SECTION("point on edge is considered other side") {
        // Iterate through all triangles
        for (size_t tri = 0; tri < d.triangles.size() / 3; ++tri) {
            DYNAMIC_SECTION("Triangle " << tri) {
                // Test each edge of the triangle
                for (size_t edgeIdx = 0; edgeIdx < 3; ++edgeIdx) {
                    DYNAMIC_SECTION("Edge " << edgeIdx) {
                        const auto edge = getEdgeVertices(d, tri, edgeIdx);
                        const auto v0 = getPointFromVertex(d, edge.first);
                        const auto v1 = getPointFromVertex(d, edge.second);

                        Point2D pointOnEdge = (v1 + v0) * 0.5;  // Midpoint of edge

                        // Point on the edge should be considered "other side" to allow crossing
                        REQUIRE(pointOnOtherSide(d, tri, edgeIdx, pointOnEdge));
                    }
                }
            }
        }
    }
}

TEST_CASE("neighbor tests", "[neighbor]") {
    SECTION("No neighbor") {
        const std::vector points = {
            Point2D(0.0f, 0.0f),
            Point2D(1.0f, 0.0f),
            Point2D(0.0f, 1.0f)
        };
        auto d = buildDelaunator(points);
        const auto tri = d.triangles.front();
        const auto [v1, v2] = getEdgeVertices(d, tri, 0);
        const auto nonexistent = neighbor(d, tri, v1, v2);
        REQUIRE(nonexistent == delaunator::INVALID_INDEX);
    }
    SECTION("Neighbor found") {
        const std::vector points = {
            Point2D(0.0f, 0.0f),
            Point2D(1.0f, 0.0f),
            Point2D(0.0f, 1.0f),
            Point2D(1.0f, 1.0f)
        };
        auto d = buildDelaunator(points);
        const auto tri = d.triangles.front();

        bool someNeighborFound = false;
        for (auto he : d.halfedges) {
            const auto [v1, v2] = getEdgeVertices(d, tri, he);
            const auto existent = neighbor(d, tri, v1, v2);
            if (existent != delaunator::INVALID_INDEX) {
                someNeighborFound = true;
                break;
            }
        }
        REQUIRE(someNeighborFound);
    }
}

TEST_CASE("isNeighbor tests", "[isNeighbor]") {
    // Simple 2x2 grid: 4 points, 2 triangles
    std::vector<Point2D> points = {
        Point2D(0.0f, 0.0f),  // vertex 0
        Point2D(1.0f, 0.0f),  // vertex 1
        Point2D(0.0f, 1.0f),  // vertex 2
        Point2D(1.0f, 1.0f)   // vertex 3
    };
    auto d = buildDelaunator(points);

    REQUIRE(d.triangles.size() == 6);

    SECTION("triangle has neighbor across internal edge") {
        bool hasNeighbor = false;
        for (size_t tri = 0; tri < d.triangles.size() / 3; ++tri) {
            if (tri != 0 && isNeighbor(d, 0, tri)) {
                hasNeighbor = true;
                break;
            }
        }
        REQUIRE(hasNeighbor);
    }

    SECTION("edge not in triangle returns false") {
        // Try an edge that definitely isn't in triangle 0 by using wrong direction
        // or vertices not in this triangle
        bool result = isNeighbor(d, 0, 100);
        REQUIRE_FALSE(result);
    }

    SECTION("triangle is not neighbor to itself") {
        REQUIRE_FALSE(isNeighbor(d, 0, 0));
    }

    SECTION("triangles in 2x2 grid - check neighbor relationships") {
        size_t numTriangles = d.triangles.size() / 3;
        REQUIRE(numTriangles >= 2);

        // Count how many neighbors triangle 0 has
        size_t neighborCount = 0;
        for (size_t tri = 1; tri < numTriangles; ++tri) {
            if (isNeighbor(d, 0, tri)) {
                neighborCount++;
            }
        }

        // Triangle 0 should have at least 1 neighbor
        REQUIRE(neighborCount >= 1);
    }
}



TEST_CASE("rememberingStochasticWalk basic tests", "[stochasticWalk]") {
    {
        // 3x3 grid
        std::vector<Point2D> points;
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                points.emplace_back(static_cast<float>(x), static_cast<float>(y));
            }
        }
        const auto d = buildDelaunator(points);

        SECTION("point already in starting triangle") {
            // Find a triangle and a point clearly inside it
            const Point2D inside(0.5f, 0.2f);

            // Find which triangle contains this point
            size_t containingTri = SIZE_MAX;
            for (size_t tri = 0; tri < d.triangles.size() / 3; ++tri) {
                auto triPts = TrianglePoints::create(d, tri);
                assert(triPts != std::nullopt);
                if (pointInTriangle(inside, triPts->p0, triPts->p1, triPts->p2)) {
                    containingTri = tri;
                    break;
                }
            }
            REQUIRE(containingTri != SIZE_MAX);

            auto containingPoints = TrianglePoints::create(d, containingTri);

            // Starting from the correct triangle should immediately return it
            const auto result = rememberingStochasticWalk(d, inside, containingTri);
            REQUIRE(result.has_value());
            auto algoDetectedContainingPoints = TrianglePoints::create(d, *result);

            REQUIRE(result == containingTri);
        }

        SECTION("point in adjacent triangle") {
            // Start from triangle 0, look for point in a neighboring triangle
            Point2D target(1.3f, 1.3f);

            auto result = rememberingStochasticWalk(d, target, 0);
            REQUIRE(result.has_value());
            // Verify result contains the point
            auto tri = TrianglePoints::create(d, *result);
            REQUIRE(tri != std::nullopt);
            REQUIRE(pointInTriangle(target, tri->p0, tri->p1, tri->p2));
        }

        SECTION("point outside convex hull") {
            Point2D outside(10.0f, 10.0f);

            auto result = rememberingStochasticWalk(d, outside, 0);
            REQUIRE_FALSE(result.has_value());
        }
    }

    SECTION("convergence from arbitrary triangle in big set") {
        std::vector<Point2D> largePoints;
        srand(1423);
        for (int i = 0; i < 100; ++i) {
            float x = static_cast<float>(rand()) / RAND_MAX * 10.0f;
            float y = static_cast<float>(rand()) / RAND_MAX * 10.0f;
            largePoints.emplace_back(x, y);
        }
        auto d_big = buildDelaunator(largePoints);
        for (size_t tri = 0; tri < d_big.triangles.size() / 3; ++tri) {
            DYNAMIC_SECTION("Triangle " << tri)
            {
                const auto target = Timbre2DPoint(static_cast<float>(rand()) / RAND_MAX * 10.0f,
                                                  static_cast<float>(rand()) / RAND_MAX * 10.0f);
                auto result = rememberingStochasticWalk(d_big, target, tri);
                REQUIRE(result.has_value());
                auto triPoints = TrianglePoints::create(d_big, *result);
                REQUIRE(pointInTriangle(target, triPoints->p0, triPoints->p1, triPoints->p2));
            }
        }
    }
}

TEST_CASE("hybridWalk basic tests", "[hybridWalk]") {
    {
        // 3x3 grid
        std::vector<Point2D> points;
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                points.emplace_back(static_cast<float>(x), static_cast<float>(y));
            }
        }
        auto d = buildDelaunator(points);

        std::cout << "All triangles in triangulation:" << std::endl;
        for (size_t tri = 0; tri < d.triangles.size() / 3; ++tri) {
            auto t = TrianglePoints::create(d, tri);
            assert(t != std::nullopt);
            std::cout << "  Triangle " << tri << ": "
                    << t->p0.x() << "," << t->p0.y() << " | "
                    << t->p1.x() << "," << t->p1.y() << " | "
                    << t->p2.x() << "," << t->p2.y();
            if (pointInTriangle(Point2D(1.3f, 1.3f), t->p0, t->p1, t->p2)) {
                std::cout << " <- CONTAINS (1.3, 1.3)";
            }
            std::cout << std::endl;
        }

        SECTION("point at vertex - early exit") {
            // Point at vertex 0 (0,0), starting from triangle containing it
            Point2D atVertex(0.0f, 0.0f);

            // Find a triangle containing vertex 0
            size_t startTri = 0;
            for (size_t tri = 0; tri < d.triangles.size() / 3; ++tri) {
                size_t t0 = tri * 3;
                if (d.triangles[t0] == 0 || d.triangles[t0 + 1] == 0 || d.triangles[t0 + 2] == 0) {
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
            REQUIRE(pointInTriangle(center, tri->p0, tri->p1, tri->p2));
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
            assert(tri != std::nullopt);
            INFO("Result triangle: " << tri->p0.x() << "," << tri->p0.y() << " | "
                    << tri->p1.x() << "," << tri->p1.y() << " | "
                    << tri->p2.x() << "," << tri->p2.y());
            REQUIRE(pointInTriangle(offCenter, tri->p0, tri->p1, tri->p2));
        }

        SECTION("point outside convex hull") {
            Point2D outside(10.0f, 10.0f);

            auto result = hybridWalk(d, 0, outside);
            // Should return nullopt when point is outside
            REQUIRE_FALSE(result.has_value());
        }
    }
    SECTION("a bunch of random points") {
        std::vector<Point2D> largePoints;
        srand(423);
        for (int i = 0; i < 1000; ++i) {
            float x = static_cast<float>(rand() * 2.f - 1.f) / RAND_MAX * 1000.0f;
            float y = static_cast<float>(rand() * 2.f - 1.f) / RAND_MAX * 1000.0f;
            largePoints.emplace_back(x, y);
        }
        auto d = buildDelaunator(largePoints);

        for (size_t tri = 0; tri < d.triangles.size(); ++tri) {
            DYNAMIC_SECTION("Triangle " << tri)
            {
                size_t startTri = rand() * d.triangles.size();
                const auto target = Point2D(static_cast<float>(rand() * 2.f - 1.f) / RAND_MAX * 1000.0f,
                                            static_cast<float>(rand() * 2.f - 1.f) / RAND_MAX * 1000.0f);
                const auto result = hybridWalk(d, startTri, target);
                REQUIRE(result.has_value());
                const auto triPoints = TrianglePoints::create(d, *result);
                REQUIRE(pointInTriangle(target, triPoints->p0, triPoints->p1, triPoints->p2));
            }
        }
    }
}