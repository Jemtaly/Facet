# Facet

**Facet** is a header-only C++ library for exposing ordinary objects through declarative interfaces. It uses C++26 static reflection to generate interface metadata, function tables, inheritance conversions, and method dispatch at compile time.

An interface is a class containing member-function declarations. An implementation is any type that provides compatible operations. Facet binds the two without requiring the implementation to inherit from, or otherwise depend on, the interface. This separation lets interface authors and implementation authors evolve independently.

## Requirements

- A compiler with C++26 static-reflection support.
- A C++26-compliant standard library with static reflection support.
- The `-std=c++26` and `-freflection` compiler options.

Facet has no build-system or binary dependency. Add the [`include/`](./include/) directory to the compiler's include paths.

The headers rely on C++ compile-time reflection facilities including `<meta>`, reflection operators, splicing, and `template for`. Selecting an ordinary C++23 or C++26 language mode without an implementation of those facilities is insufficient.

The test runners default to `g++-16` on Unix-like platforms and `g++` on Windows. Set `CXX` when the compiler executable has another name or location:

```bash
CXX=/path/to/compiler ./scripts/run-tests
```

```powershell
$env:CXX = "C:\path\to\compiler.exe"
.\scripts\run-tests.ps1
```

## Headers

| Header | Purpose |
| --- | --- |
| [`<facet.hpp>`](./include/facet.hpp) | Core handles, interface protocol types, generated facade dispatch, conversions, implementation lookup, and the `local_id` and `uuid` interface-ID annotations |
| [`<facet/reflection.hpp>`](./include/facet/reflection.hpp) | Reflection-generated protocol and the `spec`, `version`, `since`, and `adapter` annotations |
| [`<facet/memory.hpp>`](./include/facet/memory.hpp) | Shared and unique ownership wrappers and factories; includes `<facet.hpp>` |

## Quick start

Include the core library and reflection support, declare an interface, and create a non-owning handle for an implementation object:

```cpp
#include <facet.hpp>
#include <facet/reflection.hpp>

#include <string>

struct [[= fct::spec]] IGreeter {
    std::string greet() const;
};

struct Greeter {
    std::string name;

    std::string greet() const {
        return "Hello, " + name;
    }
};

int main() {
    Greeter implementation{"Facet"};
    fct::facets<IGreeter> greeter = fct::as_facets<IGreeter>(&implementation);

    return greeter->greet() == "Hello, Facet" ? 0 : 1;
}
```

Compile it as follows:

```bash
g++-16 -std=c++26 -freflection -Iinclude example.cpp -o example
```

`fct::as_facets` borrows the supplied object. The caller must keep that object alive while any handle that refers to it is used.

## Capabilities at a glance

1. **Separated, reflection-generated architecture** — interface declarations are separate from implementation types. Static reflection turns the interface declarations into vtables, invokers, inheritance metadata, and dispatch code at compile time.
2. **Non-intrusive adaptation and composition** — `fct::as_facets` adapts an object without base classes, virtual members, registration, or implementation annotations. One object can expose multiple independent interfaces.
3. **Predictable method resolution** — public members of an implementation and its public bases participate in method resolution. Static interface-aware functions and ordinary member functions can coexist under explicit precedence rules.
4. **Interface inheritance and conversions** — interfaces may have transitive or multiple inheritance; handles convert through that hierarchy and can query other interfaces at runtime.
5. **Ownership-aware type erasure** — borrowed, shared, and unique wrappers preserve interface access while making lifetime semantics explicit.
6. **Extensible protocol** — applications can replace reflection-generated protocol definitions with manually supplied identifiers, vtables, inheritance metadata, method invokers, and implementation tables.
7. **Shared-library and versioning support** — UUID annotations provide stable interface identifiers across dynamic-library boundaries; version and method annotations support compatibility fallbacks for evolving APIs.

### Cost model

Facet does not allocate when creating a borrowed handle with `as_facets`; it stores an object pointer, type information, and vtable pointers. Reflection generation, interface metadata construction, and method-selection analysis occur at compile time.

