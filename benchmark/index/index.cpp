//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <benchmark/benchmark.h>

#include <type_traits>

struct Message {
  int value;
};

struct MessageWrapper {
  template <typename T> MessageWrapper(T &&message) {
    messages_ = std::forward<T>(message);
  }

  size_t id() const { return messages_.index(); }
  const Message &GetMessage() const { return std::get<Message>(messages_); }

private:
  std::variant<std::monostate, Message> messages_;
};

struct IProcessor {
  virtual ~IProcessor() {}
  virtual void process(const MessageWrapper &) = 0;
};

template <typename... Args> struct Processor : IProcessor {
  Processor(Args &&...) {}
  void process(const MessageWrapper &message) override { message.GetMessage(); }
};

// TODO: can't do value & interface
// template <typename Container>
// static void index_value_unique(benchmark::State& state);

template <typename Container>
static void index_value_shared(benchmark::State &state) {
  using namespace dingo;
  Container container;
  container.template register_type<scope<shared>, storage<Processor<>>,
                                   interfaces<IProcessor>>(
      dingo::key_value{size_t(1)});

  MessageWrapper msg(Message{1});
  for (auto _ : state) {
    container.template resolve<IProcessor *>(size_t(1))->process(msg);
  }

  state.SetBytesProcessed(state.iterations());
}

template <typename Container>
static void index_ptr_unique(benchmark::State &state) {
  using namespace dingo;
  Container container;
  container.template register_type<scope<unique>,
                                   storage<std::shared_ptr<Processor<>>>,
                                   interfaces<IProcessor>>(
      dingo::key_value{size_t(1)});

  // TODO: can't register the same type multiple times under different index,
  // as storage type is used as a key
  MessageWrapper msg(Message{1});
  for (auto _ : state) {
    container.template resolve<std::shared_ptr<IProcessor>>(size_t(1))->process(
        msg);
  }

  state.SetBytesProcessed(state.iterations());
}

template <typename Container>
static void index_ptr_shared(benchmark::State &state) {
  using namespace dingo;
  Container container;
  container.template register_type<scope<shared>,
                                   storage<std::shared_ptr<Processor<>>>,
                                   interfaces<IProcessor>>(
      dingo::key_value{size_t(1)});

  MessageWrapper msg(Message{1});
  for (auto _ : state) {
    container.template resolve<std::shared_ptr<IProcessor> &>(size_t(1))
        ->process(msg);
  }

  state.SetBytesProcessed(state.iterations());
}

struct static_container_traits : dingo::static_container_traits<void> {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<size_t, IProcessor>>;
};

BENCHMARK_TEMPLATE(index_value_shared,
                   dingo::container<static_container_traits>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(index_ptr_shared, dingo::container<static_container_traits>)
    ->UseRealTime();

struct dynamic_container_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<size_t, IProcessor>>;
};

BENCHMARK_TEMPLATE(index_value_shared,
                   dingo::container<dynamic_container_traits>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(index_ptr_shared, dingo::container<dynamic_container_traits>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(index_ptr_unique, dingo::container<dynamic_container_traits>)
    ->UseRealTime();
