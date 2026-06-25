#!/usr/bin/env python3
"""
Run one-factor-at-a-time experiment sweeps for stochsuite FS configs.

All runs use hardened PRNG (HWRNG / --harden-prng). 14 FS runs total; other
knobs at baseline unless that factor is swept:
  - 2 HWRNG backends (prng-select Taus88 / JKISS)
  - 3 branch predictors (LocalBP, TournamentBP, TAGE)
  - 3 prefetch configs (Ruby none, Ruby RubyPrefetcher, classic L1D Stride)
  - 3 RDRAND/RDSEED latency pairs
  - 3 iteration counts (1000, 10000, 50000)

Runs that exceed --timeout (default 30 minutes) are queued and retried
after all other runs finish.

Collects simInsts, simTicks, and aggregated stoch-branch predictions /
mispredictions (plus mispredict rate) from each run's stats.txt.

Example:
  export STOCHSUITE_HOME=/path/to/stochsuite
  ./configs/run_stochsuite_experiments.py \\
      --gem5-binary build/ALL/gem5.opt \\
      --benchmark pi --iters 1000 \\
      --output-dir experiment_results
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import subprocess
import sys
from dataclasses import dataclass, fields
from pathlib import Path
from typing import Any, Iterator, Optional

# ---------------------------------------------------------------------------
# Experiment grid (edit here to change the sweep)
# ---------------------------------------------------------------------------

HWRNG_CONFIGS = [
    {"rng": "HWRNG", "hwrng": "Taus88"},
    {"rng": "HWRNG", "hwrng": "JKISS"},
]

BRANCH_PREDICTORS = ["LocalBP", "TournamentBP", "TAGE"]

# Ruby build: none + RubyPrefetcher; third uses classic L1D StridePrefetcher.
PREFETCH_CONFIGS = [
    {"mem_system": "ruby", "prefetcher_type": "none"},
    {"mem_system": "ruby", "prefetcher_type": "RubyPrefetcher"},
    {
        "mem_system": "classic",
        "prefetcher_type": "none",
        "l1d_hwp_type": "StridePrefetcher",
    },
]

# (hwrng_lat, rdseed_lat) cycles for O3 functional units
HWRNG_LATENCIES = [(1, 1), (10, 10), (50, 50)]

ITER_SWEEP = [1000, 10000, 50000]

# Defaults for dimensions not being swept in a given run (match config scripts).
BASELINE = {
    "rng": "HWRNG",
    "hwrng": "Taus88",
    "bp_type": "LocalBP",
    "mem_system": "ruby",
    "prefetcher_type": "none",
    "l1d_hwp_type": "",
    "hwrng_lat": 1,
    "rdseed_lat": 1,
}

RUNS_PER_MODE = (
    len(HWRNG_CONFIGS)
    + len(BRANCH_PREDICTORS)
    + len(PREFETCH_CONFIGS)
    + len(HWRNG_LATENCIES)
    + len(ITER_SWEEP)
)

FS_CONFIG = "configs/x86-fs-stochsuite-workloads.py"
DEFAULT_TIMEOUT_S = 30 * 60

STAT_LINE_RE = re.compile(
    r"^(\S+)\s+([\d.eE+-]+)(?:\s+.*)?$"
)


@dataclass(frozen=True)
class Experiment:
    mode: str
    exp_id: str
    sweep: str
    rng: str
    hwrng: str
    bp_type: str
    mem_system: str
    prefetcher_type: str
    l1d_hwp_type: str
    hwrng_lat: int
    rdseed_lat: int
    iters: Optional[int] = None

    def to_row(self, **extra: Any) -> dict[str, Any]:
        row = {f.name: getattr(self, f.name) for f in fields(self)}
        row.update(extra)
        return row


def _make_experiment(
    mode: str,
    exp_id: str,
    sweep: str,
    overrides: dict[str, Any],
    *,
    iters: Optional[int] = None,
) -> Experiment:
    params = {**BASELINE, **overrides}
    return Experiment(
        mode=mode,
        exp_id=exp_id,
        sweep=sweep,
        rng=params["rng"],
        hwrng=params["hwrng"],
        bp_type=params["bp_type"],
        mem_system=params["mem_system"],
        prefetcher_type=params["prefetcher_type"],
        l1d_hwp_type=params["l1d_hwp_type"],
        hwrng_lat=params["hwrng_lat"],
        rdseed_lat=params["rdseed_lat"],
        iters=iters,
    )


def generate_experiments(modes: list[str]) -> Iterator[Experiment]:
    """One factor varied per run; RUNS_PER_MODE experiments (FS only)."""
    for mode in modes:
        if mode != "fs":
            raise ValueError(f"only FS mode is supported, got {mode!r}")
        for hw in HWRNG_CONFIGS:
            yield _make_experiment(
                mode,
                f"{mode}_hw_rng_{hw['hwrng']}",
                "rng_hw",
                hw,
            )
        for bp in BRANCH_PREDICTORS:
            yield _make_experiment(
                mode,
                f"{mode}_bp_{bp}",
                "bp",
                {"bp_type": bp},
            )
        for i, pf in enumerate(PREFETCH_CONFIGS):
            names = ["ruby_none", "ruby_stream", "classic_stride"]
            yield _make_experiment(
                mode,
                f"{mode}_prefetch_{names[i]}",
                "prefetch",
                pf,
            )
        for hwrng_lat, rdseed_lat in HWRNG_LATENCIES:
            yield _make_experiment(
                mode,
                f"{mode}_lat_{hwrng_lat}_{rdseed_lat}",
                "latency",
                {"hwrng_lat": hwrng_lat, "rdseed_lat": rdseed_lat},
            )
        for iters in ITER_SWEEP:
            yield _make_experiment(
                mode,
                f"{mode}_iters_{iters}",
                "iters",
                {},
                iters=iters,
            )


def parse_stats(stats_path: Path) -> dict[str, Any]:
    """Parse gem5 stats.txt for global and stoch-branch metrics."""
    result: dict[str, Any] = {
        "simInsts": None,
        "simTicks": None,
        "stoch_predictions": 0,
        "stoch_mispredictions": 0,
        "stoch_mispredict_rate": None,
    }
    per_symbol: dict[str, dict[str, float]] = {}

    if not stats_path.is_file():
        return result

    with stats_path.open() as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            m = STAT_LINE_RE.match(line)
            if not m:
                continue
            name, value_s = m.group(1), m.group(2)
            try:
                value = float(value_s)
            except ValueError:
                continue

            if name == "simInsts":
                result["simInsts"] = value
            elif name == "simTicks":
                result["simTicks"] = value
            elif "stoch_branch_monitor" in name:
                if name.endswith(".predictions"):
                    sym = name.rsplit(".", 2)[-2]
                    per_symbol.setdefault(sym, {})["predictions"] = value
                    result["stoch_predictions"] = (
                        (result["stoch_predictions"] or 0) + value
                    )
                elif name.endswith(".mispredictions"):
                    sym = name.rsplit(".", 2)[-2]
                    per_symbol.setdefault(sym, {})["mispredictions"] = value
                    result["stoch_mispredictions"] = (
                        (result["stoch_mispredictions"] or 0) + value
                    )

    preds = result["stoch_predictions"] or 0
    misps = result["stoch_mispredictions"] or 0
    if preds > 0:
        result["stoch_mispredict_rate"] = misps / preds

    for sym, counts in per_symbol.items():
        p = counts.get("predictions", 0)
        m = counts.get("mispredictions", 0)
        result[f"{sym}.predictions"] = p
        result[f"{sym}.mispredictions"] = m
        result[f"{sym}.mispredict_rate"] = (m / p) if p > 0 else None

    return result


def resolve_stochsuite_home(
    disk_image: Optional[Path],
    gem5_root: Path,
) -> Path:
    """Resolve STOCHSUITE_HOME for gem5 (apps/, disk_images/)."""
    candidates: list[Path] = []
    env_home = os.environ.get("STOCHSUITE_HOME")
    if env_home:
        candidates.append(Path(env_home).resolve())
    if disk_image is not None:
        # .../stochsuite/disk_images/<image>.img
        candidates.append(disk_image.resolve().parent.parent)
    candidates.append((gem5_root.parent / "stochsuite").resolve())

    seen: set[Path] = set()
    for path in candidates:
        if path in seen:
            continue
        seen.add(path)
        if (path / "apps").is_dir():
            return path

    tried = ", ".join(str(p) for p in candidates)
    raise ValueError(
        f"could not find stochsuite tree with apps/ (tried: {tried})"
    )


def stochsuite_stripped_binary(stoch_home: Path, benchmark: str) -> Path:
    if benchmark == "sgd":
        return stoch_home / "apps" / "sgd" / "sgd.stripped"
    return stoch_home / "apps" / f"{benchmark}.stripped"


def validate_stochsuite_setup(stoch_home: Path, benchmark: str) -> Optional[str]:
    """Return an error message if the workload binary is missing."""
    binary = stochsuite_stripped_binary(stoch_home, benchmark)
    if not binary.is_file():
        make_target = "gem5_fs" if benchmark == "sgd" else "gem5_fs or make"
        return (
            f"workload binary not found: {binary}\n"
            f"  cd {stoch_home}/apps && make {make_target}"
        )
    return None


def build_gem5_cmd(
    gem5_root: Path,
    gem5_binary: Path,
    exp: Experiment,
    *,
    benchmark: str,
    iters: int,
    seed: int,
    disk_image: Optional[Path],
    outdir: Path,
    extra_gem5_args: list[str],
) -> list[str]:
    config = FS_CONFIG
    cmd = [
        str(gem5_binary),
        *extra_gem5_args,
        "-d",
        str(outdir),
        str(gem5_root / config),
        "--benchmark",
        benchmark,
        "--iters",
        str(iters),
        "--seed",
        str(seed),
        "--bp-type",
        exp.bp_type,
    ]
    prng_select = exp.hwrng if exp.rng == "HWRNG" else exp.rng
    cmd.extend(["--prng-select", prng_select])
    if exp.rng == "HWRNG":
        cmd.append("--harden-prng")
    cmd.extend(
        [
            "--mem-system",
            exp.mem_system,
            "--hwrng-lat",
            str(exp.hwrng_lat),
            "--rdseed-lat",
            str(exp.rdseed_lat),
        ]
    )
    if exp.mem_system == "ruby":
        cmd.extend(["--prefetcher-type", exp.prefetcher_type])
    else:
        if exp.prefetcher_type != "none":
            cmd.extend(["--prefetcher-type", exp.prefetcher_type])
        if exp.l1d_hwp_type:
            cmd.extend(["--l1d-hwp-type", exp.l1d_hwp_type])
    if disk_image is None:
        raise ValueError("FS mode requires --disk-image or STOCHSUITE_HOME")
    cmd.extend(["--disk-image", str(disk_image)])
    return cmd


def run_experiment(
    gem5_root: Path,
    gem5_binary: Path,
    exp: Experiment,
    *,
    benchmark: str,
    default_iters: int,
    seed: int,
    disk_image: Optional[Path],
    output_dir: Path,
    extra_gem5_args: list[str],
    dry_run: bool,
    skip_existing: bool,
    timeout_s: Optional[int],
    stochsuite_home: Path,
    retry: bool = False,
) -> dict[str, Any]:
    run_iters = exp.iters if exp.iters is not None else default_iters
    run_dir = output_dir / exp.mode / exp.exp_id
    stats_path = run_dir / "stats.txt"
    row = exp.to_row(
        run_dir=str(run_dir),
        run_iters=run_iters,
        status="pending",
        returncode=None,
        simInsts=None,
        simTicks=None,
        stoch_predictions=None,
        stoch_mispredictions=None,
        stoch_mispredict_rate=None,
        retry=retry,
    )

    if skip_existing and stats_path.is_file() and stats_path.stat().st_size > 0:
        row["status"] = "skipped_existing"
        row.update(parse_stats(stats_path))
        return row

    run_dir.mkdir(parents=True, exist_ok=True)
    cmd = build_gem5_cmd(
        gem5_root,
        gem5_binary,
        exp,
        benchmark=benchmark,
        iters=run_iters,
        seed=seed,
        disk_image=disk_image,
        outdir=run_dir,
        extra_gem5_args=extra_gem5_args,
    )

    log_path = run_dir / "run.log"
    row["command"] = " ".join(cmd)

    if dry_run:
        row["status"] = "dry_run"
        return row

    run_env = os.environ.copy()
    run_env["STOCHSUITE_HOME"] = str(stochsuite_home)

    with log_path.open("w") as logf:
        try:
            proc = subprocess.run(
                cmd,
                cwd=gem5_root,
                env=run_env,
                stdout=logf,
                stderr=subprocess.STDOUT,
                timeout=timeout_s,
                check=False,
            )
        except subprocess.TimeoutExpired:
            row["status"] = "timeout"
            return row

    row["returncode"] = proc.returncode
    if proc.returncode != 0:
        row["status"] = "gem5_failed"
        return row

    if not stats_path.is_file():
        alt = run_dir / "m5out" / "stats.txt"
        if alt.is_file():
            stats_path = alt

    if not stats_path.is_file():
        row["status"] = "no_stats"
        return row

    row["status"] = "ok"
    row.update(parse_stats(stats_path))
    return row


def write_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    if not rows:
        return
    all_keys: list[str] = []
    seen = set()
    for row in rows:
        for k in row:
            if k not in seen:
                seen.add(k)
                all_keys.append(k)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=all_keys, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "One-factor-at-a-time stochsuite experiment runner (HWRNG only, "
            f"{RUNS_PER_MODE} runs per mode)."
        )
    )
    parser.add_argument(
        "--gem5-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="gem5 tree root (default: parent of configs/)",
    )
    parser.add_argument(
        "--gem5-binary",
        type=Path,
        default=None,
        help="gem5 binary (default: <gem5-root>/build/ALL/gem5.opt)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("experiment_results"),
        help="Directory for per-run outputs and CSV summaries",
    )
    parser.add_argument("--benchmark", default="pi")
    parser.add_argument("--iters", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=12312332)
    parser.add_argument(
        "--disk-image",
        type=Path,
        default=None,
        help="FS disk image (default: $STOCHSUITE_HOME/disk_images/stochsuite-base.img)",
    )
    parser.add_argument(
        "--modes",
        default="fs",
        help="Must be fs (SE mode is not supported by this runner)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print planned runs without executing gem5",
    )
    parser.add_argument(
        "--skip-existing",
        action="store_true",
        help="Skip runs whose stats.txt already exists and is non-empty",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=None,
        help="Run at most N experiments (after mode filter)",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=DEFAULT_TIMEOUT_S,
        help=(
            "Per-run timeout in seconds (default: 1800). Timed-out runs are "
            "retried after all other runs complete."
        ),
    )
    parser.add_argument(
        "--extra-gem5-arg",
        action="append",
        default=[],
        dest="extra_gem5_args",
        help="Extra argument(s) passed before the config script (e.g. --silent)",
    )
    args = parser.parse_args()

    gem5_root = args.gem5_root.resolve()
    gem5_binary = (
        args.gem5_binary.resolve()
        if args.gem5_binary
        else gem5_root / "build/ALL/gem5.opt"
    )
    if not gem5_binary.is_file():
        print(f"error: gem5 binary not found: {gem5_binary}", file=sys.stderr)
        return 1

    modes = [m.strip() for m in args.modes.split(",") if m.strip()]
    for m in modes:
        if m != "fs":
            print(f"error: only fs mode is supported, got {m!r}", file=sys.stderr)
            return 1

    disk_image = args.disk_image
    if disk_image is None:
        stoch_home = os.environ.get("STOCHSUITE_HOME")
        if stoch_home:
            disk_image = (
                Path(stoch_home) / "disk_images" / "stochsuite-base.img"
            )
        if disk_image is None or not disk_image.is_file():
            print(
                "error: FS mode needs --disk-image or "
                "STOCHSUITE_HOME/disk_images/stochsuite-base.img",
                file=sys.stderr,
            )
            return 1
    disk_image = disk_image.resolve()

    try:
        stochsuite_home = resolve_stochsuite_home(disk_image, gem5_root)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    setup_err = validate_stochsuite_setup(stochsuite_home, args.benchmark)
    if setup_err:
        print(f"error: {setup_err}", file=sys.stderr)
        return 1

    try:
        experiments = list(generate_experiments(modes))
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    if args.limit is not None:
        experiments = experiments[: args.limit]

    n = len(experiments)
    print(f"STOCHSUITE_HOME={stochsuite_home}")
    print(
        f"Planning {n} FS runs ({RUNS_PER_MODE} total, HWRNG baseline, "
        f"one factor at a time, iters sweep {ITER_SWEEP})"
    )
    if args.timeout:
        print(f"Per-run timeout: {args.timeout}s (timed-out runs retried at end)")

    output_dir = args.output_dir.resolve()
    rows: list[dict[str, Any]] = []
    deferred: list[Experiment] = []

    def _run_one(exp: Experiment, label: str, *, retry: bool) -> dict[str, Any]:
        run_iters = exp.iters if exp.iters is not None else args.iters
        print(f"{label} {exp.exp_id} sweep={exp.sweep} iters={run_iters}")
        return run_experiment(
            gem5_root,
            gem5_binary,
            exp,
            benchmark=args.benchmark,
            default_iters=args.iters,
            seed=args.seed,
            disk_image=disk_image,
            output_dir=output_dir,
            extra_gem5_args=args.extra_gem5_args,
            dry_run=args.dry_run,
            skip_existing=args.skip_existing,
            timeout_s=args.timeout,
            stochsuite_home=stochsuite_home,
            retry=retry,
        )

    for i, exp in enumerate(experiments, 1):
        row = _run_one(exp, f"[{i}/{n}]", retry=False)
        if row.get("status") == "timeout" and not args.dry_run:
            deferred.append(exp)
        rows.append(row)

    if deferred:
        print(
            f"\nRetrying {len(deferred)} timed-out run(s) after "
            f"{n - len(deferred)} completed run(s)"
        )
        for j, exp in enumerate(deferred, 1):
            row = _run_one(exp, f"[retry {j}/{len(deferred)}]", retry=True)
            rows.append(row)

    summary_path = output_dir / "results.csv"
    write_csv(summary_path, rows)
    print(f"Wrote {summary_path} ({len(rows)} rows)")

    ok = sum(1 for r in rows if r.get("status") == "ok")
    skipped = sum(1 for r in rows if r.get("status") == "skipped_existing")
    dry = sum(1 for r in rows if r.get("status") == "dry_run")
    failed = len(rows) - ok - skipped - dry
    print(f"Done: ok={ok} skipped={skipped} dry_run={dry} failed={failed}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
