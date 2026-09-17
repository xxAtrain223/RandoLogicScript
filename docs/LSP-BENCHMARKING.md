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
for local before-and-after development comparisons only. Stage output separates
source reads, parsing, semantic passes, index construction, accepted-snapshot
replacement, and residual scheduler work.

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

## Whole-project stage profile

A separate instrumented run on the same configuration measured a 461.0 ms mean edit:

| Stage | Mean | Share |
| --- | ---: | ---: |
| Parse and source-text construction | 293.1 ms | 63.6% |
| Semantic index | 123.7 ms | 26.8% |
| Accepted-snapshot replacement | 20.7 ms | 4.5% |
| Type resolution | 11.2 ms | 2.4% |
| Validation | 3.7 ms | 0.8% |
| Source reads | 3.6 ms | 0.8% |
| Scheduler and other work | 3.7 ms | 0.8% |
| Declaration collection | 1.3 ms | 0.3% |
| Other snapshot work | 0.1 ms | <0.1% |

Parsing and semantic-index construction account for about 90% of edit latency.
Source I/O and the three semantic passes excluding index construction are small by
comparison. Incremental work should therefore measure parse reuse first, followed by
per-document semantic-index reuse if indexing remains the second-largest cost.

## Parsed-document reuse

Measured on 2026-09-16 after reusing immutable parse results for unchanged documents:

| Metric | Result | Change from baseline |
| --- | ---: | ---: |
| Sources parsed per edit | 1 of 60 | -98.3% |
| Edit mean | 160.7 ms | -63.7% |
| Edit median | 159.9 ms | -63.7% |
| Edit p95 | 161.0 ms | -63.9% |
| Edit range | 158.2-168.2 ms | |
| Initial analysis | 438.7 ms | |

The mean incremental edit was distributed as follows:

| Stage | Mean | Share |
| --- | ---: | ---: |
| Semantic index | 119.1 ms | 74.1% |
| Accepted-snapshot replacement | 10.5 ms | 6.5% |
| Type resolution | 10.0 ms | 6.2% |
| AST materialization | 9.0 ms | 5.6% |
| Scheduler and other work | 3.7 ms | 2.3% |
| Source reads | 3.6 ms | 2.2% |
| Validation | 3.3 ms | 2.1% |
| Declaration collection | 1.1 ms | 0.7% |
| Parse | 0.2 ms | 0.1% |
| Other snapshot work | 0.2 ms | 0.1% |

Semantic-index construction is now the dominant cost. The next optimization should
reuse or rebuild index contributions per document while retaining full semantic
analysis as the correctness reference.