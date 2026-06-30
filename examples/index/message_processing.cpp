//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <string>

////
// Declare Messages and a wrapper that can hold one of the messages,
// resembling protobuf structure
struct MessageA {
  int value;
};
struct MessageB {
  float value;
};

struct MessageWrapper {
  template <typename T> MessageWrapper(T &&message) {
    messages_ = std::forward<T>(message);
  }

  size_t id() const { return messages_.index(); }
  const MessageA &GetA() const { return std::get<MessageA>(messages_); }
  const MessageB &GetB() const { return std::get<MessageB>(messages_); }

private:
  std::variant<std::monostate, MessageA, MessageB> messages_;
};

// Declare message processor hierarchy with dependencies
struct IProcessor {
  virtual ~IProcessor() = default;
  virtual void process(const MessageWrapper &) = 0;
};

struct RepositoryA {};
struct ProcessorA : IProcessor {
  ProcessorA(RepositoryA &) {}
  void process(const MessageWrapper &message) override { message.GetA(); }
};

struct RepositoryB {};
struct ProcessorB : IProcessor {
  ProcessorB(RepositoryB &) {}
  void process(const MessageWrapper &message) override { message.GetB(); }
};

struct Pipeline {
  Pipeline(dingo::query<IProcessor &, dingo::key<size_t, 1>> first_processor,
           dingo::query<IProcessor &, dingo::key<size_t, 2>> second_processor)
      : first(first_processor), second(second_processor) {}

  IProcessor &first;
  IProcessor &second;
};

////
int main() {
  using namespace dingo;

  ////
  // Define traits type with a single query using size_t as a key
  struct container_traits : static_container_traits<void> {
    using query_definition_type = queries<associative<size_t, IProcessor>>;
  };
  // Runtime query storage is dynamic even when this
  // example uses static_container_traits for the rest of the container.

  container<container_traits> container;

  // Register processors into the container, keyed by the type they process
  container.register_indexed_type<scope<shared>,
                                  storage<std::shared_ptr<ProcessorA>>,
                                  interfaces<IProcessor>>(size_t(1));
  container.register_indexed_type<scope<shared>,
                                  storage<std::shared_ptr<ProcessorB>>,
                                  interfaces<IProcessor>>(size_t(2));

  // Register repositories used by the processors
  container.register_type<scope<shared>, storage<RepositoryA>>();
  container.register_type<scope<shared>, storage<RepositoryB>>();

  auto pipeline = container.construct<Pipeline>();

  // Invokes the processor for MessageA that is stateful
  {
    MessageWrapper msg((MessageA{1}));
    container.template resolve<IProcessor &>(msg.id()).process(msg);
  }

  // Invokes the processor for MessageB that is stateless
  {
    MessageWrapper msg((MessageB{1.1f}));
    container.template resolve<IProcessor &>(msg.id()).process(msg);
  }
  ////
  (void)pipeline;
}
