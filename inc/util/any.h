#include "meta/NotBasedOf.h"

#include <new>

namespace zoo {

template<int Size, int Alignment>
struct IAnyContainer {
    virtual void destroy() {}
    virtual void copy(IAnyContainer *to) { new(to) IAnyContainer; }
    virtual void move(IAnyContainer *to) { new(to) IAnyContainer; }
    virtual void *value() { return nullptr; }

    alignas(Alignment)
    char m_space[Size];
};

template<int Size, int Alignment, typename ValueType>
struct ValueContainer: IAnyContainer<Size, Alignment> {
    using IAC = IAnyContainer<Size, Alignment>;

    ValueType *thy() { return reinterpret_cast<ValueType *>(this->m_space); }

    template<typename Value>
    ValueContainer(Value &&value) {
        new(this->m_space) ValueType{std::forward<Value>(value)};
    }

    void destroy() override { thy()->~ValueType(); }

    void copy(IAC *to) override { new(to) ValueContainer{*thy()}; }

    void move(IAC *to) override { new(to) ValueContainer{std::move(*thy())}; }

    void *value() override { return thy(); }
};

template<int Size, int Alignment, typename ValueType>
struct ReferentialContainer: IAnyContainer<Size, Alignment> {
    using IAC = IAnyContainer<Size, Alignment>;

    ValueType **pThy() {
        return reinterpret_cast<ValueType **>(this->m_space);
    }

    ValueType *thy() { return *pThy(); }

    template<typename Value>
    ReferentialContainer(Value &&value) {
        *pThy() = new ValueType{std::forward<Value>(value)};
    }

    ReferentialContainer(ValueType *ptr) { *pThy() = ptr; }

    void destroy() override { thy()->~ValueType(); }

    void copy(IAC *to) override { new(to) ReferentialContainer{*thy()}; }

    void move(IAC *to) override {
        new(to) ReferentialContainer{thy()};
        *pThy() = nullptr;
    }

    void *value() override { return thy(); }
};

template<int Size, int Alignment, typename ValueType, bool Value>
struct PolymorphicImplementationDecider {
    using type = ReferentialContainer<Size, Alignment, ValueType>;
};

template<int Size, int Alignment, typename ValueType>
struct PolymorphicImplementationDecider<Size, Alignment, ValueType, true> {
    using type = ValueContainer<Size, Alignment, ValueType>;
};

struct PolymorphicTypeSwitch {
    template<int Size, int Alignment>
    using empty = IAnyContainer<Size, Alignment>;

    template<int Size, int Alignment, typename ValueType>
    static constexpr bool useValueSemantics() {
        return
            Alignment % alignof(ValueType) == 0 &&
            sizeof(ValueType) <= Size;
    }

    template<int Size, int Alignment, typename ValueType>
    using Implementation =
        typename PolymorphicImplementationDecider<
            Size,
            Alignment,
            ValueType,
            useValueSemantics<Size, Alignment, ValueType>()
        >::type;
};

template<int Size, int Alignment, typename TypeSwitch = PolymorphicTypeSwitch>
struct AnyContainer {
    using Container = typename TypeSwitch::template empty<Size, Alignment>;

    alignas(alignof(Container))
    char m_space[sizeof(Container)];

    AnyContainer() noexcept { new(m_space) Container; }

    AnyContainer(const AnyContainer &model) {
        auto source = model.container();
        source->copy(container());
    }

    AnyContainer(AnyContainer &&moveable) noexcept {
        auto source = moveable.container();
        source->move(container());
    }

    template<
        typename Initializer,
        std::enable_if_t<
            meta::NotBasedOf<Initializer, AnyContainer>(),
            int
        > = 0
    >
    AnyContainer(Initializer &&initializer);

    ~AnyContainer() { container()->destroy(); }

    AnyContainer &operator=(const AnyContainer &model) {
        auto myself = container();
        myself->destroy();
        model.container()->copy(myself);
        return *this;
    }

    AnyContainer &operator=(AnyContainer &&moveable) noexcept {
        auto myself = container();
        myself->destroy();
        moveable.container()->move(myself);
        return *this;
    }

    template<
        typename Argument,
        std::enable_if_t<
            meta::NotBasedOf<Argument, AnyContainer>(),
            int
        > = 0
    >
    AnyContainer &operator=(Argument &&argument) {
        container()->destroy();
        new(this) AnyContainer(std::forward<Argument>(argument));
        return *this;
    }

    Container *container() const {
        return reinterpret_cast<Container *>(const_cast<char *>(m_space));
    }
};

using Any = AnyContainer<8, 8, PolymorphicTypeSwitch>;

template<typename T>
inline T *any_cast(Any *ptr) {
    return reinterpret_cast<T *>(ptr->container()->value());
}

template<int Size, int Alignment, typename TypeSwitch>
template<
    typename Initializer,
    std::enable_if_t<
        meta::NotBasedOf<
            Initializer, AnyContainer<Size, Alignment, TypeSwitch>
        >(),
        int
    >
>
AnyContainer<Size, Alignment, TypeSwitch>::AnyContainer(Initializer &&i) {
    using Decayed = std::decay_t<Initializer>;
    static_assert(std::is_copy_constructible<Decayed>::value, "");
    using Implementation =
        typename TypeSwitch::template Implementation<Size, Alignment, Decayed>;
    new(m_space) Implementation(std::forward<Initializer>(i));
}

}