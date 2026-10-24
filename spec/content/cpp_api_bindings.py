"""Content of the C++ spellings of public API entries section.

Every public API entry has one binding. An include path binds the header, a
configuration macro binds the macro, and a name or extension point binds its
spelling, the public include path that makes it available, and what the public
API check compiles: `declarations` at namespace scope and `usage` as the body
of a function. The text `{name}` in them stands for the spelling, so that a
wrong spelling fails the check. All fragments share one translation unit, so
the names they declare at namespace scope start with `@P@`, which stands for
`api_` and the entry id.
"""

from spec.schema import CppApiBinding

CONTAINER = "dingo/container.h"
LOOKUP = "dingo/lookup/lookup.h"
TYPE_TRAITS = "dingo/type/type_traits.h"
CONVERSION = "dingo/type/type_conversion_traits.h"

# Local types of the usages: a service, and an interface with an implementation.
TYPES = """\
struct service {};
struct interface { virtual ~interface() = default; };
struct implementation : interface {};
"""

# A minimal shared-ownership handle type, for the extension points that
# specialize traits for a user type. "@H@" stands for the name of the handle.
HANDLE_CLASS = """\
template <typename T> class @H@ {
public:
  @H@() = default;
  explicit @H@(std::shared_ptr<T> pointer) : pointer_(std::move(pointer)) {}
  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  @H@(const @H@<U> &other) : pointer_(other.pointer_) {}
  T *get() const { return pointer_.get(); }
  void reset() { pointer_.reset(); }
  std::shared_ptr<T> pointer_;
};
"""

HANDLE_TRAITS = """\
template <typename T> struct @TRAITS@<@H@<T>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = true;
  template <typename> static constexpr bool is_handle_rebindable = true;
  using value_type = T;
  template <typename U> using rebind_t = @H@<U>;
  template <typename Target>
  static constexpr bool is_rebindable = std::is_same_v<Target, @H@<T>>;
  static T *get(@H@<T> &wrapper) { return wrapper.get(); }
  static const T *get(const @H@<T> &wrapper) { return wrapper.get(); }
  static T &borrow(@H@<T> &wrapper) { return *wrapper.get(); }
  static const T &borrow(const @H@<T> &wrapper) { return *wrapper.get(); }
  static bool empty(const @H@<T> &wrapper) { return !wrapper.get(); }
  static void reset(@H@<T> &wrapper) { wrapper.reset(); }
  template <typename... Args> static @H@<T> make(Args &&...args) {
    return @H@<T>(std::make_shared<T>(T{std::forward<Args>(args)...}));
  }
  static @H@<T> from_pointer(T *pointer) {
    return @H@<T>(std::shared_ptr<T>(pointer));
  }
  template <typename TargetWrapper, typename Factory, typename Context>
  static TargetWrapper &resolve_type(Factory &factory, Context &context,
                                     dingo::type_descriptor, dingo::type_descriptor) {
    return factory.template resolve<TargetWrapper>(context);
  }
};
"""

HANDLE_STORAGE_TRAITS = """\
template <typename T, typename U> struct @STORAGE_TRAITS@<dingo::shared, @H@<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;
  using value_types = dingo::type_list<U>;
  using lvalue_reference_types = dingo::type_list<U &, @H@<U> &>;
  using rvalue_reference_types = dingo::type_list<>;
  using pointer_types = dingo::type_list<U *, @H@<U> *>;
};
"""


WRAPPER_TRAITS = """\
struct api_wrapper_traits_family {};
template <typename T> struct {name}<@H@<T>> {
  using semantics = dingo::semantics::shared_owner;
  using family = api_wrapper_traits_family;
  using arguments = dingo::type_list<T>;
  template <typename U> using rebind = @H@<U>;
  static T &element(const @H@<T> &wrapper) { return *wrapper.get(); }
  template <typename... Args> static @H@<T> make(Args &&...args) {
    return @H@<T>(std::make_shared<T>(std::forward<Args>(args)...));
  }
  static @H@<T> adopt(T *element) { return @H@<T>(std::shared_ptr<T>(element)); }
};
"""


def wrapper(text: str) -> str:
    """Fills in the handle name of the wrapper_traits entry."""
    return text.replace("@H@", "api_wrapper_traits_handle")


def handle(api: str, text: str, traits: str = "dingo::type_traits", storage_traits: str = "dingo::storage_traits") -> str:
    """Fills in the handle name, derived from the entry, and the trait spellings."""
    return (
        text.replace("@H@", f"api_{api}_handle")
        .replace("@TRAITS@", traits)
        .replace("@STORAGE_TRAITS@", storage_traits)
    )


def path(api: str, spelling: str) -> CppApiBinding:
    return CppApiBinding(id=f"{api}_cpp", api=api, spelling=spelling)


