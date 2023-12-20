//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <string>

namespace dingo {
// A non-trivial testing hierarchy
struct IClass {
    virtual ~IClass() {}
    virtual const std::string& GetName() const = 0;
    virtual size_t GetTag() const = 0;
};

struct IClass1 : virtual IClass {
    virtual ~IClass1() {}
};

struct IClass2 : virtual IClass {
    virtual ~IClass2() {}
};

template <typename T> struct ClassStats {
    ClassStats() { ++Constructor; }
    ~ClassStats() { ++Destructor; }
    ClassStats(const ClassStats&) { ++CopyConstructor; }
    ClassStats(ClassStats&&) { ++MoveConstructor; }

    static size_t Constructor;
    static size_t CopyConstructor;
    static size_t MoveConstructor;
    static size_t Destructor;

    static size_t GetTotalInstances() {
        return Constructor + CopyConstructor + MoveConstructor;
    }

    static void ClearStats() {
        Constructor = CopyConstructor = MoveConstructor = Destructor = 0;
    }
};

template <typename T> size_t ClassStats<T>::Constructor;
template <typename T> size_t ClassStats<T>::CopyConstructor;
template <typename T> size_t ClassStats<T>::MoveConstructor;
template <typename T> size_t ClassStats<T>::Destructor;

template <size_t Tag = 0>
struct Class : IClass1, IClass2, ClassStats<Class<Tag>> {
    Class() : name_("Class") {}
    ~Class() {}
    Class(const Class& cls) : ClassStats<Class<Tag>>(cls), name_(cls.name_) {}
    Class(Class&& cls)
        : ClassStats<Class<Tag>>(std::move(cls)), name_(std::move(cls.name_)) {}

    const std::string& GetName() const override { return name_; }
    size_t GetTag() const override { return Tag; }

  private:
    std::string name_;
};

} // namespace dingo
