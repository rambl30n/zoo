#include "zoo/Any/DerivedVTablePolicy.h"
#include "zoo/Any/AnyMovable.hpp"
#include "zoo/AlignedStorage.h"

#include <utility>
#include <type_traits>

namespace zoo {

using namespace std;

using MoveOnlyPolicy = Policy<void *, Destroy, Move>;
using MOAC = AnyContainer<MoveOnlyPolicy>;

namespace dp {

//AnyContainer<DerivedVTablePolicy<MoveOnlyPolicy, Copy>> instance;

}

static_assert(is_nothrow_move_constructible_v<MOAC>);
//static_assert(!is_copy_constructible_v<MOAC>);

using LargeMoveOnlyPolicy = Policy<void *[2], Destroy, Move>;
static_assert(
    !std::is_constructible_v<zoo::AnyContainerBase<MoveOnlyPolicy>,
    const zoo::AnyContainerBase<LargeMoveOnlyPolicy> &>
);

using CVTP = Policy<void *, Destroy, Move, Copy>;

struct CanonicalVTableAny {
    AlignedStorageFor<typename CVTP::MemoryLayout> space_;
};

static_assert(detail::AffordsCopying<CVTP>::value);

namespace detail { // the traits are correct

struct Unrelated {};
static_assert(!IsAnyContainer_impl<Unrelated>::value);

struct HasPolicyType { using Policy = int; };
static_assert(!IsAnyContainer_impl<HasPolicyType>::value);

struct MoacDerivative: MOAC {};
static_assert(IsAnyContainer_impl<MoacDerivative>::value);

}}

#include "catch2/catch.hpp"

#include <sstream>

#define MAY_CALL_EXPRESSION(name, ...) \
template<typename, typename = void> \
struct name: std::false_type {}; \
template<typename T> \
struct name<T, std::void_t<decltype(__VA_ARGS__(std::declval<const T &>()))>>: \
    std::true_type \
{};

MAY_CALL_EXPRESSION(HasToString, to_string)
#define INSERTION_OPERATION_EXPRESSION(arg) std::declval<std::ostream &>() << arg
MAY_CALL_EXPRESSION(HasOperatorInsertion, INSERTION_OPERATION_EXPRESSION)

struct Complex { double real, imaginary; };

std::ostream &operator<<(std::ostream &out, Complex in) {
    return out << in.real << " + i" << in.imaginary;
}

std::string to_string(Complex cx) {
    std::ostringstream ostr;
    ostr << '(' << cx << ')';
    return ostr.str();
}

static_assert(!HasToString<int>::value);
static_assert(HasOperatorInsertion<int>::value);

template<typename Container>
std::string stringize(const void *ptr) {
    auto downcast = static_cast<const Container *>(ptr);
    using T = std::decay_t<decltype(*downcast->value())>;
    auto &t = *downcast->value();
    if constexpr(HasToString<T>::value) { return to_string(t); }
    else if constexpr(HasOperatorInsertion<T>::value) {
        std::ostringstream ostr;
        ostr << t;
        return ostr.str();
    }
    else return "";
}

struct Stringize {
    struct VTableEntry { std::string (*str)(const void *); };

    template<typename>
    constexpr static inline VTableEntry Default = {
        [](const void *) { return std::string(); }
    };

    template<typename Container>
    constexpr static inline VTableEntry Operation = { stringize<Container> };

    template<typename Container>
    struct Mixin {};

    template<typename AnyC>
    struct UserAffordance {
        std::string stringize() const {
            auto container =
                const_cast<AnyC *>(static_cast<const AnyC *>(this))->container();
            return container->template vTable<Stringize>()->str(container);
        }
    };
};

TEST_CASE("Composed Policies") {
    using ComposedPolicy = zoo::DerivedVTablePolicy<zoo::MoveOnlyPolicy, zoo::Copy>;
    using ComposedAC = zoo::AnyContainer<ComposedPolicy>;
    SECTION("Derived defaults preserved") {
        ComposedAC defaulted;
        REQUIRE(&ComposedPolicy::DefaultImplementation::Default == defaulted.container()->ptr_);
    }
    SECTION("Operational") {
        ComposedAC operational = 34;
        REQUIRE(&ComposedPolicy::Builder<int>::Operations == operational.container()->ptr_);
    }
    SECTION("Compatibility with super container") {
        ComposedAC operational = 34;
        static_assert(!zoo::detail::AffordsCopying<zoo::MoveOnlyPolicy>::value);
        static_assert(!std::is_copy_constructible_v<zoo::AnyContainer<zoo::MoveOnlyPolicy>>);
        zoo::AnyContainer<zoo::MoveOnlyPolicy> &moacReference = operational, moac;
        REQUIRE(&ComposedPolicy::Builder<int>::Operations == moacReference.container()->ptr_);
        ComposedPolicy::DefaultImplementation di, other;
        static_assert(zoo::detail::AffordsCopying<ComposedPolicy>::value);
        ComposedAC copy = operational;
        REQUIRE(34 == *copy.state<int>());
        moac = std::move(copy);
        REQUIRE(34 == *moac.state<int>());
        // this line won't compile, exactly as intended: a copiable "any" requires copyability
        //copy = std::move(moac);
    }
}