# A sharing kind of the user and a handle of it, for the wrapper semantics entry.
LAYER_KIND_TRAITS = """\
struct @P@kind {};
template <> struct {name}<@P@kind> {
  static constexpr std::uint16_t properties =
      dingo::property::owning | dingo::property::sharing |
      dingo::property::nullable | dingo::property::borrowable |
      dingo::property::constructible | dingo::property::copyable |
      dingo::property::mapped;
  using operations = dingo::type_list<
      dingo::op<dingo::operation::borrow>,
      dingo::op<dingo::operation::adopt, dingo::semantics::pointer>,
      dingo::op<dingo::operation::upcast_copied>>;
};
struct @P@family {};
template <typename T> struct dingo::wrapper_traits<@H@<T>> {
  using semantics = @P@kind;
  using family = @P@family;
  using arguments = dingo::type_list<T>;
  template <typename U> using rebind = @H@<U>;
  static T &element(const @H@<T> &wrapper) { return *wrapper.get(); }
  template <typename... Args> static @H@<T> make(Args &&...args) {
    return @H@<T>(std::make_shared<T>(std::forward<Args>(args)...));
  }
  static @H@<T> adopt(T *element) { return @H@<T>(std::shared_ptr<T>(element)); }
};
"""

def name(
    api: str,
    spelling: str,
    header: str,
    usage: str,
    declarations: str | None = None,
) -> CppApiBinding:
    prefix = f"api_{api}_"
    return CppApiBinding(
        id=f"{api}_cpp",
        api=api,
        spelling=spelling,
        header=header,
        declarations=None if declarations is None else declarations.replace("@P@", prefix),
        usage=usage.replace("@P@", prefix),
    )


# Global declarations shared by several lookup entries.
LOOKUP_TYPES = """\
struct @P@processor {
  virtual ~@P@processor() = default;
};
struct @P@processor_impl : @P@processor {};
"""

