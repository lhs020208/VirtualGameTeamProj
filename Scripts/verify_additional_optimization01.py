from pathlib import Path
import json,importlib.util,struct,shutil,uuid,sys
sys.dont_write_bytecode=True
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('baseline_runner',root/'Scripts/run_additional_optimization_baseline.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
base=root/'Docs/DEM/additional_optimization01';manifest=json.loads((base/'baseline.json').read_text())
results={}
scratch=root/'Saved'/('AdditionalOpt01Selfcheck_'+uuid.uuid4().hex);scratch.mkdir()
for label,seconds in [('integrated12',12),('integrated30',30)]:
 archived=root/'Docs/DEM/emitter_integration06'/label
 case=scratch/label;case.mkdir()
 for name in ['result.json','final.bin','trace.csv']:shutil.copy2(archived/name,case/name)
 shutil.copy2(base/manifest['initial_file'],case/'initial.bin')
 results[label]=module.audit(case,manifest,seconds)
first=next(iter(manifest['protected_sha256']))
assert first in module.differences({first:'0'*64})
source=scratch/'integrated12'
scratch=scratch/'negative';scratch.mkdir()
for name in ['result.json','initial.bin','final.bin','trace.csv']:shutil.copy2(source/name,scratch/name)
def rejects():
 try:module.audit(scratch,manifest,12)
 except RuntimeError:return True
 return False
raw=(scratch/'final.bin').read_bytes();modified=bytearray(raw);struct.pack_into('<f',modified,0,float('nan'));(scratch/'final.bin').write_bytes(modified);assert rejects()
modified=bytearray(raw);struct.pack_into('<f',modified,12,1);(scratch/'final.bin').write_bytes(modified);assert rejects()
(scratch/'final.bin').write_bytes(raw);(scratch/'initial.bin').write_bytes(b'changed');assert rejects()
(base/'verification.json').write_text(json.dumps(dict(success=True,historical_runs_validated=results,negative_checks=['changed protected hash','NaN final state','changed group','changed initial state'],new_measurement=False),indent=2))
print('PASS: historical 12s/30s invariants and four rejection checks; no new performance measurement')
