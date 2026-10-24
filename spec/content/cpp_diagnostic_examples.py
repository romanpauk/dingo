"""Content of the C++ diagnostic examples section.

A program per diagnostic reported when compiled, which the next
implementation rejects with it. The generator makes a lit test of each for
every language tier: the formatted message where the compiler takes a
user-generated static_assert message, and the literal message otherwise.
"""

from spec.schema import CppDiagnosticExample

CONTAINER = ("dingo/container.h",)
STATIC = ("dingo/static_container.h", "dingo/storage/shared.h")
SHARED = ("dingo/container.h", "dingo/storage/shared.h")

SERVICE = """\
struct service {
  virtual ~service() = default;
  virtual int run() const = 0;
};

template <int Id> struct implementation : service {
  int run() const override { return Id; }
};
"""

CONFIG = """\
struct config {
  config() : value(1) {}
  int value;
};
"""


# A user wrapper of the wrapper semantics `handle_kind`, whose wrapper_semantics each
# example declares wrongly in its own way.
HANDLE = """\
#include <cstdint>
#include <memory>
#include <utility>

struct service {};

template <typename T> struct handle {
  std::shared_ptr<T> pointer;
};

struct handle_kind {};

template <typename T> struct dingo::wrapper_traits<handle<T>> {
  using semantics = handle_kind;
  using arguments = dingo::type_list<T>;
  template <typename U> using rebind = handle<U>;
  static T &element(const handle<T> &value) { return *value.pointer; }
  template <typename... Args> static handle<T> make(Args &&...args) {
    return handle<T>{std::make_shared<T>(std::forward<Args>(args)...)};
  }
};
"""

REGISTER_HANDLE = """
int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<handle<service>>>();
}
"""


def kind_example(diagnostic, traits, subject="handle_kind"):
    """A program whose handle kind has the wrapper_semantics `traits`."""
    return example(diagnostic, SHARED, HANDLE + traits + REGISTER_HANDLE, subject)


HANDLE_PROPERTIES = (
    "dingo::property::owning | dingo::property::sharing |\n"
    "      dingo::property::nullable | dingo::property::borrowable |\n"
    "      dingo::property::constructible | dingo::property::copyable |\n"
    "      dingo::property::mapped"
)


def kind_traits(forms, properties=HANDLE_PROPERTIES):
    return f"""
template <> struct dingo::wrapper_semantics<handle_kind> {{
  static constexpr std::uint16_t properties = {properties};
  using operations = {forms};
}};
"""


def example(diagnostic, includes, program, *subjects):
    return CppDiagnosticExample(
        id=diagnostic,
        diagnostic=diagnostic,
        includes=includes,
        program=program,
        subjects=subjects,
    )