cpp_api_bindings: tuple[CppApiBinding, ...] = (
    # Include paths
    path("include_container", CONTAINER),
    path("include_runtime_container", "dingo/runtime_container.h"),
    path("include_static_container", "dingo/static_container.h"),
    path("include_scope_shared", "dingo/storage/shared.h"),
    path("include_scope_unique", "dingo/storage/unique.h"),
    path("include_scope_external", "dingo/storage/external.h"),
    path("include_scope_shared_cyclical", "dingo/storage/shared_cyclical.h"),
    path("include_factory_constructor", "dingo/factory/constructor.h"),
    path("include_factory_function", "dingo/factory/function.h"),
    path("include_factory_callable", "dingo/factory/callable.h"),
    path("include_constructor_annotation", "dingo/registration/constructor.h"),
    path("include_annotated", "dingo/type/annotated.h"),
    path("include_lookup", LOOKUP),
    path("include_wrapper_traits", TYPE_TRAITS),
    path("include_conversion_traits", CONVERSION),
    path("include_collection_traits", "dingo/type/collection_traits.h"),
    path("include_auto_constructible", "dingo/type/auto_constructible.h"),
    path("include_rebind", "dingo/type/rebind_type.h"),
    path("include_rtti_typeid", "dingo/rtti/typeid_provider.h"),
    path("include_rtti_static", "dingo/rtti/static_provider.h"),
    # Containers
    name(
        "container",
        "dingo::container",
        CONTAINER,
        TYPES
        + """\
using static_part = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<service>>>;
{name}<> runtime_only;
{name}<static_part> mixed;
{name}<dingo::dynamic_container_traits, std::allocator<char>> with_allocator{
    std::allocator<char>()};
runtime_only.register_type<dingo::scope<dingo::shared>, dingo::storage<service>>();
service &resolved = runtime_only.resolve<service &>();
service &mixed_resolved = mixed.resolve<service &>();
(void)resolved;
(void)mixed_resolved;
""",
    ),
    name(
        "runtime_container",
        "dingo::runtime_container",
        "dingo/runtime_container.h",
        TYPES
        + """\
{name}<> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<service>>();
{name}<> moved = std::move(container);
service &resolved = moved.resolve<service &>();
(void)resolved;
""",
    ),
    name(
        "static_container",
        "dingo::static_container",
        "dingo/static_container.h",
        TYPES
        + """\
using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<service>>>;
{name}<source> container;
service &resolved = container.resolve<service &>();
service made = container.construct<service>();
(void)resolved;
(void)made;
""",
    ),
    name(
        "container_parent",
        "dingo::container",
        CONTAINER,
        TYPES
        + """\
using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<service>>>;
{name}<> parent;
{name}<> child(&parent);
{name}<source, {name}<>> mixed_child(&parent);
dingo::static_container<source, {name}<>> static_child(&parent);
""",
    ),
    # Container operations
    name(
        "register_type",
        "register_type",
        CONTAINER,
        TYPES
        + """\
dingo::container<> container;
int value = 1;
container.{name}<dingo::scope<dingo::shared>, dingo::storage<service>>();
container.{name}<dingo::scope<dingo::unique>,
                 dingo::storage<std::unique_ptr<implementation>>,
                 dingo::interfaces<interface>>();
container.{name}<dingo::scope<dingo::external>, dingo::storage<int *>>(&value);
container.{name}<dingo::scope<dingo::external>, dingo::storage<double>>(1.5);
""",
    ),
    name(
        "register_type_collection",
        "register_type_collection",
        CONTAINER,
        """\
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<dingo::collection<@P@processor>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::unique>,
                         dingo::storage<std::unique_ptr<@P@processor_impl>>,
                         dingo::interfaces<@P@processor>>();
container.{name}<
    dingo::scope<dingo::unique>,
    dingo::storage<std::vector<std::unique_ptr<@P@processor>>>>(
    [](auto &collection, auto &&value) {
      collection.emplace_back(std::forward<decltype(value)>(value));
    });
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "registration_proxy",
        "register_type",
        CONTAINER,
        """\
struct holder {
  int value;
};
dingo::container<> container;
container.register_type<dingo::scope<dingo::unique>, dingo::storage<holder>>()
    ->{name}<dingo::scope<dingo::external>, dingo::storage<int>>(2);
holder resolved = container.resolve<holder>();
(void)resolved;
""",
    ),
    name(
        "resolve",
        "resolve",
        CONTAINER,
        TYPES
        + """\
dingo::container<> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<std::shared_ptr<service>>>();
container.register_type<dingo::scope<dingo::unique>, dingo::storage<std::unique_ptr<implementation>>,
                        dingo::interfaces<interface>>();
service &reference = container.{name}<service &>();
service *pointer = container.{name}<service *>();
std::shared_ptr<service> shared = container.{name}<std::shared_ptr<service>>();
std::unique_ptr<interface> owned = container.{name}<std::unique_ptr<interface>>();
(void)reference;
(void)pointer;
""",
    ),
    name(
        "construct",
        "construct",
        CONTAINER,
        """\
struct dependency_holder {
  int &value;
};
struct alternative_a {
  explicit alternative_a(int init) : value(init) {}
  int value;
};
struct alternative_b {
  explicit alternative_b(float init) : value(init) {}
  float value;
};
dingo::container<> container;
container.register_type<dingo::scope<dingo::external>, dingo::storage<int>>(7);
container.register_type<dingo::scope<dingo::external>, dingo::storage<float>>(3.5f);
dependency_holder made = container.{name}<dependency_holder>();
auto chosen = container.{name}<std::variant<alternative_a, alternative_b>,
                               dingo::constructor<alternative_a>>();
alternative_a called = container.{name}<alternative_a>(
    dingo::callable([](int value) { return alternative_a(value); }));
(void)made;
(void)chosen;
(void)called;
""",
    ),
    name(
        "invoke",
        "invoke",
        CONTAINER,
        """\
struct overloaded {
  int operator()(int value) const { return value * 2; }
  float operator()(float value) const { return value; }
};
dingo::container<> container;
container.register_type<dingo::scope<dingo::external>, dingo::storage<int>>(7);
int called = container.{name}([](int value) { return value + 1; });
int selected = container.{name}<int(int)>(overloaded{});
(void)called;
(void)selected;
""",
    ),
    # Registration vocabulary
    name(
        "bindings",
        "dingo::bindings",
        CONTAINER,
        TYPES
        + """\
using source = {name}<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<service>>,
    dingo::bind<dingo::scope<dingo::unique>,
                dingo::storage<std::unique_ptr<implementation>>,
                dingo::interfaces<interface>>>;
dingo::container<source> container;
// Local bindings of a registration configure its child container.
dingo::container<> runtime;
runtime.register_type<
    dingo::scope<dingo::unique>, dingo::storage<service>,
    {name}<dingo::bind<dingo::scope<dingo::shared>, dingo::storage<int>>>>();
(void)container;
""",
    ),
    name(
        "bind",
        "dingo::bind",
        CONTAINER,
        TYPES
        + """\
struct consumer {
  explicit consumer(service &) {}
};
using source = dingo::bindings<
    {name}<dingo::scope<dingo::shared>, dingo::storage<service>>,
    {name}<dingo::scope<dingo::shared>, dingo::storage<consumer>,
           dingo::dependencies<service &>>,
    {name}<dingo::scope<dingo::shared>, dingo::storage<implementation>,
           dingo::interfaces<interface>,
           dingo::factory<dingo::constructor<implementation()>>>>;
dingo::static_container<source> container;
consumer &resolved = container.resolve<consumer &>();
(void)resolved;
""",
    ),
    name(
        "scope",
        "dingo::scope",
        CONTAINER,
        TYPES
        + """\
dingo::container<> container;
container.register_type<{name}<dingo::shared>, dingo::storage<service>>();
container.register_type<{name}<dingo::unique>, dingo::storage<std::string>>();
""",
    ),
    name(
        "storage",
        "dingo::storage",
        CONTAINER,
        TYPES
        + """\
dingo::container<> container;
container.register_type<dingo::scope<dingo::shared>, {name}<service>>();
container.register_type<dingo::scope<dingo::shared>, {name}<std::shared_ptr<implementation>>>();
container.register_type<dingo::scope<dingo::unique>, {name}<std::unique_ptr<std::string>>>();
container.register_type<dingo::scope<dingo::unique>, {name}<std::optional<int>>>();
container.register_type<dingo::scope<dingo::shared>, {name}<int[2][3]>>();
""",
    ),
    name(
        "factory",
        "dingo::factory",
        CONTAINER,
        TYPES
        + """\
dingo::container<> container;
container.register_type<dingo::scope<dingo::unique>, dingo::storage<service>,
                        {name}<dingo::constructor<service>>>();
""",
    ),
    name(
        "interfaces",
        "dingo::interfaces",
        CONTAINER,
        TYPES
        + """\
struct second_interface { virtual ~second_interface() = default; };
struct both : interface, second_interface {};
dingo::container<> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<both>,
                        {name}<interface, second_interface>>();
interface &first = container.resolve<interface &>();
second_interface &second = container.resolve<second_interface &>();
(void)first;
(void)second;
""",
    ),
    name(
        "dependencies",
        "dingo::dependencies",
        CONTAINER,
        TYPES
        + """\
struct consumer {
  explicit consumer(service &) {}
};
using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<service>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<consumer>,
                {name}<service &>>>;
dingo::container<source> container;
""",
    ),
    # Keys and requests
    name(
        "key_type",
        "dingo::key_type",
        CONTAINER,
        """\
struct tag {};
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::typed<tag, @P@processor>, dingo::associative<std::size_t, @P@processor>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>, {name}<tag>>();
@P@processor &typed = container.resolve<@P@processor &>({name}<tag>{});
using fixed = dingo::dependency<@P@processor &, {name}<std::size_t, 1>>;
static_assert(!std::is_void_v<fixed>);
(void)typed;
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "key_value",
        "dingo::key_value",
        CONTAINER,
        """\
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::string, @P@processor>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>>(
    {name}{std::string("processor")});
@P@processor &resolved = container.resolve<@P@processor &>(std::string("processor"));
(void)resolved;
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "annotated",
        "dingo::annotated",
        "dingo/type/annotated.h",
        """\
struct primary_tag {};
struct replica_tag {};
struct database { int id; };
struct repository {
  repository({name}<database &, primary_tag> primary_db,
             {name}<database &, replica_tag> replica_db)
      : primary(primary_db), replica(replica_db) {}
  database &primary;
  database &replica;
};
database primary{1};
database replica{2};
dingo::container<> container;
container.register_type<dingo::scope<dingo::external>, dingo::storage<database *>,
                        dingo::interfaces<{name}<database, primary_tag>>>(&primary);
container.register_type<dingo::scope<dingo::external>, dingo::storage<database *>,
                        dingo::interfaces<{name}<database, replica_tag>>>(&replica);
repository resolved = container.construct<repository>();
(void)resolved;
""",
    ),
    name(
        "dependency",
        "dingo::dependency",
        CONTAINER,
        """\
struct pipeline {
  explicit pipeline(
      {name}<@P@processor &, dingo::key_type<std::size_t, 1>> first)
      : processor(first) {}
  @P@processor &processor;
};
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, @P@processor>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>>(
    dingo::key_value{std::size_t(1)});
pipeline resolved = container.construct<pipeline>();
(void)resolved;
""",
        declarations=LOOKUP_TYPES,
    ),
    # Scopes
    name(
        "scope_shared",
        "dingo::shared",
        "dingo/storage/shared.h",
        TYPES
        + """\
dingo::container<> container;
container.register_type<dingo::scope<{name}>, dingo::storage<service>>();
bool same = &container.resolve<service &>() == container.resolve<service *>();
(void)same;
""",
    ),
    name(
        "scope_unique",
        "dingo::unique",
        "dingo/storage/unique.h",
        TYPES
        + """\
dingo::container<> container;
container.register_type<dingo::scope<{name}>, dingo::storage<service>>();
service made = container.resolve<service>();
(void)made;
""",
    ),
    name(
        "scope_external",
        "dingo::external",
        "dingo/storage/external.h",
        TYPES
        + """\
service instance;
dingo::container<> container;
container.register_type<dingo::scope<{name}>, dingo::storage<service *>>(&instance);
bool same = &container.resolve<service &>() == &instance;
(void)same;
""",
    ),
    name(
        "scope_shared_cyclical",
        "dingo::shared_cyclical",
        "dingo/storage/shared_cyclical.h",
        """\
struct cyclic_b;
struct cyclic_a {
  cyclic_a(cyclic_b &b, std::shared_ptr<cyclic_b> pointer)
      : b_(b), pointer_(std::move(pointer)) {}
  cyclic_b &b_;
  std::shared_ptr<cyclic_b> pointer_;
};
struct cyclic_b {
  cyclic_b(cyclic_a &a, cyclic_a *pointer) : a_(a), pointer_(pointer) {}
  cyclic_a &a_;
  cyclic_a *pointer_;
};
dingo::container<> container;
container.register_type<dingo::scope<{name}>, dingo::storage<cyclic_a>>();
container.register_type<dingo::scope<{name}>, dingo::storage<std::shared_ptr<cyclic_b>>>();
cyclic_a &a = container.resolve<cyclic_a &>();
cyclic_b &b = container.resolve<cyclic_b &>();
(void)a;
(void)b;
""",
    ),
    # Factory kinds
    name(
        "factory_constructor",
        "dingo::constructor",
        "dingo/factory/constructor.h",
        """\
dingo::container<> container;
container.register_type<dingo::scope<dingo::external>, dingo::storage<double>>(1.1);
container.register_type<dingo::scope<dingo::external>, dingo::storage<int>>(1);
container.register_type<dingo::scope<dingo::external>, dingo::storage<float>>(1.f);
container.register_type<dingo::scope<dingo::unique>,
                        dingo::storage<api_constructor_overloaded>,
                        dingo::factory<{name}<api_constructor_overloaded(double)>>>();
container.register_type<dingo::scope<dingo::unique>, dingo::storage<api_constructor_generic>,
                        dingo::factory<{name}<api_constructor_generic(int, float)>>>();
""",
        declarations="""\
struct api_constructor_overloaded {
  api_constructor_overloaded(int);
  api_constructor_overloaded(double) {}
};
struct api_constructor_generic {
  template <typename First, typename Second> api_constructor_generic(First, Second) {}
};
""",
    ),
    name(
        "factory_function",
        "dingo::function",
        "dingo/factory/function.h",
        """\
struct with_factory {
  static with_factory make() { return with_factory(); }

private:
  with_factory() = default;
};
dingo::container<> container;
container.register_type<dingo::scope<dingo::unique>, dingo::storage<with_factory>,
                        dingo::factory<{name}<&with_factory::make>>>();
""",
    ),
    name(
        "factory_callable",
        "dingo::callable",
        "dingo/factory/callable.h",
        """\
struct product {
  int value;
};
struct overloaded {
  product operator()(int value) const { return product{value * 2}; }
  product operator()(float value) const { return product{static_cast<int>(value)}; }
};
dingo::container<> container;
container.register_type<dingo::scope<dingo::external>, dingo::storage<int>>(2);
container.register_type<dingo::scope<dingo::unique>, dingo::storage<product>>(
    {name}([](int value) { return product{value * 2}; }));
product made = container.construct<product>({name}<product(int)>(overloaded{}));
(void)made;
""",
    ),
    # Lookups
    name(
        "lookups",
        "dingo::lookups",
        LOOKUP,
        """\
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = {name}<dingo::collection<@P@processor>>;
};
dingo::container<traits> container;
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "lookup_associative",
        "dingo::associative",
        LOOKUP,
        """\
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<{name}<std::string, @P@processor>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>>(
    dingo::key_value{std::string("first")});
@P@processor &resolved = container.resolve<@P@processor &>(std::string("first"));
(void)resolved;
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "lookup_typed",
        "dingo::typed",
        LOOKUP,
        """\
struct primary {};
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<{name}<primary, @P@processor, dingo::many>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>, dingo::key_type<primary>>();
auto all = container.resolve<std::vector<@P@processor *>>(dingo::key_type<primary>{});
(void)all;
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "lookup_collection",
        "dingo::collection",
        LOOKUP,
        """\
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<{name}<@P@processor>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>>();
auto all = container.resolve<std::vector<@P@processor *>>();
(void)all;
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "cardinality_one",
        "dingo::one",
        LOOKUP,
        """\
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, @P@processor, {name}>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>>(
    dingo::key_value{std::size_t(1)});
@P@processor &resolved = container.resolve<@P@processor &>(std::size_t(1));
(void)resolved;
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "cardinality_many",
        "dingo::many",
        LOOKUP,
        """\
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, @P@processor, {name}>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>>(
    dingo::key_value{std::size_t(1)});
auto all = container.resolve<std::vector<@P@processor *>>(std::size_t(1));
(void)all;
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "backend_ordered",
        "dingo::ordered",
        LOOKUP,
        """\
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, @P@processor, dingo::one, {name}>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>>(
    dingo::key_value{std::size_t(1)});
@P@processor &resolved = container.resolve<@P@processor &>(std::size_t(1));
(void)resolved;
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "backend_unordered",
        "dingo::unordered",
        LOOKUP,
        """\
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, @P@processor, dingo::one, {name}>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>>(
    dingo::key_value{std::size_t(1)});
@P@processor &resolved = container.resolve<@P@processor &>(std::size_t(1));
(void)resolved;
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "backend_array",
        "dingo::array",
        LOOKUP,
        """\
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, @P@processor, dingo::one, {name}<8>>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>>(
    dingo::key_value{std::size_t(1)});
@P@processor &resolved = container.resolve<@P@processor &>(std::size_t(1));
(void)resolved;
""",
        declarations=LOOKUP_TYPES,
    ),
    # Container traits and RTTI
    name(
        "dynamic_container_traits",
        "dingo::dynamic_container_traits",
        CONTAINER,
        """\
struct traits : {name} {};
dingo::container<traits> container;
dingo::container<{name}> plain;
""",
    ),
    name(
        "static_container_traits",
        "dingo::static_container_traits",
        CONTAINER,
        """\
struct traits : {name} {};
struct service {};
using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<service>>>;
dingo::static_container<source, traits> container;
""",
    ),
    name(
        "rtti",
        "dingo::rtti",
        "dingo/rtti/typeid_provider.h",
        """\
using typeid_rtti = {name}<dingo::typeid_provider>;
using static_rtti = {name}<dingo::static_provider>;
static_assert(!std::is_same_v<typeid_rtti, static_rtti>);
""",
    ),
    name(
        "rtti_typeid_provider",
        "dingo::typeid_provider",
        "dingo/rtti/typeid_provider.h",
        """\
struct traits : dingo::dynamic_container_traits {
  using rtti_type = dingo::rtti<{name}>;
};
dingo::container<traits> container;
""",
    ),
    name(
        "rtti_static_provider",
        "dingo::static_provider",
        "dingo/rtti/static_provider.h",
        """\
struct traits : dingo::dynamic_container_traits {
  using rtti_type = dingo::rtti<{name}>;
};
dingo::container<traits> container;
""",
    ),
    # Exceptions
    name(
        "exception",
        "dingo::exception",
        CONTAINER,
        """\
static_assert(std::is_base_of_v<std::exception, {name}>);
try {
  throw {name}("message");
} catch (const {name} &caught) {
  (void)caught.what();
}
""",
    ),
    name(
        "type_not_found_exception",
        "dingo::type_not_found_exception",
        CONTAINER,
        """\
static_assert(std::is_base_of_v<dingo::exception, {name}>);
dingo::container<> container;
struct missing {
  explicit missing(int) {}
};
try {
  container.resolve<missing &>();
} catch (const {name} &) {
}
""",
    ),
    name(
        "type_not_convertible_exception",
        "dingo::type_not_convertible_exception",
        CONTAINER,
        """\
static_assert(std::is_base_of_v<dingo::exception, {name}>);
struct alternative_a {};
struct alternative_b {};
dingo::container<> container;
container.register_type<dingo::scope<dingo::unique>, dingo::storage<std::variant<alternative_a, alternative_b>>,
                        dingo::factory<dingo::constructor<alternative_a>>>();
try {
  container.resolve<alternative_b>();
} catch (const {name} &) {
}
""",
    ),
    name(
        "type_ambiguous_exception",
        "dingo::type_ambiguous_exception",
        CONTAINER,
        """\
static_assert(std::is_base_of_v<dingo::exception, {name}>);
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<dingo::collection<@P@processor>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>>();
container.register_type<dingo::scope<dingo::shared>, dingo::storage<std::shared_ptr<@P@processor_impl>>,
                        dingo::interfaces<@P@processor>>();
try {
  container.resolve<@P@processor &>();
} catch (const {name} &) {
}
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "type_recursion_exception",
        "dingo::type_recursion_exception",
        CONTAINER,
        """\
static_assert(std::is_base_of_v<dingo::exception, {name}>);
struct recursive_b;
struct recursive_a {
  explicit recursive_a(recursive_b &) {}
};
struct recursive_b {
  explicit recursive_b(recursive_a &) {}
};
dingo::container<> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<recursive_a>,
                        dingo::factory<dingo::constructor<recursive_a(recursive_b &)>>>();
container.register_type<dingo::scope<dingo::shared>, dingo::storage<recursive_b>,
                        dingo::factory<dingo::constructor<recursive_b(recursive_a &)>>>();
try {
  container.resolve<recursive_a &>();
} catch (const {name} &) {
}
""",
    ),
    name(
        "lookup_already_registered_exception",
        "dingo::lookup_already_registered_exception",
        CONTAINER,
        """\
static_assert(std::is_base_of_v<dingo::exception, {name}>);
struct service {};
dingo::container<> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<service>>();
try {
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<service>>();
} catch (const {name} &) {
}
""",
    ),
    name(
        "type_index_out_of_range_exception",
        "dingo::type_index_out_of_range_exception",
        LOOKUP,
        """\
static_assert(std::is_base_of_v<dingo::exception, {name}>);
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<std::size_t, @P@processor, dingo::one, dingo::array<2>>>;
};
dingo::container<traits> container;
try {
  container.resolve<@P@processor &>(std::size_t(5));
} catch (const {name} &) {
}
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "virtual_pointer_exception",
        "dingo::virtual_pointer_exception",
        CONTAINER,
        """\
static_assert(std::is_base_of_v<dingo::exception, {name}>);
try {
  throw {name}("message");
} catch (const dingo::exception &) {
}
""",
    ),
    # Names used by the extension points
    name(
        "type_list",
        "dingo::type_list",
        TYPE_TRAITS,
        "using list = {name}<int, long>;\nstatic_assert(!std::is_void_v<list>);",
    ),
    name(
        "exact_lookup",
        "dingo::exact_lookup",
        "dingo/type/rebind_type.h",
        "using request = {name}<std::optional<int>> &;\nstatic_assert(std::is_reference_v<request>);",
    ),
    name(
        "access_borrow",
        "dingo::borrow",
        CONVERSION,
        "using access = {name};\nstatic_assert(!std::is_void_v<access>);",
    ),
    name(
        "access_consume",
        "dingo::consume",
        CONVERSION,
        "using access = {name};\nstatic_assert(!std::is_void_v<access>);",
    ),
    # Extension points
    name(
        "wrapper_traits",
        "dingo::wrapper_traits",
        TYPE_TRAITS,
        TYPES
        + wrapper(
            """\
dingo::container<> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@H@<service>>>();
service &borrowed = container.resolve<service &>();
@H@<service> &handle = container.resolve<@H@<service> &>();
@H@<service> copy = container.resolve<@H@<service>>();
(void)borrowed;
(void)handle;
(void)copy;
""",
        ),
        declarations=wrapper(HANDLE_CLASS + WRAPPER_TRAITS),
    ),
    name(
        "wrapper_semantics",
        "dingo::wrapper_semantics",
        TYPE_TRAITS,
        TYPES
        + handle(
            "wrapper_semantics",
            """\
dingo::container<> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@H@<service>>>();
service &borrowed = container.resolve<service &>();
@H@<service> &handle = container.resolve<@H@<service> &>();
@H@<service> copy = container.resolve<@H@<service>>();
(void)borrowed;
(void)handle;
(void)copy;
""",
        ),
        declarations=handle("wrapper_semantics", HANDLE_CLASS + LAYER_KIND_TRAITS),
    ),
    name(
        "conversion_traits",
        "dingo::type_conversion_traits",
        CONVERSION,
        TYPES
        + handle(
            "conversion_traits",
            """\
@H@<implementation> source(std::make_shared<implementation>());
@H@<interface> target = {name}<@H@<interface>, @H@<implementation>>::convert(source);
static_assert(std::is_same_v<
              {name}<@H@<interface>, @H@<implementation>>::required_access<
                  const @H@<implementation> &>,
              dingo::borrow>);
static_assert(std::is_same_v<
              {name}<@H@<interface>, @H@<implementation>>::required_access<
                  @H@<implementation> &&>,
              dingo::consume>);
(void)target;
""",
        ),
        declarations=handle(
            "conversion_traits",
            HANDLE_CLASS
            + """\
template <typename Target, typename Source>
struct @TRAITS@<@H@<Target>, @H@<Source>,
                std::enable_if_t<std::is_convertible_v<Source *, Target *>>> {
  template <typename From>
  using required_access =
      std::conditional_t<std::is_rvalue_reference_v<From>, dingo::consume, dingo::borrow>;

  static @H@<Target> convert(const @H@<Source> &source) {
    return @H@<Target>(source);
  }
};
""",
            traits="{name}",
        ),
    ),
    name(
        "collection_traits",
        "dingo::collection_traits",
        "dingo/type/collection_traits.h",
        """\
static_assert(dingo::collection_traits<std::vector<int>>::is_collection);
static_assert({name}<api_collection_traits_stack<int>>::is_collection);
""",
        declarations="""\
template <typename T> struct api_collection_traits_stack {
  std::vector<T> items;
};
template <typename T> struct {name}<api_collection_traits_stack<T>> {
  static const bool is_collection = true;
  static const bool has_fixed_size_construct = false;
  using resolve_type = T;
  static void reserve(api_collection_traits_stack<T> &collection, std::size_t size) {
    collection.items.reserve(size);
  }
  template <typename U>
  static void add(api_collection_traits_stack<T> &collection, U &&value) {
    collection.items.emplace_back(std::forward<U>(value));
  }
};
""",
    ),
    name(
        "auto_construction",
        "dingo::is_auto_constructible",
        "dingo/type/auto_constructible.h",
        """\
dingo::container<> container;
container.register_type<dingo::scope<dingo::external>, dingo::storage<int>>(5);
api_auto_construction_service made = container.resolve<api_auto_construction_service>();
static_assert({name}<api_auto_construction_service>::value);
(void)made;
""",
        declarations="""\
struct api_auto_construction_service {
  explicit api_auto_construction_service(int) {}
};
template <> struct {name}<api_auto_construction_service> : std::true_type {};
""",
    ),
    name(
        "constructor_annotation",
        "DINGO_CONSTRUCTOR",
        "dingo/registration/constructor.h",
        """\
dingo::container<> container;
container.register_type<dingo::scope<dingo::external>, dingo::storage<int>>(5);
container.register_type<dingo::scope<dingo::external>, dingo::storage<float>>(1.5f);
container.register_type<dingo::scope<dingo::unique>,
                        dingo::storage<api_constructor_annotation_service>>();
api_constructor_annotation_service made =
    container.resolve<api_constructor_annotation_service>();
(void)made;
""",
        declarations="""\
struct api_constructor_annotation_service {
  api_constructor_annotation_service(int, double);
  {name}(api_constructor_annotation_service(int, float)) {}
};
""",
    ),
    name(
        "container_traits",
        "lookup_definition_type",
        CONTAINER,
        """\
struct traits {
  using rtti_type = dingo::rtti<dingo::typeid_provider>;
  using allocator_type = std::allocator<char>;
  using {name} = dingo::lookups<dingo::collection<@P@processor>>;
};
dingo::container<traits> runtime;
struct static_traits {
  using rtti_type = dingo::rtti<dingo::static_provider>;
  using allocator_type = std::allocator<char>;
  using {name} = dingo::lookups<dingo::collection<@P@processor>>;
};
using source = dingo::bindings<dingo::bind<
    dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
    dingo::interfaces<@P@processor>>>;
dingo::static_container<source, static_traits> compile_time;
""",
        declarations=LOOKUP_TYPES,
    ),
    name(
        "lookup_backend",
        "index",
        LOOKUP,
        """\
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<std::size_t, @P@processor, dingo::many, @P@backend>>;
};
dingo::container<traits> container;
container.register_type<dingo::scope<dingo::shared>, dingo::storage<@P@processor_impl>,
                        dingo::interfaces<@P@processor>>(
    dingo::key_value{std::size_t(1)});
