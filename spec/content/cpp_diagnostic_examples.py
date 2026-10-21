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


# A user wrapper of the layer kind `handle_kind`, whose layer_kind_traits each
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
  using kind = handle_kind;
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
    """A program whose handle kind has the layer_kind_traits `traits`."""
    return example(diagnostic, SHARED, HANDLE + traits + REGISTER_HANDLE, subject)


HANDLE_PROPERTIES = (
    "dingo::layer_property::owning | dingo::layer_property::sharing |\n"
    "      dingo::layer_property::nullable | dingo::layer_property::borrowable |\n"
    "      dingo::layer_property::constructible | dingo::layer_property::copyable |\n"
    "      dingo::layer_property::wrapper"
)


def kind_traits(forms, properties=HANDLE_PROPERTIES):
    return f"""
template <> struct dingo::layer_kind_traits<handle_kind> {{
  static constexpr std::uint16_t properties = {properties};
  using forms = {forms};
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
        "scope_form_not_stored",
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
        "scope_nested_layers",
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
    kind_example("layer_kind_undeclared", ""),
    kind_example(
        "kind_forms_malformed",
        kind_traits("dingo::kind_form<dingo::route_form::borrow>"),
    ),
    kind_example(
        "kind_form_of_leaf",
        kind_traits("dingo::type_list<dingo::kind_form<dingo::route_form::copy>>"),
    ),
    kind_example(
        "kind_form_needs_target",
        kind_traits("dingo::type_list<dingo::kind_form<dingo::route_form::share>>"),
    ),
    kind_example(
        "kind_form_takes_no_target",
        kind_traits(
            "dingo::type_list<\n"
            "      dingo::kind_form<dingo::route_form::borrow, dingo::shared_kind>>"
        ),
    ),
    kind_example(
        "kind_form_repeated",
        kind_traits(
            "dingo::type_list<dingo::kind_form<dingo::route_form::borrow>,\n"
            "                                 dingo::kind_form<dingo::route_form::borrow>>"
        ),
    ),
    kind_example(
        "kind_property_unknown",
        kind_traits(
            "dingo::type_list<dingo::kind_form<dingo::route_form::borrow>>",
            properties=HANDLE_PROPERTIES + " |\n      std::uint16_t(1) << 15",
        ),
    ),
)
