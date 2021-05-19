#pragma once

#include <vector>

namespace s3q::detail {

template <class Cfg>
struct Bucket {
    using Key = typename Cfg::Key;
    using Item = typename Cfg::Item;
    using KeyRange = typename Cfg::KeyRange;
    using Buffer = std::vector<Item>;

    Key sup = KeyRange::sup();
    Buffer buf;

    Bucket() {}
    Bucket(Key sup) : sup(sup) {}

    // Functor to get the supremum of a bucket
    static constexpr struct GetSup {
        constexpr auto operator()(const Bucket &b) const noexcept {
            return b.sup;
        }
    } getSup{};
};

} // namespace s3q::detail
