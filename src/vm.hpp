// vm.hpp — VM lifecycle and guest physical memory map.
//
// Thin C++ wrapper around the per-VM portion of Apple's Hypervisor.framework:
// hv_vm_create, hv_vm_map, hv_vm_unmap, hv_vm_destroy. RAM is host-mmap'd
// anonymous memory mapped 1:1 at a chosen guest physical address. The MMU is
// off in the guest by default, so guest physical and virtual addresses match.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace chubby {

// One contiguous slice of host RAM mapped into the guest address space.
struct MemRegion {
    void*    host_addr;  // host virtual address of the backing mmap
    uint64_t guest_pa;   // guest physical address the region is mapped at
    size_t   size;       // size in bytes; must be 16 KiB-aligned on Apple Silicon
};

// Owns a Hypervisor.framework VM. Construct one per process: Hypervisor.framework
// permits a single active hv_vm_create per process at a time.
class VM {
public:
    explicit VM(bool verbose = false);
    ~VM();

    VM(const VM&)            = delete;
    VM& operator=(const VM&) = delete;

    // Map a region of zero-initialized RAM into the guest at `guest_pa`.
    // `guest_pa` and `size` must be aligned to the host page size (16 KiB on
    // Apple Silicon). The region is RWX from the guest's perspective.
    void  add_ram(uint64_t guest_pa, size_t size);

    // Copy a flat binary file into guest memory at `guest_pa`. The destination
    // range must lie inside a previously-added RAM region.
    void  load_flat_binary(const std::string& path, uint64_t guest_pa);

    // Translate guest_pa -> host pointer for a contiguous `len`-byte range.
    // Returns nullptr if any byte in the range is unmapped.
    void* host_ptr(uint64_t guest_pa, size_t len) const;

    bool verbose() const { return verbose_; }

private:
    std::vector<MemRegion> regions_;
    bool                   verbose_;
};

}  // namespace chubby
