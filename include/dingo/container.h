#pragma once

#include <dingo/config.h>

#include <dingo/annotated.h>
#include <dingo/class_instance_factory.h>
#include <dingo/class_instance_traits.h>
#include <dingo/collection_traits.h>
#include <dingo/decay.h>
#include <dingo/exceptions.h>
#include <dingo/resolving_context.h>
#include <dingo/rtti.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>
#include <dingo/type_map.h>
#include <dingo/type_registration.h>

#include <map>
#include <optional>
#include <typeindex>

namespace dingo {
struct dynamic_container_traits {
    using rtti_type = dynamic_rtti;
    template <typename Value, typename Allocator>
    using type_factory_map_type = dynamic_type_map<rtti_type, Value, Allocator>;
};

template <typename Tag = void> struct static_container_traits {
    using rtti_type = static_rtti;
    template <typename Value, typename Allocator>
    using type_factory_map_type = static_type_map<rtti_type, Tag, Value, Allocator>;
};

// TODO wrt. multibindinds, is this like map/multimap?
template <typename ContainerTraits = dynamic_container_traits, typename Allocator = std::allocator<char>>
class container {
  public:
    using allocator_type = Allocator;
    using container_type = container<ContainerTraits, Allocator>;
    using rtti_type = typename ContainerTraits::rtti_type;

    friend class resolving_context<container_type>;

    container(Allocator allocator = Allocator()) : allocator_(allocator), type_factories_(allocator) {}

    template <typename... TypeArgs> void register_type() {
        using registration = type_registration<TypeArgs...>;
        using storage_type =
            detail::storage<typename registration::scope_type::type, typename registration::storage_type::type,
                            typename registration::factory_type::type, container_type,
                            typename registration::conversions_type::type>;

        // TODO: remove tuple usage... or remove type_list usage
        if constexpr (std::tuple_size_v<typename registration::interface_type::type_tuple> == 1) {
            using interface_type = std::tuple_element_t<0, typename registration::interface_type::type_tuple>;

            register_type_factory<interface_type, storage_type>(
                std::make_unique<class_instance_factory<container_type, typename annotated_traits<interface_type>::type,
                                                        typename storage_type::type, storage_type>>());
        } else {
            auto storage = std::make_shared<storage_type>();
            for_each(typename registration::interface_type::type{}, [&](auto element) {
                using interface_type = typename decltype(element)::type;
                register_type_factory<interface_type, storage_type>(
                    std::make_unique<
                        class_instance_factory<container_type, typename annotated_traits<interface_type>::type,
                                               typename storage_type::type, std::shared_ptr<storage_type>>>(storage));
            });
        }
    }

    template <typename... TypeArgs, typename Arg> void register_type(Arg&& arg) {
        using registration = type_registration<TypeArgs..., factory<Arg>>;

        using storage_type =
            detail::storage<typename registration::scope_type::type, typename registration::storage_type::type,
                            typename registration::factory_type::type, container_type,
                            typename registration::conversions_type::type>;

        // TODO: remove tuple usage... or remove type_list usage
        if constexpr (std::tuple_size_v<typename registration::interface_type::type_tuple> == 1) {
            using interface_type = std::tuple_element_t<0, typename registration::interface_type::type_tuple>;

            register_type_factory<interface_type, storage_type>(
                std::make_unique<class_instance_factory<container_type, typename annotated_traits<interface_type>::type,
                                                        typename storage_type::type, storage_type>>(
                    std::forward<Arg>(arg)));
        } else {
            auto storage = std::make_shared<storage_type>(std::forward<Arg>(arg));
            for_each(typename registration::interface_type::type{}, [&](auto element) {
                using interface_type = typename decltype(element)::type;
                register_type_factory<interface_type, storage_type>(
                    std::make_unique<
                        class_instance_factory<container_type, typename annotated_traits<interface_type>::type,
                                               typename storage_type::type, std::shared_ptr<storage_type>>>(storage));
            });
        }
    }

