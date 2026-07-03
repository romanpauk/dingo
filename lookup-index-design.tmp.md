# Lookup Index Design Note

Temporary tracker for the runtime lookup-index refactor.

## Goal

`lookup_definition_type` should define the runtime indexes available in the
registry. Compile-time lookup shape selects an index object; runtime values only
search inside that selected object.

The design must not carry the current slot identity key into the new index. If
the selected index is already for `IProcessor`, the runtime key is only the user
key, such as `std::size_t`. If the selected index is the interface index, the
runtime key is only the interface/request type id.

## Current Pieces To Reuse

The ordered, unordered, and array lookup backends already expose a small
STL-like API.

For `one`:

```c++
find(key)
end()
try_emplace(key, mapped)
erase(iterator)
```

For `many`:

```c++
find(key)
end()
equal_range(key)
emplace(key, mapped)
erase(iterator)
```

The new lookup-index path should reuse these backend storages directly for
runtime-key lookups.

## What Not To Reuse

Do not build the new `lookup_index` on top of current `slot_storage`.

`slot_storage` currently solves an older problem: a generic runtime slot path
where each row carries a full `slot_key` and insertion checks whether the row
matches the slot. That means rows carry:

- interface type id
- key type id
- domain
- cardinality

That is exactly the oversized key shape this refactor is meant to remove.

In the new model, compile time selects the index, so rows do not need to prove
which slot they belong to.

## New Layers

Use two new concepts:

```c++
lookup_backend<Entry, Value, Allocator>
lookup_index<EntryList, Value, Allocator>
```

`lookup_backend` is the selected backend storage type with the existing
STL-like API. It should stay honest to the backend API instead of introducing a
second semantic API.

Do not add generic `insert`, `find_singular`, `for_each`, custom insertion
results, or custom handles to `lookup_backend`. Registry code can use the
backend API directly:

- `one`: `try_emplace`, `find`, `end`, `erase`
- `many`: `emplace`, `find`, `equal_range`, `end`, `erase`

The mapped type should be named `Value`, not `Row`. Production can instantiate
`Value` as `registered_binding_entry *`, but the lookup layer should remain
generic and not know that.

`lookup_index` is only a compile-time aggregate/router:

```c++
template <typename Entry>
auto &get();
```

## Backend Selection

Use `lookup_entry` directly to select storage. No separate query type is needed.

### Runtime-Key Entry

For:

```c++
lookup_entry<Interface, runtime_key<Key>, Cardinality, Backend>
```

the minimal storage is:

```c++
typename Backend::template storage<Key, Value, Cardinality, Allocator>
```

The runtime key is only `Key`. It must not include interface/type-domain data,
because `Interface`, `Cardinality`, and `Backend` are already selected by the
`lookup_entry` type.

### No-Key Entry

For:

```c++
lookup_entry<Interface, no_key, Cardinality, Backend>
```

add a small storage that models the same STL-like backend API. It can use
`none_t` as the key.

For `one`, it stores zero or one `Value`.

For `many`, it stores zero or more `Value`.

### Typed-Key Entry

For:

```c++
lookup_entry<Interface, typed_key<Key>, Cardinality, Backend>
```

use the same storage shape as no-key. The typed key is compile-time information
and selects the entry type. It is not a runtime map key.

## Static-Key Backend API

The no-key and typed-key storage should model the same STL-like API as the
existing ordered/unordered/array backends:

- `one`: `try_emplace`, `find`, `end`, `erase`
- `many`: `emplace`, `equal_range`, `find`, `end`, `erase`

For no-key and typed-key entries, the key can be `none_t{}` and is ignored by
the storage. Typed key information is compile-time information in `Entry`; it is
not a runtime map key.

No additional adapter should wrap these operations unless registry integration
later proves a small helper is useful. If such a helper is needed, it should be
local registry glue, not part of `lookup_backend`.

## lookup_index Shape

