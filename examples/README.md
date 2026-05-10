# Example guest

`hello.S` is a tiny aarch64 bare-metal payload that prints a string and exits,
using the chubby-cat HVC ABI.

## Build

```bash
make            # produces hello.bin (flat binary)
```

The Makefile uses `xcrun clang` and `xcrun llvm-objcopy`, both of which ship
with the Xcode command-line tools.

## Run

From the repository root, after building chubby-cat:

```bash
./build/chubby-cat examples/hello.bin
```

You should see:

```
Hello from the chubby-cat microVM guest!
```

## HVC ABI

| HVC #  | Name    | Args                                   |
|--------|---------|----------------------------------------|
| `#1`   | putchar | `x0` = byte to print                   |
| `#2`   | exit    | `x0` = process exit code (low 8 bits)  |
| `#3`   | puts    | `x0` = guest physical addr, `x1` = len |
