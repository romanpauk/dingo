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

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace dingo {
namespace {

struct notifier {
  virtual ~notifier() = default;
  virtual int channel() const = 0;
};

struct health_probe {
  virtual ~health_probe() = default;
  virtual int status() const = 0;
};

struct email_notifier : notifier, health_probe {
  int channel() const override { return 1; }
  int status() const override { return 10; }
};

struct sms_notifier : notifier, health_probe {
  int channel() const override { return 2; }
  int status() const override { return 20; }
};

struct push_notifier : notifier {
  int channel() const override { return 3; }
};

struct billing_service {
  explicit billing_service(notifier &n) : sender(n) {}

  notifier &sender;
};

struct audit_log {
  explicit audit_log(notifier &n) : sender(n) {}

  notifier &sender;
};

struct invoice {
  explicit invoice(int count) : retries(count) {}

  int retries;
};

struct exporter {
  explicit exporter(notifier &) { throw std::runtime_error("exporter"); }
};

struct replacing_traits : dynamic_container_traits {
  using duplicate_registration_policy = replace_until_resolved;
};

struct keyed_traits : replacing_traits {
  using lookup_definition_type = lookups<associative<std::string, notifier>>;
};

// Every registration with all three keys is inserted into three lookups. The
// last one can reject a key, which fails the registration after the first two
// were already replaced.
struct multi_keyed_traits : replacing_traits {
  using lookup_definition_type =
      lookups<associative<std::string, notifier>, associative<int, notifier>,
              associative<std::size_t, notifier, one, array<2>>>;
};

struct collecting_traits : replacing_traits {
  using lookup_definition_type = lookups<collection<notifier>>;
};

using replacing_container = container<replacing_traits>;

void register_defaults(replacing_container &c) {
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  c.register_type<scope<unique>, storage<billing_service>>();
  c.register_type<scope<shared>, storage<audit_log>>();
}

} // namespace

// Success paths.

TEST(replace_registration_test, duplicate_is_rejected_by_default) {
  container<> c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  EXPECT_THROW((c.register_type<scope<shared>, storage<sms_notifier>,
                                interfaces<notifier>>()),
               lookup_already_registered_exception);
  ASSERT_EQ(c.resolve<notifier &>().channel(), 1);
}

TEST(replace_registration_test, duplicate_is_rejected_after_default_resolve) {
  container<> c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  ASSERT_EQ(c.resolve<notifier &>().channel(), 1);
  EXPECT_THROW((c.register_type<scope<shared>, storage<sms_notifier>,
                                interfaces<notifier>>()),
               lookup_already_registered_exception);
}

TEST(replace_registration_test, replacement_before_resolve_reaches_dependents) {
  replacing_container c;
  register_defaults(c);
  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>();

  ASSERT_EQ(c.resolve<notifier &>().channel(), 2);
  ASSERT_EQ(c.resolve<billing_service>().sender.channel(), 2);
  ASSERT_EQ(c.resolve<audit_log &>().sender.channel(), 2);
}

TEST(replace_registration_test, last_replacement_wins) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>();
  c.register_type<scope<shared>, storage<push_notifier>,
                  interfaces<notifier>>();
  ASSERT_EQ(c.resolve<notifier &>().channel(), 3);
}

TEST(replace_registration_test, replacement_may_change_scope) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  c.register_type<scope<unique>, storage<std::unique_ptr<push_notifier>>,
                  interfaces<notifier>>();
  ASSERT_EQ(c.resolve<std::unique_ptr<notifier>>()->channel(), 3);
}

TEST(replace_registration_test, replacement_applies_per_interface) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier, health_probe>>();
  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>();

  ASSERT_EQ(c.resolve<notifier &>().channel(), 2);
  ASSERT_EQ(c.resolve<health_probe &>().status(), 10);
}

TEST(replace_registration_test, replacement_of_every_interface) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier, health_probe>>();
  c.register_type<scope<shared>, storage<sms_notifier>,
                  interfaces<notifier, health_probe>>();

  ASSERT_EQ(c.resolve<notifier &>().channel(), 2);
  ASSERT_EQ(c.resolve<health_probe &>().status(), 20);
}

TEST(replace_registration_test, shared_interfaces_resolve_one_instance) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier, health_probe>>();
  c.register_type<scope<shared>, storage<sms_notifier>,
                  interfaces<notifier, health_probe>>();

  auto &as_notifier = c.resolve<notifier &>();
  auto &as_probe = c.resolve<health_probe &>();
  ASSERT_EQ(dynamic_cast<health_probe *>(&as_notifier), &as_probe);
}

