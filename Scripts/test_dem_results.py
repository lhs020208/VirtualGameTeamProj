"""Independent JSON/CSV parsing and analytic result comparison checks."""
import csv
import json
import math
import shutil
import sys
import tempfile
from pathlib import Path
from compare_dem_results import compare, load_run

def rejects(f):
    try:
        f()
    except (ValueError, FileNotFoundError):
        return
    raise AssertionError("expected comparison rejection")

root = Path(sys.argv[1])
meta, summary = load_run(root)
mass = math.pi / 6 * .1 ** 3 * 1000
assert abs(summary["gross_exit_kg"] - mass) < 1e-12
assert summary["residence_samples_s"] == [1]
with (root / "events.csv").open(newline="") as f:
    events = list(csv.DictReader(f))
assert [float(e["time_s"]) for e in events] == [1, 2]
assert events[1]["residence_s"] == "1"
with (root / "timeseries.csv").open(newline="") as f:
    rows = list(csv.DictReader(f))
assert float(rows[0]["time_s"]) == 0 and rows[0]["interval_exit_kg_s"] == ""
assert abs(float(rows[-1]["interval_exit_kg_s"]) - mass/3) < 1e-12
with (root / "particles.csv").open(newline="") as f:
    particles = list(csv.DictReader(f))
assert particles[0]["z_m"] == "0" and particles[0]["birth_s"] == "0"
assert all(v["b_minus_a"] == 0 for v in compare(root, root)["metrics"].values())
different_run = compare(root, root / "condition_b")
assert abs(different_run["metrics"]["gross_exit_kg"]["b_minus_a"] - mass) < 1e-12
assert different_run["metrics"]["residence_mean_s"]["b_minus_a"] == 0
assert different_run["conditions"]["settings_differ"]
with tempfile.TemporaryDirectory() as td:
    other = Path(td) / "condition_b"
    shutil.copytree(root, other)
    def put(name, value):
        (other / name).write_text(json.dumps(value), encoding="utf-8")
    changed = dict(meta);changed["settings"] += "# test alternative input\n";changed["seed"] += 1
    put("metadata.json", changed)
    assert compare(root, other)["conditions"]["settings_differ"]
    assert compare(root, other)["conditions"]["seed_differ"]
    changed["criteria"] = dict(meta["criteria"], long_stall_s=2)
    put("metadata.json", changed);rejects(lambda: compare(root, other))
    put("metadata.json", meta)
    different = dict(summary, physical_time_s=4)
    put("summary.json", different);rejects(lambda: compare(root, other))
    put("summary.json", summary)
    changed=dict(meta);changed["measurement"]={"enabled":False};put("metadata.json",changed)
    rejects(lambda: compare(root, other))
    put("metadata.json",meta);(other/"complete.json").unlink();rejects(lambda: compare(root,other))
print("Independent result serialization and comparison checks passed")
