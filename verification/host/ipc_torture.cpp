#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "microkernel/core/ipc_manager.h"

namespace {

using jixia::microkernel::ipc::EndpointManager;
using jixia::microkernel::ipc::kErrorAgain;
using jixia::microkernel::ipc::kErrorInvalidArgument;
using jixia::microkernel::ipc::Message;
using jixia::microkernel::task::TaskId;

constexpr TaskId kOwner = 1U;
constexpr unsigned kProducerCount = 4U;
constexpr unsigned kConsumerCount = 4U;

uint64_t xorshift64(uint64_t& state) {
    state ^= state << 13U;
    state ^= state >> 7U;
    state ^= state << 17U;
    return state;
}

void perturb(uint64_t& state) {
    const uint64_t value = xorshift64(state);
    if ((value & 0x1FU) == 0U) {
        std::this_thread::yield();
        return;
    }

    const unsigned spins = static_cast<unsigned>(value & 0x3FU);
    for (unsigned index = 0U; index < spins; ++index) {
        std::atomic_signal_fence(std::memory_order_seq_cst);
    }
}

bool deadline_expired(const std::chrono::steady_clock::time_point deadline) {
    return std::chrono::steady_clock::now() >= deadline;
}

bool create_endpoint(uint64_t& handle) {
    handle = 0U;
    const intptr_t result = EndpointManager::instance().create_endpoint(kOwner, &handle);
    if (result != 0 || handle == 0U) {
        std::cerr << "create_endpoint failed: " << result << '\n';
        return false;
    }
    return true;
}

bool destroy_endpoint(uint64_t handle) {
    const intptr_t result = EndpointManager::instance().destroy_endpoint(kOwner, handle);
    if (result != 0) {
        std::cerr << "destroy_endpoint failed: " << result << '\n';
        return false;
    }
    return true;
}

bool run_mpsc_fifo(uint64_t seed, unsigned messages_per_producer) {
    uint64_t handle = 0U;
    if (!create_endpoint(handle)) {
        return false;
    }

    const uint64_t total = static_cast<uint64_t>(kProducerCount) * messages_per_producer;
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::atomic<unsigned> producers_done{0U};
    std::vector<std::thread> producers;

    for (unsigned producer = 0U; producer < kProducerCount; ++producer) {
        producers.emplace_back([&, producer] {
            uint64_t random = seed ^ (0x9E3779B97F4A7C15ULL * (producer + 1U));
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (unsigned sequence = 0U; sequence < messages_per_producer && !failed.load();
                 ++sequence) {
                const uint64_t checksum =
                    seed ^ (static_cast<uint64_t>(producer) << 32U) ^ sequence;
                const uint64_t words[4] = {producer, sequence, seed, checksum};
                for (;;) {
                    const intptr_t result = EndpointManager::instance().send(
                        static_cast<TaskId>(100U + producer), handle, words);
                    if (result == 0) {
                        break;
                    }
                    if (result != kErrorAgain) {
                        failed.store(true, std::memory_order_release);
                        return;
                    }
                    perturb(random);
                }
                perturb(random);
            }
            producers_done.fetch_add(1U, std::memory_order_release);
        });
    }

    std::vector<unsigned> expected(kProducerCount, 0U);
    uint64_t received = 0U;
    uint64_t random = seed ^ 0xD1B54A32D192ED03ULL;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    start.store(true, std::memory_order_release);

    while (received < total && !failed.load(std::memory_order_acquire)) {
        Message message{};
        const intptr_t result = EndpointManager::instance().try_recv(handle, &message);
        if (result == kErrorAgain) {
            if (deadline_expired(deadline)) {
                failed.store(true, std::memory_order_release);
                break;
            }
            perturb(random);
            continue;
        }
        if (result != 0 || message.words[0] >= kProducerCount) {
            failed.store(true, std::memory_order_release);
            break;
        }

        const unsigned producer = static_cast<unsigned>(message.words[0]);
        const unsigned sequence = static_cast<unsigned>(message.words[1]);
        const uint64_t checksum = seed ^ (static_cast<uint64_t>(producer) << 32U) ^ sequence;
        if (message.sender != static_cast<TaskId>(100U + producer) ||
            sequence != expected[producer] || message.words[2] != seed ||
            message.words[3] != checksum) {
            failed.store(true, std::memory_order_release);
            break;
        }

        ++expected[producer];
        ++received;
        perturb(random);
    }

    for (std::thread& producer : producers) {
        producer.join();
    }

    const bool complete = producers_done.load(std::memory_order_acquire) == kProducerCount &&
                          received == total && !failed.load(std::memory_order_acquire);
    return destroy_endpoint(handle) && complete;
}

bool run_mpmc_exactly_once(uint64_t seed, unsigned messages_per_producer) {
    uint64_t handle = 0U;
    if (!create_endpoint(handle)) {
        return false;
    }

    const uint64_t total = static_cast<uint64_t>(kProducerCount) * messages_per_producer;
    auto seen = std::make_unique<std::atomic<uint32_t>[]>(total);
    for (uint64_t index = 0U; index < total; ++index) {
        seen[index].store(0U, std::memory_order_relaxed);
    }

    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::atomic<uint64_t> consumed{0U};
    std::vector<std::thread> threads;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);

