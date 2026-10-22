import argparse,csv,json,subprocess,hashlib,statistics
from pathlib import Path
from optimization13_analysis import finite,distance,accepted,maximum_prefix

def rows(path):
    with path.open(encoding='utf-8-sig',newline='') as f:return list(csv.DictReader(f))
def write(path,obj):path.write_text(json.dumps(obj,indent=2,allow_nan=False),encoding='utf-8')

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',required=True);args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];out=Path(args.output);out.mkdir(parents=True,exist_ok=False)
    ratios=[1,2,4,8];fractions=[1,2,3];seeds=[11,22,33]
    limits={'mixing2':.1,'mixing4':.1,'com_relative':.1};numeric={'mixing2':.02,'mixing4':.02,'com_relative':.02}
    contract={'ratios':ratios,'a_quarters':fractions,'seeds':seeds,'half_dt_seed':11,'physical_count':128,'physical_diameter_m':.01,'density_kg_m3':2500,'dt_s':5e-7,'duration_s':.12,'physical_kn':10000,'physical_cn':.2,'physical_kt':2500,'physical_ct':.05,'mu':.1,'speed_m_s':.5,'cohorts':16,'samples':21,'grids':[[2,1,1],[4,1,1]],'bounds':[[-.22,-.5,-1],[.22,.5,.5]],'minimum_cohorts':[4,2],'weight':'equal physical mass per fixed eight-grain cohort COM; not independent grains','comparison_limits':limits,'numerical_limits':numeric,'maximum_overlap_radius_ratio':.05,'initial_conditions':'shared deterministic cohort jitter +/-1mm; seed parity swaps internal x/y; q1/2/4/8 use 8/4/2/1 nonoverlapping spheres centered on each cohort','aperture_m':.012,'aperture_physical_diameters_m':[.01,.014],'aperture_duration_s':.3,'forbidden_scopes':['clearance_or_clogging','size_segregation'],'field_validation':False,'speed_timings':'single serial runs; not kg-scale acceptance'}
    write(out/'contract.json',contract)
    exe=root/'Intermediate/PowderCore/Release/powder_rep13.exe';data={};failures={};progress=[]
    def run(case,q,fraction,seed,divisor):
        key=f'{case}_q{q}_a{fraction}_s{seed}_dt{divisor}';dest=out/key
        print('RUN '+key,flush=True)
        try:
            proc=subprocess.run([str(exe),case,str(q),str(fraction),str(seed),str(divisor),str(dest)],capture_output=True,text=True,timeout=180)
            (out/(key+'.log')).write_text(proc.stdout+proc.stderr,encoding='utf-8')
            if proc.returncode:raise RuntimeError(f'exit {proc.returncode}: {proc.stderr.strip()}')
            series=rows(dest/'series.csv');meta=rows(dest/'metadata.csv')[0];summary=rows(dest/'summary.csv')[0]
            if len(series)!=21:raise ValueError('sample count')
            total=finite(meta['total_mass_kg'])
            for i,row in enumerate(series):
                if int(row['sample'])!=i:raise ValueError('sample order')
                if abs(finite(row['time_s'])-i*finite(meta['duration_s'])/20)>1e-12:raise ValueError('sample time')
                expected=total*fraction/4 if case=='mixing' else total
                if abs(finite(row['mass_a_kg'])-expected)>1e-12 or abs(finite(row['mass_a_kg'])+finite(row['mass_b_kg'])-total)>1e-12:raise ValueError('group mass conservation')
            if case=='mixing':
                if int(meta['physical_count'])!=128 or int(meta['computed_count'])!=128//q:raise ValueError('inventory count')
                cells=rows(dest/'cells.csv')
                for i in range(21):
                    for grid in [2,4]:
                        subset=[c for c in cells if int(c['sample'])==i and int(c['grid_x'])==grid]
                        if len(subset)!=grid:raise ValueError('cell count')
                        # An outside-region observation is an invalid result, never silently filled in.
                        if sum(int(c['samples_a'])+int(c['samples_b']) for c in subset)!=16:raise ValueError('observation outside region')
                        if abs(sum(finite(c['mass_a_kg'])+finite(c['mass_b_kg']) for c in subset)-total)>1e-12:raise ValueError('observation mass')
                if int(summary['contacts'])<=0:raise ValueError('no contacts')
            result={'series':series,'metadata':meta,'summary':summary};data[key]=result;progress.append({'run':key,'completed':True});write(out/'progress.json',progress);print(proc.stdout.strip(),flush=True);return result
        except (subprocess.TimeoutExpired,RuntimeError,ValueError,OSError) as e:
            failures[key]=str(e);progress.append({'run':key,'completed':False,'error':str(e)});write(out/'progress.json',progress);print('FAILED '+str(e),flush=True);return None
    for fraction in fractions:
        for seed in seeds:
            for q in ratios:run('mixing',q,fraction,seed,1)
    for fraction in fractions:
        for q in ratios:run('mixing',q,fraction,11,2)
    small=run('aperture_small',1,2,11,1);large=run('aperture_large',1,2,11,1)
    def get(q,fraction,seed,divisor=1):return data.get(f'mixing_q{q}_a{fraction}_s{seed}_dt{divisor}')
    baseline_ok=True;convergence={};variability={};paired={};grid_sensitivity={}
    for fraction in fractions:
        for q in ratios:
            a,b=get(q,fraction,11),get(q,fraction,11,2);key=f'q{q}_a{fraction}'
            if a and b:
                err=distance(a['series'],b['series']);ok=accepted(err,numeric);convergence[key]={'errors':err,'within_numerical_limit':ok}
            else:ok=False;convergence[key]={'within_numerical_limit':False,'missing':True}
            if q==1:baseline_ok=baseline_ok and ok
        refs=[get(1,fraction,seed) for seed in seeds]
        if all(refs):
            spans=[];stds=[];grid_delta=[]
            for i in range(21):
                values=[finite(r['series'][i]['uniformity2']) for r in refs if r['series'][i]['valid2']=='1']
                if len(values)==3:spans.append(max(values)-min(values));stds.append(statistics.stdev(values))
                for r in refs:
                    row=r['series'][i]
                    if row['valid2']=='1' and row['valid4']=='1':grid_delta.append(abs(finite(row['uniformity2'])-finite(row['uniformity4'])))
            variability[str(fraction)]={'max_reference_seed_range':max(spans,default=None),'max_reference_sample_std':max(stds,default=None),'common_valid_times':len(spans),'confidence_interval':False}
            grid_sensitivity[str(fraction)]={'max_reference_grid_index_difference':max(grid_delta,default=None),'note':'resolution sensitivity, not statistical measurement error'}
            for r in refs:
                values=[finite(row['uniformity2']) for row in r['series'] if row['valid2']=='1']
                baseline_ok=baseline_ok and len(values)==21 and max(values)-min(values)>.01 and finite(r['summary']['max_overlap_radius_ratio'])<=.05
        else:baseline_ok=False
    for q in ratios[1:]:
        cases=[];ok=baseline_ok
        for fraction in fractions:
            ok=ok and convergence[f'q{q}_a{fraction}']['within_numerical_limit']
            for seed in seeds:
                a,b=get(1,fraction,seed),get(q,fraction,seed)
                if not a or not b:ok=False;continue
                errors=distance(a['series'],b['series']);overlap=finite(b['summary']['max_overlap_radius_ratio']);case_ok=accepted(errors,limits) and overlap<=.05
                cases.append({'a_quarters':fraction,'seed':seed,'errors':errors,'overlap_ratio':overlap,'within_limits':case_ok,'speedup':finite(a['summary']['wall_s'])/finite(b['summary']['wall_s'])});ok=ok and case_ok
        paired[str(q)]={'eligible_in_test_domain':bool(ok and len(cases)==9),'cases':cases}
    aperture_ok=bool(small and large and small['summary']['passed']=='1' and large['summary']['passed']=='0' and int(large['summary']['contacts'])>0 and finite(large['summary']['max_overlap_radius_ratio'])<=.05)
    geometric=[]
    for q in ratios:
        geometric.append({'q':q,'small_diameter_m':.01*q**(1/3),'large_diameter_m':.014*q**(1/3),'small_fits':.01*q**(1/3)<.012,'large_fits':.014*q**(1/3)<.012,'runtime_coarse_allowed':q==1})
    report={'all_runs_completed':len(data)==50 and not failures,'failures':failures,'baseline_numerical_sampling_and_overlap_passed':baseline_ok,'aperture_reference_passed':aperture_ok,'convergence':convergence,'reference_initial_condition_variability':variability,'reference_grid_sensitivity':grid_sensitivity,'paired_ratio_results':paired,'maximum_validated_test_ratio':maximum_prefix(paired) if baseline_ok else 1,'maximum_process_ratio':1,'process_coarse_adoption':False,'forbidden_scopes':contract['forbidden_scopes'],'geometry':geometric,'limitations':['3 deterministic initial conditions are not a confidence interval','cohort COM hides within-cohort dispersion','single-grain aperture is not a multi-grain arching or segregation validation','no real material calibration or kg-scale throughput claim']}
    paths=[root/'Physics/PowderCore/app/representative_sensitivity.cpp',root/'Physics/PowderCore/include/powder/representative_applicability.hpp',root/'Scripts/optimization13_analysis.py',Path(__file__),exe]
    report['sha256']={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    write(out/'acceptance.json',report);print(json.dumps({k:report[k] for k in ['all_runs_completed','baseline_numerical_sampling_and_overlap_passed','aperture_reference_passed','maximum_validated_test_ratio','maximum_process_ratio']},indent=2),flush=True)
    if failures or not aperture_ok:raise SystemExit(1)
if __name__=='__main__':main()
