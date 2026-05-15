#include <s3q/config.hpp>

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

// Scenario 3: Config provides a nested functor type `GetKey` for custom extraction.
struct TestCustomGetKeyType {
    struct Cfg : s3q::DefaultCfg {
        struct Item {
            int id;
        };
        struct GetKey {
            constexpr int operator()(const Item& i) const noexcept { return i.id; }
        };
    };
    using Ext = s3q::detail::ExtendedCfg<Cfg>;
    static_assert(std::is_same_v<Ext::Key, int>);
};
// Value check is placed after the struct since the custom functor is defined inside it.
static_assert(TestCustomGetKeyType::Ext::getKey(TestCustomGetKeyType::Cfg::Item{99}) == 99);

int main() { return 0; }
