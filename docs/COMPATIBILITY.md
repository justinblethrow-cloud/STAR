# BlackSTAR Compatibility Contract

This document defines the compatibility promises made by stable BlackSTAR
releases. Anything not stated here remains best-effort.

## Compatibility Identities

BlackSTAR tracks three separate identities:

| Identity | Meaning |
| --- | --- |
| BlackSTAR release | Independent project release and support boundary |
| STAR compatibility base | Upstream source release from which inherited behavior is evaluated |
| Genome format | `versionGenome` accepted when loading a conventional index |

For BlackSTAR `1.0.0`, the executable lineage token is
`2.7.11b-blackstar.3`. Its compatibility base is official STAR `2.7.11b` at
commit `b1edc1208d91a53bf40ebae8669f71d50b994851`. The conventional genome format
remains `2.7.4a`.

## Command-Line Interface

The installed executable remains named `STAR`. Inherited STAR parameters and
defaults are intended to retain official STAR 2.7.11b behavior unless release
notes identify a qualified correction.

BlackSTAR adds parameters for named-sequence insertion, input scheduling, and
NUMA policy. Official STAR is not expected to recognize those parameters.

The project does not promise that log timestamps, absolute paths, performance
counters, thread scheduling, or command-line provenance comments are
byte-identical.

## Conventional Genome Indexes

BlackSTAR loads existing conventional STAR indexes whose `versionGenome`
matches the supported format. Conventional full indexes produced by BlackSTAR
use the established file set plus an optional `blackstar.complete.tsv`
integrity manifest. Official STAR ignores that additional file.

The qualified full-index boundary includes:

- `Genome`, `SA`, and `SAindex`;
- chromosome names, starts, and lengths;
- genome parameters;
- splice-junction metadata; and
- gene, transcript, and exon sidecars when annotations are supplied.

Official STAR 2.7.11b successfully aligned against a full BlackSTAR index in
release qualification. BlackSTAR does not promise future official STAR
versions will retain that behavior.

## Named-Sequence Packages

`genomeInsert Full` writes a self-contained conventional index.

Overlay and Delta are BlackSTAR-specific package formats. They:

- retain their own inserted FASTA and optional GTF;
- refer to a content-identified base index;
- require `--genomeLoad NoSharedMemory`;
- validate strict manifests and payload identities; and
- reject unsupported development-format versions.

Official STAR cannot load Overlay or Delta packages. Delta's virtual-SA
no-junction path does not support two-pass mapping; use Full mode when two-pass
mapping is required.

## Alignment Outputs

Compatibility tests compare applicable timing-independent metrics, splice
junctions, gene counts, and canonicalized BAM records. Byte identity is
required where file order and metadata are specified to be deterministic.

BlackSTAR does not promise byte-identical BAM container bytes when compression,
headers, record ordering, timestamps, or absolute command paths can vary. It
does require equivalent canonical records for a test that claims BAM
compatibility.

Changes caused by explicit BlackSTAR-only references are expected. In a
base-versus-Delta test, noninserted biological output must remain equivalent
outside mappings and counts attributable to requested added references.

## Resource and Runtime Behavior

Wall time, peak memory, CPU placement, NUMA allocation, I/O scheduling, and
thread utilization are not compatibility surfaces. They are measured
operational properties.

Full-index acceleration may use materially more RAM than official STAR.
Consumers must apply a host-memory gate rather than infer capacity from
official STAR behavior.

## Platforms

The stable release target is x86-64 Linux with an OpenMP runtime. Source paths
for macOS and other architectures are inherited but not release-qualified.
Compilation alone does not establish support.

## Regression Standard

A BlackSTAR regression is established when a documented promise fails under a
reproducible test and the failure is not present in the pinned official STAR
control, or when a BlackSTAR-only feature violates its documented contract.

An inherited upstream defect can still be fixed by BlackSTAR, but it is tracked
as upstream-origin behavior rather than presented as a BlackSTAR regression.
