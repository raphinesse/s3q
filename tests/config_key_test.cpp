#include <s3q/config.hpp>

#include <cstdint>
#include <type_traits>

// Scenario 1: Item is an arithmetic type — the item itself is the key.
struct TestArithmeticItem {
    struct Cfg : s3q::DefaultCfg {
        using Item = float;
    };
    using Ext = s3q::detail::ExtendedCfg<Cfg>;
    static_assert(std::is_same_v<Ext::Key, float>);
    static_assert(Ext::getKey(3.14f) == 3.14f);
};

// Scenario 2: Item is a struct with a member named `key` — extracted by default.
struct TestMemberKeyDefault {
    struct Cfg : s3q::DefaultCfg {
        struct Item {
            int key;
            double value;
        };
    };
    using Ext = s3q::detail::ExtendedCfg<Cfg>;
    static_assert(std::is_same_v<Ext::Key, int>);
    static_assert(Ext::getKey(Cfg::Item{42, 0.0}) == 42);
};

// Custom GetKey functor for TestCustomGetKeyType, defined at file scope so it
// can be called in static_assert inside the (still incomplete) test struct.
struct GetKeyLower32 {
    constexpr std::int32_t operator()(std::int64_t i) const noexcept {
        return static_cast<std::int32_t>(i);
    }
};

// Scenario 3: Config provides a nested `GetKey` type that takes precedence over
// the defaults. Item is int64_t (arithmetic, which would be its own key by
// default), but the custom GetKey truncates it to int32_t (lower half), showing
// that the custom functor overrides the default behaviour.
struct TestCustomGetKeyType {
    struct Cfg : s3q::DefaultCfg {
        using Item = std::int64_t;
        using GetKey = GetKeyLower32;
    };
    using Ext = s3q::detail::ExtendedCfg<Cfg>;
    static_assert(std::is_same_v<Ext::Key, std::int32_t>);
    static_assert(Ext::getKey(std::int64_t{(1LL << 32) | 99}) == 99);
};

int main() { return 0; }
