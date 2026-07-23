# BlackSTAR Benchmark Harness

These scripts collect reproducible performance evidence; they do not replace
correctness tests.

## Public A00 Corpus

`public_alignment_fixture.tsv` pins an ENCODE GM12878 paired-end RNA-seq run.
Prepare and optionally subset it with:

```bash
SUBSET_READS=100000 \
  extras/benchmarks/preparePublicAlignmentFixture.sh /local/path/a00-fixture
```

Run the five-minute quiescence gate before performance measurements:

```bash
python3 extras/benchmarks/quietSystemGate.py \
  --path /local/path \
  --output /evidence/quiet-system.tsv
```

Run one instrumented mode:

```bash
THREADS=16 PERF_MODE=stat extras/benchmarks/runAlignmentA00.sh \
  mapping-only source/STAR /local/index \
  /local/fixture/R1.fastq.gz /local/fixture/R2.fastq.gz /evidence/run-01
```

Alignment runners reject inherited `OMP_PROC_BIND` or `OMP_PLACES` settings.
OpenMP runtimes can bind the initial process thread before `main()`, causing
STAR's pthread mapping workers to inherit one place. Keep these variables
unset for ordinary release/candidate comparisons. The
`ALLOW_OMP_THREAD_BINDING=1` override exists only for deliberate affinity
recovery tests and must be recorded with their evidence.

Use `summarizeAlignmentA00.py` for compact timing tables and
`compareAlignmentRuns.py` for timing-independent log, junction, count, and
optional canonical BAM comparisons.

When a fix targets a reproduced failure mode rather than ordinary-path speed,
evaluate an existing five-pair series against a predeclared no-regression
margin:

```bash
extras/benchmarks/evaluateAlignmentNoninferiority.py \
  /local/evidence/H01/pairs.tsv \
  --margin-percent 2 --output /local/evidence/H01/noninferiority.json
```

For an experiment decision, use the order-balanced pair driver. It records the
schedule before execution, enforces the quiet-system gate, compares every pair,
and evaluates the wall-time, RSS, replicate, and correctness gates:

```bash
extras/benchmarks/runAlignmentPairs.py \
  --baseline-bin /local/bin/STAR-release \
  --candidate-bin /local/bin/STAR-candidate \
  --genome-dir /local/index \
  --read1 /local/reads/R1.fastq --read2 /local/reads/R2.fastq \
  --warmup-read1 /local/reads/subset-R1.fastq.gz \
  --warmup-read2 /local/reads/subset-R2.fastq.gz \
  --threads 96 --pairs 3 --output /local/evidence/A01
```

The driver hashes measured FASTQs before the quiet gate and can run both
binaries on a small untimed warmup pair. This creates a documented warm-cache,
new-process benchmark. Cold-cache end-to-end timing is a separate mode and
must explicitly evict cache state between runs.

## Local Slurm Launch Isolation

For node-local benchmarks, stage the binary, index, reads, scripts, and output
directory on the compute node's local filesystem. Set Slurm's working
directory to that local path and start with a clean environment so shell
startup, the shared working directory, or inherited affinity variables cannot
silently enter the measurement:

```bash
srun --nodes=1 --ntasks=1 --cpus-per-task=96 --chdir=/tmp \
  env -i PATH=/usr/bin:/bin HOME=/home/user TMPDIR=/tmp \
  /bin/bash /tmp/blackstar-benchmark/run-local.sh
```

Record the resulting environment and CPU mask in every run. A shared mount
error before STAR starts is a failed launch, not a slow benchmark, and must not
enter timing summaries.

Large raw evidence belongs outside Git under
`benchmarks/labs/<experiment>/<UTC timestamp>/`. Copy only anonymous,
reviewed summaries into the architecture evidence ledger.
