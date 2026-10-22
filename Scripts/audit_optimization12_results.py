from pathlib import Path
import csv,json,hashlib
import sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]) if len(sys.argv)>1 else root/'Docs/DEM/optimization12/final'
a=json.loads((out/'acceptance.json').read_text())
for name,digest in a['source_sha256'].items():assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest
extra={}
for case in ['packing','discharge','mixing']:
 for q in [1,8]:
  folder=out/f'{case}_q{q}'
  with (folder/'summary.csv').open() as f:s=next(csv.DictReader(f))
  extra[f'{case}_q{q}']={'max_overlap_radius_ratio':float(s['max_overlap_m'])/(.005 if q==1 else .01)}
  with (folder/'cells.csv').open() as f:cells=list(csv.DictReader(f))
  for step in range(0,125001,6250):
   sample=[c for c in cells if int(c['step'])==step];assert len(sample)==4
   assert sum(int(c['samples_a'])+int(c['samples_b']) for c in sample)==32
   assert abs(sum(float(c['mass_a_kg'])+float(c['mass_b_kg']) for c in sample)-.3351032163829113)<1e-12
(out/'observation_audit.json').write_text(json.dumps({'matched_cells_sample_counts_mass_passed':True,'overlap_diagnostics':extra,'warning':'large linear overlap is permitted by the solver; execution success is not material fidelity'},indent=2),encoding='utf-8')
print(json.dumps(extra,indent=2))