An interface call is not generally a zero-overhead abstraction in the strict sense: it dispatches through a generated vtable function pointer. An inheritance upcast uses generated vtable links and does not perform a runtime interface lookup. A runtime cross-cast queries the object's interface metadata and compares interface identifiers, so its cost depends on the exposed interface closure. Shared and unique factories allocate their respective control block and object. These properties should be measured in the target build when call overhead is performance-critical.

## Guide

### 1. Declare a reflective interface

With `<facet/reflection.hpp>` included, Facet uses static reflection to turn an interface declaration into its protocol definition. Mark each reflective interface with `[[= fct::spec]]`; unmarked types are not given the generated protocol. An interface must be a class type whose non-special members are non-static, non-template, non-operator member functions. The declarations describe the callable surface; they do not need definitions.

```cpp
struct [[= fct::spec]] ICounter {
    int count() const;
    void reset();
};
```

The generated protocol contains a vtable, method invokers, and metadata for `ICounter`. No base class, virtual member function, registration step, or implementation annotation is required.

### 2. Adapt an implementation and call it

An implementation can satisfy an interface by defining ordinary compatible public member functions:

```cpp
struct Counter {
    int value = 0;

    int count() const {
        return value;
    }

    void reset() {
        value = 0;
    }
};

Counter counter{42};
fct::facets<ICounter> erased = fct::as_facets<ICounter>(&counter);

assert(erased);
assert(erased->count() == 42);
erased->reset();
```

`fct::as_facets<I...>(pointer)` returns a `typed_facets` that exposes the listed interfaces. `fct::facets<I...>` is its type-erased alias. A handle is nullable, supports `operator bool`, and is non-owning: use it for objects owned elsewhere, including stack objects, and do not access it after the original object is destroyed.

You can use `fct::implements<Implementation, Interface>` to verify the same compatibility at compile time:

```cpp
static_assert(fct::implements<Counter, ICounter>);
```

An implementation can expose more than one interface in a single handle:

```cpp
struct [[= fct::spec]] ICountable {
    int count() const;
};

struct [[= fct::spec]] IResettable {
    void reset();
};

auto interfaces = fct::as_facets<ICountable, IResettable>(&counter);
assert(interfaces->count() == 42);
interfaces->reset();
```

### 3. Control method resolution

For every interface method, Facet searches public functions with the same name on the implementation type and its public bases. Both ordinary non-static member functions and static member functions are valid implementations.

A static implementation receives an `typed_facets<Implementation, Interface>` as its first parameter. This makes the selected interface explicit and allows the implementation to access the original object through `self.get()`:

```cpp
struct [[= fct::spec]] ILeft {
    int value() const;
};

struct [[= fct::spec]] IRight {
    int value() const;
};

struct ConflictModel {
    static int value(fct::typed_facets<ConflictModel, ILeft>) {
        return 1;
    }

    static int value(fct::typed_facets<ConflictModel, IRight>) {
        return 2;
    }
};

ConflictModel model;
auto handle = fct::as_facets<ILeft, IRight>(&model);
assert(handle.as<ILeft>()->value() == 1);
assert(handle.as<IRight>()->value() == 2);
```

Regular overloads are also supported:

```cpp
struct [[= fct::spec]] ICalculator {
    int calculate() const;
    int calculate(int value) const;
};

struct Calculator {
    int calculate() const {
        return 3;
    }

    int calculate(int value) const {
        return value * 4;
    }
};

Calculator implementation;
auto calculator = fct::as_facets<ICalculator>(&implementation);
assert(calculator->calculate() == 3);
assert(calculator->calculate(5) == 20);
```

Resolution is deterministic. Facet considers candidates in this order:

1. A static function whose first parameter is exactly `fct::typed_facets<Implementation, Interface>` and whose remaining parameter types exactly match the interface method.
2. A non-static member function whose parameter types exactly match the interface method.
3. A static function callable with the interface arguments after the interface handle.
4. A non-static member function callable with the interface arguments.

Each candidate's result must be convertible to the interface method's result. Consequently, an exact static function overrides an exact member function, while an exact member function overrides a merely compatible static function. Use a static function to deliberately specialize behavior by requested interface; use ordinary members for the usual object-oriented implementation.

If Facet finds no suitable candidate for an interface method, instantiating that implementation's function table fails at compile time.

