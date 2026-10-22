import argparse,csv,json,subprocess,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output);out.mkdir(parents=True,exist_ok=False)
contract={'ratios':[1,8],'physical_count':256,'physical_diameter_m':.01,'density_kg_m3':2500,'duration_s':.25,'dt_s':.000002,'sample_count':21,'observation_samples':32,'grid':[2,2,1],'minimum_samples_per_occupied_cell':4,'weight':'mass','observation':'32 fixed initial eight-physical-particle cohort COMs; not independent physical grains','mixing_initial_speed_m_s':1.2,'mixing_group_b_y_offset_m':.0175,'adoption':False,'comparison_limits':{'packing_final_com_relative':.1,'discharge_max_mass_fraction_error':.1,'mixing_max_absolute_index_error':.1},'scope':'small dry linear fixtures only; no mm/kg throughput claim'}
(out/'contract.json').write_text(json.dumps(contract,indent=2),encoding='utf-8')
exe=root/'Intermediate/PowderCore/Release/powder_rep12.exe'
def rows(path):
 with path.open(encoding='utf-8-sig',newline='') as f:return list(csv.DictReader(f))
results={}
for case in ['packing','discharge','mixing']:
 data={}
 for q in [1,8]:
  dest=out/f'{case}_q{q}';subprocess.run([str(exe),case,str(q),str(dest)],check=True)
  series=rows(dest/'series.csv');meta=rows(dest/'metadata.csv')[0];summary=rows(dest/'summary.csv')[0]
  assert len(series)==21 and int(meta['physical_count'])==256 and int(meta['computational_count'])==256//q
  total=float(meta['total_mass_kg'])
  for i,row in enumerate(series):
   assert int(row['step'])==i*6250 and int(row['sample_count'])==32
   assert abs(float(row['mass_a_kg'])-total/2)<1e-12 and abs(float(row['mass_b_kg'])-total/2)<1e-12
  if case!='discharge':assert int(summary['contacts'])>0
  else:assert int(summary['arrived_physical_count'])==256
  data[q]=(series,summary,total)
 ref,coarse=data[1][0],data[8][0]
 if case=='packing':error=abs(float(ref[-1]['com_z'])-float(coarse[-1]['com_z']))/abs(float(ref[-1]['com_z']));limit=.1
 elif case=='discharge':error=max(abs(float(r['arrived_mass_kg'])-float(c['arrived_mass_kg']))/data[1][2] for r,c in zip(ref,coarse));limit=.1
 else:
  assert all(r['mixing_valid']=='1' for r in ref+coarse),'invalid mixing sampling'
  assert max(float(r['mixing_index']) for r in ref)-min(float(r['mixing_index']) for r in ref)>.01,'reference mixing must change'
  error=max(abs(float(r['mixing_index'])-float(c['mixing_index'])) for r,c in zip(ref,coarse));limit=.1
 results[case]={'error':error,'limit':limit,'within_screening_limit':error<=limit,'speedup':float(data[1][1]['wall_s'])/float(data[8][1]['wall_s']),'reference_rate':float(data[1][1]['rate']),'coarse_rate':float(data[8][1]['rate'])}
report={'execution_and_mass_checks_passed':True,'adopt_for_process':False,'comparisons':results,'source_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'Physics/PowderCore/include/powder/representative_runtime.hpp',root/'Physics/PowderCore/app/representative_run.cpp',Path(__file__)]}}
(out/'acceptance.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
