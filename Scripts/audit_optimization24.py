"""Audit frozen full-target CSVs; short numerical agreement is not process acceptance."""
import argparse,csv,json,math,statistics
from pathlib import Path

def audit(base, final):
    def read(path, expected):
        with Path(path).open(encoding='utf-8') as stream: rows=list(csv.DictReader(stream))
        assert len(rows)==expected, 'incomplete windows'
        for r in rows:
            for k,v in r.items():
                if v: assert math.isfinite(float(v)), 'nonfinite '+k
        return rows
    b=read(base,18); f=read(final,33)
    lookup={(int(r['repeat']),int(r['window'])):r for r in b}
    assert len(lookup)==18
    seen=set()
    for r in f:
        key=(int(r['repeat']),int(r['window'])); assert key not in seen;seen.add(key)
        rep,w=key; assert 0<=rep<3 and 0<=w<11
        batch=1 if w<6 else 64
        assert int(r['batch_steps'])==batch and int(r['bodies'])==1591550
        assert float(r['dt_s'])==2e-8
        assert abs(float(r['time_s'])-(w+1 if w<6 else 6+64*(w-5))*2e-8)<1e-15
        compute=float(r['compute_s']);measure=float(r['measurement_snapshot_s'])
        assert compute>0 and measure>0
        assert math.isclose(float(r['inclusive_rate']),batch*2e-8/(compute+measure),rel_tol=1e-12)
        assert int(r['contacts'])==4733669 and float(r['overlap_m'])<1e-5
        for k in ['mass_a_kg','mass_b_kg']: assert abs(float(r[k])-5.0000017878208354)<5e-12
        if w<6:
            prior=lookup[key]
            assert int(r['momentum_checked'])==1 and float(r['momentum_residual'])<1e-10
            for k in ['contacts','wall_contacts']: assert r[k]==prior[k]
            for k in ['overlap_m','torque_z_nm','wall_work_j','mix_index']:
                assert abs(float(r[k])-float(prior[k]))<1e-10, k
        else: assert r['momentum_residual']=='' and r['momentum_checked']=='0'
    warm=[r for r in f if 0<int(r['window'])<6];old=[r for r in b if int(r['window'])>0]
    extended=[r for r in f if int(r['window'])>=6]
    def stats(rows,key):
        a=[float(r[key]) for r in rows];return {'min':min(a),'median':statistics.median(a),'max':max(a)}
    n=1591550; chunks=(n+127)//128
    return {'numerical_comparison_passed':True,'baseline_warm_rate':stats(old,'inclusive_rate'),'final_warm_rate':stats(warm,'inclusive_rate'),
        'matched_warm_median_speedup':statistics.median(float(r['inclusive_rate'])/float(lookup[(int(r['repeat']),int(r['window']))]['inclusive_rate']) for r in warm),
        'extended_batch_rate':stats(extended,'inclusive_rate'),'all_windows_rate':stats(f,'inclusive_rate'),
        'baseline_measurement_seconds':stats(old,'measurement_snapshot_s'),'final_measurement_seconds':stats(warm,'measurement_snapshot_s'),
        'core_device_buffer_bytes':int(f[0]['device_buffers_bytes']), 'observation_additional_bytes':n*4+(chunks+1)*9*32,
        'required_rate':.1,'throughput_gate':False,'end_to_end_completed':False,'accepted':False,
        'scope':'3 repeats, each 326 steps / 6.52 microseconds; profiling on; render/setup/final snapshot audit excluded; contact sets tested separately on small scenes; cold windows excluded only from warm comparison'}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('baseline',type=Path);p.add_argument('final',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
    result=audit(a.baseline,a.final);a.output.write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result,indent=2))
