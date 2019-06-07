//
//  PolymorphicContainer.h
//
//  Created by Eduardo Madrid on 6/7/19.
//  Copyright © 2019 Eduardo Madrid. All rights reserved.
//

#ifndef PolymorphicContainer_h
#define PolymorphicContainer_h

#include "ValueContainer.h"

namespace zoo {

template<int Size, int Alignment>
struct IAnyContainer {
    virtual void destroy() {}
    virtual void copy(IAnyContainer *to) { new(to) IAnyContainer; }
    virtual void move(IAnyContainer *to) noexcept { new(to) IAnyContainer; }
    
    /// Note: it is a fatal error to call the destructor after moveAndDestroy
    virtual void moveAndDestroy(IAnyContainer *to) noexcept {
        new(to) IAnyContainer;
    }
    virtual void *value() noexcept { return nullptr; }
    virtual bool nonEmpty() const noexcept { return false; }
    virtual const std::type_info &type() const noexcept { return typeid(void); }
    
    alignas(Alignment)
    char m_space[Size];
    
    using NONE = void (IAnyContainer::*)();
    constexpr static NONE None = nullptr;
};

template<int Size, int Alignment>
struct BaseContainer: IAnyContainer<Size, Alignment> {
    bool nonEmpty() const noexcept { return true; }
};

template<int Size, int Alignment, typename ValueType>
struct ValueContainer: BaseContainer<Size, Alignment> {
    using IAC = IAnyContainer<Size, Alignment>;
    
    ValueType *thy() { return reinterpret_cast<ValueType *>(this->m_space); }
    
    ValueContainer(typename IAC::NONE) {}
    
    template<typename... Args>
    ValueContainer(Args &&... args) {
        new(this->m_space) ValueType{std::forward<Args>(args)...};
    }
    
    void destroy() override { thy()->~ValueType(); }
    
    void copy(IAC *to) override { new(to) ValueContainer{*thy()}; }
    
    void move(IAC *to) noexcept override {
        new(to) ValueContainer{std::move(*thy())};
    }
    
    void moveAndDestroy(IAC *to) noexcept override {
        ValueContainer::move(to);
        ValueContainer::destroy();
        // note: the full qualification prevents the penalty of dynamic
        // dispatch
    }
    
    void *value() noexcept override { return thy(); }
    
    const std::type_info &type() const noexcept override {
        return typeid(ValueType);
    }
};

template<int Size, int Alignment, typename ValueType>
struct ReferentialContainer: BaseContainer<Size, Alignment> {
    using IAC = IAnyContainer<Size, Alignment>;
    
    ValueType **pThy() {
        return reinterpret_cast<ValueType **>(this->m_space);
    }
    
    ValueType *thy() { return *pThy(); }
    
    ReferentialContainer(typename IAC::NONE) {}
    
    template<typename... Values>
    ReferentialContainer(Values &&... values) {
        *pThy() = new ValueType{std::forward<Values>(values)...};
    }
    
    ReferentialContainer(typename IAC::NONE, ValueType *ptr) { *pThy() = ptr; }
    
    void destroy() override { delete thy(); }
    
    void copy(IAC *to) override { new(to) ReferentialContainer{*thy()}; }
    
    void transferPointer(IAC *to) {
        new(to) ReferentialContainer{IAC::None, thy()};
    }
    
    void move(IAC *to) noexcept override {
        new(to) ReferentialContainer{IAC::None, thy()};
        new(this) IAnyContainer<Size, Alignment>;
    }
    
    void moveAndDestroy(IAC *to) noexcept override {
        transferPointer(to);
    }
    
    void *value() noexcept override { return thy(); }
    
    const std::type_info &type() const noexcept override {
        return typeid(ValueType);
    }
};

template<int Size, int Alignment, typename ValueType, bool Value>
struct RuntimePolymorphicAnyPolicyDecider {
    using type = ReferentialContainer<Size, Alignment, ValueType>;
};

template<int Size, int Alignment, typename ValueType>
struct RuntimePolymorphicAnyPolicyDecider<Size, Alignment, ValueType, true> {
    using type = ValueContainer<Size, Alignment, ValueType>;
};

template<typename ValueType>
constexpr bool canUseValueSemantics(int size, int alignment) {
    return
    alignment % alignof(ValueType) == 0 &&
    sizeof(ValueType) <= size &&
    std::is_nothrow_move_constructible<ValueType>::value;
}

template<int Size, int Alignment>
struct RuntimePolymorphicAnyPolicy {
    using MemoryLayout = IAnyContainer<Size, Alignment>;
    
    template<typename ValueType>
    using Builder =
    typename RuntimePolymorphicAnyPolicyDecider<
    Size,
    Alignment,
    ValueType,
    canUseValueSemantics<ValueType>(Size, Alignment)
    >::type;
};

}
#endif /* PolymorphicContainer_h */
