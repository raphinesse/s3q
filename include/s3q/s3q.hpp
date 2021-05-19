#pragma once

#include "batched_pq.hpp"
#include "config.hpp"
#include "pq.hpp"

namespace s3q {

template <class Cfg = DefaultCfg>
using PriorityQueue = detail::PriorityQueue<detail::ExtendedCfg<Cfg>>;

template <class Cfg = DefaultCfg>
using BatchedPriorityQueue =
    detail::BatchedPriorityQueue<detail::ExtendedCfg<Cfg>>;

} // namespace s3q
