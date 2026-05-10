#pragma once

#include <Hypervisor/Hypervisor.h>

#include <cstdint>

namespace chubby {

class VM;

class VCPU {
public:
    VCPU(VM& vm, uint64_t entry_pc, uint64_t stack_top, bool verbose = false);
    ~VCPU();

    VCPU(const VCPU&)            = delete;
    VCPU& operator=(const VCPU&) = delete;

    int run();

private:
    bool handle_exception();
    bool handle_hvc(uint16_t imm);

    uint64_t reg(hv_reg_t r) const;
    void     set_reg(hv_reg_t r, uint64_t v);
    void     set_sysreg(hv_sys_reg_t r, uint64_t v);

    VM&              vm_;
    hv_vcpu_t        handle_{};
    hv_vcpu_exit_t*  exit_{nullptr};
    bool             verbose_;
    int              exit_code_{0};
};

}  // namespace chubby
