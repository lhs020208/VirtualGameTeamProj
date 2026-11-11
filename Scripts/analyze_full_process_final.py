"""Final full-run analysis; reads original evidence only and requires finished run."""
from pathlib import Path
import json,csv,datetime,collections
import numpy as np
P=Path('Docs/ProcessPlant_20261110_LowPoly/FullProcess/20261110_182948')
s=json.loads((P/'status.json').read_text());assert 'finished_utc' in s
change=json.loads((P/'protocol_change_start_mixing.json').read_text());start=json.loads((P/'mix_applied_start.json').read_text());cohort=set(json.loads((P/'cohort_stage_4_ids.json').read_text()))
def read(name):return {int(r['id']):r for r in csv.DictReader((P/name).open(encoding='utf-8-sig'))}
a=read(change['baseline_snapshot']);b=read(s['latest']['snapshot']);assert len(a)==len(b)==20000 and set(a)==set(b)
ids=sorted(cohort);xyz=lambda rows:np.array([[float(rows[i][k]) for k in ['x_m','y_m','z_m']] for i in ids]);x,y=xyz(a),xyz(b);dist=np.linalg.norm(y-x,axis=1)
def variance(rows):
 cells={}
 for i in ids:
  r=rows[i];q=[float(r[k]) for k in ['x_m','y_m','z_m']];idx=tuple(np.clip(((np.array(q)-[2.6225,-.2265,.941])/[.755/8,.453/4,.369/4]).astype(int),[0,0,0],[7,3,3]));v=cells.setdefault(idx,[0,0]);v[int(r['species'])]+=1
 return sum(sum(v)*(v[0]/sum(v)-.5)**2 for v in cells.values())/len(ids)
final_class=json.loads((P/'classified'/(Path(s['latest']['snapshot']).stem+'.json')).read_text());assert final_class['snapshot']==s['latest']['snapshot']
logs=list(csv.DictReader(open(s['run_log'],encoding='utf8')));mixlog=[r for r in logs if float(r['sim_s'])>=start['applied_sim_s']];assert mixlog
end=s['latest']['sim_s'];result={'finished_utc':s['finished_utc'],'whole_process_passed':s.get('completed',False),'error':s.get('error'),'total_particle_count':len(b),'species_counts':dict(collections.Counter(int(r['species']) for r in b.values())),'final_regions':final_class['counts'],'stages':s['stages'],'protocol_changes':[{k:v for k,v in c.items() if k not in ['residual','residuals']} for c in s['protocol_changes']],'mix':{'baseline_snapshot':change['baseline_snapshot'],'final_snapshot':s['latest']['snapshot'],'baseline_counts':change['mixer_species_counts'],'final_counts':s['latest']['zones']['MX'],'cohort_count':len(ids),'cohort_final_in_mixer':int(((y[:,0]>2.6225)&(y[:,0]<3.3775)&(abs(y[:,1])<.2265)&(y[:,2]>.941)&(y[:,2]<1.31)).sum()),'actual_start_sim_s':start['applied_sim_s'],'end_sim_s':end,'observed_seconds_after_applied':end-start['applied_sim_s'],'minimum_12s_completed':end-start['applied_sim_s']>=12,'nominal_rotations_at_15RPM':(end-start['applied_sim_s'])/4,'all_recorded_RPM_15':all(float(r['mixer_RPM'])==15 for r in mixlog),'all_recorded_RH_closed':all(r['RH_applied_open']=='0' for r in mixlog),'all_particle_variance_before':change['baseline_composition_variance'],'all_particle_variance_after':s['latest']['composition_variance'],'same_cohort_variance_before':variance(a),'same_cohort_variance_after':variance(b),'cohort_endpoint_displacement_mean_m':float(dist.mean()),'cohort_endpoint_displacement_median_m':float(np.median(dist)),'cohort_endpoint_displacement_p95_m':float(np.quantile(dist,.95)),'cohort_endpoint_displacement_max_m':float(dist.max()),'cohort_moved_over_diameter_count':int((dist>.004).sum()),'note':'Endpoint displacement is not travelled distance. 8x4x4 variance is coarse and was already low before rotation; not a validated drug-quality measure.'},'timing':{'total_wall_s':s['total_wall_s'],'level_load_wall_s':s['level_load_wall_s'],'PIE_ready_wall_s':s.get('PIE_ready_wall_s'),'last_runtime':logs[-1]}}
previous=x.copy();sampled_path=np.zeros(len(ids));maximum_offset=np.zeros(len(ids))
for file in sorted(P.glob('*_mix_15RPM.csv')):
 current=xyz(read(file.name));sampled_path+=np.linalg.norm(current-previous,axis=1);maximum_offset=np.maximum(maximum_offset,np.linalg.norm(current-x,axis=1));previous=current
result['mix']['sampled_path_mean_m']=float(sampled_path.mean())
result['mix']['sampled_path_median_m']=float(np.median(sampled_path))
result['mix']['ever_observed_over_diameter_from_start_count']=int((maximum_offset>.004).sum())
result['mix']['maximum_offset_median_m']=float(np.median(maximum_offset))
result['mix']['trajectory_note']='Sampled path length is a lower bound; maximum offset includes intermediate snapshots so returning to the starting location does not hide observed motion.'
(P/'FINAL_ANALYSIS.json').write_text(json.dumps(result,indent=2),encoding='utf8')
print(json.dumps({k:v for k,v in result.items() if k not in ['stages','protocol_changes','timing']},indent=2))