TEST(replace_registration_test, replaced_registration_proxy_stays_valid) {
  replacing_container c;
  auto replaced = c.register_type<scope<unique>, storage<billing_service>>();
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  c.register_type<scope<unique>, storage<billing_service>>();

  // The first registration is no longer reachable, registering through its
  // proxy must still be well-formed.
  replaced->register_type<scope<external>, storage<int>>(7);
  ASSERT_EQ(c.resolve<billing_service>().sender.channel(), 1);
}

TEST(replace_registration_test, replaces_registration_local_bindings) {
  replacing_container c;
  auto registration = c.register_type<scope<unique>, storage<invoice>>();
  registration->register_type<scope<external>, storage<int>>(2);
  registration->register_type<scope<external>, storage<int>>(3);

  ASSERT_EQ(c.resolve<invoice>().retries, 3);
}

TEST(replace_registration_test, keyed_lookup_replaces_only_the_given_key) {
  container<keyed_traits> c;
  c.register_type<scope<shared>, storage<email_notifier>, interfaces<notifier>>(
      key_value{std::string("primary")});
  c.register_type<scope<shared>, storage<email_notifier>, interfaces<notifier>>(
      key_value{std::string("backup")});
  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>(
      key_value{std::string("primary")});

  ASSERT_EQ(c.resolve<notifier &>(std::string("primary")).channel(), 2);
  ASSERT_EQ(c.resolve<notifier &>(std::string("backup")).channel(), 1);
}

TEST(replace_registration_test, collections_append_and_never_replace) {
  container<collecting_traits> c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>();

  auto all = c.resolve<std::vector<notifier *>>();
  ASSERT_EQ(all.size(), 2u);
}

// Sealing.

TEST(replace_registration_test, replacement_after_resolve_throws) {
  replacing_container c;
  register_defaults(c);
  ASSERT_EQ(c.resolve<notifier &>().channel(), 1);

  EXPECT_THROW((c.register_type<scope<shared>, storage<sms_notifier>,
                                interfaces<notifier>>()),
               container_sealed_exception);
  ASSERT_EQ(c.resolve<notifier &>().channel(), 1);
  ASSERT_EQ(c.resolve<audit_log &>().sender.channel(), 1);
}

TEST(replace_registration_test, sealed_error_is_not_a_duplicate_error) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  ASSERT_EQ(c.resolve<notifier &>().channel(), 1);

  try {
    c.register_type<scope<shared>, storage<sms_notifier>,
                    interfaces<notifier>>();
    FAIL() << "expected container_sealed_exception";
  } catch (const lookup_already_registered_exception &) {
    FAIL() << "sealed container reported a plain duplicate";
  } catch (const container_sealed_exception &) {
  }
}

TEST(replace_registration_test, resolving_an_unrelated_type_seals) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  c.register_type<scope<external>, storage<int>>(1);
  ASSERT_EQ(c.resolve<int>(), 1);

  EXPECT_THROW((c.register_type<scope<shared>, storage<sms_notifier>,
                                interfaces<notifier>>()),
               container_sealed_exception);
}

TEST(replace_registration_test, construct_seals) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  ASSERT_EQ(c.construct<billing_service>().sender.channel(), 1);

  EXPECT_THROW((c.register_type<scope<shared>, storage<sms_notifier>,
                                interfaces<notifier>>()),
               container_sealed_exception);
}

TEST(replace_registration_test, collections_still_append_after_seal) {
  container<collecting_traits> c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  ASSERT_EQ(c.resolve<std::vector<notifier *>>().size(), 1u);
  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>();
  ASSERT_EQ(c.resolve<std::vector<notifier *>>().size(), 2u);
}

TEST(replace_registration_test, new_registrations_are_allowed_after_seal) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  ASSERT_EQ(c.resolve<notifier &>().channel(), 1);

  c.register_type<scope<unique>, storage<billing_service>>();
  ASSERT_EQ(c.resolve<billing_service>().sender.channel(), 1);
}

TEST(replace_registration_test, sealed_keyed_lookup_rejects_replacement) {
  container<keyed_traits> c;
  c.register_type<scope<shared>, storage<email_notifier>, interfaces<notifier>>(
      key_value{std::string("primary")});
  c.register_type<scope<shared>, storage<email_notifier>, interfaces<notifier>>(
      key_value{std::string("backup")});
  ASSERT_EQ(c.resolve<notifier &>(std::string("backup")).channel(), 1);

  EXPECT_THROW(
      (c.register_type<scope<shared>, storage<sms_notifier>,
                       interfaces<notifier>>(key_value{std::string("backup")})),
      container_sealed_exception);
  ASSERT_EQ(c.resolve<notifier &>(std::string("backup")).channel(), 1);
}

