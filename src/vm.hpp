#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace chubby {

struct MemRegion {
    void*    host_addr;
    uint64_t guest_pa;
    size_t   size;
};

class VM {
public:
    explicit VM(bool verbose = false);
    ~VM();

    VM(const VM&)            = delete;
    VM& operator=(const VM&) = delete;

    void  add_ram(uint64_t guest_pa, size_t size);
    void  load_flat_binary(const std::string& path, uint64_t guest_pa);
    void* host_ptr(uint64_t guest_pa, size_t len) const;

    bool verbose() const { return verbose_; }

private:
    std::vector<MemRegion> regions_;
    bool                   verbose_;
};

}  // namespace chubby
