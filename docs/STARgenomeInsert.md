# STAR Genome Insert

`--runMode genomeInsert` persistently adds named FASTA records to an existing STAR genome index and writes a complete updated index to a new directory. It is intended for small sequence additions such as spike-ins, controls, plasmid/transgene records, or other named sequences that should become normal reference contigs for subsequent alignment runs.

## Usage

```bash
STAR \
  --runMode genomeInsert \
  --runThreadN 16 \
  --genomeDir /path/to/prebuilt/index \
  --genomeFastaFiles customer_transgenes.fa \
  --genomeInsertOutDir /path/to/updated/index
```

`--genomeDir` is read as the base index and is not modified. `--genomeInsertOutDir` receives a complete updated index containing `Genome`, `SA`, `SAindex`, chromosome metadata, `genomeParameters.txt`, and supported annotation sidecar files copied from the base index.

Multiple inserted FASTA files can be supplied with `--genomeFastaFiles`:

```bash
STAR \
  --runMode genomeInsert \
  --genomeDir /path/to/prebuilt/index \
  --genomeFastaFiles gfp.fa gst.fa \
  --genomeInsertOutDir /path/to/updated/index
```

## Validation Expectations

An updated index should be validated against a full rebuild from the base FASTA files plus the inserted FASTA files. The following outputs are expected to match byte-for-byte when the base build inputs and parameters are identical:

- `Genome`
- `SAindex`
- `chrName.txt`
- `chrStart.txt`
- `chrLength.txt`
- `chrNameLength.txt`
- copied annotation sidecar files such as `sjdbInfo.txt`, `sjdbList.out.tab`, `exonInfo.tab`, `geneInfo.tab`, and `transcriptInfo.tab`

`SA` may not be byte-identical to a full rebuild because suffixes with equal ordering keys can be stored in a different but equivalent order. Validate `SA` behavior by aligning reads that target the inserted sequences and comparing alignment records against a full rebuild.

## Limitations

- `genomeInsert` adds new named FASTA sequences. It does not edit existing chromosome sequence.
- `genomeInsert` does not update in place; it writes a complete new index to `--genomeInsertOutDir`.
- New GTF annotations for inserted sequences are not currently accepted in this mode. Existing annotation sidecar files from the base index are preserved.
- On-the-fly junction insertion parameters are not supported with `--runMode genomeInsert`.

## Test and Benchmark Helpers

Fast regression test:

```bash
make -C source STAR
extras/tests/scripts/testGenomeInsert.sh
```

Benchmark harness:

```bash
BASE_GENOME_DIR=/path/to/prebuilt/index \
BASE_FASTA_FILES="/path/reference.fa /path/ercc.fa" \
INSERT_FASTA_FILES="/path/gfp.fa /path/gst.fa" \
SJDB_GTF_FILE=/path/annotation.gtf \
SJDB_OVERHANG=93 \
THREADS=96 \
extras/tests/scripts/benchmarkGenomeInsert.sh
```

The benchmark harness records command lines, `time -v` metrics, iostat snapshots when available, and byte-level validation for files expected to match a full rebuild.