TEST_CASE("Ligtests") {
    //using VTA = zoo::AnyContainer<zoo::CanonicalVTablePolicy>;
    using Policy = zoo::Policy<void *, zoo::Destroy, zoo::Move, zoo::Copy>;
    using VTA = zoo::AnyContainer<Policy>;
    static_assert(std::is_default_constructible_v<VTA>);
    static_assert(std::is_nothrow_move_constructible_v<VTA>);

    SECTION("Destruction") {
        SECTION("Destruction of default constructed") {
            VTA a;
        }
        int trace = 0;

        struct TracesDestruction {
            int &trace_;

            TracesDestruction(int &trace): trace_(trace) {}
            ~TracesDestruction() { trace_ = 1; }
        };

        SECTION("By value") {
            REQUIRE(0 == trace);
            {
                VTA a(std::in_place_type<TracesDestruction>, trace);
            }
            REQUIRE(1 == trace);
        }
        SECTION("By reference") {
            struct Large: TracesDestruction {
                using TracesDestruction::TracesDestruction;
                double whatever[3];
            };
            REQUIRE(0 == trace);
            {
                VTA a(std::in_place_type<Large>, trace);
            }
            REQUIRE(1 == trace);
        }
    }
    SECTION("Moving") {
        struct TracesMoves {
            int *resource_;

            TracesMoves(int *p): resource_(p) {}

            TracesMoves(const TracesMoves &) {
                throw std::runtime_error("unreachable called");
            }

            TracesMoves(TracesMoves &&m) noexcept: resource_(m.resource_) {
                m.resource_ = nullptr;
            }

            ~TracesMoves() {
                if(resource_) { *resource_ = 0; }
            }
        };
        int something = 1;
        SECTION("By Value") {
            {
                VTA a;
                {
                    VTA tmp(std::in_place_type<TracesMoves>, &something);
                    REQUIRE(1 == something);
                    a = std::move(tmp);
                }
                REQUIRE(1 == something); // tmp if moved does not refer to something
            }
            REQUIRE(0 == something); // a referred to something, cleared
        }
        SECTION("By Reference") {
            struct Big {
                void *myself_;
                int *padding_;
                Big(): myself_(this) {}
            };
            VTA a(std::in_place_type<Big>);
            auto originalPlace = a.state<Big>()->myself_;
            VTA other(std::move(a));
            a.reset();
            REQUIRE(other.state<Big>()->myself_ == originalPlace);
        }
    }
    SECTION("Flexible affordances") {
        using RTTIMO = zoo::Policy<void *, zoo::Destroy, zoo::Move, zoo::RTTI>;
        using RTTIPolicy = zoo::ExtendedAffordancePolicy<RTTIMO, zoo::RTTI>;
        using RTTIMOAC = zoo::AnyContainer<RTTIPolicy>;
        RTTIMOAC mo;
        SECTION("Defaulted") {
            auto &typeidVoid = typeid(void);
            auto &mot = mo.type();
            REQUIRE(typeidVoid == mot);
        }
        SECTION("Value") {
            mo = 4;
            REQUIRE(typeid(int) == mo.type());
        }
        SECTION("Reference") {
            struct Complex { double real; double img; };
            mo = Complex{};
            REQUIRE(typeid(Complex) == mo.type());
        }
    }
    SECTION("User affordance") {
        zoo::AnyContainer<
            zoo::ExtendedAffordancePolicy<
                zoo::Policy<void *, zoo::Destroy, zoo::Move, Stringize>,
                Stringize
            >
        > stringizable;
        REQUIRE("" == stringizable.stringize());
        stringizable = 88;
        auto str = stringizable.stringize();
        REQUIRE("88" == stringizable.stringize());
        Complex c1{0, 1};
        auto c2 = c1;
        stringizable = std::move(c2);
        REQUIRE(to_string(c1) == stringizable.stringize());
    }
}

namespace zoo {

template<std::size_t Pointers, typename Signature>
using NewZooFunction = AnyContainer<typename GenericPolicy<void *[Pointers], Destroy, Move, CallableViaVTable<Signature>>::Policy>;

template<std::size_t Pointers, typename Signature>
using NewCopyableFunction = AnyContainer<DerivedVTablePolicy<typename GenericPolicy<void *[Pointers], Destroy, Move, CallableViaVTable<Signature>>::Policy, Copy>>;

}

TEST_CASE("New zoo function") {
    auto doubler = [&](int a) { return 2.0 * a; };
    zoo::NewCopyableFunction<2, double(int)> cf = doubler;
    zoo::NewZooFunction<2, double(int)> di = std::move(cf);
    REQUIRE(6.0 == di(3));
    //cf = std::move(di);
}