auto all = container.resolve<std::vector<@P@processor *>>(std::size_t(1));
(void)all;
""",
        declarations=LOOKUP_TYPES
        + """\
struct @P@backend {
  template <typename Value, typename Candidate> class {name} {
  public:
    void build(const dingo::index_item<Value, Candidate> *items, std::size_t count) {
      items_ = items;
      count_ = count;
    }
    template <typename Visit> void visit(const Value &key, Visit &&visit) const {
      for (std::size_t at = 0; at < count_; ++at) {
        if (items_[at].key == key && !visit(items_[at].candidate)) {
          return;
        }
      }
    }

  private:
    const dingo::index_item<Value, Candidate> *items_ = nullptr;
    std::size_t count_ = 0;
  };
};
""",
    ),
    name(
        "index_item",
        "dingo::index_item",
        LOOKUP,
        """\
{name}<int, char> item{1, 'a'};
(void)item;
""",
    ),
    name(
        "scope_traits",
        "dingo::scope_traits",
        CONTAINER,
        """\
struct service {};
dingo::container<> container;
container.register_type<dingo::scope<@P@scope>, dingo::storage<service>>();
service &resolved = container.resolve<service &>();
(void)resolved;
""",
        declarations="""\
struct @P@scope {};
template <> struct {name}<@P@scope> {
  static constexpr dingo::scope_descriptor descriptor =
      {name}<dingo::shared>::descriptor;
};
""",
    ),
    name(
        "scope_descriptor",
        "dingo::scope_descriptor",
        CONTAINER,
        """\
