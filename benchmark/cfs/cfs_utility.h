#ifndef ZOO_BENCHMARK_CFS_UTILITY
#define ZOO_BENCHMARK_CFS_UTILITY

#include <vector>
#include <random>

extern std::mt19937 gen;
extern std::uniform_int_distribution<int> dist;

std::vector<int> linear_vector(int n);

int randomTwo30();

std::vector<int> makeRandomVector(int size, int range = 0);

constexpr auto RangeLow = 10000;
constexpr auto RangeHigh = RangeLow * 10000;

#endif
