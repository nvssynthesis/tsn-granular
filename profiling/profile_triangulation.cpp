#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include "TimbreSpace/TimbrePointTypes.h"
#include "TimbreSpace/TimbreSpaceTriangulation.h"

using namespace std::chrono;
using namespace nvs::timbrespace;

// Generate random test data
std::vector<Timbre5DPoint> generateRandomDatabase(size_t numPoints, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::vector<Timbre5DPoint> database;
    database.reserve(numPoints);

    for (size_t i = 0; i < numPoints; ++i) {
        Timbre5DPoint pt;
        pt[0] = dist(rng);
        pt[1] = dist(rng);
        pt[2] = dist(rng);
        pt[3] = dist(rng);
        pt[4] = dist(rng);
        database.push_back(pt);
    }

    return database;
}

// Generate random target points
std::vector<Timbre5DPoint> generateRandomTargets(size_t numTargets, std::mt19937& rng) {
    static constexpr float r = 1.5f;
    std::uniform_real_distribution<float> dist(-r, r); // Slightly outside to test edge cases

    std::vector<Timbre5DPoint> targets;
    targets.reserve(numTargets);

    for (size_t i = 0; i < numTargets; ++i) {
        Timbre5DPoint pt;
        pt[0] = dist(rng);
        pt[1] = dist(rng);
        pt[2] = dist(rng);
        pt[3] = dist(rng);
        pt[4] = dist(rng);
        targets.push_back(pt);
    }

    return targets;
}

// Build Delaunator triangulation from database
delaunator::Delaunator buildTriangulation(const std::vector<Timbre5DPoint>& database) {
    std::vector<double> coords;
    coords.reserve(database.size() * 2);

    for (const auto& pt : database) {
        Timbre2DPoint pt2d = get2D(pt);
        coords.push_back(pt2d.x());
        coords.push_back(pt2d.y());
    }

    return delaunator::Delaunator(coords);
}

struct BenchmarkResult {
    std::string name;
    size_t dbSize;
    size_t numQueries;
    double avgTimeMs;
    double minTimeMs;
    double maxTimeMs;
};

BenchmarkResult runBenchmark(const std::string& name,
                             const std::vector<Timbre5DPoint>& database,
                             const std::vector<Timbre5DPoint>& targets) {

    std::cout << "Running benchmark: " << name << " (DB size: " << database.size()
              << ", Queries: " << targets.size() << ")" << std::endl;

    // Build triangulation once
    auto triStart = high_resolution_clock::now();
    auto d = buildTriangulation(database);
    auto triEnd = high_resolution_clock::now();
    auto triTime = duration_cast<microseconds>(triEnd - triStart).count() / 1000.0;

    std::cout << "  Triangulation build time: " << triTime << " ms" << std::endl;

    // Warm up
    for (size_t i = 0; i < std::min(targets.size(), size_t(10)); ++i) {
        auto result = findPointsTriangulationBased(targets[i], database, d);
    }

    // Actual benchmark
    std::vector<double> queryTimes;
    queryTimes.reserve(targets.size());

    for (const auto& target : targets) {
        auto start = high_resolution_clock::now();
        auto result = findPointsTriangulationBased(target, database, d);
        auto end = high_resolution_clock::now();

        auto duration = duration_cast<nanoseconds>(end - start).count();
        queryTimes.push_back(duration / 1000.0); // Convert to microseconds
    }

    // Calculate statistics
    double sum = 0.0;
    double minTime = std::numeric_limits<double>::max();
    double maxTime = 0.0;

    for (double t : queryTimes) {
        sum += t;
        minTime = std::min(minTime, t);
        maxTime = std::max(maxTime, t);
    }

    double avgTime = sum / queryTimes.size();

    BenchmarkResult result;
    result.name = name;
    result.dbSize = database.size();
    result.numQueries = targets.size();
    result.avgTimeMs = avgTime / 1000.0;
    result.minTimeMs = minTime / 1000.0;
    result.maxTimeMs = maxTime / 1000.0;

    return result;
}

void printResults(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n=== Benchmark Results ===" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    std::cout << std::setw(30) << "Test Name"
              << std::setw(12) << "DB Size"
              << std::setw(12) << "Queries"
              << std::setw(15) << "Avg (ms)"
              << std::setw(15) << "Min (ms)"
              << std::setw(15) << "Max (ms)" << std::endl;
    std::cout << std::string(99, '-') << std::endl;

    for (const auto& r : results) {
        std::cout << std::setw(30) << r.name
                  << std::setw(12) << r.dbSize
                  << std::setw(12) << r.numQueries
                  << std::setw(15) << r.avgTimeMs
                  << std::setw(15) << r.minTimeMs
                  << std::setw(15) << r.maxTimeMs << std::endl;
    }
}

int main() {
    std::cout << "Triangulation Selection Profiler\n" << std::endl;

    std::random_device rd;
    std::mt19937 rng(rd());

    std::vector<BenchmarkResult> results;

    // Test different database sizes
    std::vector<size_t> dbSizes = {
        10,
        100,
        1000,
        10000,
        100000
    };
    const size_t numQueries = 10000;

    for (size_t dbSize : dbSizes) {
        auto database = generateRandomDatabase(dbSize, rng);
        auto targets = generateRandomTargets(numQueries, rng);

        std::string testName = "DB_" + std::to_string(dbSize);
        auto result = runBenchmark(testName, database, targets);
        results.push_back(result);
    }

    printResults(results);

    // Additional test: measure overhead of different parts
    std::cout << "\n=== Detailed Profiling (DB size: 1000) ===" << std::endl;
    auto testDb = generateRandomDatabase(1000, rng);
    auto testTargets = generateRandomTargets(100, rng);
    auto d = buildTriangulation(testDb);

    // Profile individual steps
    size_t insideHull = 0;
    size_t outsideHull = 0;

    for (const auto& target : testTargets) {
        Timbre2DPoint targetPoint = get2D(target);
        auto triangleOpt = findContainingTriangle(d, targetPoint);
        if (triangleOpt.has_value()) {
            insideHull++;
        } else {
            outsideHull++;
        }
    }

    std::cout << "Points inside hull: " << insideHull << std::endl;
    std::cout << "Points outside hull: " << outsideHull << std::endl;

    return 0;
}