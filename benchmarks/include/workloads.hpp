#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <string>

#include <tlx/die.hpp>

//! The queue item used by all workloads
template <typename K, typename V = std::uint32_t>
struct Item {
    K key;
    V value;

    Item() : Item<K, V>(0, 0){};
    Item(K key, V value) : key(key), value(value){};

    constexpr bool operator<(const Item<K, V> &b) const noexcept {
        return key < b.key;
    }
    constexpr bool operator>(const Item<K, V> &b) const noexcept {
        return key > b.key;
    }
};

using IntItem = Item<std::uint32_t>;
using FloatItem = Item<float>;

template <class ItemType>
struct ItemHelper {
    using key_type = decltype(ItemType::key);
    using key_limits = std::numeric_limits<key_type>;

    template <typename T>
    static auto make_item(T key) {
        return ItemType(static_cast<key_type>(key),
                        static_cast<decltype(ItemType::value)>(key));
    }
};

template <typename HeapType>
class BaseDriver {
protected:
    HeapType heap_;
    std::minstd_rand rand_engine_;

public:
    using heap_type = HeapType;

    BaseDriver() : rand_engine_(42) {}
    size_t size() const noexcept { return heap_.size(); }
    bool empty() const noexcept { return heap_.empty(); }
    void pop() { heap_.pop(); }
};

template <template <class> class HeapTemplate, class ItemType = IntItem>
class RandomDriver : public BaseDriver<HeapTemplate<ItemType>> {
    using item_helper = ItemHelper<ItemType>;
    using key_type = typename item_helper::key_type;
    using key_limits = typename item_helper::key_limits;

    using RNE = decltype(RandomDriver::rand_engine_);
    static_assert(key_limits::min() <= RNE::min());
    static_assert(key_limits::max() >= RNE::max());

public:
    static auto name() { return "random"; }

    void push() {
        auto key = key_type(this->rand_engine_());
        this->heap_.push(item_helper::make_item(key));
    }
};

template <template <class> class HeapTemplate, class ItemType = FloatItem>
class MonotoneDriver : public BaseDriver<HeapTemplate<ItemType>> {
    using item_helper = ItemHelper<ItemType>;
    using key_type = typename item_helper::key_type;
    key_type max_deleted_key{0};
    std::exponential_distribution<key_type> incr_dist_;

public:
    static auto name() { return "monotone"; }

    void push() {
        auto key = max_deleted_key + incr_dist_(this->rand_engine_);
        this->heap_.push(item_helper::make_item(key));
    }

    void pop() {
        max_deleted_key = this->heap_.top().key;
        this->heap_.pop();
    }
};

template <unsigned S, template <template <typename> class> class Driver>
struct Wiggle {
    static constexpr unsigned wiggle_count = S;

    template <template <typename> class HeapType>
    class type {
        using DriverType = Driver<HeapType>;

    public:
        using subject_type = typename DriverType::heap_type;

        static auto name() {
            return "heap_wiggle_" + std::to_string(wiggle_count) + "_" +
                   DriverType::name();
        }

        void run(size_t items) {
            DriverType heap;

            // Fill heap
            for (size_t i = 0; i < items; i++) {
                for (size_t j = 0; j < wiggle_count; j++) {
                    heap.push();
                    heap.pop();
                }
                heap.push();
            }

            die_unless(heap.size() == items);

            // Empty heap
            for (size_t i = 0; i < items; i++) {
                heap.pop();
                for (size_t j = 0; j < wiggle_count; j++) {
                    heap.push();
                    heap.pop();
                }
            }

            die_unless(heap.empty());
        }
    };
};
