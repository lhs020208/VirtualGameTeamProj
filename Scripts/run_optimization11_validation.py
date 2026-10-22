"""Opt11 inventory/contact contract verification; no runtime coarsening."""
from pathlib import Path
import argparse,csv,hashlib,json,math,subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 contract={'schema':1,'work':11,'runtime_enabled':False,'q_definition':'integer physical particle count represented by one computational sphere','length_scale':'cbrt(q)','density_scale':1,'mass_scale':'q','inertia_scale':'q*length_scale^2','linear_stiffness_scale':'length_scale','linear_damping_scale':'length_scale^2','friction_scale':1,'supported_candidate':'dry nonrolling nonadhesive linear sphere pair/wall, one common q','q1':'identity for all existing validated contact laws; no new drag support','forbidden_q_gt_1':['Hertz','rolling','adhesion','drag','clearance/clogging','size segregation','impact/deformation','mixed weights/remainder rounding','nonspherical shape substitution'],'fixed_gravity_bulk_similarity':False,'physical_psd':'explicit number-count bins; one density per group; no implicit mean diameter','real_material_validation':False,'mass_consistency_relative':1e-12,'test_relative':1e-10,'test_absolute':1e-24}
 (out/'contract.json').write_text(json.dumps(contract,indent=2),encoding='utf-8')
 exe=ROOT/'Intermediate/PowderCore/Release/representative_tests.exe'
 run=subprocess.run([str(exe),str(out/'inventories')],capture_output=True,text=True,check=True);(out/'tests.log').write_text(run.stdout,encoding='utf-8')
 rows={q:list(csv.DictReader((out/'inventories'/f'inventory_q{q}.csv').open())) for q in [1,2,8]}
 summaries=[]
 for q,data in rows.items():
  groups={}
  for row,base in zip(data,rows[1]):
   assert row['group']==base['group'] and row['bin']==base['bin']
   assert int(row['physical_count'])==int(row['computational_count'])*q
   assert math.isclose(float(row['total_mass_kg']),float(row['computational_particle_mass_kg'])*int(row['computational_count']),rel_tol=1e-12,abs_tol=0)
   assert row['total_mass_kg']==base['total_mass_kg']
   group=groups.setdefault(row['group'],{'physical_count':0,'computational_count':0,'mass_kg':0})
   group['physical_count']+=int(row['physical_count']);group['computational_count']+=int(row['computational_count']);group['mass_kg']+=float(row['total_mass_kg'])
  summaries.append({'q':q,'groups':groups})
 sources={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in ['Physics/PowderCore/include/powder/representative.hpp','Physics/PowderCore/tests/representative_tests.cpp','Physics/PowderCore/CMakeLists.txt','Scripts/run_optimization11_validation.py']}
 (out/'report.json').write_text(json.dumps({'passed':True,'contract':contract,'inventories':summaries,'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'source_sha256':sources},indent=2),encoding='utf-8')
 print('PASS representative contract + independent exported inventory audit')
if __name__=='__main__':main()
