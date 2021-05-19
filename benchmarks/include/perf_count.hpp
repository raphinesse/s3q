#pragma once

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <syscall.h>
#include <unistd.h>

struct PerfEvent {
    const uint32_t type;
    const uint64_t config;
    const std::string name;
};

/*
 * Specify an event to be monitored, `type` and `config` are defined in the
 * documentation of the `perf_event_open` system call.
 */
#define PERF_EVENT(type, config) PERF_EVENT_NAMED(type, config, config)

#define PERF_EVENT_NAMED(type, config, name)                                   \
    PerfEvent { type, config, #name }

/*
 * Same as `PERF_EVENT` but for hardware events; prefix `PERF_COUNT_HW_` must be
 * omitted from `config`.
 */
#define PERF_EVENT_HW(config)                                                  \
    PERF_EVENT(PERF_TYPE_HARDWARE, PERF_COUNT_HW_##config)

/*
 * Same as `PERF_EVENT` but for software events; prefix `PERF_COUNT_SW_` must be
 * omitted from `config`.
 */
#define PERF_EVENT_SW(config)                                                  \
    PERF_EVENT(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_##config)

/*
 * Same as `PERF_EVENT` but for cache events; prefixes `PERF_COUNT_HW_CACHE_`,
 * `PERF_COUNT_HW_CACHE_OP_` and `PERF_COUNT_HW_CACHE_RESULT_` must be omitted
 * from `cache`, `op` and `result`, respectively. Again `cache`, `op` and
 * `result` are defined in the documentation of the `perf_event_open` system
 * call.
 */
#define PERF_EVENT_CACHE(cache, op, result)                                    \
    PERF_EVENT_NAMED(PERF_TYPE_HW_CACHE,                                       \
                     (PERF_COUNT_HW_CACHE_##cache) |                           \
                         (PERF_COUNT_HW_CACHE_OP_##op << 8) |                  \
                         (PERF_COUNT_HW_CACHE_RESULT_##result << 16),          \
                     PERF_COUNT_HW_CACHE_##cache##_##op##_##result)

class PerfGroup {
private:
    int group_leader_fd_{-1};
    std::vector<PerfEvent> events_;
    std::vector<uint64_t> results_buf_;

    int perf_event_open(const PerfEvent &event) const {
        struct perf_event_attr pe {};

        pe.size = sizeof(struct perf_event_attr);
        pe.read_format = PERF_FORMAT_GROUP;
        pe.disabled = 1;

        pe.type = event.type;
        pe.config = event.config;

        // count user events only
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;

        // pid == 0 and cpu == -1: measures the calling process on any CPU.
        auto result = perf_event_open(&pe, 0, -1, group_leader_fd_);
        if (result == -1) {
            if (errno == ENOENT) {
                // Ignore missing events
                // TODO log a warning here
            } else {
                throw std::system_error(errno, std::generic_category(),
                                        "PerfGroup::perf_event_open");
            }
        }
        return result;
    }

    static int perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu,
                               int group_fd, unsigned long flags = 0) {
        return static_cast<int>(
            syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags));
    }

    void group_ioctl(unsigned long int request) const {
        if (group_leader_fd_ == -1) return;
        check_error("PerfGroup::group_ioctl",
                    ioctl(group_leader_fd_, request, PERF_IOC_FLAG_GROUP));
    }

    template <typename T>
    static T check_error(const std::string &what, T result) {
        if (result == -1) {
            throw std::system_error(errno, std::generic_category(), what);
        }
        return result;
    }

public:
    PerfGroup() = delete;
    PerfGroup(PerfGroup const &) = delete;

    PerfGroup(std::initializer_list<PerfEvent> events) : events_(events) {
        for (auto &event : events) {
            int fd = perf_event_open(event);

            if (group_leader_fd_ == -1) {
                group_leader_fd_ = fd;
            }
        }

        results_buf_.resize(size() + 1);
    }

    PerfGroup(PerfGroup &&other) noexcept {
        group_leader_fd_ = other.group_leader_fd_;
        other.group_leader_fd_ = -1;

        results_buf_ = std::move(other.results_buf_);
        events_ = std::move(other.events_);
    }

    ~PerfGroup() {
        if (group_leader_fd_ != -1) close(group_leader_fd_);
    }

    size_t size() const { return events_.size(); }

    const PerfEvent &event(unsigned idx) const {
        assert(idx < size());
        return events_[idx];
    }

    uint64_t result(unsigned idx) const {
        assert(idx < size());
        return results_buf_[idx + 1];
    }

    void reset() const { group_ioctl(PERF_EVENT_IOC_RESET); }
    void enable() const { group_ioctl(PERF_EVENT_IOC_ENABLE); }
    void disable() const { group_ioctl(PERF_EVENT_IOC_DISABLE); }

    void read_results() {
        if (group_leader_fd_ == -1) return;
        const auto to_read = sizeof(uint64_t) * (size() + 1);
        const auto read_bytes =
            ::read(group_leader_fd_, results_buf_.data(), to_read);
        check_error("PerfGroup::read_results", read_bytes);
        if (static_cast<size_t>(read_bytes) != to_read) {
            throw std::runtime_error(
                "PerfGroup::read_results: unexpected number of bytes read");
        }
    }
};

class PerfCount {
private:
    std::vector<PerfGroup> groups_;

    using init_t = std::initializer_list<std::initializer_list<PerfEvent>>;
    using results_t = std::vector<std::pair<std::string, uint64_t>>;

public:
    PerfCount(init_t event_lists) {
        for (auto &&event_list : event_lists) {
            groups_.emplace_back(event_list);
        }
    }

    void reset() const { for (auto &&g : groups_) g.reset(); }
    void enable() const { for (auto &&g : groups_) g.enable(); }
    void disable() const { for (auto &&g : groups_) g.disable(); }

    std::vector<std::pair<std::string, uint64_t>> get_results() {
        results_t results;
        for (auto &&group : groups_) {
            group.read_results();
            for (unsigned i = 0; i < group.size(); ++i) {
                results.push_back(
                    std::make_pair(group.event(i).name, group.result(i)));
            }
        }
        return results;
    }
};
