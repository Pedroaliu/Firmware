#include <stdint.h>

#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"
#include "microkernel/memory/memory_lifecycle.h"
#include "microkernel/memory/page_manager.h"
#include "platform/qemu_virt/contained_memory.h"
#include "platform/qemu_virt/ddr.h"

#ifdef JIXIA_M00_07_04_PROBE

namespace {

using jixia::microkernel::memory::BackingKind;
using jixia::microkernel::memory::DdrState;
using jixia::microkernel::memory::MemoryDomain;
using jixia::microkernel::memory::PhysicalRange;
using jixia::microkernel::memory::Snapshot;
using jixia::microkernel::memory::page_manager::Allocation;

constexpr uint64_t kStableObjectMagic = 0x704070407040704ULL;

[[noreturn]] void fail(const char* reason) {
    jixia::microkernel::printk(
        "M00_07_MAINSTORE_TRANSITION: FAIL (%s)\n",
        reason);
    jixia::microkernel::hart::park();
}

[[nodiscard]] bool state_is(MemoryDomain domain, DdrState ddr) {
    const Snapshot state = jixia::microkernel::memory::snapshot();
    return state.domain == domain && state.ddr == ddr;
}

[[nodiscard]] PhysicalRange remaining_mainstore(const Snapshot& state) {
    const uintptr_t ddr_end =
        state.ddr_range.base + state.ddr_range.size;

    const uintptr_t remaining_base =
        state.contained.base + state.contained.size;

    if (ddr_end <= remaining_base) {
        return {
            .base = 0U,
            .size = 0U,
        };
    }

    return {
        .base = remaining_base,
        .size = ddr_end - remaining_base,
    };
}

} // namespace

