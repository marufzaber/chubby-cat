#include "vcpu.hpp"

#include "vm.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace chubby {

namespace {

void check(hv_return_t r, const char* what) {
    if (r != HV_SUCCESS) {
        std::ostringstream oss;
        oss << what << " failed: 0x" << std::hex << r;
        throw std::runtime_error(oss.str());
    }
}

// PSTATE/CPSR bits for an EL1h entry with all interrupts masked.
constexpr uint64_t kPsrModeEL1h = 0x5;
constexpr uint64_t kPsrF        = 1ull << 6;
constexpr uint64_t kPsrI        = 1ull << 7;
constexpr uint64_t kPsrA        = 1ull << 8;
constexpr uint64_t kPsrD        = 1ull << 9;
constexpr uint64_t kInitialCpsr = kPsrModeEL1h | kPsrF | kPsrI | kPsrA | kPsrD;

// ESR_EL2.EC values we care about.
constexpr uint32_t kEcHvcAArch64        = 0x16;
constexpr uint32_t kEcDataAbortLowerEL  = 0x24;
constexpr uint32_t kEcDataAbortSameEL   = 0x25;
constexpr uint32_t kEcInstAbortLowerEL  = 0x20;

// Custom HVC ABI used by guests of chubby-cat.
constexpr uint16_t kHvcPutchar = 0x1;
constexpr uint16_t kHvcExit    = 0x2;
constexpr uint16_t kHvcPuts    = 0x3;

}  // namespace

VCPU::VCPU(VM& vm, uint64_t entry_pc, uint64_t stack_top, bool verbose)
    : vm_(vm), verbose_(verbose) {
    check(hv_vcpu_create(&handle_, &exit_, nullptr), "hv_vcpu_create");

    set_reg(HV_REG_CPSR, kInitialCpsr);
    set_reg(HV_REG_PC, entry_pc);
    set_sysreg(HV_SYS_REG_SP_EL1, stack_top);

    // Allow guest FP/SIMD without trapping (CPACR_EL1.FPEN = 0b11).
    set_sysreg(HV_SYS_REG_CPACR_EL1, 0x300000);

    if (verbose_) {
        std::cerr << "[vmm] vCPU created, PC=0x" << std::hex << entry_pc
                  << " SP=0x" << stack_top << std::dec << "\n";
    }
}

VCPU::~VCPU() {
    hv_vcpu_destroy(handle_);
}

uint64_t VCPU::reg(hv_reg_t r) const {
    uint64_t v = 0;
    check(hv_vcpu_get_reg(handle_, r, &v), "hv_vcpu_get_reg");
    return v;
}

void VCPU::set_reg(hv_reg_t r, uint64_t v) {
    check(hv_vcpu_set_reg(handle_, r, v), "hv_vcpu_set_reg");
}

void VCPU::set_sysreg(hv_sys_reg_t r, uint64_t v) {
    check(hv_vcpu_set_sys_reg(handle_, r, v), "hv_vcpu_set_sys_reg");
}

int VCPU::run() {
    bool running = true;
    while (running) {
        check(hv_vcpu_run(handle_), "hv_vcpu_run");

        switch (exit_->reason) {
            case HV_EXIT_REASON_EXCEPTION:
                running = handle_exception();
                break;
            case HV_EXIT_REASON_VTIMER_ACTIVATED:
                // Nothing to do until the guest waits on the virtual timer.
                break;
            case HV_EXIT_REASON_CANCELED:
                if (verbose_) std::cerr << "[vmm] run cancelled\n";
                running = false;
                break;
            default:
                std::cerr << "[vmm] unknown exit reason: " << exit_->reason << "\n";
                exit_code_ = 2;
                running    = false;
                break;
        }
    }
    return exit_code_;
}

bool VCPU::handle_exception() {
    const uint64_t esr = exit_->exception.syndrome;
    const uint64_t far = exit_->exception.virtual_address;
    const uint64_t ipa = exit_->exception.physical_address;
    const uint32_t ec  = static_cast<uint32_t>((esr >> 26) & 0x3f);
    const uint64_t pc  = reg(HV_REG_PC);

    switch (ec) {
        case kEcHvcAArch64: {
            const uint16_t imm = static_cast<uint16_t>(esr & 0xffff);
            return handle_hvc(imm);
        }
        case kEcDataAbortLowerEL:
        case kEcDataAbortSameEL:
            std::cerr << "[vmm] data abort: PC=0x" << std::hex << pc
                      << " FAR=0x" << far << " IPA=0x" << ipa
                      << " ESR=0x" << esr << std::dec << "\n";
            exit_code_ = 1;
            return false;
        case kEcInstAbortLowerEL:
            std::cerr << "[vmm] instruction abort: PC=0x" << std::hex << pc
                      << " IPA=0x" << ipa << " ESR=0x" << esr << std::dec << "\n";
            exit_code_ = 1;
            return false;
        default:
            std::cerr << "[vmm] unhandled exception EC=0x" << std::hex << ec
                      << " ESR=0x" << esr << " PC=0x" << pc << std::dec << "\n";
            exit_code_ = 1;
            return false;
    }
}

bool VCPU::handle_hvc(uint16_t imm) {
    switch (imm) {
        case kHvcPutchar: {
            const uint64_t c = reg(HV_REG_X0);
            std::putchar(static_cast<int>(c & 0xff));
            std::fflush(stdout);
            return true;
        }
        case kHvcExit: {
            exit_code_ = static_cast<int>(reg(HV_REG_X0) & 0xff);
            if (verbose_) {
                std::cerr << "[vmm] guest exit code " << exit_code_ << "\n";
            }
            return false;
        }
        case kHvcPuts: {
            const uint64_t pa  = reg(HV_REG_X0);
            const uint64_t len = reg(HV_REG_X1);
            void*          p   = vm_.host_ptr(pa, static_cast<size_t>(len));
            if (!p) {
                std::cerr << "[vmm] HVC puts: invalid guest pointer 0x"
                          << std::hex << pa << " len " << len << std::dec << "\n";
                exit_code_ = 1;
                return false;
            }
            std::fwrite(p, 1, static_cast<size_t>(len), stdout);
            std::fflush(stdout);
            return true;
        }
        default:
            std::cerr << "[vmm] unknown HVC #" << imm
                      << " at PC=0x" << std::hex << reg(HV_REG_PC) << std::dec << "\n";
            exit_code_ = 1;
            return false;
    }
}

}  // namespace chubby