    for (unsigned producer = 0U; producer < kProducerCount; ++producer) {
        threads.emplace_back([&, producer] {
            uint64_t random = seed ^ (0xA0761D6478BD642FULL * (producer + 1U));
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (unsigned sequence = 0U; sequence < messages_per_producer && !failed.load();
                 ++sequence) {
                const uint64_t id =
                    static_cast<uint64_t>(producer) * messages_per_producer + sequence;
                const uint64_t words[4] = {id, ~id, seed, id ^ seed ^ 0xC0DEC0DEULL};
                for (;;) {
                    const intptr_t result = EndpointManager::instance().send(
                        static_cast<TaskId>(200U + producer), handle, words);
                    if (result == 0) {
                        break;
                    }
                    if (result != kErrorAgain || deadline_expired(deadline)) {
                        failed.store(true, std::memory_order_release);
                        return;
                    }
                    perturb(random);
                }
                perturb(random);
            }
        });
    }

    for (unsigned consumer = 0U; consumer < kConsumerCount; ++consumer) {
        threads.emplace_back([&, consumer] {
            uint64_t random = seed ^ (0xE7037ED1A0B428DBULL * (consumer + 1U));
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            while (consumed.load(std::memory_order_acquire) < total && !failed.load()) {
                Message message{};
                const intptr_t result = EndpointManager::instance().try_recv(handle, &message);
                if (result == kErrorAgain) {
                    if (deadline_expired(deadline)) {
                        failed.store(true, std::memory_order_release);
                        return;
                    }
                    perturb(random);
                    continue;
                }
                if (result != 0) {
                    failed.store(true, std::memory_order_release);
                    return;
                }

                const uint64_t id = message.words[0];
                if (id >= total || message.words[1] != ~id || message.words[2] != seed ||
                    message.words[3] != (id ^ seed ^ 0xC0DEC0DEULL)) {
                    failed.store(true, std::memory_order_release);
                    return;
                }

                const unsigned producer = static_cast<unsigned>(id / messages_per_producer);
                if (message.sender != static_cast<TaskId>(200U + producer) ||
                    seen[id].fetch_add(1U, std::memory_order_acq_rel) != 0U) {
                    failed.store(true, std::memory_order_release);
                    return;
                }

                consumed.fetch_add(1U, std::memory_order_release);
                perturb(random);
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (std::thread& thread : threads) {
        thread.join();
    }

    bool complete = consumed.load(std::memory_order_acquire) == total &&
                    !failed.load(std::memory_order_acquire);
    for (uint64_t index = 0U; index < total; ++index) {
        complete = complete && seen[index].load(std::memory_order_acquire) == 1U;
    }
    return destroy_endpoint(handle) && complete;
}

bool run_destroy_race(uint64_t seed) {
    uint64_t stale_handle = 0U;
    if (!create_endpoint(stale_handle)) {
        return false;
    }

    std::atomic<unsigned> phase{0U};
    std::atomic<uint64_t> operations{0U};
    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;

    for (unsigned worker = 0U; worker < 4U; ++worker) {
        workers.emplace_back([&, worker] {
            uint64_t random = seed ^ (0x8EBC6AF09C88C6E3ULL * (worker + 1U));
            while (phase.load(std::memory_order_acquire) == 0U && !failed.load()) {
                intptr_t result = 0;
                if ((worker & 1U) == 0U) {
                    const uint64_t words[4] = {worker, operations.load(), seed, random};
                    result = EndpointManager::instance().send(300U + worker, stale_handle, words);
                } else {
                    Message message{};
                    result = EndpointManager::instance().try_recv(stale_handle, &message);
                }
                if (result != 0 && result != kErrorAgain && result != kErrorInvalidArgument) {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                operations.fetch_add(1U, std::memory_order_release);
                perturb(random);
            }

            // Every operation invoked after phase 1 was published starts after
            // destroy_endpoint returned; success or -EAGAIN would violate the
            // stale-handle contract.
            for (unsigned attempt = 0U; attempt < 1000U && !failed.load(); ++attempt) {
                intptr_t result = 0;
                if ((worker & 1U) == 0U) {
                    const uint64_t words[4] = {worker, attempt, seed, random};
                    result = EndpointManager::instance().send(300U + worker, stale_handle, words);
                } else {
                    Message message{};
                    result = EndpointManager::instance().try_recv(stale_handle, &message);
                }
                if (result != kErrorInvalidArgument) {
                    failed.store(true, std::memory_order_release);
                    return;
                }
                perturb(random);
            }
        });
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (operations.load(std::memory_order_acquire) < 4000U && !deadline_expired(deadline)) {
        std::this_thread::yield();
    }
    if (operations.load(std::memory_order_acquire) < 4000U) {
        failed.store(true, std::memory_order_release);
    }

    if (!destroy_endpoint(stale_handle)) {
        failed.store(true, std::memory_order_release);
    }
    phase.store(1U, std::memory_order_release);

    for (std::thread& worker : workers) {
        worker.join();
    }

    uint64_t live_handle = 0U;
    if (failed.load(std::memory_order_acquire) || !create_endpoint(live_handle)) {
        return false;
    }

    const uint64_t words[4] = {0xDEADU, 0xBEEFU, seed, live_handle};
    Message message{};
    const bool isolated =
        live_handle != stale_handle &&
        EndpointManager::instance().send(999U, stale_handle, words) == kErrorInvalidArgument &&
        EndpointManager::instance().try_recv(stale_handle, &message) == kErrorInvalidArgument &&
        EndpointManager::instance().send(999U, live_handle, words) == 0 &&
        EndpointManager::instance().try_recv(live_handle, &message) == 0 &&
        message.sender == 999U && message.words[0] == words[0];
    return destroy_endpoint(live_handle) && isolated;
}

bool run_generation_churn(uint64_t seed, unsigned iterations) {
    uint64_t oldest_handle = 0U;
    for (unsigned iteration = 0U; iteration < iterations; ++iteration) {
        uint64_t handle = 0U;
        if (!create_endpoint(handle)) {
            return false;
        }
        if (iteration == 0U) {
            oldest_handle = handle;
        } else {
            const uint64_t words[4] = {iteration, seed, handle, oldest_handle};
            Message message{};
            if (EndpointManager::instance().send(777U, oldest_handle, words) !=
                    kErrorInvalidArgument ||
                EndpointManager::instance().try_recv(oldest_handle, &message) !=
                    kErrorInvalidArgument) {
                return false;
            }
        }
        if (!destroy_endpoint(handle)) {
            return false;
        }
    }
    return true;
}

unsigned parse_unsigned(const char* value, const char* name) {
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 0);
    if (end == value || *end != '\0' || parsed == 0U || parsed > 1000000UL) {
        std::cerr << "invalid " << name << ": " << value << '\n';
        std::exit(2);
    }
    return static_cast<unsigned>(parsed);
}

uint64_t parse_seed(const char* value) {
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(value, &end, 0);
    if (end == value || *end != '\0') {
        std::cerr << "invalid seed: " << value << '\n';
        std::exit(2);
    }
    return static_cast<uint64_t>(parsed);
}

} // namespace

int main(int argc, char** argv) {
    const uint64_t seed = argc > 1 ? parse_seed(argv[1]) : 1U;
    const unsigned messages = argc > 2 ? parse_unsigned(argv[2], "message count") : 2000U;
    const auto started = std::chrono::steady_clock::now();

    const bool fifo = run_mpsc_fifo(seed, messages);
    const bool exactly_once = fifo && run_mpmc_exactly_once(seed ^ 0x5555AAAAULL, messages);
    const bool destroy_race = exactly_once && run_destroy_race(seed ^ 0xA5A5A5A5ULL);
    const bool churn = destroy_race && run_generation_churn(seed, messages);

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    std::cout << "HOST_IPC_TORTURE: "
              << (fifo && exactly_once && destroy_race && churn ? "PASS" : "FAIL")
              << " seed=" << seed << " messages_per_producer=" << messages
              << " elapsed_ms=" << elapsed.count() << '\n';
    std::cout << "HOST_IPC_MPSC_FIFO: " << (fifo ? "PASS" : "FAIL") << '\n';
    std::cout << "HOST_IPC_MPMC_EXACTLY_ONCE: " << (exactly_once ? "PASS" : "FAIL") << '\n';
    std::cout << "HOST_IPC_DESTROY_RACE: " << (destroy_race ? "PASS" : "FAIL") << '\n';
    std::cout << "HOST_IPC_GENERATION_CHURN: " << (churn ? "PASS" : "FAIL") << '\n';
    return fifo && exactly_once && destroy_race && churn ? 0 : 1;
}