To bind a method differently from the automatic name-and-signature lookup, annotate that interface method with `fct::adapter`. The adapter is called with a `typed_facets<Implementation, Interface>` followed by the interface method's arguments, and its result must be convertible to the interface method's return type. An annotated adapter takes precedence over automatic lookup:

```cpp
struct [[= fct::spec]] IShippingQuote {
    [[= fct::adapter([](auto self, double weight_grams) -> double {
        if constexpr (requires { self.get()->quote_cents(weight_grams); }) {
            return self.get()->quote_cents_per_gram(weight_grams);
        } else {
            return self.get()->quote_dollars_per_kilogram(weight_grams / 1000) * 100;
        }
    })]]
    double quote_cents_per_gram(double weight_grams) const;
};

struct CentRateService {
    double quote_cents_per_gram(double weight_grams) const {
        return weight_grams / 10;
    }
};

struct DollarRateService {
    double quote_dollars_per_kilogram(double weight_grams) const {
        return weight_grams * 1.25;
    }
};

CentRateService cent_rate_service;
auto cent_quote = fct::as_facets<IShippingQuote>(&cent_rate_service);
assert(cent_quote->quote_cents_per_gram(2000) == 200);

DollarRateService dollar_rate_service;
auto dollar_quote = fct::as_facets<IShippingQuote>(&dollar_rate_service);
assert(dollar_quote->quote_cents_per_gram(2000) == 250);
```

The lambda can also accept the interface method's arguments and use the object handle to access the implementation, making it possible to rename or transform a call without adding a matching implementation member.

### 4. Model interface inheritance and conversions

Interface inheritance is declarative and independent of the implementation type. Facet generates the base-vtable links from the interface declaration, so derived handles convert to their base interfaces.

```cpp
struct [[= fct::spec]] IValue {
    int value() const;
};

struct [[= fct::spec]] IAdjustable : IValue {
    void add(int amount);
};

struct Adjustable {
    int current = 0;

    int value() const {
        return current;
    }

    void add(int amount) {
        current += amount;
    }
};

Adjustable object{5};
fct::facets<IAdjustable> adjustable = fct::as_facets<IAdjustable>(&object);
fct::facets<IValue> value = adjustable;

assert(value->value() == 5);
```

This applies to transitive and multiple interface inheritance. An inheritance upcast uses the generated interface hierarchy and does not perform a runtime metadata query. A conversion is implicit when the relationship makes it unambiguous; otherwise construct the desired handle explicitly.

### 5. Query optional interfaces at runtime

A handle preserves runtime type information for the interfaces it exposes. To make a reflective interface discoverable by a runtime query, give it an interface ID. Use `[[= fct::local_id]]` when every query is within the same translation unit; use `[[= fct::uuid("...")]]` when the interface is shared between translation units or dynamic libraries. Constructing another `facets` from a handle then performs a runtime interface query: the result is non-null when the object supports the requested interface and null otherwise.

```cpp
struct [[= fct::spec]]
       [[= fct::local_id]] IRoot {
    int root() const;
};

struct [[= fct::spec]]
       [[= fct::local_id]] IExtra {
    int extra() const;
};

struct [[= fct::spec]]
       [[= fct::local_id]] IAbsent {
    int absent() const;
};

struct Model {
    int root() const { return 10; }
    int extra() const { return 20; }
};

Model model;
fct::facets<IRoot> root = fct::as_facets<IRoot, IExtra>(&model);

fct::facets<IExtra> extra{root};
fct::facets<IAbsent> absent{root};

assert(extra && extra->extra() == 20);
assert(!absent);
```

Use `fct::opaque` when only the type-erased handle is needed before choosing an interface:

```cpp
fct::opaque data = fct::as_facets<IExtra>(&model);
fct::facets<IExtra> extra{data};
```

Runtime queries are explicit: direct interface-base conversions use normal handle conversion, while constructing a new `facets<Target>` from another handle performs the query. The resulting handle is null when `Target` is not among the interfaces exposed by the object or their base interfaces.

### 6. Manage lifetimes with type-erased ownership

Include `<facet/memory.hpp>` for owning wrappers. The library provides `make_shared` and `make_unique`, plus aliases that erase the implementation type while retaining the exposed interfaces:

