# BlackSTAR Genome Insert

`--runMode genomeInsert` adds new named FASTA records to an existing STAR index. It is intended for small additions such as transgenes, spike-ins, controls, plasmids, or decoy sequences. The base index is read but never modified.

## Full Index

The default `Full` mode writes a conventional, self-contained STAR index:

```bash
STAR \
  --runMode genomeInsert \
  --runThreadN 16 \
  --genomeDir /references/prebuilt-star \
  --genomeFastaFiles customer-transgenes.fa \
  --sjdbGTFfile customer-transgenes.gtf \
  --genomeInsertOutDir /references/customer-star
```

The output contains `Genome`, `SA`, `SAindex`, chromosome metadata, `genomeParameters.txt`, and applicable annotation sidecars. It does not require the base index or input FASTA/GTF after publication.

## Overlay And Delta

`Overlay` writes a small reusable package. At alignment startup, BlackSTAR loads the base index and computes the inserted suffixes in memory:

```bash
STAR \
  --runMode genomeInsert \
  --runThreadN 16 \
  --genomeDir /references/prebuilt-star \
  --genomeFastaFiles customer-transgenes.fa \
  --sjdbGTFfile customer-transgenes.gtf \
  --genomeInsertOutMode Overlay \
  --genomeInsertOutDir /references/customer-overlay
```

`Delta` additionally caches the suffix insertion plan in `genomeInsertDelta.bin`. This avoids repeating suffix search and sorting on each alignment startup:

```bash
STAR \
  --runMode genomeInsert \
  --runThreadN 16 \
  --genomeDir /references/prebuilt-star \
  --genomeFastaFiles customer-transgenes.fa \
  --sjdbGTFfile customer-transgenes.gtf \
  --genomeInsertOutMode Delta \
  --genomeInsertOutDir /references/customer-delta

STAR \
  --runThreadN 16 \
  --genomeDir /references/customer-delta \
  --readFilesIn reads_R1.fastq.gz reads_R2.fastq.gz \
  --readFilesCommand zcat
```

Overlay and Delta packages contain their own `inserted.fa` and optional `inserted.gtf`. They do not depend on the original inserted FASTA/GTF paths. They do depend on the base index at the absolute path recorded in `genomeInsertOverlay.tsv`.

Overlay loading requires `--genomeLoad NoSharedMemory`, which is the default. The base index is identified after it is loaded; changing any required base index file or annotation sidecar invalidates the package.

## Annotation Contract

The optional GTF must contain exons only on sequences supplied by the same `--genomeFastaFiles` invocation. Existing base annotations are retained automatically.

BlackSTAR rejects:

- duplicate inserted reference names;
- inserted names already present in the base index;
- inserted `gene_id` values already present in `geneInfo.tab`;
- inserted `transcript_id` values already present in `transcriptInfo.tab`;
- one inserted `transcript_id` reused across incompatible chromosome, strand, or gene contexts;
- malformed exon records or exons on base references.

An inserted sequence may be left unannotated by omitting it from the GTF. If no inserted sequences need annotations, omit `--sjdbGTFfile`.

Unless explicitly supplied, `sjdbOverhang` is inherited from the base index. If the base already contains a splice-junction database, an explicit value must match it; this is the same compatibility rule STAR applies during normal base-index loading.

## Artifact Integrity

Genome-insert output is built in a sibling staging directory and published with one atomic rename. An existing nonempty destination is never overwritten. A normal error after staging removes the temporary directory; an abrupt process or host failure can leave an unpublished `.tmp.<pid>.<attempt>` directory, which is never accepted as the final artifact.

Every published artifact contains `blackstar.complete.tsv`, written last, with the expected file set, sizes, and SHA-256 tree identities. Overlay and Delta packages validate this manifest before loading. Full indexes retain the completion manifest for deployment checks, but normal STAR loading does not hash a full index on every alignment run.

The v2 overlay manifest uses an exact schema and packaged relative paths. Delta v2 has a fixed little-endian header, explicit dimensions, base/FASTA/GTF identities, payload bounds, payload identity, and sorted-record validation. Legacy development Overlay/Delta artifacts are intentionally rejected and must be rebuilt.

## Reproducibility And Validation

Repeated Full or Delta creation from the same base and inserted inputs is byte-reproducible across thread counts. The exact invocation remains in `Log.out`; `genomeParameters.txt` uses a canonical input-only provenance comment so output path and thread count do not perturb the index.

A Full output should be compared with `genomeGenerate` over the combined base and inserted FASTA/GTF inputs. `Genome`, `SAindex`, chromosome metadata, and annotation sidecars should match byte-for-byte. `SA` can differ where equal suffixes admit multiple valid orderings, so mapping records, junction output, and gene counts must also be compared.

Fast acceptance tests:

```bash
make -C source STAR
extras/tests/scripts/testBlackstarSha256.sh
extras/tests/scripts/testGenomeInsert.sh
extras/tests/scripts/testGenomeInsertHardening.sh
extras/tests/scripts/testSAindexParallel.sh
```

The benchmark helper requires inserted GTF annotations by default and exits nonzero on failed or incomplete index validation:

```bash
BASE_GENOME_DIR=/references/prebuilt-star \
BASE_FASTA_FILES="/references/genome.fa /references/spikeins.fa" \
INSERT_FASTA_FILES=/work/customer-transgenes.fa \
INSERT_GTF_FILE=/work/customer-transgenes.gtf \
SJDB_GTF_FILE=/work/base-plus-customer.gtf \
SJDB_OVERHANG=93 \
THREADS=96 \
extras/tests/scripts/benchmarkGenomeInsert.sh
```

Set `REQUIRE_INSERT_GTF=0` only for an intentionally unannotated benchmark.

If `SA` differs, the helper records an inconclusive result and exits 2 because mapping-level equivalence has not been established. Set `ALLOW_UNVALIDATED_SA_DIFFERENCE=1` only to retain timing from such a run; that output is not complete release evidence until alignments, junctions, and counts are validated separately.

## Limitations

- Existing reference sequences and base annotations cannot be edited or replaced.
- `genomeInsert` writes a new artifact; it does not update an index in place.
- Overlay and Delta require the recorded base path and a content-identical base index.
- Overlay and Delta reconstruct the expanded index in memory at alignment startup.
- `--sjdbFileChrStartEnd` is not supported while creating a genome-insert artifact.
- Overlay and Delta loading do not support shared-memory genome modes.
- Two-pass alignment is not supported by the no-junction Delta virtual-SA path; use a Full artifact when two-pass mapping is required.
