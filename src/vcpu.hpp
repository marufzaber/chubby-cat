// vcpu.hpp — single vCPU and its exit-handler loop.
//
// Hypervisor.framework binds a vCPU to the thread that called hv_vcpu_create:
// hv_vcpu_run and hv_vcpu_destroy must be invoked from that same thread.
// VCPU is therefore implicitly thread-affine to its constructing thread.

#pragma once

#include <Hypervisor/Hypervisor.h>

#include <cstdint>

namespace chubby {

class VM;

// A single AArch64 vCPU. Boots at EL1h with the MMU off, runs until the guest
// exits cleanly via the chubby-cat HVC ABI or hits an unrecoverable exception.
class VCPU {
public:
    // entry_pc:  initial PC (a guest physical address — MMU is off at boot)
    // stack_top: initial SP_EL1
    VCPU(VM& vm, uint64_t entry_pc, uint64_t stack_top, bool verbose = false);
    ~VCPU();

    VCPU(const VCPU&)            = delete;
    VCPU& operator=(const VCPU&) = delete;

    // Drive the vCPU until the guest exits cleanly or faults.
    // Returns the guest's HVC #2 exit code, or non-zero on a fault.
    int run();

private:
    // Decode the trap from ESR_EL2 and dispatch. Returns false to stop the loop.
    bool handle_exception();
    // Dispatch a chubby-cat HVC immediate (1=putchar, 2=exit, 3=puts).
    bool handle_hvc(uint16_t imm);

    uint64_t reg(hv_reg_t r) const;
    void     set_reg(hv_reg_t r, uint64_t v);
    void     set_sysreg(hv_sys_reg_t r, uint64_t v);

    VM&              vm_;
    hv_vcpu_t        handle_{};
    hv_vcpu_exit_t*  exit_{nullptr};   // populated by hv_vcpu_run with exit info
    bool             verbose_;
    int              exit_code_{0};
};

}  // namespace chubby