TEST(replace_registration_test, sealed_registration_local_bindings_reject) {
  replacing_container c;
  auto registration = c.register_type<scope<unique>, storage<invoice>>();
  registration->register_type<scope<external>, storage<int>>(2);
  ASSERT_EQ(c.resolve<invoice>().retries, 2);

  EXPECT_THROW((registration->register_type<scope<external>, storage<int>>(3)),
               container_sealed_exception);
  ASSERT_EQ(c.resolve<invoice>().retries, 2);
}

// Sealing is transactional.

TEST(replace_registration_test, failed_resolve_does_not_seal) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  c.register_type<scope<unique>, storage<exporter>>();
  EXPECT_THROW(c.resolve<exporter>(), std::runtime_error);

  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>();
  ASSERT_EQ(c.resolve<notifier &>().channel(), 2);
}

TEST(replace_registration_test, missing_type_resolve_does_not_seal) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  EXPECT_THROW(c.resolve<audit_log &>(), exception);

  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>();
  ASSERT_EQ(c.resolve<notifier &>().channel(), 2);
}

TEST(replace_registration_test, failed_construct_does_not_seal) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  EXPECT_THROW(c.construct<exporter>(), std::runtime_error);

  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>();
  ASSERT_EQ(c.resolve<notifier &>().channel(), 2);
}

TEST(replace_registration_test,
     outer_failure_after_successful_nested_resolve_does_not_seal) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  // The factory resolves through the container, which is a nested transaction
  // that succeeds, and then fails the enclosing resolve.
  c.register_type<scope<unique>, storage<exporter>>(
      callable([&c]() -> exporter {
        c.resolve<notifier &>();
        throw std::runtime_error("outer");
      }));
  EXPECT_THROW(c.resolve<exporter>(), std::runtime_error);

  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>();
  ASSERT_EQ(c.resolve<notifier &>().channel(), 2);
}

TEST(replace_registration_test, nested_resolve_seals_when_outer_succeeds) {
  replacing_container c;
  c.register_type<scope<shared>, storage<email_notifier>,
                  interfaces<notifier>>();
  c.register_type<scope<unique>, storage<invoice>>(
      callable([&c]() { return invoice(c.resolve<notifier &>().channel()); }));
  ASSERT_EQ(c.resolve<invoice>().retries, 1);

  EXPECT_THROW((c.register_type<scope<shared>, storage<sms_notifier>,
                                interfaces<notifier>>()),
               container_sealed_exception);
}

TEST(replace_registration_test, replacement_survives_a_failed_resolve) {
  replacing_container c;
  register_defaults(c);
  c.register_type<scope<unique>, storage<exporter>>();
  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>();
  EXPECT_THROW(c.resolve<exporter>(), std::runtime_error);

  ASSERT_EQ(c.resolve<notifier &>().channel(), 2);
}

// Registration rollback.

TEST(replace_registration_test, failed_registration_restores_replaced_lookups) {
  container<multi_keyed_traits> c;
  c.register_type<scope<shared>, storage<email_notifier>, interfaces<notifier>>(
      key_value{std::string("primary")}, key_value{7},
      key_value{std::size_t(1)});

  // A registration that is accepted by all three lookups replaces all three.
  {
    container<multi_keyed_traits> accepted;
    accepted.register_type<scope<shared>, storage<email_notifier>,
                           interfaces<notifier>>(
        key_value{std::string("primary")}, key_value{7},
        key_value{std::size_t(1)});
    accepted.register_type<scope<shared>, storage<sms_notifier>,
                           interfaces<notifier>>(
        key_value{std::string("primary")}, key_value{7},
        key_value{std::size_t(1)});
    ASSERT_EQ(accepted.resolve<notifier &>(std::string("primary")).channel(),
              2);
    ASSERT_EQ(accepted.resolve<notifier &>(7).channel(), 2);
    ASSERT_EQ(accepted.resolve<notifier &>(std::size_t(1)).channel(), 2);
  }

  // Key 5 does not fit array<2>: the string and int lookups were already
  // replaced when the array lookup throws, and must be restored.
  EXPECT_THROW((c.register_type<scope<shared>, storage<sms_notifier>,
                                interfaces<notifier>>(
                   key_value{std::string("primary")}, key_value{7},
                   key_value{std::size_t(5)})),
               std::out_of_range);

  ASSERT_EQ(c.resolve<notifier &>(std::string("primary")).channel(), 1);
  ASSERT_EQ(c.resolve<notifier &>(7).channel(), 1);
  ASSERT_EQ(c.resolve<notifier &>(std::size_t(1)).channel(), 1);
}

