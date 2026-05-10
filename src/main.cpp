// main.cpp — chubby-cat CLI entry point.
//
// Parses arguments, builds a VM with one RAM region, loads a flat-binary
// payload at the guest entry address, and runs a single vCPU until it exits.

#include "vcpu.hpp"
#include "vm.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace chubby {

// Parsed command-line configuration. Defaults match the README.
struct Args {
    std::string payload;
    uint64_t    mem_bytes = 64ull * 1024ull * 1024ull;
    uint64_t    entry_pa  = 0x80000000ull;
    uint64_t    stack_top = 0;  // 0 sentinel: defer to "top of RAM" default.
    bool        verbose   = false;
};

void print_usage(const char* prog) {
    std::cerr <<
        "chubby-cat: a tiny C++ microVM hypervisor for macOS (Apple Silicon)\n"
        "\n"
        "Usage: " << prog << " [options] <payload.bin>\n"
        "\n"
        "Options:\n"
        "  --mem <MiB>      Guest RAM size in MiB (default: 64)\n"
        "  --entry <addr>   Guest entry physical address (default: 0x80000000)\n"
        "  --stack <addr>   Initial SP_EL1 (default: top of RAM)\n"
        "  -v, --verbose    Verbose VMM output\n"
        "  -h, --help       Show this help\n"
        "\n"
        "Guest HVC ABI:\n"
        "  HVC #1: putchar  (x0 = byte)\n"
        "  HVC #2: exit     (x0 = exit code)\n"
        "  HVC #3: puts     (x0 = guest_pa, x1 = length)\n";
}

// std::stoull with auto base detection (decimal, 0x.., 0..).
static uint64_t parse_u64(const std::string& s) {
    return std::stoull(s, nullptr, 0);
}

// Returns 0 on success, 1 if --help was requested, 2 on a usage error.
int parse_args(int argc, char** argv, Args& out) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            return 1;
        } else if (a == "-v" || a == "--verbose") {
            out.verbose = true;
        } else if (a == "--mem" && i + 1 < argc) {
            out.mem_bytes = parse_u64(argv[++i]) * 1024ull * 1024ull;
        } else if (a == "--entry" && i + 1 < argc) {
            out.entry_pa = parse_u64(argv[++i]);
        } else if (a == "--stack" && i + 1 < argc) {
            out.stack_top = parse_u64(argv[++i]);
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "unknown option: " << a << "\n";
            print_usage(argv[0]);
            return 2;
        } else {
            if (!out.payload.empty()) {
                std::cerr << "only one payload may be specified\n";
                return 2;
            }
            out.payload = a;
        }
    }
    if (out.payload.empty()) {
        print_usage(argv[0]);
        return 2;
    }
    if (out.stack_top == 0) {
        out.stack_top = out.entry_pa + out.mem_bytes;
    }
    return 0;
}

}  // namespace chubby

int main(int argc, char** argv) {
    chubby::Args args;
    int rc = chubby::parse_args(argc, argv, args);
    if (rc != 0) return rc == 1 ? 0 : rc;

    try {
        chubby::VM vm(args.verbose);
        vm.add_ram(args.entry_pa, args.mem_bytes);
        vm.load_flat_binary(args.payload, args.entry_pa);

        chubby::VCPU vcpu(vm, args.entry_pa, args.stack_top, args.verbose);
        return vcpu.run();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