| Factory or wrapper | Ownership |
| --- | --- |
| `make_shared<Impl, I...>(args...)` | Reference-counted ownership |
| `shared_facets<I...>` | Shared, type-erased ownership |
| `weak_facets<I...>` | Non-owning view of a shared object; does not extend its lifetime |
| `make_unique<Impl, I...>(args...)` | Unique ownership |
| `unique_facets<I...>` | Unique, type-erased ownership |
| `weak_opaque`, `shared_opaque`, `unique_opaque` | The respective type-erased wrappers with no exposed interface methods |

```cpp
#include <facet/memory.hpp>

struct [[= fct::spec]] ICounter {
    int count() const;
};

struct Counter {
    int count() const {
        return 42;
    }
};

auto shared = fct::make_shared<Counter, ICounter>();
fct::weak_facets<ICounter> view = shared;
fct::shared_facets<ICounter> retained = view;

assert(retained->count() == 42);
```

`shared_facets` is copyable. `unique_facets` is move-only:

```cpp
auto unique = fct::make_unique<Counter, ICounter>();
fct::unique_facets<ICounter> owner = std::move(unique);
assert(owner->count() == 42);
```

`weak_facets` is a borrowing view, not an expiring `std::weak_ptr`-like observer. It does not keep the shared control block or implementation alive. Keep a `shared_typed_facets` or `shared_facets` owner alive while accessing a weak view or converting it back to a shared holder; otherwise the view dangles.

#### Return shared ownership from an implementation

Derive a shared implementation from `fct::enable_shared_from_this<Impl>` when one of its operations must return a shared reference to itself. Construct that implementation with `make_shared`; the factory initializes the control-block link required by `retain_shared` and `retain_weak`. This is commonly expressed as a static interface-aware implementation function: it receives the typed object handle and calls `fct::retain_shared`.

```cpp
struct [[= fct::spec]] ISelf {
    fct::shared_facets<ISelf> self() const;
};

struct SharedObject : fct::enable_shared_from_this<SharedObject> {
    static fct::shared_facets<ISelf> self(fct::typed_facets<SharedObject, ISelf> self) {
        return fct::retain_shared(self);
    }
};

auto object = fct::make_shared<SharedObject, ISelf>();
fct::shared_facets<ISelf> retained = object->self();
```

Both `retain_shared` and `retain_weak` require an `typed_facets<Impl, I...>` for an object created by `make_shared` and an implementation derived from `enable_shared_from_this<Impl>`. `retain_shared` increments the shared control block's reference count. `retain_weak` returns a `weak_typed_facets`; converting that view to `shared_facets` also increments the reference count, provided a shared owner is still alive. Do not use either function for borrowed objects or objects owned by `make_unique`.

### 7. Cross a dynamic-library boundary

For runtime interface lookup across dynamic libraries, annotate every shared interface with a non-empty UUID. The UUID becomes the stable identifier used by cross-casts rather than relying on process-local type identity. `local_id` is valid only in its declaring translation unit. An interface without `local_id`, `uuid`, or a manually supplied `interface_id` can be used directly and through declared base conversions, but cannot be the target of a runtime cross-cast.

```cpp
#include <facet.hpp>
#include <facet/reflection.hpp>

struct [[= fct::spec]]
       [[= fct::uuid("example.service.v1")]] IService {
    int value() const;
};
```

The provider creates the handle and exports it through its C ABI:

```cpp
extern "C" void make_service(fct::facets<IService>* out) {
    static Service service;
    *out = fct::as_facets<IService>(&service);
}
```

The consumer declares the same UUID-annotated interface and receives the handle. See the complete shared-library build and usage pattern in [`examples/abi/`](./examples/abi/).

### 8. Evolve an interface compatibly

Use `fct::version` to mark the interface version embedded in its generated function table. Mark a later-added method with `fct::since`; its fallback is called when a client invokes that method through an older function table.

```cpp
#ifndef EXAMPLE_API_LEVEL
#define EXAMPLE_API_LEVEL 2
#endif

struct [[= fct::spec]]
       [[= fct::uuid("example.versioned-service")]]
       [[= fct::version(EXAMPLE_API_LEVEL)]] IService {
    int value() const;

#if EXAMPLE_API_LEVEL >= 2
    [[= fct::since(2, [](auto self, int increment) {
        return self->value() + increment;
    })]]
    int value_after_upgrade(int increment) const;
#endif
};
```

