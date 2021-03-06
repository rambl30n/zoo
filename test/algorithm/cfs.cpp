#include <zoo/algorithm/cfs.h>
#include <array>
#include <vector>
#include <zoo/util/container_insertion.h>

#include <zoo/util/range_equivalence.h>

using zoo::operator==;

#include <catch2/catch.hpp>


template<int... Pack>
auto indices_to_array(std::integer_sequence<int, Pack...>)
{
    return std::array{Pack...};
}

template<int Size>
std::array<int, Size> make_integer_array() {
    return indices_to_array(std::make_integer_sequence<int, Size>{});
}

TEST_CASE("Cache friendly search conversion", "[cfs][conversion]") {
    SECTION("Empty") {
        std::vector<int> empty, output;
        zoo::transformToCFS(back_inserter(output), cbegin(empty), cend(empty));
        REQUIRE(empty == output);
    }
    SECTION("Single element") {
        std::array input{77};
        std::vector<int> output;
        zoo::transformToCFS(back_inserter(output), cbegin(input), cend(input));
        REQUIRE(input == output);
    }
    SECTION("Two elements") {
        const std::array input{77, 88}, expected{88, 77};
        std::vector<int> output;
        zoo::transformToCFS(back_inserter(output), cbegin(input), cend(input));
        REQUIRE(expected == output);
    }
    SECTION("Three elements") {
        const std::array input{77, 88, 99}, expected{88, 77, 99};
        std::vector<int> output;
        zoo::transformToCFS(back_inserter(output), cbegin(input), cend(input));
        REQUIRE(expected == output);
    }
    SECTION("Ten elements (excess of 3)") {
        const std::array
            input{0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
            expected{6, 3, 8, 1, 5, 7, 9, 0, 2, 4};
        std::vector<int> output;
        zoo::transformToCFS(back_inserter(output), cbegin(input), cend(input));
        REQUIRE(expected == output);
    }
    SECTION("Exactly power of 2") {
        const std::array
            input = make_integer_array<8>(),
            expected{4, 2, 6, 1, 3, 5, 7, 0};
        std::vector<int> output;
        zoo::transformToCFS(back_inserter(output), cbegin(input), cend(input));
        REQUIRE(expected == output);
    }
    SECTION("Deficit of 3 to 15 (12 elements, excess of 5 to 7)") {
        const std::array
            input = make_integer_array<12>(),
            expected{7, 3, 10, 1, 5, 9, 11, 0, 2, 4, 6, 8};
            /*  7
                3     10
                1  5  9  11
                02 46 8 */
        std::vector<int> output;
        zoo::transformToCFS(back_inserter(output), cbegin(input), cend(input));
        REQUIRE(expected == output);
        //std::cout << output << std::endl;
    } 
}

TEST_CASE("Cache friendly search validation", "[cfs][search][validator]") {
    SECTION("Validation of empty cfs") {
        std::array<int, 0> empty;
        REQUIRE(zoo::validHeap(empty));
    }
    SECTION("Validation of single element") {
        std::array singleElement{1};
        REQUIRE(zoo::validHeap(singleElement));
    }
    SECTION("run of equal elements") {
        std::array runOfEqual{0, 1, 1, 1, 1, 2};
        std::vector<int> cfs;
        zoo::transformToCFS(
            back_inserter(cfs), runOfEqual.begin(), runOfEqual.end()
        );
        REQUIRE(zoo::validHeap(cfs));
    }
    SECTION("General validation") {
        std::array lookup{14, 6, 20, 2, 10, 18, 22, 0, 4, 8, 12, 16};
        REQUIRE(zoo::validHeap(lookup));
    }
    SECTION("Invalid lower tree") {
        std::array invalid20{14, 6, 18, 2, 10, 20, 22, 0, 4, 8, 12, 16};
            /*  14
                6         18
                2   10    20# 22
                0 4 8  12 16 */

        REQUIRE(not(zoo::validHeap(invalid20)));
    }
    SECTION("Invalid higher tree") {
        std::array invalid10{14, 6, 20, 2, 12, 18, 22, 0, 4, 8, 10, 16};
            /*  14
                6          20
                2   12     18 22
                0 4 8  10# 16 */

        REQUIRE(not(zoo::validHeap(invalid10)));
    }
}

TEST_CASE("Cache friendly search lookup", "[cfs][search]") {
    SECTION("Empty search") {
        std::array<int, 0> empty;
        const auto end = cend(empty);
        auto s = zoo::cfsLowerBound(cbegin(empty), end, 1);
        REQUIRE(end == s); 
    }
    SECTION("Search on a single element space") {
        std::array single{1};
        const auto end = cend(single), begin = cbegin(single);
        REQUIRE(zoo::cfsLowerBound(begin, end, 1) == begin);
        REQUIRE(zoo::cfsLowerBound(begin, end, 0) == begin);
        REQUIRE(zoo::cfsLowerBound(begin, end, 2) == end);
    }
    SECTION("General search") {
        //std::array lookup{7, 3, 10, 1, 5, 9, 11, 0, 2, 4, 6, 8};
        std::array lookup{14, 6, 20, 2, 10, 18, 22, 0, 4, 8, 12, 16};
            /*  14
                6         20
                2   10    18 22
                0 4 8  12 16 */
        auto b{cbegin(lookup)}, e{cend(lookup)};
        SECTION("Element present") {
            REQUIRE(18 == *zoo::cfsLowerBound(b, e, 18));
        }
        SECTION("Search for less than minimum") {
            REQUIRE(0 == *zoo::cfsLowerBound(b, e, -1));
        }
        SECTION("Search for higher than maximum") {
            REQUIRE(zoo::cfsLowerBound(b, e, 23) == e);
        }
        SECTION("Search for element higher than high-branch leaf (12)") {
            REQUIRE(14 == *zoo::cfsLowerBound(b, e, 13));
        }
        SECTION("Search for element lower than high-branch leaf (12)") {
            REQUIRE(12 == *zoo::cfsLowerBound(b, e, 11));
        }
        SECTION("Search for element lower than low-branch leaf (16)") {
            REQUIRE(16 == *zoo::cfsLowerBound(b, e, 15));
        }
        SECTION("Search for element higher than low-branch leaf (16)") {
            REQUIRE(18 == *zoo::cfsLowerBound(b, e, 17));
        }
    }
    SECTION("Upper bound") {
        std::array lookup{14, 6, 20, 2, 10, 18, 22, 0, 4, 8, 12, 16};
            /*  14
                6         20
                2   10    18 22
                0 4 8  12 16 */
        auto b{cbegin(lookup)}, e{cend(lookup)};
        SECTION("Successor of root") {
            REQUIRE(16 == *zoo::cfsHigherBound(b, e, 14));
        }
        SECTION("Epsilon after root") {
            REQUIRE(16 == *zoo::cfsHigherBound(b, e, 15));
        }
        SECTION("Last element") {
            REQUIRE(zoo::cfsHigherBound(b, e, 22) == e);
        }
        SECTION("Successor of high leaf") {
            REQUIRE(14 == *zoo::cfsHigherBound(b, e, 12));
        }
        SECTION("Successor of epsilon after high leaf") {
            REQUIRE(14 == *zoo::cfsHigherBound(b, e, 13));
        }
        SECTION("Successor of low leaf") {
            REQUIRE(10 == *zoo::cfsHigherBound(b, e, 8));
        }
        SECTION("Successor of parent of high leaf") {
            REQUIRE(12 == *zoo::cfsHigherBound(b, e, 10));
        }
        SECTION("Successor of past the end") {
            REQUIRE(zoo::cfsHigherBound(b, e, 25) == e);
        }
    }
}

struct Equivalence {
    int number;
    char occurrence;
    bool operator<(Equivalence o) const { return number < o.number; }
};

bool eq(Equivalence l, Equivalence r) {
    return l.number == r.number && l.occurrence == r.occurrence;
}

TEST_CASE(
    "Cache friendly search lookup with repetitions",
    "[cfs][search][range][equalRange]"
) {
    SECTION("CFS equal ranges") {
        std::array<Equivalence, 9> raw
        {{
            {1, 'a'},
            // 0
            {4, 'a'}, {4, 'b'}, {4, 'c'}, {4, 'd'},
            // 1       2         3         4
            {6, 'a'},
            // 5
            {8, 'a'}, {8, 'b'}, {8, 'c'},
            // 6       7         8
        }};
        std::vector<Equivalence> cfs;
        zoo::transformToCFS(back_inserter(cfs), cbegin(raw), cend(raw));
        /* 6a
           0
           4c       8b
           1        2
           4a    4d 8a 8c
           3     4  5  6
           1a 4b
           7  8 */
        REQUIRE(zoo::validHeap(cfs));
        auto b{cfs.begin()}, e{cfs.end()};
        auto fours = zoo::cfsEqualRange(b, e, Equivalence{4, 'x'});
        REQUIRE(eq(*fours.first, raw[1]));
        REQUIRE(eq(*fours.second, raw[5]));
        auto eights = zoo::cfsEqualRange(b, e, Equivalence{8, 'x'});
        REQUIRE(eq(*eights.first, raw[6]));
        REQUIRE(eights.second == e);
    }
    SECTION("CFS with repetitions") {
        std::array hasRepetitions{1, 4, 4, 6, 8, 8, 10};
                                /*0  1  2  3  4  5  6
                                  3
                                  1  5
                                  02 46
                                  6, 4, 6, 1, 4, 6, 8
                              */
        std::vector<int> cfs;
        zoo::transformToCFS(
            back_inserter(cfs), hasRepetitions.begin(), hasRepetitions.end()
        );
        REQUIRE(std::array{6, 4, 8, 1, 4, 8, 10} == cfs);
                        /* 6
                           4   8
                           1 4 8 10*/
        auto b{cbegin(cfs)}, e{cend(cfs)};
        SECTION("Finds the global minimum") {
            REQUIRE(1 == *zoo::cfsLowerBound(b, e, 1));
        }
        SECTION("Finds position in root of lower branch") {
            auto rootOfSubtreeIndex1{zoo::cfsLowerBound(b, e, 4)};
            REQUIRE(4 == *rootOfSubtreeIndex1);
            REQUIRE(rootOfSubtreeIndex1 == b + 1);
        }
        SECTION("Goes to low leaf") {
            auto lowLeafIndex5 = zoo::cfsLowerBound(b, e, 8);
            REQUIRE(8 == *lowLeafIndex5);
            REQUIRE(lowLeafIndex5 == b + 5);
        }
        SECTION("Search elements not found") {
            SECTION("Search lower than minimum") {
                REQUIRE(1 == *zoo::cfsLowerBound(b, e, 0));
            }
            SECTION("Search between low leaf and its parent") {
                REQUIRE(&*zoo::cfsLowerBound(b, e, 3) == &*(b + 1));
            }
            SECTION("Search epsilon over high leaf") {
                REQUIRE(6 == *zoo::cfsLowerBound(b, e, 5));
            }
            SECTION("Search epsilon below repetitions") {
                REQUIRE(zoo::cfsLowerBound(b, e, 7) == b + 5);
            }
            SECTION("Search past the end") {
                REQUIRE(zoo::cfsLowerBound(b, e, 11) == e);
            }
        }
        SECTION("Search element in bend lower to higher") {
            cfs[0] = 4; cfs[1] = 3;
            /* 4
               3   8
               1 4 8 10*/
            REQUIRE(zoo::validHeap(cfs));
            auto elbow = zoo::cfsLowerBound(b, e, 4);
            REQUIRE(4 == *elbow);
            REQUIRE(elbow == b + 4);
        }
    }
}