constexpr {name} descriptor = dingo::scope_traits<dingo::unique>::descriptor;
(void)descriptor;
""",
    ),
    name(
        "scope_slot",
        "dingo::scope_slot",
        CONTAINER,
        """\
struct service {};
dingo::container<> container;
container.register_type<dingo::scope<@P@scope>, dingo::storage<service>>();
service &resolved = container.resolve<service &>();
(void)resolved;
""",
        declarations="""\
template <typename Binding> class @P@slot : public {name}<Binding> {
public:
  using stored_type = typename Binding::stored_type;
  using {name}<Binding>::scope_slot;
  template <typename Resolver> stored_type &get(Resolver &resolver) {
    if (!made_) {
      this->construct(storage_, resolver);
      made_ = true;
    }
    return *reinterpret_cast<stored_type *>(storage_);
  }
  template <typename Resolver> bool settled(Resolver &) const { return true; }

private:
  alignas(stored_type) unsigned char storage_[sizeof(stored_type)];
  bool made_ = false;
};
struct @P@scope {};
template <> struct dingo::scope_traits<@P@scope> {
  static constexpr dingo::scope_descriptor descriptor =
      dingo::scope_traits<dingo::shared>::descriptor;
  template <typename Binding> using slot = @P@slot<Binding>;
};
""",
    ),
    name(
        "rtti_provider",
        "dingo::rtti",
        "dingo/rtti/typeid_provider.h",
        """\