Build an older provider with `EXAMPLE_API_LEVEL` set to `1`, while compiling the consumer with the default level `2`. A version-2 consumer can call `value_after_upgrade`; for a version-1 provider, Facet calls the fallback instead. The provider and consumer may therefore compile the same interface header with different API levels. The full producer/consumer demonstration is in [`examples/evolution/`](./examples/evolution/).

### 9. Define the protocol manually

Reflection is the preferred path for ordinary C++ interface declarations. When that path is unavailable or deliberately avoided, Facet's public non-reflective protocol can describe the same interface explicitly. Provide the following specializations in the `fct` namespace:

1. Optionally, `interface_id<I>` defines `static constexpr interface_id_t value`. Its presence enables runtime cross-casts to `I`; use a non-empty stable value whenever the interface must be found across translation units or dynamic-library boundaries.
2. `vtable<I>` defines `type`, the function-table type for `I`. Its layout is application-defined.
3. `direct_bases<I>` exposes `value`, a `std::array` or span of `direct_base_t` entries. Each `direct_base<Base>(converter)` records a direct base and a converter from `vtable_t<I> const*` to `vtable_t<Base> const*`.
4. `direct_methods<I>` exposes `value`, a `std::array` or span of `direct_method_t` entries. Each `direct_method(name, invoker)` maps a method name to an invoker callable with `facets<I>` followed by that method's arguments.
5. `vtable_implements<Impl, I>` defines `static constexpr vtable_t<I> const* value`, the table used when `Impl` is adapted as `I`.

The protocol is what powers the reflective path as well: base metadata determines hierarchy conversions, method metadata builds the call facade, interface identifiers enable runtime lookup, and implementation tables select the actual dispatch targets. All table pointers, base converters, and invokers must remain valid for the lifetime of every handle that refers to them. Prefer static-storage tables and keep the method names and invoker signatures consistent with the interface facade.

For a manual interface template, specialize `interface_id` for the template. A controlled build can generate a distinct ID for each specialization from the reflected type:

```cpp
template<typename T>
struct IManualValue {};

namespace fct {
template<typename T>
struct interface_id<IManualValue<T>> {
    static constexpr interface_id_t value =
        static_string::make(std::meta::display_string_of(^^IManualValue<T>)).data;
};
}
```

`std::meta::display_string_of` distinguishes template specializations, so a runtime query can find `IManualValue<int>` while rejecting `IManualValue<double>` for the same object. Its spelling is compiler-defined; use an explicit, stable ID instead when interfaces cross a dynamic-library, compiler, or independently versioned build boundary. Provide the corresponding partial specializations of `vtable`, `direct_bases`, `direct_methods`, and `vtable_implements`.

An `typed_facets` stores its vtable pointers in `fct::tuple`, which is an ABI-stable version of `std::tuple`; use `fct::get<I>(handle.vtables)` or `fct::apply` to access them rather than `std::get` or `std::apply`.

## Appendix: examples and verification

The [`examples/`](./examples/) directory contains self-contained programs:

| Example | Demonstrates |
| --- | --- |
| [`basic`](./examples/basic/main.cpp) | Basic interfaces, interface templates, and hierarchy conversions |
| [`dynamic_cast`](./examples/dynamic_cast/main.cpp) | Runtime interface queries from interface and data handles |
| [`dispatch`](./examples/dispatch/main.cpp) | Overloads, dispatch priority, interface-specific static dispatch, and adapters |
| [`manual`](./examples/manual/main.cpp) | Explicit vtable and metadata specializations, including template interface IDs |
| [`memory`](./examples/memory/main.cpp) | Shared, unique, and borrowed wrappers |
| [`abi`](./examples/abi/main.cpp) | Shared-library interfaces identified by UUID |
| [`evolution`](./examples/evolution/main.cpp) | Versioned interfaces with method fallbacks |

Build and run every example from the repository root:

```bash
./scripts/run-tests
```

```powershell
.\scripts\run-tests.ps1
```

The scripts compile all standalone and shared-library examples in a temporary directory and print `All tests passed.` when every executable succeeds.