TEST(replace_registration_test, registration_works_after_a_rolled_back_one) {
  container<multi_keyed_traits> c;
  c.register_type<scope<shared>, storage<email_notifier>, interfaces<notifier>>(
      key_value{std::string("primary")}, key_value{7},
      key_value{std::size_t(1)});
  EXPECT_THROW((c.register_type<scope<shared>, storage<sms_notifier>,
                                interfaces<notifier>>(
                   key_value{std::string("primary")}, key_value{7},
                   key_value{std::size_t(5)})),
               std::out_of_range);

  c.register_type<scope<shared>, storage<push_notifier>, interfaces<notifier>>(
      key_value{std::string("primary")}, key_value{7},
      key_value{std::size_t(1)});
  ASSERT_EQ(c.resolve<notifier &>(std::string("primary")).channel(), 3);
  ASSERT_EQ(c.resolve<notifier &>(7).channel(), 3);
  ASSERT_EQ(c.resolve<notifier &>(std::size_t(1)).channel(), 3);
}

TEST(replace_registration_test, rolled_back_replacement_does_not_seal) {
  container<multi_keyed_traits> c;
  c.register_type<scope<shared>, storage<email_notifier>, interfaces<notifier>>(
      key_value{std::string("primary")}, key_value{7},
      key_value{std::size_t(1)});
  EXPECT_THROW((c.register_type<scope<shared>, storage<sms_notifier>,
                                interfaces<notifier>>(
                   key_value{std::string("primary")}, key_value{7},
                   key_value{std::size_t(5)})),
               std::out_of_range);

  c.register_type<scope<shared>, storage<sms_notifier>, interfaces<notifier>>(
      key_value{std::string("primary")}, key_value{7},
      key_value{std::size_t(1)});
  ASSERT_EQ(c.resolve<notifier &>(std::string("primary")).channel(), 2);
}

TEST(replace_registration_test, sealed_registration_leaves_lookups_untouched) {
  container<multi_keyed_traits> c;
  c.register_type<scope<shared>, storage<email_notifier>, interfaces<notifier>>(
      key_value{std::string("primary")}, key_value{7},
      key_value{std::size_t(1)});
  ASSERT_EQ(c.resolve<notifier &>(std::string("primary")).channel(), 1);

  EXPECT_THROW((c.register_type<scope<shared>, storage<sms_notifier>,
                                interfaces<notifier>>(
                   key_value{std::string("primary")}, key_value{7},
                   key_value{std::size_t(1)})),
               container_sealed_exception);
  ASSERT_EQ(c.resolve<notifier &>(std::string("primary")).channel(), 1);
  ASSERT_EQ(c.resolve<notifier &>(7).channel(), 1);
  ASSERT_EQ(c.resolve<notifier &>(std::size_t(1)).channel(), 1);
}

// Parent and child containers.

TEST(replace_registration_test, child_resolve_through_parent_seals_parent) {
  replacing_container parent;
  register_defaults(parent);
  replacing_container child(&parent);

  ASSERT_EQ(child.resolve<audit_log &>().sender.channel(), 1);

  EXPECT_THROW((parent.register_type<scope<shared>, storage<sms_notifier>,
                                     interfaces<notifier>>()),
               container_sealed_exception);
}

TEST(replace_registration_test, child_local_resolve_does_not_seal_parent) {
  replacing_container parent;
  register_defaults(parent);
  replacing_container child(&parent);
  child.register_type<scope<shared>, storage<sms_notifier>,
                      interfaces<notifier>>();
  ASSERT_EQ(child.resolve<notifier &>().channel(), 2);

  parent.register_type<scope<shared>, storage<push_notifier>,
                       interfaces<notifier>>();
  ASSERT_EQ(parent.resolve<notifier &>().channel(), 3);
}

TEST(replace_registration_test, sealed_parent_does_not_seal_child) {
  replacing_container parent;
  register_defaults(parent);
  ASSERT_EQ(parent.resolve<notifier &>().channel(), 1);

  replacing_container child(&parent);
  child.register_type<scope<shared>, storage<sms_notifier>,
                      interfaces<notifier>>();
  child.register_type<scope<shared>, storage<push_notifier>,
                      interfaces<notifier>>();
  ASSERT_EQ(child.resolve<notifier &>().channel(), 3);
}

TEST(replace_registration_test, child_overriding_parent_is_not_a_duplicate) {
  container<> parent;
  parent.register_type<scope<shared>, storage<email_notifier>,
                       interfaces<notifier>>();
  container<> child(&parent);
  child.register_type<scope<shared>, storage<sms_notifier>,
                      interfaces<notifier>>();
  ASSERT_EQ(child.resolve<notifier &>().channel(), 2);
  ASSERT_EQ(parent.resolve<notifier &>().channel(), 1);
}

} // namespace dingo
