//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <map>
#include <string>
#include <typeindex>

////
struct ProcessorBase {
    virtual ~ProcessorBase() {}
    virtual void process(const void*) = 0;
    virtual std::type_index type() = 0;
};

template <typename T, typename ProcessorT> struct Processor : ProcessorBase {
    virtual std::type_index type() override { return typeid(T); }

    void process(const void* transaction) override {
        static_cast<ProcessorT*>(this)->process_impl(
            *reinterpret_cast<const T*>(transaction));
    }
};

struct StringProcessor : Processor<std::string, StringProcessor> {
    void process_impl([[maybe_unused]] const std::string& value) {};
};

struct VectorIntProcessor : Processor<std::vector<int>, VectorIntProcessor> {
    void process_impl([[maybe_unused]] const std::vector<int>& value) {};
};

struct Dispatcher {
    Dispatcher(
        std::map<std::type_index, std::unique_ptr<ProcessorBase>>&& processors)
        : processors_(std::move(processors)) {}

    template <typename T> void process(const T& value) {
        processors_.at(typeid(T))->process(&value);
    }

  private:
    std::map<std::type_index, std::unique_ptr<ProcessorBase>> processors_;
};

////
int main() {
    using namespace dingo;
    ////
    container<> container;
    container
        .register_type<scope<unique>, storage<std::unique_ptr<StringProcessor>>,
                       interface<ProcessorBase>>();
    container.register_type<scope<unique>,
                            storage<std::unique_ptr<VectorIntProcessor>>,
                            interface<ProcessorBase>>();
    container.register_type_collection<
        scope<unique>,
        storage<std::map<std::type_index, std::unique_ptr<ProcessorBase>>>>(
        [](auto& collection, auto&& value) {
            collection.emplace(value->type(), std::move(value));
        });

    auto dispatcher = container.construct<Dispatcher>();
    dispatcher.process(std::string(""));
    ////
}
