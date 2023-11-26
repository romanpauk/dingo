#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/storage/shared.h>

#include <string>

// Declare Messages and a wrapper that can hold one of the messages,
// resembling protobuf structure
struct MessageA {
    int value;
};
struct MessageB {
    float value;
};

struct MessageWrapper {
    template <typename T> MessageWrapper(T&& message) {
        messages_ = std::forward<T>(message);
    }

    size_t id() const { return messages_.index(); }
    const MessageA& GetA() const { return std::get<MessageA>(messages_); }
    const MessageB& GetB() const { return std::get<MessageB>(messages_); }

  private:
    std::variant<std::monostate, MessageA, MessageB> messages_;
};

// Declare message processor hierarchy with dependencies
struct IProcessor {
    virtual ~IProcessor() {}
    virtual void process(const MessageWrapper&) = 0;
};

struct RepositoryA {};
struct ProcessorA : IProcessor {
    ProcessorA(RepositoryA&) {}
    void process(const MessageWrapper& message) override { message.GetA(); }
};

struct RepositoryB {};
struct ProcessorB : IProcessor {
    ProcessorB(RepositoryB&) {}
    void process(const MessageWrapper& message) override { message.GetB(); }
};

int main() {
    using namespace dingo;

    // Define traits type with a single index using size_t as a key,
    // backed by a std::array of size 10
    struct container_traits : static_container_traits<void> {
        using index_definition_type =
            std::tuple<std::tuple<size_t, index_type::array<10>>>;
    };

    container<container_traits> container;

    // Register processors into the container, indexed by the type they process
    container.register_indexed_type<scope<shared>,
                                    storage<std::shared_ptr<ProcessorA>>,
                                    interface<IProcessor>>(size_t(1));
    container.register_indexed_type<scope<unique>,
                                    storage<std::shared_ptr<ProcessorB>>,
                                    interface<IProcessor>>(size_t(2));

    // Register repositories used by the processors
    container.register_type<scope<shared>, storage<RepositoryA>>();
    container.register_type<scope<shared>, storage<RepositoryB>>();

    // Invokes the processor for MessageA that is stateful
    {
        MessageWrapper msg((MessageA{1}));
        container.template resolve<std::shared_ptr<IProcessor>>(msg.id())
            ->process(msg);
    }

    // Invokes the processor for MessageB that is stateless
    {
        MessageWrapper msg((MessageB{1.1}));
        container.template resolve<std::shared_ptr<IProcessor>>(msg.id())
            ->process(msg);
    }
}