    template <typename T, typename R = typename annotated_traits<
                              std::conditional_t<std::is_rvalue_reference_v<T>, std::remove_reference_t<T>, T>>::type>
    R resolve() {
        // TODO: it is the destructor that is slowing the resolving
        // std::aligned_storage_t< sizeof(resolving_context< container_type >) >
        // storage; new (&storage) resolving_context< container_type >(*this);
        // auto& context = *reinterpret_cast< resolving_context< container_type
        // >*
        // >(&storage);

        // TODO: context needs to be placed here, but initialized on the fly
        // later if needed. The only use of it is for cleanup during exception
        // and for cyclical types.

        // TODO: another idea - returning address of an temporary is fast
        // So what if we create home for that temporary and in-place construct
        // the unique object there? This is now in resolver_.

        resolving_context<container_type> context(*this);
        return resolve<T, true>(context);
    }

    template <typename T, typename Factory = constructor<decay_t<T>>> T construct(Factory factory = Factory()) {
        // TODO: nothrow constructuble
        resolving_context<container_type> context(*this);
        return factory.template construct<T>(context);
    }

  private:
    template <typename TypeInterface, typename TypeStorage, typename Factory>
    void register_type_factory(Factory&& factory) {
        check_interface_requirements<TypeStorage, typename annotated_traits<TypeInterface>::type,
                                     typename TypeStorage::type>();
        auto pb = type_factories_.template insert<TypeInterface>(
            typename ContainerTraits::template type_factory_map_type<
                std::unique_ptr<class_instance_factory_i<container_type>>, allocator_type>(allocator_));

        if (!pb.first
                 .template insert<type_list<TypeInterface, typename TypeStorage::type>>(std::forward<Factory>(factory))
                 .second) {
            throw type_already_registered_exception();
        }
    }

    template <typename T, bool RemoveRvalueReferences,
              typename R = std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>, std::remove_reference_t<T>, T>, T>>
    R resolve(resolving_context<container_type>& context) {
        using Type = decay_t<T>;

        auto factories = type_factories_.template at<Type>();
        if (factories && factories->size() == 1) {
            auto& factory = factories->front();
            // TODO: no longer class instance
            return class_instance_traits<rtti_type, typename annotated_traits<T>::type>::resolve(*factory, context);
        }

        return resolve_multiple<T>(context);
    }

    template <typename T>
    std::enable_if_t<!collection_traits<decay_t<T>>::is_collection, T>
    resolve_multiple(resolving_context<container_type>&) {
        throw type_not_found_exception();
    }

    template <typename T>
    std::enable_if_t<collection_traits<decay_t<T>>::is_collection, T>
    resolve_multiple(resolving_context<container_type>& context) {
        using Type = decay_t<T>;
        using ValueType = decay_t<typename Type::value_type>;

        auto range = type_factories_.equal_range(typeid(ValueType));

        // TODO: destructor for results is not called, leaking the memory.
        // Unfortunatelly for the case when this is called from resolve(),
        // return value is T&.
        auto results = context.template get_allocator<Type>().allocate(1);
        new (results) Type;
        collection_traits<Type>::reserve(*results, std::distance(range.first, range.second));
        if (range.first != range.second) {
            for (auto it = range.first; it != range.second; ++it) {
                collection_traits<Type>::add(
                    *results,
                    class_instance_traits<rtti_type, typename Type::value_type>::resolve(*it->second, context));
            }

            return *results;
        }

        throw type_not_found_exception();
    }

    template <class Storage, class TypeInterface, class Type> void check_interface_requirements() {
        static_assert(!std::is_reference_v<TypeInterface>);
        static_assert(std::is_convertible_v<decay_t<Type>*, decay_t<TypeInterface>*>);
        if constexpr (!std::is_same_v<decay_t<Type>, decay_t<TypeInterface>>) {
            static_assert(detail::storage_interface_requirements_v<Storage, decay_t<Type>, decay_t<TypeInterface>>,
                          "storage requirements not met");
        }
    }

    allocator_type allocator_;

    typename ContainerTraits::template type_factory_map_type<
        typename ContainerTraits::template type_factory_map_type<
            std::unique_ptr<class_instance_factory_i<container_type>>, allocator_type>,
        allocator_type>
        type_factories_;
};
} // namespace dingo