# Console 2D on the SGX engine: hardened, tested, and slower than software

2026-11-05, one OpenFrame (Z520, 800x480). Kernel 6.18.60 + hardened patch 05 +
`FRAMEBUFFER_CONSOLE_LEGACY_ACCELERATION=y`, built CI-style from a clean tree.
Patch: `05-gma500_console_copyarea-hardened.patch`. It applies after the patches in
`patches/6.18/`, but is not built: CI only uses that directory. Kept for anything that
really has to move pixels already in the framebuffer, such as a future UI.

## Verdict
The engine and the hardened driver are reliable. But scrolling real text is slower
with the engine than with fbcon's software redraw, so it has no benefit on the console.

## Speed (best of 3)
| Text | Engine scroll | Software redraw |
|---|---|---|
| Near-identical lines | 914 us/line | 526 |
| Prose (GPL-3) | 3491 | 2164 |
| `ls -l` | 2948 | 1436 |
| Random, every cell changes | 3748 | 3012 |

Per operation, measured through fbops under the console lock:
| Op | Engine | Software |
|---|---|---|
| Fill 16x16 | 8us | 1us |
| Fill 800x16 | 52us | 35us |
| Fill 800x480 | 1338us | 945us |
| Copy 16x16 | 8us | 35us |
| Copy 400x16 | 53us | 884us |
| Copy 800x464 (scroll one row) | 2733us | 48423us |

The engine beats a CPU *copy*, which reads back from write-combined memory. Software
redraw never reads back, and skips unchanged cells. The CPU spins while waiting for
the engine, so there is no CPU saving either. The engine would be useful for a client
that really has to move pixels already in the framebuffer.

## Reliability
- Negative control: test reports FAIL on one altered pixel (needed fixing; the first
  version passed because later ops painted over the error).
- Strict: 2500 ops, whole screen compared after every op, 0 mismatches.
- Soak, 15 min: 86000 pixel-checked ops in 40 runs, 40 PASS, with scrolling, kernel log
  output, VT switching, blank/unblank, region scroll, insert/delete and a KMS client
  taking the display. No stalls, faults or warnings. Max op 7.6ms.
- Earlier soak (10 FAIL of 68, each 2 or 4 rows differing): attributed to the test, not
  the engine. One failure was reproduced with pixel logging: 8x2px, console grey, on the
  bottom text row at x=0, consistent with the cursor being drawn while the test had
  dropped the console lock. After re-reading the screen on re-taking the lock: 0 FAIL of
  40. The 10 original failures weren't logged at pixel level or rerun individually, so
  the cursor explanation is inferred for them.
- Deliberate engine fault (REQ_BASE into unmapped memory, signature identical to the M0
  stall: BIF_INT=0x4004, busy, FIFO not empty): one 188ms stall, one log line, software
  from then on, 2000 ops pixel-correct, normal reboot works.
- Real oops: printed to the console text buffer (read via /dev/vcs1, not the pixels);
  engine skipped by `oops_in_progress`; system survived.
- `accel_2d=0`: no engine blits. Tested via insmod, not the kernel command line.

## Not tested
- OpenFrame 2 or any second unit; one unit only.
- Suspend/resume: `CONFIG_SUSPEND` is off, so the guard in `gma_power_suspend` can't run.
- A real Xorg or other full KMS desktop (a minimal dumb-buffer client was used).

## Bugs fixed against the committed patch 05
Jiffies-based timeouts that may never expire with interrupts off; idle timeouts ignored;
no latch after a stall (1s per console op, reboot hang); engine used during oops; engine
offered on every gma500 chip; untested 8/16bpp paths; zero-size ops; fills on the engine.
Also: f60a706's claim that console scroll runs on the GPU was false on the shipped config.

Tools in `tools/`: `fbstress.c` (pixel test, wedge, bench), `kmsclient.c`, `vtswitch.c`, `soak.sh`. Paths in `soak.sh` assume they are built under `/home/of`.