extern "C" [[noreturn]]
void jixia_m00_07_04_run_mainstore_transition_probe() {
    using namespace jixia::microkernel;

    /*
     * ------------------------------------------------------------
     * Phase 0:
     * Verify the initial pre-DDR contained-memory state.
     * ------------------------------------------------------------
     */
    if (!memory::validate_contained_invariants()) {
        fail("initial contained state invalid");
    }

    /*
     * The PageManager initially owns only the bootstrap EarlyMemory
     * pool inside the contained domain.
     */
    memory::page_manager::reset();

    if (!memory::page_manager::add_contained_bootstrap_pool()) {
        fail("contained PageManager range rejected");
    }

    /*
     * Allocate a live object before DDR exists.
     *
     * This object is deliberately kept alive across the
     * contained -> mainstore transition.  Its address and contents
     * must not change merely because the storage backing changes.
     */
    const Allocation stable_page =
        memory::page_manager::allocate_page();

    if (!stable_page.valid() ||
        stable_page.backing != BackingKind::contained) {
        fail("pre-DDR stable page is not contained-backed");
    }

    volatile uint64_t* const stable_object =
        reinterpret_cast<volatile uint64_t*>(
            stable_page.physical_address);

    *stable_object = kStableObjectMagic;

    const uintptr_t stable_address =
        stable_page.physical_address;

    /*
     * QEMU's configured DDR range already physically exists, but
     * Jixia must not treat it as allocator-visible before the DDR
     * lifecycle and contained-memory transition are complete.
     */
    const PhysicalRange future_ddr =
        jixia::platform::qemu_virt::ddr::configured_range();

    const Snapshot initial_state = memory::snapshot();

    if (future_ddr.size < initial_state.contained.size) {
        fail("configured DDR smaller than contained range");
    }

    const PhysicalRange future_remaining = {
        .base =
            future_ddr.base + initial_state.contained.size,

        .size =
            future_ddr.size - initial_state.contained.size,
    };

    /*
     * Before DDR discovery, PageManager must reject a normal DDR
     * range completely.
     *
     * Note that EarlyMemory allocation is still legal here.
     */
    if (memory::page_manager::add_range(
            future_remaining.base,
            future_remaining.size,
            BackingKind::ddr)) {
        fail("DDR allocator range accepted before DDR lifecycle");
    }

    printk("M00_07_DDR_ALLOCATOR_GATED: PASS\n");

    /*
     * ------------------------------------------------------------
     * Phase 1:
     * Explicit DDR hardware lifecycle.
     * ------------------------------------------------------------
     */

    if (!jixia::platform::qemu_virt::ddr::discover() ||
        !state_is(
            MemoryDomain::contained,
            DdrState::discovered)) {
        fail("DDR discovery transition");
    }

    printk("M00_07_DDR_DISCOVERED: PASS\n");

    if (!jixia::platform::qemu_virt::ddr::start_training() ||
        !state_is(
            MemoryDomain::contained,
            DdrState::training)) {
        fail("DDR training start transition");
    }

    printk("M00_07_DDR_TRAINING: PASS\n");

    if (!jixia::platform::qemu_virt::ddr::finish_training() ||
        !state_is(
            MemoryDomain::contained,
            DdrState::trained)) {
        fail("DDR training completion transition");
    }

    printk("M00_07_DDR_TRAINED: PASS\n");

    /*
     * Topology construction currently does not introduce another
     * LifecycleState enum value, but remains an explicit platform
     * operation because later real DDR implementations will need it.
     */
    if (!jixia::platform::qemu_virt::ddr::build_topology()) {
        fail("DDR topology stage");
    }

    printk("M00_07_DDR_TOPOLOGY_READY: PASS\n");

    if (!jixia::platform::qemu_virt::ddr::build_address_map() ||
        !state_is(
            MemoryDomain::contained,
            DdrState::address_map_ready)) {
        fail("DDR address-map transition");
    }

    printk("M00_07_DDR_ADDRESS_MAP_READY: PASS\n");

    if (!jixia::platform::qemu_virt::ddr::program_decode() ||
        !state_is(
            MemoryDomain::contained,
            DdrState::decode_committed)) {
        fail("DDR decode commit transition");
    }

    printk("M00_07_DDR_DECODE_COMMITTED: PASS\n");

    if (!jixia::platform::qemu_virt::ddr::online() ||
        !state_is(
            MemoryDomain::contained,
            DdrState::online)) {
        fail("DDR online transition");
    }

    /*
     * Critical invariant:
     *
     * DDR hardware being ONLINE does NOT mean the normal DDR
     * allocator is visible yet.
     *
     * We are still running in the contained memory domain.
     */
    if (memory::ddr_allocation_enabled() ||
        memory::page_manager::add_range(
            future_remaining.base,
            future_remaining.size,
            BackingKind::ddr)) {
        fail("DDR became allocator-visible before contained exit");
    }

    printk("M00_07_DDR_ONLINE: PASS\n");

    /*
     * The live pre-DDR object must still be contained-backed and
     * retain its contents before the transition begins.
     */
    if (*stable_object != kStableObjectMagic ||
        memory::backing_for(stable_address) !=
            BackingKind::contained) {
        fail("stable object changed before transition");
    }

    /*
     * ------------------------------------------------------------
     * Phase 2:
     * Begin contained -> mainstore transition.
     * ------------------------------------------------------------
     */

    if (!memory::begin_mainstore_transition() ||
        !state_is(
            MemoryDomain::transitioning,
            DdrState::online)) {
        fail("mainstore transition did not begin");
    }

    /*
     * On QEMU this is a semantic flush/castout hook.
     *
     * A future real contained-cache implementation may need to
     * actually clean/cast out dirty cache lines into DDR here.
     */
    if (!jixia::platform::qemu_virt::
            contained_memory::flush_to_mainstore()) {
        fail("contained flush/castout hook");
    }

    const Snapshot flushed = memory::snapshot();

    if (!flushed.contained_flush_complete ||
        memory::backing_for(stable_address) !=
            BackingKind::contained ||
        *stable_object != kStableObjectMagic) {
        fail("contained backing changed before transition commit");
    }

    printk("M00_07_CONTAINED_FLUSH: PASS\n");

    /*
     * Commit the memory-domain semantic transition.
     *
     * IMPORTANT:
     * complete_mainstore_transition() must NOT publish allocator
     * availability yet.
     */
    if (!memory::complete_mainstore_transition()) {
        fail("mainstore transition commit");
    }

    if (memory::ddr_allocation_enabled()) {
        fail("allocator opened before PageManager commit");
    }

    /*
     * After semantic commit:
     *
     *     address: unchanged
     *     contents: unchanged
     *     backing: CONTAINED -> DDR
     *
     * No pointer fixup is required.
     */
    if (reinterpret_cast<uintptr_t>(stable_object) !=
            stable_address ||
        *stable_object != kStableObjectMagic ||
        memory::backing_for(stable_address) !=
            BackingKind::ddr) {
        fail("stable address/backing invariant");
    }

    printk("M00_07_STABLE_ADDRESS: PASS\n");

    /*
     * ------------------------------------------------------------
     * Phase 3:
     * Prepare PageManager metadata for normal mainstore.
     * ------------------------------------------------------------
     *
     * Existing allocator-owned EarlyMemory pages keep exactly the
     * same physical addresses and allocation cursor.  Only their
     * backing classification changes.
     */
    if (memory::page_manager::promote_backing(
            BackingKind::contained,
            BackingKind::ddr) != 1U) {
        fail("PageManager contained range promotion");
    }

    const Snapshot mainstore = memory::snapshot();

    const PhysicalRange remaining =
        remaining_mainstore(mainstore);

    if (remaining.size == 0U) {
        fail("no remaining mainstore range");
    }

    const size_t ddr_pages_before_extend =
        memory::page_manager::remaining_pages(
            BackingKind::ddr);

    /*
     * At this point DDR has valid mainstore backing semantics, so
     * PageManager may register the remaining DDR resource even
     * though allocation has not yet been published.
     */
    if (!memory::page_manager::add_range(
            remaining.base,
            remaining.size,
            BackingKind::ddr)) {
        fail("remaining mainstore range rejected");
    }

    const size_t ddr_pages_after_extend =
        memory::page_manager::remaining_pages(
            BackingKind::ddr);

    if (ddr_pages_after_extend <=
        ddr_pages_before_extend) {
        fail("mainstore extension did not add pages");
    }

    /*
     * The complete DDR resource map is now known to PageManager,
     * but the allocator publication gate must still be closed.
     *
     * This deliberately proves:
     *
     *     resource registered != resource allocatable
     */
    const Allocation gated_page =
        memory::page_manager::allocate_page();

    if (gated_page.valid()) {
        fail("allocation escaped mainstore gate");
    }

    printk(
        "M00_07_MAINSTORE_ALLOCATOR_GATED: PASS\n");

    /*
     * ------------------------------------------------------------
     * Phase 4:
     * Publish allocator availability LAST.
     * ------------------------------------------------------------
     */
    if (!memory::enable_mainstore_allocation() ||
        !memory::validate_mainstore_invariants()) {
        fail("mainstore allocator enable");
    }

    /*
     * The first allocation after publication must come from DDR.
     *
     * There is deliberately no fallback to contained backing once
     * the mainstore allocator has been published.
     */
    const Allocation post_transition_page =
        memory::page_manager::allocate_page();

    if (!post_transition_page.valid() ||
        post_transition_page.backing !=
            BackingKind::ddr ||
        memory::backing_for(
            post_transition_page.physical_address) !=
            BackingKind::ddr) {
        fail("post-transition allocation is not DDR-backed");
    }

    printk(
        "M00_07_MAINSTORE_TRANSITION: PASS\n"
        "M00_07_MAINSTORE_EXTEND: PASS\n"
        "M00-07.04 fake DDR lifecycle and mainstore transition: PASS\n");

    hart::park();
}

#endif