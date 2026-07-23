# BlackSTAR Labs Experiments

Labs experiments are isolated from the qualified release. Every experiment has
a stable ID, a frozen parent commit, a falsifiable hypothesis, correctness and
resource gates, raw evidence locations, diagrams, and an explicit decision.

## Required Lifecycle

1. Register the experiment in [ROADMAP.md](ROADMAP.md).
2. Copy [EXPERIMENT_TEMPLATE.md](EXPERIMENT_TEMPLATE.md) to `ID-short-name.md`.
3. Freeze a public baseline and record toolchain, host topology, storage, input
   identities, command line, and thread policy.
4. Add focused unit and differential tests before performance claims.
5. Run randomized A/B or B/A pairs only after the quiet-system gate passes.
6. Compare the experiment with both its immediate parent and the qualified
   release.
7. Accept only if correctness passes and the practical performance gate passes.
8. Update the architecture atlas, claim ledger, status history, and decision.

## Practical Performance Gate

An experiment must improve median end-to-end wall time by at least 2 percent,
or improve its targeted stage by at least 15 percent with no end-to-end
regression. Alignment experiments may not increase peak RSS by more than
5 percent, and index experiments may not increase peak RAM or temporary storage
by more than 10 percent without an explicit exception.

For expected effects of at least 5 percent, run at least three randomized pairs
with coefficient of variation no greater than 3 percent. For smaller effects,
run at least five pairs and require the paired bootstrap 95 percent confidence
interval to exclude zero.

Hardening fixes that recover a specific failure mode may use a two-part gate:
a matched positive control that reproduces and corrects the failure, plus at
least five ordinary-path pairs whose bootstrap interval remains inside a
predeclared no-regression margin. This noninferiority gate cannot be used to
claim an ordinary-path speedup.

## Active Records

- [A00](A00-public-alignment-baseline.md): corrected public alignment baseline.
- [A01](A01-touched-window-bins.md): excluded experiment; historical effect
  size invalidated by inherited CPU affinity.
- [A02](A02-input-dispatch-diagnostics.md): accepted adaptive input-chunk Labs
  candidate; not included in `blackstar.1`.
- [H01](H01-alignment-affinity-recovery.md): accepted Labs hardening candidate
  for inherited OpenMP binding; not included in `blackstar.1`.

## Quiet-System Gate

The benchmark host must remain suitable for five continuous minutes:

- CPU idle at least 90 percent.
- I/O wait no greater than 2 percent.
- Target storage utilization below 10 percent.
- No competing benchmark or bulk-transfer process.

The gate is evidence, not a lock. If contention starts during a run, invalidate
the pair and retain the telemetry.

## Evidence Location

Large raw evidence is stored outside Git under:

`benchmarks/labs/<experiment-id>/<UTC timestamp>/`

Small, anonymous summaries needed by public claims are copied into
`docs/architecture/evidence/`. Never place customer identifiers or absolute
internal paths in tracked public artifacts.