struct traits : dingo::dynamic_container_traits {
  using rtti_type = {name}<api_rtti_provider>;
};
dingo::container<traits> container;
struct service {};
container.register_type<dingo::scope<dingo::shared>, dingo::storage<service>>();
service &resolved = container.resolve<service &>();
(void)resolved;
""",
        declarations="""\
struct api_rtti_provider {};
template <> class {name}<api_rtti_provider> {
  template <typename T> struct tag {};

public:
  class type_index {
  public:
    type_index(std::type_index value) : value_(value) {}
    bool operator<(const type_index &other) const { return value_ < other.value_; }
    bool operator==(const type_index &other) const { return value_ == other.value_; }
    std::size_t hash() const { return value_.hash_code(); }

  private:
    std::type_index value_;
  };

  template <typename T> static type_index get_type_index() {
    return std::type_index(typeid(tag<T>));
  }
};
template <> struct std::hash<dingo::rtti<api_rtti_provider>::type_index> {
  std::size_t operator()(const dingo::rtti<api_rtti_provider>::type_index &value) const {
    return value.hash();
  }
};
""",
    ),
    name(
        "allocator",
        "dingo::container",
        CONTAINER,
        TYPES
        + """\
{name}<dingo::dynamic_container_traits, api_allocator<char>> container{api_allocator<char>()};
container.register_type<dingo::scope<dingo::shared>, dingo::storage<service>>();
service &resolved = container.resolve<service &>();
(void)resolved;
""",
        declarations="""\
template <typename T> struct api_allocator {
  using value_type = T;
  api_allocator() = default;
  template <typename U> api_allocator(const api_allocator<U> &) {}
  T *allocate(std::size_t count) { return std::allocator<T>().allocate(count); }
  void deallocate(T *pointer, std::size_t count) { std::allocator<T>().deallocate(pointer, count); }
  template <typename U> bool operator==(const api_allocator<U> &) const { return true; }
  template <typename U> bool operator!=(const api_allocator<U> &) const { return false; }
};
""",
    ),
    # Configuration macros
    path("config_constructor_detection_args", "DINGO_CONSTRUCTOR_DETECTION_ARGS"),
    path("config_context_arena_buffer_size", "DINGO_CONTEXT_ARENA_BUFFER_SIZE"),
    path("config_runtime_arena_block_size", "DINGO_RUNTIME_ARENA_BLOCK_SIZE"),
    path("config_always_inline", "DINGO_ALWAYS_INLINE"),
    path("config_noinline", "DINGO_NOINLINE"),
    path("config_empty_bases", "DINGO_EMPTY_BASES"),
)
