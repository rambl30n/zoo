#pragma once

#include "any.h"

#include <catch2/catch.hpp>

struct Destructor {
    int *ptr;

    Destructor(int *p): ptr(p) {}
    Destructor(const Destructor &) = default;
    Destructor(Destructor &&) = default;

    ~Destructor() { *ptr = 1; }
};

struct alignas(2 * sizeof(void *)) D2: Destructor
{ using Destructor::Destructor; };

struct Big { double a, b; };

struct Moves {
    enum Kind {
        DEFAULT,
        COPIED,
        MOVING,
        MOVED
    } kind;

    Moves(): kind{DEFAULT} {}

    Moves(const Moves &): kind{COPIED} {}

    Moves(Moves &&moveable) noexcept: kind{MOVING} {
        moveable.kind = MOVED;
    }
};

struct BuildsFromInt {
    BuildsFromInt(int) {};
};

struct TakesInitializerList {
    int s;
    double v;
    TakesInitializerList(std::initializer_list<int> il, double val):
        s(int(il.size())), v(val)
    {}
};

struct TwoArgumentConstructor {
    bool boolean;
    int value;

    TwoArgumentConstructor(void *p, int q): boolean(p), value(q) {};
};

template<typename ExtAny>
void testAnyImplementation() {
    static_assert(
        std::is_same_v<
            ExtAny &,
            decltype(std::declval<ExtAny &>() = std::declval<ExtAny &&>())
        >, "the move-assignment operator must return the container reference"
    );
    static_assert(
        noexcept(std::declval<ExtAny &>() = std::declval<ExtAny &&>()),
        "move-assignment of an any container must be noexcept"
    );

    SECTION("Value Destruction") {
        int value;
        {
            ExtAny a{Destructor{&value}};
            REQUIRE(zoo::isRuntimeValue<Destructor>(a));
            value = 0;
        }
        REQUIRE(1 == value);
    }
    SECTION("Referential Semantics - Alignment, Destruction") {
        int value;
        {
            ExtAny a{D2{&value}};
            REQUIRE(zoo::isRuntimeReference<D2>(a));
            value = 0;
        }
        REQUIRE(1 == value);
    }
    SECTION("Referential Semantics - Size") {
        ExtAny v{Big{}};
        REQUIRE(zoo::isRuntimeReference<Big>(v));
        auto hasValue = v.has_value();
        REQUIRE(hasValue);
    }
    SECTION("Copy constructor -- Referential") {
        Big b = { 3.14159265, 2.7182 };
        ExtAny big{b};
        auto bigA = zoo::anyContainerCast<Big>(&big);
        REQUIRE(bigA->a == b.a);
        REQUIRE(bigA->b == b.b);
        ExtAny copy{big};
        REQUIRE(zoo::isRuntimeReference<Big>(copy));
        REQUIRE(typeid(Big) == copy.type());
        auto addr = zoo::anyContainerCast<Big>(&copy);
        REQUIRE(addr != bigA);
        CHECK(addr->a == b.a);
        CHECK(addr->b == b.b);
    }
    SECTION("Move constructor -- Value") {
        ExtAny movingFrom{Moves{}};
        REQUIRE(zoo::isRuntimeValue<Moves>(movingFrom));
        ExtAny movedTo{std::move(movingFrom)};
        auto ptrFrom = zoo::anyContainerCast<Moves>(&movingFrom);
        auto ptrTo = zoo::anyContainerCast<Moves>(&movedTo);
        REQUIRE(Moves::MOVED == ptrFrom->kind);
        REQUIRE(Moves::MOVING == ptrTo->kind);
    }
    SECTION("Move constructor -- Referential") {
        ExtAny movingFrom{Big{}};
        REQUIRE(zoo::isRuntimeReference<Big>(movingFrom));
        auto original = zoo::anyContainerCast<Big>(&movingFrom);
        ExtAny movingTo{std::move(movingFrom)};
        auto afterMove = zoo::anyContainerCast<Big>(&movingTo);
        REQUIRE(original == afterMove);
        SECTION("Moved-from referential may be assigned") {
            movingFrom = 5; // may be assigned
            REQUIRE(movingFrom.has_value());
            REQUIRE(typeid(int) == movingFrom.type());
            CHECK(5 == *zoo::anyContainerCast<int>(&movingFrom));
        }
    }
    SECTION("Initializer constructor -- copying") {
        Moves value;
        ExtAny copied{value};
        auto ptr = zoo::anyContainerCast<Moves>(&copied);
        REQUIRE(Moves::COPIED == ptr->kind);
    }
    SECTION("Initializer constructor -- moving") {
        Moves def;
        CHECK(Moves::DEFAULT == def.kind);
        ExtAny moving{std::move(def)};
        REQUIRE(Moves::MOVED == def.kind);
        REQUIRE(Moves::MOVING == zoo::anyContainerCast<Moves>(&moving)->kind);
    }
    SECTION("Assignments") {
        using namespace zoo;
        int willChange = 0;
        ExtAny willBeTrampled{Destructor{&willChange}};
        ExtAny integer{5};
        SECTION("Destroys trampled object") {
            willBeTrampled = integer;
            REQUIRE(typeid(int) == willBeTrampled.type());
            auto asInt = anyContainerCast<int>(&willBeTrampled);
            REQUIRE(nullptr != asInt);
            CHECK(5 == *asInt);
            CHECK(1 == willChange);
        }
        SECTION("Move and copy assignments") {
            integer = Moves{};
            auto movPtr = anyContainerCast<Moves>(&integer);
            CHECK(Moves::MOVING == movPtr->kind);
            willBeTrampled = *movPtr;
            auto movPtr2 = anyContainerCast<Moves>(&willBeTrampled);
            REQUIRE(nullptr != movPtr2);
            CHECK(Moves::COPIED == movPtr2->kind);
            ExtAny anotherTrampled{D2{&willChange}};
            anotherTrampled = std::move(*movPtr2);
            CHECK(Moves::MOVED == movPtr2->kind);
            auto p = anyContainerCast<Moves>(&anotherTrampled);
            CHECK(Moves::MOVING == p->kind);
        }
    }
    ExtAny empty;
    SECTION("reset()") {
        REQUIRE(!empty.has_value());
        empty = 5;
        REQUIRE(empty.has_value());
        empty.reset();
        REQUIRE(!empty.has_value());
    }
    SECTION("typeid") {
        REQUIRE(typeid(void) == empty.type());
        empty = Big{};
        REQUIRE(typeid(Big) == empty.type());
    }
    SECTION("swap") {
        SECTION("synopsis") {
            ExtAny other{5};
            swap(empty, other);
            REQUIRE(typeid(int) == empty.type());
            REQUIRE(typeid(void) == other.type());
            auto valuePointerAtEmpty = zoo::anyContainerCast<int>(&empty);
            REQUIRE(5 == *valuePointerAtEmpty);
        }
    }
    SECTION("inplace") {
        ExtAny bfi{std::in_place_type<BuildsFromInt>, 5};
        REQUIRE(typeid(BuildsFromInt) == bfi.type());
        ExtAny il{std::in_place_type<TakesInitializerList>, { 9, 8, 7 }, 2.2};
        REQUIRE(typeid(TakesInitializerList) == il.type());
        auto ptr = zoo::anyContainerCast<TakesInitializerList>(&il);
        REQUIRE(3 == ptr->s);
        REQUIRE(2.2 == ptr->v);
    }
    SECTION("Multiple argument constructor -- value") {
        ExtAny mac{TwoArgumentConstructor{nullptr, 3}};
        REQUIRE(zoo::isRuntimeValue<TwoArgumentConstructor>(mac));
        auto ptr = zoo::anyContainerCast<TwoArgumentConstructor>(&mac);
        REQUIRE(false == ptr->boolean);
        REQUIRE(3 == ptr->value);
    }
}
