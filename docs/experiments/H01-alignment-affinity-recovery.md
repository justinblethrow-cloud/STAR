# H01: Alignment Affinity Recovery

## Status

- State: accepted in `2.7.11b-blackstar.2`
- Parent commit: `821457378fa38bfb23b061b8f11ee0a09431dda7`
- Implementation commits: `64cd86aa90a5827b6f39ea202448f1833597a8f1` and
  `496ee8a5687c21520b91603b6e04141fd16a4fa0`
- Opened: 2026-07-23
- Decided: 2026-07-23
- Cumulative qualification: Q01 passed 2026-07-24

## Failure Mode

STAR links OpenMP for index construction but uses pthreads for alignment.
With `OMP_PROC_BIND=close` and `OMP_PLACES=cores`, libgomp can bind the initial
process thread to one OpenMP place before `main()`. STAR then creates all
mapping pthreads from that thread, so every worker inherits the same two-CPU
SMT affinity mask even when `--runThreadN 96` is requested.

The exact qualified release processed 2,000,000 public read pairs in 223.54
seconds at 185% CPU under this environment. Affinity inspection confirmed that
all 96 workers shared one physical core and its SMT sibling. Unsetting the
binding variables restored the full 96-CPU allocation and reduced wall time to
about 43 seconds without changing STAR source.

## Implementation

When alignment uses more than one thread and OpenMP binding was explicitly
requested, H01 queries the complete OpenMP place list and restores the spawning
thread to the union of those CPUs before STAR creates pthread workers. The
workers then inherit the full intended allocation. Failure to restore affinity
is fatal rather than silently running a severely degraded job.

The ordinary path does not initialize or query the OpenMP runtime when binding
is absent or explicitly false. Single-thread alignment also remains unchanged.

## Correctness Contract

- Bound and unbound runs preserve timing-independent final metrics, junctions,
  and gene counts.
- A child pthread must inherit the recovered union affinity mask.
- `OMP_PROC_BIND=FALSE` and an absent binding environment take the fast path.
- Single-thread alignment does not broaden affinity.
- Focused sanitizer tests cover affinity, packed arrays, suffix comparison,
  transcript initialization, parameters, junctions, and SHA-256 handling.

## Results

The matched bound comparison used the same index, reads, compute allocation, 96
requested threads, and environment:

| Binary | Wall time | Mean CPU use | Peak RSS |
| --- | ---: | ---: | ---: |
| Qualified release | 223.54 s | 185% | 40,380,416 KiB |
| H01 candidate | 43.00 s | 731% | 40,364,032 KiB |

H01 reduced wall time by 80.76%, a 5.20-fold recovery of the pathological
binding failure mode. The recovery log recorded 48 OpenMP places and 96 CPUs.

Five randomized unbound pairs tested ordinary-path noninferiority:

| Metric | Release | H01 candidate |
| --- | ---: | ---: |
| Median wall time | 43.49 s | 43.48 s |
| Wall-time CV | 0.286% | 0.463% |
| Median RSS increase | - | 0% |

The median paired improvement was -0.0687% and its bootstrap 95% interval was
-0.3684% to +0.8266%, inside the predeclared 2% no-regression margin. All five
unbound comparisons, the bound comparison, and the thread-mode smoke
comparison passed their exact-output or timing-independent correctness gates
(7/7 total).

## Decision

- Outcome: accepted as a Labs hardening candidate
- Release effect: included in `blackstar.2` after Q01 satisfied the cumulative
  technical gate
- Reason: it corrects a severe, reproducible environment-dependent failure
  while passing ordinary-path noninferiority, resource, sanitizer, and
  correctness gates.
- Follow-up: completed by the cumulative Q01 safety and performance gates and
  promoted in `blackstar.2`.

## Plain-Language Takeaway

An OpenMP setting could quietly confine STAR's many alignment workers to one
physical core. H01 restores the CPU allocation before those workers start. It
recovers the lost performance when the setting is present and adds no
measurable cost when it is absent.
