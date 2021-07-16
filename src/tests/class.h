#pragma once

#include <string>

namespace dingo
{
    struct IClass
    {
        virtual ~IClass() {}
        virtual const std::string& GetName() = 0;
    };

    struct IClass1 : virtual IClass
    {
        virtual ~IClass1() {}
    };

    struct IClass2 : virtual IClass
    {
        virtual ~IClass2() {}
    };

    template < size_t Counter > struct Class : IClass1, IClass2
    {
        Class() : name_("Class") { ++Constructor; }
        ~Class() { ++Destructor; }
        Class(const Class& cls) : name_(cls.name_) { ++CopyConstructor; }
        Class(Class&& cls) : name_(std::move(cls.name_)) { ++MoveConstructor; }

        const std::string& GetName() { return name_; }

        static size_t Constructor;
        static size_t CopyConstructor;
        static size_t MoveConstructor;
        static size_t Destructor;

    private:
        std::string name_;
    };

    template < size_t Counter > size_t Class< Counter >::Constructor;
    template < size_t Counter > size_t Class< Counter >::CopyConstructor;
    template < size_t Counter > size_t Class< Counter >::MoveConstructor;
    template < size_t Counter > size_t Class< Counter >::Destructor;
}
