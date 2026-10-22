"""Physical q=1 aperture controls; coarse scopes remain forbidden."""
import argparse,csv,json,subprocess,hashlib
from pathlib import Path

def rows(path):
 with path.open(encoding='utf-8-sig',newline='') as f:return list(csv.DictReader(f))
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args();root=Path(__file__).resolve().parents[1];out=Path(a.output);out.mkdir(parents=True,exist_ok=False)
 contract={'q':1,'opening_m':.012,'bridge_diameters_m':[.01,.01],'psd_diameters_m':[.01,.014],'density_kg_m3':2500,'duration_s':.3,'dt_s':5e-7,'scope':'symmetric two-grain bridge and two isolated size streams, not a calibrated arching probability','coarse_scopes_allowed':False,'maximum_overlap_radius_ratio':.05}
 (out/'contract.json').write_text(json.dumps(contract,indent=2),encoding='utf-8');result={};exe=root/'Intermediate/PowderCore/Release/powder_aperture13.exe'
 for case in ['bridge','psd']:
  dest=out/case;proc=subprocess.run([str(exe),case,str(dest)],capture_output=True,text=True,timeout=120);(out/(case+'.log')).write_text(proc.stdout+proc.stderr,encoding='utf-8')
  if proc.returncode:raise RuntimeError(proc.stdout+proc.stderr)
  summary=rows(dest/'summary.csv')[0];series=rows(dest/'series.csv');geometry=rows(dest/'geometry.csv')
  assert len(series)==21 and float(summary['max_overlap_radius_ratio'])<=.05 and int(summary['contacts'])>0
  if case=='bridge':assert int(summary['passed_count'])==0 and float(series[-1]['z1'])>0 and float(series[-1]['z2'])>0
  else:assert int(summary['passed_count'])==1 and float(summary['passed_mass_a_kg'])>0 and float(summary['passed_mass_b_kg'])==0
  for row in geometry:
   assert (row['scope_rejected']=='1')==(int(row['q'])>1)
   if float(row['physical_diameter_m'])==.01:assert (row['computed_fits']=='1')==(int(row['q'])==1)
   else:assert row['computed_fits']=='0'
  result[case]={'summary':summary,'final_energy_j':float(series[-1]['energy_j']),'geometry':geometry,'checks_passed':True};print(proc.stdout.strip(),flush=True)
 report={'controls_passed':True,'cases':result,'maximum_allowed_ratio_in_clearance_or_segregation':1,'multi_grain_bulk_arching_validated':False,'sha256':{str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [exe,root/'Physics/PowderCore/app/representative_aperture.cpp',Path(__file__)]}}
 (out/'acceptance.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
if __name__=='__main__':main()
