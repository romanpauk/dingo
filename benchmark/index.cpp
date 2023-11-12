#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/index/map.h>
#include <dingo/index/unordered_map.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <benchmark/benchmark.h>

struct Message {
    int value;
};

struct MessageWrapper {
    template <typename T> MessageWrapper(T&& message) {
        messages_ = std::forward<T>(message);
    }

    int id() const { return (int)messages_.index(); }
    const Message& GetMessage() const { return std::get<Message>(messages_); }

  private:
    std::variant<std::monostate, Message> messages_;
};

struct IProcessor {
    virtual ~IProcessor() {}
    virtual void process(const MessageWrapper&) = 0;
};

template <typename... Args> struct Processor : IProcessor {
    Processor(Args&&...) {}
    void process(const MessageWrapper& message) override {
        message.GetMessage();
    }
};

// TODO: can't do value & interface
// template <typename Container>
// static void index_value_unique(benchmark::State& state);

template <typename Container>
static void index_value_shared(benchmark::State& state) {
    using namespace dingo;
    Container container;
    container.template register_indexed_type<
        scope<shared>, storage<Processor<>>, interface<IProcessor>>(1);

    // TODO: can't register the same type multiple times, as storagae type is
    // used as a key
    container.template register_indexed_type<
        scope<shared>, storage<Processor<int>>, interface<IProcessor>>(2);
    container.template register_indexed_type<
        scope<shared>, storage<Processor<float>>, interface<IProcessor>>(3);
    container.template register_indexed_type<
        scope<shared>, storage<Processor<double>>, interface<IProcessor>>(4);
    container.template register_indexed_type<
        scope<shared>, storage<Processor<int, int>>, interface<IProcessor>>(5);
    container.template register_indexed_type<scope<shared>,
                                             storage<Processor<int, int, int>>,
                                             interface<IProcessor>>(6);

    MessageWrapper msg(Message{1});
    for (auto _ : state) {
        container.template resolve<IProcessor*>(1)->process(msg);
    }

    state.SetBytesProcessed(state.iterations());
}

template <typename Container>
static void index_ptr_unique(benchmark::State& state) {
    using namespace dingo;
    Container container;
    container.template register_indexed_type<
        scope<unique>, storage<std::shared_ptr<Processor<>>>,
        interface<IProcessor>>(1);

    // TODO: can't register the same type multiple times, as storagae type is
    // used as a key
    container.template register_indexed_type<
        scope<unique>, storage<std::shared_ptr<Processor<int>>>,
        interface<IProcessor>>(2);
    container.template register_indexed_type<
        scope<unique>, storage<std::shared_ptr<Processor<float>>>,
        interface<IProcessor>>(3);
    container.template register_indexed_type<
        scope<unique>, storage<std::shared_ptr<Processor<double>>>,
        interface<IProcessor>>(4);
    container.template register_indexed_type<
        scope<unique>, storage<std::shared_ptr<Processor<int, int>>>,
        interface<IProcessor>>(5);
    container.template register_indexed_type<
        scope<unique>, storage<std::shared_ptr<Processor<int, int, int>>>,
        interface<IProcessor>>(6);

    MessageWrapper msg(Message{1});
    for (auto _ : state) {
        container.template resolve<std::shared_ptr<IProcessor>>(1)->process(
            msg);
    }

    state.SetBytesProcessed(state.iterations());
}

template <typename Container>
static void index_ptr_shared(benchmark::State& state) {
    using namespace dingo;
    Container container;
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<Processor<>>>,
        interface<IProcessor>>(1);

    // TODO: can't register the same type multiple times, as storage type is
    // used as a key
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<Processor<int>>>,
        interface<IProcessor>>(2);
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<Processor<float>>>,
        interface<IProcessor>>(3);
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<Processor<double>>>,
        interface<IProcessor>>(4);
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<Processor<int, int>>>,
        interface<IProcessor>>(5);
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<Processor<int, int, int>>>,
        interface<IProcessor>>(6);

    MessageWrapper msg(Message{1});
    for (auto _ : state) {
        container.template resolve<std::shared_ptr<IProcessor>&>(1)->process(
            msg);
    }

    state.SetBytesProcessed(state.iterations());
}

template <typename IndexType>
struct static_container_traits : dingo::static_container_traits<void> {
    using index_definition_type = std::tuple<std::tuple<int, IndexType>>;
};

BENCHMARK_TEMPLATE(
    index_value_shared,
    dingo::container<static_container_traits<dingo::index_type::array<10>>>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(
    index_ptr_shared,
    dingo::container<static_container_traits<dingo::index_type::array<10>>>)
    ->UseRealTime();

template <typename IndexType>
struct dynamic_container_traits : dingo::dynamic_container_traits {
    using index_definition_type = std::tuple<std::tuple<int, IndexType>>;
};

BENCHMARK_TEMPLATE(
    index_value_shared,
    dingo::container<dynamic_container_traits<dingo::index_type::array<10>>>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(
    index_value_shared,
    dingo::container<dynamic_container_traits<dingo::index_type::map>>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(
    index_value_shared,
    dingo::container<
        dynamic_container_traits<dingo::index_type::unordered_map>>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(
    index_ptr_shared,
    dingo::container<dynamic_container_traits<dingo::index_type::array<10>>>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(
    index_ptr_shared,
    dingo::container<dynamic_container_traits<dingo::index_type::map>>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(
    index_ptr_shared,
    dingo::container<
        dynamic_container_traits<dingo::index_type::unordered_map>>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(
    index_ptr_unique,
    dingo::container<dynamic_container_traits<dingo::index_type::array<10>>>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(
    index_ptr_unique,
    dingo::container<dynamic_container_traits<dingo::index_type::map>>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(
    index_ptr_unique,
    dingo::container<
        dynamic_container_traits<dingo::index_type::unordered_map>>)
    ->UseRealTime();