`lookup_index` owns one `lookup_backend` member per entry:

```c++
template <typename Entry, typename Value, typename Allocator>
struct lookup_index_member {
  lookup_backend<Entry, Value, Allocator> backend;
};

template <typename... Entries, typename Value, typename Allocator>
class lookup_index<type_list<Entries...>, Value, Allocator>
    : lookup_index_member<Entries, Value, Allocator>... {
public:
  explicit lookup_index(Allocator &allocator)
      : lookup_index_member<Entries, Value, Allocator>(allocator)... {}

  template <typename Entry>
  auto &get();
};
```

No `ensure` is needed for entries in the compile-time shape. The member exists.
An empty index is represented by an empty backend.

## Runtime Lookup Modes

The runtime registry only needs two practical lookup modes.

### Interface Lookup

Resolve implementation for a known interface/request without a runtime key.

The selected index can be keyed by the interface/request type id when the index
groups interface lookups, or it can be entry-specific if the compile-time shape
chooses that. In either case, the runtime key is minimal: only the type id if a
runtime key is needed at all.

Typed-key lookup is compile-time-qualified interface lookup. The typed key
selects the entry; it is not part of the runtime key.

### Runtime-Key Lookup For Known Interface

Resolve implementation for a known interface and runtime key:

```c++
container.resolve<IProcessor &>(std::size_t{7});
```

The selected entry is:

```c++
lookup_entry<IProcessor, runtime_key<std::size_t>, one, Backend>
```

The backend is searched with:

```c++
std::size_t{7}
```

No composite `(interface, key)` key is needed.

## Migration Tracker

### 1. Correct The Initial lookup_index Prototype

The current prototype in `include/dingo/runtime/lookup_index.h` uses
`slot_storage` in older drafts and may still contain a second semantic wrapper
API. Replace that direction with:

- `lookup_backend<Entry, Value, Allocator>`
- direct reuse of `Backend::storage<Key, Value, Cardinality, Allocator>` for
  runtime-key entries
- new static-key storage with the same STL-like API for no-key/typed-key
  entries

Tests must use a plain mapped value such as `row *` or `int *`, with no slot
metadata. They should not construct `slot_key` or call `make_slot_key`.

Status: TODO

### 2. Add Focused Backend Tests

Add tests proving:

- runtime-key `lookup_backend` uses `try_emplace/find/end/erase` for `one`
- runtime-key `lookup_backend` uses `emplace/equal_range/find/end/erase` for
  `many`
- no-key `lookup_backend` uses the same backend API with `none_t{}`
- typed-key `lookup_backend` uses the same backend API with `none_t{}`
- `lookup_index.get<Entry>()` returns the new `lookup_backend`, not
  `slot_storage`

Status: TODO

### 3. Add lookup_index To runtime_bindings_state

Once the standalone backend tests are correct, add the index to
`runtime_bindings_state` without rewiring behavior.

Status: TODO

### 4. Migrate Declared Runtime-Key Slots First

Route explicit `associative<K, I, Cardinality, Backend>` registration and
resolution through `lookup_index.get<entry>()`.

Keep old no-key and typed-key paths temporarily.

Status: TODO

### 5. Split Row Ownership From Old Slot Storage

Move row ownership out of `runtime_type_bindings` so lookup backends only store
`registered_binding_entry *` as their `Value`.

Status: TODO

### 6. Migrate No-Key And Typed-Key

Route no-key and typed-key paths through static-key `lookup_backend` entries.

Status: TODO

### 7. Remove Old Slot Routing

After all paths use `lookup_index`, remove or shrink:

- `runtime_bindings_state::type_bindings`
- `runtime_type_bindings` as slot owner
- `slot_storage` if no longer needed
- `slot_key` plumbing if no longer needed

Status: TODO

### 8. Verification

For C++ header changes:

```bash
cmake --build build -t format
cmake --build build -t check-format
cmake --build build -t check
```

Status: TODO
