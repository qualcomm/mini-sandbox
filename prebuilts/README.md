# mini-sandbox prebuilt binaries — v1.1.15.r1

This folder ships ready-to-use **mini-sandbox** binaries for release `v1.1.15.r1`,
downloaded from the GitHub Releases page (not rebuilt). No compilation needed.

Layout:
- `bin/mini-sandbox`            — the sandbox executable
- `lib/libmini-sandbox.so`      — shared library
- `lib/libmini-sandbox.a`       — static library
- `include/linux-sandbox-api.h` — public C header
- `python/pyminisandbox/`       — Python ctypes bindings
- `SHA256SUMS.txt`              — checksums of every file above

Verify with:
```
cd prebuilts && sha256sum --check SHA256SUMS.txt
```

mini-tapbox / minitap artifacts are intentionally not included.
The rest of this branch is the source tree of release `v1.1.15.r1`.
