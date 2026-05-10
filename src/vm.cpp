// vm.cpp — implementation of VM (see vm.hpp).

#include "vm.hpp"

#include <Hypervisor/Hypervisor.h>
#include <sys/mman.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace chubby {

namespace {

// Throw a descriptive runtime_error if a Hypervisor.framework call failed.
void check(hv_return_t r, const char* what) {
    if (r != HV_SUCCESS) {
        std::ostringstream oss;
        oss << what << " failed: 0x" << std::hex << r;
        throw std::runtime_error(oss.str());
    }
}

// Apple Silicon's hardware page size. hv_vm_map enforces this alignment for
// both the host pointer (mmap returns 16 KiB-aligned pages by default) and
// the guest physical address.
constexpr size_t kPageSize = 16384;

}  // namespace

VM::VM(bool verbose) : verbose_(verbose) {
    check(hv_vm_create(nullptr), "hv_vm_create");
    if (verbose_) std::cerr << "[vmm] VM created\n";
}

VM::~VM() {
    // Reverse of add_ram: unmap from the guest, then release the host mmap.
    for (auto& r : regions_) {
        hv_vm_unmap(r.guest_pa, r.size);
        munmap(r.host_addr, r.size);
    }
    hv_vm_destroy();
}

void VM::add_ram(uint64_t guest_pa, size_t size) {
    if (size == 0) throw std::invalid_argument("RAM size must be > 0");
    if (size % kPageSize) throw std::invalid_argument("RAM size must be 16K-aligned");
    if (guest_pa % kPageSize) throw std::invalid_argument("RAM guest_pa must be 16K-aligned");

    void* mem = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_ANON | MAP_PRIVATE, -1, 0);
    if (mem == MAP_FAILED) throw std::runtime_error("mmap failed for guest RAM");
    std::memset(mem, 0, size);

    // RWX from the guest's perspective. Tighter per-region permissions are
    // possible (e.g. RO for a kernel image), but one combined region is
    // simpler for the bare-metal payload case.
    check(hv_vm_map(mem, guest_pa, size,
                    HV_MEMORY_READ | HV_MEMORY_WRITE | HV_MEMORY_EXEC),
          "hv_vm_map");

    regions_.push_back({mem, guest_pa, size});

    if (verbose_) {
        std::cerr << "[vmm] mapped " << (size >> 20) << " MiB RAM at 0x"
                  << std::hex << guest_pa << std::dec << "\n";
    }
}

void* VM::host_ptr(uint64_t guest_pa, size_t len) const {
    for (auto& r : regions_) {
        if (guest_pa >= r.guest_pa && guest_pa + len <= r.guest_pa + r.size) {
            return static_cast<uint8_t*>(r.host_addr) + (guest_pa - r.guest_pa);
        }
    }
    return nullptr;
}

void VM::load_flat_binary(const std::string& path, uint64_t guest_pa) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("cannot open payload: " + path);

    const std::streamsize sz = f.tellg();
    if (sz <= 0) throw std::runtime_error("empty payload: " + path);
    f.seekg(0);

    void* dst = host_ptr(guest_pa, static_cast<size_t>(sz));
    if (!dst) throw std::runtime_error("payload doesn't fit in mapped guest RAM");

    if (!f.read(static_cast<char*>(dst), sz)) {
        throw std::runtime_error("read failed: " + path);
    }

    if (verbose_) {
        std::cerr << "[vmm] loaded " << sz << " bytes at 0x"
                  << std::hex << guest_pa << std::dec << " from " << path << "\n";
    }
}

}  // namespace chubby
