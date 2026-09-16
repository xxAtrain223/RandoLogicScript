# LSP Analysis Benchmarks

`rls_lsp_benchmark` measures the language server's analysis pipeline through the
production `AnalysisScheduler`. It reports the initial project analysis followed by
repeated full-text changes to one source. The scheduler debounce is set to zero so
the result measures source loading, parsing, semantic analysis, and indexing rather
than an intentional editor delay.

Build the `rls_lsp_benchmark` CMake target, then run:

```powershell
.\build-release-benchmark\lsp\rls_lsp_benchmark.exe .\examples\soh --iterations 10 --warmup 2
```

The benchmark emits JSON, including every measured edit sample. Keep the project,
build type, iteration count, and machine unchanged when comparing implementations.
Use a Release build for user-facing latency measurements; Debug results are useful
for local before-and-after development comparisons only.

## Whole-project baseline

Measured on 2026-09-15 with MSVC Release (`/O2`) on the local development machine:

| Metric | Result |
| --- | ---: |
| Sources | 60 |
| Initial analysis | 622.3 ms |
| Edit mean | 442.1 ms |
| Edit median | 440.4 ms |
| Edit p95 | 446.5 ms |
| Edit range | 437.9-452.2 ms |

After incremental compilation is implemented, rerun the same command and compare
`edit_median_ms` and `edit_p95_ms`; initial analysis should remain a separate
regression metric.