cpp_diagnostic_examples: tuple[CppDiagnosticExample, ...] = (
    example(
        "type_not_found",
        STATIC,
        SERVICE
        + """
int main() {
  dingo::static_container<dingo::bindings<>> container;
  (void)container.resolve<service &>();
}
""",
        "service&",
    ),
    example(
        "type_not_found_keyed",
        STATIC,
        SERVICE
        + """
struct tag {};

int main() {
  dingo::static_container<dingo::bindings<>> container;
  (void)container.resolve<service &>(dingo::key_type<tag>{});
}
""",
        "service&",
        "tag",
    ),
    example(
        "type_ambiguous",
        STATIC + ("dingo/lookup/lookup.h",),
        SERVICE
        + """
struct traits : dingo::static_container_traits {
  using lookup_definition_type = dingo::lookups<dingo::collection<service>>;
};

int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<implementation<1>>,
                  dingo::interfaces<service>>,
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<implementation<2>>,
                  dingo::interfaces<service>>>;
  dingo::static_container<bindings, traits> container;
  (void)container.resolve<service &>();
}
""",
        "service&",
    ),
    example(
        "type_not_convertible",
        STATIC,
        CONFIG
        + """
int main() {
  using bindings =
      dingo::bindings<dingo::bind<dingo::scope<dingo::shared>, dingo::storage<config>>>;
  dingo::static_container<bindings> container;
  (void)container.resolve<config &&>();
}
""",
        "config&&",
        "config",
    ),
    example(
        "lookup_already_registered",
        STATIC,
        SERVICE
        + """
int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<implementation<1>>,
                  dingo::interfaces<service>>,
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<implementation<2>>,
                  dingo::interfaces<service>>>;
  dingo::static_container<bindings> container;
  (void)container;
}
""",
        "service",
        "implementation<2>",
        "dingo::none_t",
    ),
    example(
        "type_index_out_of_range",
        STATIC + ("dingo/lookup/lookup.h",),
        SERVICE
        + """
struct traits : dingo::static_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<int, service, dingo::one, dingo::array<2>>>;
};

int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<implementation<1>>,
                  dingo::interfaces<service>, dingo::key_type<int, 2>>>;
  dingo::static_container<bindings, traits> container;
  (void)container;
}
""",
        "dingo::key_type<int, 2>",
    ),
    example(
        "type_recursion",
        STATIC,
        """
struct left;
struct right;

struct left {
  explicit left(right &);
};

struct right {
  explicit right(left &);
};

left::left(right &) {}
right::right(left &) {}

int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<left>,
                  dingo::dependencies<right &>>,
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<right>,
                  dingo::dependencies<left &>>>;
  dingo::static_container<bindings> container;
  (void)container;
}
""",
        "left -> right -> left",
    ),
    example(
        "type_not_found_required_by",
        STATIC,
        """
struct dependency {};

struct service {
  explicit service(dependency &) {}
};

int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<service>,
                  dingo::dependencies<dependency &>>>;
  dingo::static_container<bindings> container;
  (void)container;
}
""",
        "dependency&",
        "service",
    ),
    example(
        "registration_dependencies_twice",
        SHARED,
        CONFIG
        + """
struct service {
  explicit service(config &) {}
};

service make_service(config &value) { return service(value); }

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<service>,
                          dingo::dependencies<config &>,
                          dingo::factory<dingo::function<&make_service>>>();
}
""",
        "service",
    ),
    example(
        "lookup_backend_without_index",
        SHARED,
        SERVICE
        + """
#include <dingo/lookup/lookup.h>

#include <map>

struct map_backend {
  template <typename Key, typename Value, typename Cardinality,
            typename Allocator>
  using storage = std::map<Key, Value>;
};

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<int, service, dingo::one, map_backend>>;
};

int main() {
  dingo::container<traits> container;
  (void)container;
}
""",
        "map_backend",
    ),
    example(
        "lookup_declared_twice",
        SHARED,
        SERVICE
        + """
#include <dingo/lookup/lookup.h>

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::single<service>, dingo::collection<service>>;
};

int main() {
  dingo::container<traits> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<implementation<1>>,
                          dingo::interfaces<service>>();
}
""",
        "service",
        "dingo::none_t",
    ),
    example(
        "registration_key_value_missing",
        SHARED,
        SERVICE
        + """
#include <dingo/lookup/lookup.h>

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, service>,
                     dingo::associative<long, service>>;
};

int main() {
  dingo::container<traits> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<implementation<1>>,
                          dingo::interfaces<service>>(dingo::key_value{7});
}
""",
        "implementation<1>",
        "long",
        "service",
    ),
    example(
        "alternatives_repeated",
        SHARED,
        """
#include <variant>

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<std::variant<int, int>>>();
}
""",
        "std::variant<int, int>",
    ),
    example(
        "collection_traits_missing",
        CONTAINER,
        """
int main() {
  dingo::container<> container;
  (void)container.construct_collection<int>();
}
""",
        "int",
    ),
    example(
        "key_value_not_of_key_type",
        SHARED,
        """
struct key {
  key() = default;
  key(int) = delete;
};

int main() { return sizeof(dingo::key_type<key, 1>); }
""",
        "key",
    ),
    example(
        "factory_not_declared",
        ("dingo/static_container.h", "dingo/storage/unique.h"),
        """
struct maker {
  int operator()() const { return 7; }
};

int main() {
  using bindings = dingo::bindings<dingo::bind<
      dingo::scope<dingo::unique>, dingo::storage<int>, dingo::factory<maker>>>;
  dingo::static_container<bindings> container;
  (void)container;
}
""",
        "maker",
    ),
    example(
        "conversion_access_undeclared",
        SHARED,
        """
#include <dingo/type/type_conversion_traits.h>

struct target {
  explicit target(int initial) : value(initial) {}
  int value;
};

struct source : target {
  source() : target(5) {}
};

template <> struct dingo::type_conversion_traits<target, source> {
  static target convert(const source &from) { return target(from.value); }
};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<source>,
                          dingo::interfaces<target>>();
  return container.resolve<target>().value;
}
""",
        "target",
        "source",
    ),
    example(
        "scope_undeclared",
        CONTAINER,
        """
struct undeclared_scope;

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<undeclared_scope>, dingo::storage<int>>();
}
""",
        "undeclared_scope",
    ),
    example(
        "registration_without_scope",
        SHARED,
        CONFIG
        + """
int main() {
  dingo::container<> container;
  container.register_type<dingo::storage<config>>();
}
""",
    ),
    example(
        "registration_without_storage",
        SHARED,
        """
int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>>();
}
""",
    ),
    example(
        "registration_key_twice",
        SHARED,
        CONFIG
        + """
struct tag {};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<config>,
                          dingo::key_type<tag>>(dingo::key_value{1});
}
""",
        "config",
    ),
    example(
        "registration_key_values",
        SHARED,
        CONFIG
        + """
int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<config>>(
      dingo::key_value{1}, dingo::key_value{2});
}
""",
        "config",
    ),
    example(
        "key_interface_undeclared",
        SHARED,
        SERVICE
        + """
struct other {
  virtual ~other() = default;
};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<implementation<1>>,
                          dingo::interfaces<service>>(
      dingo::key_value<int, other>{1});
}
""",
        "other",
        "implementation<1>",
    ),
    example(
        "key_type_values",
        SHARED,
        CONFIG
        + """
int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<config>,
                          dingo::key_type<int, 1, 2>>();
}
""",
        "int",
    ),
    example(
        "dense_key_type",
        SHARED + ("dingo/lookup/lookup.h",),
        SERVICE
        + """
struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<double, service, dingo::one, dingo::array<2>>>;
};

int main() {
  dingo::container<traits> container;
  (void)container;
}
""",
        "service",
        "double",
    ),
    example(
        "interface_limit",
        SHARED,
        """
template <int Id> struct base {
  virtual ~base() = default;
};

struct many : base<0>, base<1>, base<2>, base<3>, base<4>, base<5>, base<6>,
              base<7>, base<8> {};

int main() {
  dingo::container<> container;
  container.register_type<
      dingo::scope<dingo::shared>, dingo::storage<many>,
      dingo::interfaces<base<0>, base<1>, base<2>, base<3>, base<4>, base<5>,
                        base<6>, base<7>, base<8>>>();
}
""",
        "many",
    ),
    example(
        "key_alias_limit",
        SHARED,
        CONFIG
        + """
template <int Id> struct key {
  int value;
  bool operator==(const key &other) const { return value == other.value; }
  bool operator<(const key &other) const { return value < other.value; }
};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<config>>(
      dingo::key_value{key<0>{0}}, dingo::key_value{key<1>{1}},
      dingo::key_value{key<2>{2}}, dingo::key_value{key<3>{3}},
      dingo::key_value{key<4>{4}}, dingo::key_value{key<5>{5}});
}
""",
        "config",
    ),
    example(
        "scope_kind_not_stored",
        SHARED,
        CONFIG
        + """
int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<config &>>();
}
""",
        "config&",
    ),
    example(
        "scope_nested_wrappers",
        ("dingo/container.h", "dingo/storage/shared_cyclical.h"),
        CONFIG
        + """
#include <memory>
#include <optional>

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared_cyclical>,
                          dingo::storage<std::shared_ptr<std::optional<config>>>>();
}
""",
        "std::shared_ptr<std::optional<config>>",
    ),
    example(
        "no_default_factory",
        SHARED,
        SERVICE
        + """
int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<service>>();
}
""",
        "service",
    ),
    example(
        "interface_in_array",
        SHARED,
        SERVICE
        + """
int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<implementation<1>[2]>,
                          dingo::interfaces<service>>();
}
""",
        "implementation<1>[2]",
    ),
    example(
        "interface_not_derived",
        SHARED,
        SERVICE
        + CONFIG
        + """
int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<config>,
                          dingo::interfaces<service>>();
}
""",
        "config",
        "service",
    ),
    example(
        "no_interface_served",
        ("dingo/container.h", "dingo/storage/unique.h", "optional"),
        SERVICE
        + """
int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::unique>,
                          dingo::storage<std::optional<implementation<1>>>,
                          dingo::interfaces<service>>();
}
""",
        "std::optional<implementation<1>>",
    ),
    example(
        "cyclical_virtual_interface",
        ("dingo/container.h", "dingo/storage/shared_cyclical.h"),
        """
struct service {
  virtual ~service() = default;
};

struct implementation : virtual service {};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared_cyclical>,
                          dingo::storage<implementation>,
                          dingo::interfaces<service>>();
  (void)container.resolve<service &>();
}
""",
        "implementation",
        "service",
    ),
    example(
        "constructor_not_detected",
        SHARED + ("dingo/factory/constructor.h",),
        """
class hidden {
  hidden() {}
};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<hidden>,
                          dingo::factory<dingo::constructor<hidden>>>();
  (void)container.resolve<hidden &>();
}
""",
        "hidden",
    ),
    example(
        "constructor_generic",
        SHARED,
        """
struct generic {
  template <typename Type> explicit generic(Type &&) {}
};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<generic>>();
  (void)container.resolve<generic &>();
}
""",
        "generic",
    ),
    example(
        "static_instances",
        ("dingo/static_container.h", "dingo/storage/external.h"),
        CONFIG
        + """
int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::external>, dingo::storage<config &>>>;
  dingo::static_container<bindings> container;
  (void)container;
}
""",
    ),
    example(
        "collection_arguments",
        SHARED + ("dingo/storage/unique.h", "vector"),
        SERVICE
        + """
struct tag {};

int main() {
  dingo::container<> container;
  container.register_type_collection<dingo::scope<dingo::unique>,
                                     dingo::storage<std::vector<service *>>>(
      dingo::key_type<tag>{}, [](std::vector<service *> &, service *) {});
}
""",
    ),
    kind_example("semantics_undeclared", ""),
    kind_example(
        "operations_malformed",
        kind_traits("dingo::op<dingo::operation::borrow>"),
    ),
    kind_example(
        "operation_of_leaf",
        kind_traits("dingo::type_list<dingo::op<dingo::operation::copy>>"),
    ),
    kind_example(
        "operation_needs_target",
        kind_traits("dingo::type_list<dingo::op<dingo::operation::share>>"),
    ),
    kind_example(
        "operation_takes_no_target",
        kind_traits(
            "dingo::type_list<\n"
            "      dingo::op<dingo::operation::borrow, dingo::semantics::shared_owner>>"
        ),
    ),
    kind_example(
        "operation_repeated",
        kind_traits(
            "dingo::type_list<dingo::op<dingo::operation::borrow>,\n"
            "                                 dingo::op<dingo::operation::borrow>>"
        ),
    ),
    kind_example(
        "property_unknown",
        kind_traits(
            "dingo::type_list<dingo::op<dingo::operation::borrow>>",
            properties=HANDLE_PROPERTIES + " |\n      std::uint16_t(1) << 15",
        ),
    ),
    kind_example(
        "operation_needs_property",
        kind_traits(
            "dingo::type_list<\n"
            "      dingo::op<dingo::operation::release, dingo::semantics::pointer>>"
        ),
    ),
)
