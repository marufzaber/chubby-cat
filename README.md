# chubby-cat

A tiny C++ microVM hypervisor for macOS on Apple Silicon, built directly on
[`Hypervisor.framework`](https://developer.apple.com/documentation/hypervisor).

It's intentionally small — under ~500 lines of C++17 — and gives you the
plumbing to run a bare-metal aarch64 guest payload inside a hardware-isolated
VM:

- Creates a VM and a vCPU via `hv_vm_create` / `hv_vcpu_create`
- Allocates and maps guest RAM
- Loads a flat binary at a configurable entry address
- Boots EL1h with the MMU off and runs the vCPU
- Handles HVC traps so the guest can call back into the VMM
- Reports data / instruction aborts and unknown exceptions cleanly

## Requirements

- macOS 11+ on **Apple Silicon** (arm64 only)
- Xcode command-line tools (`xcode-select --install`) — provides clang,
  ld, codesign, and segedit
- CMake ≥ 3.20 _(optional — a plain Makefile also works)_

## Build

With the included Makefile (no extra deps):

```bash
make            # builds build/chubby-cat
make example    # builds examples/hello.bin (the demo guest)
```

Or with CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Either build step ad-hoc-signs the binary with the
`com.apple.security.hypervisor` entitlement (required by
`Hypervisor.framework`).

## Run

```bash
./build/chubby-cat examples/hello.bin
```

Expected output:

```
Hello from the chubby-cat microVM guest!
```

CLI options:

```
--mem <MiB>      Guest RAM size in MiB         (default: 64)
--entry <addr>   Guest entry physical address  (default: 0x80000000)
--stack <addr>   Initial SP_EL1                (default: top of RAM)
-v, --verbose    Verbose VMM output
```

## How it works

```
 ┌──────────────────────────── chubby-cat (host EL2 / userspace) ───────────────┐
 │                                                                              │
 │  main.cpp ── parses args, creates VM, loads payload, runs vCPU               │
 │  vm.cpp   ── hv_vm_create + hv_vm_map (mmap'd anonymous RAM)                 │
 │  vcpu.cpp ── hv_vcpu_create + run-loop + ESR_EL2 decoding                    │
 │                                                                              │
 └──────────────────────────────┬───────────────────────────────────────────────┘
                                │  hv_vcpu_run()  /  exit (HVC, abort, …)
                                ▼
 ┌──────────────────────── guest (EL1h, MMU off, flat-mapped) ─────────────────┐
 │                                                                              │
 │  flat binary loaded at --entry, vCPU starts there with SP_EL1 = --stack      │
 │  uses HVCs as a "syscall" interface back into the VMM                        │
 │                                                                              │
 └──────────────────────────────────────────────────────────────────────────────┘
```

### Guest HVC ABI

A guest interacts with the host via HVC immediates:

| HVC #  | Name    | Args                                            |
|--------|---------|-------------------------------------------------|
| `#1`   | putchar | `x0` = byte to print                            |
| `#2`   | exit    | `x0` = exit code (low 8 bits)                   |
| `#3`   | puts    | `x0` = guest physical address, `x1` = length    |

This is the smallest useful surface for a "hello world" microVM. It's easy
to extend — every new HVC immediate is one new `case` in `vcpu.cpp`.

### vCPU initial state

- `CPSR` = EL1h, all DAIF interrupts masked
- `PC`   = `--entry`
- `SP_EL1` = `--stack` (defaults to the top of RAM)
- `CPACR_EL1.FPEN` = 0b11 (FP/SIMD usable without trapping)
- All other system registers retain Hypervisor.framework defaults — notably
  the MMU is off, so guest physical and virtual addresses coincide.

## Roadmap

`chubby-cat` is intentionally minimal. The same plumbing extends naturally
to a real Linux microVM:

- [ ] PSCI handler (`CPU_ON`, `SYSTEM_OFF`, `SYSTEM_RESET`) for clean
      shutdown and SMP
- [ ] Generic Interrupt Controller (GICv3) trap-and-emulate
- [ ] virtio-mmio transport with a virtio-console for `stdout`
- [ ] virtio-block backed by a host file
- [ ] virtio-net backed by `vmnet.framework`
- [ ] Linux kernel `Image` boot protocol + flattened device tree (FDT)
- [ ] Multi-vCPU SMP

The architecture is set up so that each of these is a separable component
plugged into the existing exit handler in `vcpu.cpp`.

## License

MIT — see [LICENSE](LICENSE).
