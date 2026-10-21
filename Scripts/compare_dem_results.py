"""Compare two completed DEM result bundles using identical measurement criteria."""
import argparse
import csv
import json
import math
from pathlib import Path

def load_json(path):
    def bad(value):
        raise ValueError(f"nonfinite JSON value: {value}")
    result = json.loads(path.read_text(encoding="utf-8-sig"), parse_constant=bad)
    def finite(value):
        if isinstance(value, float) and not math.isfinite(value):
            raise ValueError("nonfinite JSON number")
        if isinstance(value, dict):
            for item in value.values():
                finite(item)
        elif isinstance(value, list):
            for item in value:
                finite(item)
    finite(result)
    return result

def load_run(directory):
    root = Path(directory)
    marker = load_json(root / "complete.json")
    if marker != {"schema": 1, "complete": True}:
        raise ValueError("incomplete or unsupported result bundle")
    meta, summary = load_json(root / "metadata.json"), load_json(root / "summary.json")
    if meta["schema"] != 1 or summary["schema"] != 1 or meta["metric_definition"] != "dem_results_v1":
        raise ValueError("unsupported results schema")
    for name in ("timeseries.csv", "particles.csv", "events.csv", "settings.dem", "run.json"):
        if not (root / name).is_file():
            raise ValueError(f"missing result file: {name}")
    if not summary["tracking_enabled"]:
        raise ValueError("tracking disabled: passage/discharge comparison unavailable")
    if summary["physical_time_s"] != meta["physical_time_s"]:
        raise ValueError("metadata/summary clocks differ")
    with (root / "timeseries.csv").open(encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows or float(rows[-1]["time_s"]) != summary["physical_time_s"]:
        raise ValueError("missing final time series sample")
    last = -1.0
    for row in rows:
        t = float(row["time_s"])
        if not math.isfinite(t) or t <= last:
            raise ValueError("invalid time series clock")
        last = t
    for field in ("gross_exit_kg", "net_exit_kg", "unique_pass_kg", "no_outflow_s"):
        if float(rows[-1][field]) != summary[field]:
            raise ValueError(f"summary/series mismatch: {field}")
    if abs(summary["tracking_balance_kg"]) > 1e-10 or abs(summary["supply_balance_kg"]) > 1e-10:
        raise ValueError("mass balance exceeds 1e-10 kg")
    return meta, summary

def compare(a, b):
    ma, sa = load_run(a)
    mb, sb = load_run(b)
    for key in ("metric_definition", "code_version", "measurement"):
        if ma[key] != mb[key]:
            raise ValueError(f"incompatible comparison: {key}")
    for key in ("sample_s", "slow_speed_m_s", "long_stall_s"):
        if ma["criteria"][key] != mb["criteria"][key]:
            raise ValueError(f"incompatible criterion: {key}")
    if ma["physical_time_s"] != mb["physical_time_s"]:
        raise ValueError("compare at exactly the same physical observation time")
    metrics = {}
    for key in ("gross_exit_kg", "net_exit_kg", "unique_pass_kg", "passages", "retained_feed_tube_kg", "collector_kg", "outside_kg", "pending_kg", "future_kg", "inserted_kg", "no_outflow_s", "slow_n", "long_slow_n", "residence_mean_s", "residence_median_s", "residence_count"):
        x, y = sa[key], sb[key]
        if x is not None and (not isinstance(x, (float, int)) or not math.isfinite(x)):
            raise ValueError(f"invalid numeric metric {key}")
        if y is not None and (not isinstance(y, (float, int)) or not math.isfinite(y)):
            raise ValueError(f"invalid numeric metric {key}")
        metrics[key] = {"a": x, "b": y, "b_minus_a": None if x is None or y is None else y-x}
    return {"schema": 1, "a": str(Path(a).resolve()), "b": str(Path(b).resolve()),
            "physical_time_s": ma["physical_time_s"], "criteria": ma["criteria"], "metrics": metrics,
            "states": {"a": {k: sa[k] for k in ("flow_state", "supply_state")}, "b": {k: sb[k] for k in ("flow_state", "supply_state")}},
            "conditions": {"settings_differ": ma["settings"] != mb["settings"], "geometry_differ": ma["geometry"] != mb["geometry"], "dt_differ": ma["dt_s"] != mb["dt_s"], "seed_differ": ma["seed"] != mb["seed"]},
            "notes": ["Residence statistics use completed passages only; unfinished particles are not zero-duration samples.",
                      "Gross outlet crossings may count recrossings; unique_pass_kg counts each completed particle once.",
                      "Code version is recorded, but source_revision is unavailable; this is not proof of identical binaries."]}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("a"); p.add_argument("b"); p.add_argument("--output", type=Path)
    args = p.parse_args()
    try:
        result = json.dumps(compare(args.a, args.b), ensure_ascii=False, indent=2, allow_nan=False) + "\n"
        if args.output:
            with args.output.open("x", encoding="utf-8") as f:
                f.write(result)
        else:
            print(result, end="")
    except (ValueError, KeyError, OSError, TypeError) as e:
        p.exit(2, f"Comparison rejected: {e}\n")
if __name__ == "__main__":
    main()
