#pragma once

#include <string>

namespace dingo {
struct IClass {
    virtual ~IClass() {}
    virtual const std::string& GetName() const = 0;
    virtual size_t tag() const = 0;
};

struct IClass1 : virtual IClass {
    virtual ~IClass1() {}
};

struct IClass2 : virtual IClass {
    virtual ~IClass2() {}
};

template <typename TestFn, size_t Counter> struct Class : IClass1, IClass2 {
    Class() : name_("Class") { ++Constructor; }
    ~Class() { ++Destructor; }
    Class(const Class& cls) : name_(cls.name_) { ++CopyConstructor; }
    Class(Class&& cls) : name_(std::move(cls.name_)) { ++MoveConstructor; }

    const std::string& GetName() const override { return name_; }
    size_t tag() const override { return Counter; }

    static size_t Constructor;
    static size_t CopyConstructor;
    static size_t MoveConstructor;
    static size_t Destructor;

    static size_t GetTotalInstances() {
        return Constructor + CopyConstructor + MoveConstructor;
    }

  private:
    std::string name_;
};

template <typename TestFn, size_t Counter>
size_t Class<TestFn, Counter>::Constructor;
template <typename TestFn, size_t Counter>
size_t Class<TestFn, Counter>::CopyConstructor;
template <typename TestFn, size_t Counter>
size_t Class<TestFn, Counter>::MoveConstructor;
template <typename TestFn, size_t Counter>
size_t Class<TestFn, Counter>::Destructor;
} // namespace dingo
