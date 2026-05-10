// Tiny "hello world" guest payload for chubby-cat.
//
// chubby-cat HVC ABI:
//   HVC #1   putchar(x0)
//   HVC #2   exit(x0)
//   HVC #3   puts(x0 = guest_pa, x1 = length)
//
// Built as a flat binary loaded at the guest entry physical address.
// The vCPU starts with PC = entry, MMU off, EL1h, all interrupts masked.

    .text
    .global _start
_start:
    adr     x0, msg
    adr     x1, msg_end
    sub     x1, x1, x0
    hvc     #3                  // puts(msg, len)

    mov     x0, #0
    hvc     #2                  // exit(0)

    .balign 4
msg:
    .ascii  "Hello from the chubby-cat microVM guest!\n"
msg_end:
