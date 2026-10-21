"""Compare completed exports at identical physical sample steps; no interpolation.
An export can represent a paused unfinished experiment. Its completion marker
means all files were written, not that the requested mixing duration elapsed.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path

CONTRACT = ('schema', 'metric', 'assignment', 'formula', 'grid', 'lower_m', 'upper_m',
            'minimum_bodies_per_occupied_cell', 'dt_s', 'sample_steps')

def finite(value):
    x = float(value)
    if not math.isfinite(x):
        raise ValueError('nonfinite numeric value')
    return x

def load(folder):
    folder = Path(folder)
    marker = json.loads((folder/'complete.json').read_text(encoding='utf-8-sig'))
    if marker.get('complete') is not True or marker.get('schema') != 1:
        raise ValueError('incomplete export')
    meta = json.loads((folder/'mixing_metadata.json').read_text(encoding='utf-8-sig'))
    if meta.get('schema') != 1 or meta.get('metric') != 'mass_weighted_cell_uniformity_v1' or meta.get('assignment') != 'body_COM':
        raise ValueError('unsupported mixing metric/schema')
    for key in CONTRACT:
        if key not in meta: raise ValueError(f'missing contract {key}')
    grid=meta['grid']
    if len(grid)!=3 or any(type(n) is not int or not 1<=n<=8 for n in grid): raise ValueError('invalid grid')
    if len(meta['lower_m'])!=3 or len(meta['upper_m'])!=3 or any(finite(a)>=finite(b) for a,b in zip(meta['lower_m'],meta['upper_m'])): raise ValueError('invalid region')
    dt=finite(meta['dt_s'])
    if dt<=0 or type(meta['sample_steps']) is not int or meta['sample_steps']<1 or type(meta['minimum_bodies_per_occupied_cell']) is not int or meta['minimum_bodies_per_occupied_cell']<1: raise ValueError('invalid sampling')
    series={}
    with (folder/'mixing_timeseries.csv').open(encoding='utf-8-sig',newline='') as f:
        for row in csv.DictReader(f):
            step=int(row['step']);time=finite(row['time_s'])
            if step<0 or (series and step<=max(series)) or abs(time-step*dt)>1e-10: raise ValueError('invalid sample sequence/time')
            if row['valid'] not in ('0','1'): raise ValueError('invalid validity flag')
            valid=row['valid']=='1'
            if valid != (row['status']=='valid'): raise ValueError('inconsistent validity/status')
            value=finite(row['uniformity']) if valid else None
            if valid and not 0<=value<=1 or not valid and row['uniformity']: raise ValueError('invalid index')
            for key in ('mass_a_kg','mass_b_kg','outside_mass_a_kg','outside_mass_b_kg'):
                if finite(row[key])<0: raise ValueError('negative mass')
            for key in ('count_a','count_b','occupied','undersampled','outside'):
                if int(row[key])<0: raise ValueError('negative count')
            series[step]={'time_s':time,'valid':valid,'status':row['status'],'uniformity':value,'raw':row}
    if not series or next(iter(series))!=0: raise ValueError('missing initial state')
    cells={step:[] for step in series}
    with (folder/'mixing_cells.csv').open(encoding='utf-8-sig',newline='') as f:
        for row in csv.DictReader(f):
            step=int(row['step'])
            if step not in cells: raise ValueError('cell sample missing from timeseries')
            xyz=tuple(int(row[k]) for k in ('ix','iy','iz'))
            if any(not 0<=n<lim for n,lim in zip(xyz,grid)): raise ValueError('cell outside grid')
            masses=[finite(row[k]) for k in ('mass_a_kg','mass_b_kg')]
            counts=[int(row[k]) for k in ('count_a','count_b')]
            if any(x<0 for x in masses+counts) or abs(finite(row['time_s'])-series[step]['time_s'])>1e-10: raise ValueError('invalid cell sample')
            total=sum(masses)
            if total>0:
                if abs(finite(row['fraction_a'])-masses[0]/total)>1e-12: raise ValueError('incorrect cell fraction')
            elif row['fraction_a']: raise ValueError('empty cell fraction must be blank')
            cells[step].append((xyz,masses,counts))
    for step,values in cells.items():
        if len(values)!=math.prod(grid) or len({v[0] for v in values})!=len(values): raise ValueError('incomplete/duplicate spatial cells')
        raw=series[step]['raw'];mass=[finite(raw['mass_a_kg']),finite(raw['mass_b_kg'])]
        for g,suffix in enumerate(('a','b')):
            if abs(sum(v[1][g] for v in values)+finite(raw[f'outside_mass_{suffix}_kg'])-mass[g])>1e-10*max(1,mass[g]): raise ValueError('cell mass ledger mismatch')
        occupied=[v for v in values if sum(v[1])>0]
        sparse=sum(sum(v[2])<meta['minimum_bodies_per_occupied_cell'] for v in occupied)
        if len(occupied)!=int(raw['occupied']) or sparse!=int(raw['undersampled']): raise ValueError('sampling diagnostic mismatch')
        if sum(sum(v[2]) for v in values)+int(raw['outside'])!=int(raw['count_a'])+int(raw['count_b']): raise ValueError('body count ledger mismatch')
        expected='outside_region' if int(raw['outside']) else 'missing_group' if min(mass)<=0 else 'insufficient_occupied_cells' if len(occupied)<2 else 'undersampled_cells' if sparse else 'valid'
        if series[step]['status']!=expected: raise ValueError('incorrect validity reason')
        if expected=='valid':
            total=sum(mass);c=mass[0]/total
            variance=sum(sum(v[1])/total*(v[1][0]/sum(v[1])-c)**2 for v in occupied)
            calculated=max(0,min(1,1-variance/(c*(1-c))))
            if abs(calculated-series[step]['uniformity'])>1e-10: raise ValueError('index does not match spatial distribution')
    digest=hashlib.sha256((folder/'configuration.txt').read_bytes()).hexdigest()
    for step,values in cells.items(): series[step]['cells']={v[0]:(v[1],v[2]) for v in values}
    return meta,series,digest

def compare(a,b):
    ma,sa,ha=load(a);mb,sb,hb=load(b)
    different=[key for key in CONTRACT if ma[key]!=mb[key]]
    if different: raise ValueError('incompatible measurement contract: '+', '.join(different))
    common=sorted(sa.keys() & sb.keys())
    if not common: raise ValueError('no matching physical samples')
    rows=[];spatial=[]
    for step in common:
        x,y=sa[step],sb[step]
        rows.append({'step':step,'time_s':x['time_s'],'a_status':x['status'],'b_status':y['status'],
                     'a_uniformity':x['uniformity'],'b_uniformity':y['uniformity'],
                     'b_minus_a':y['uniformity']-x['uniformity'] if x['valid'] and y['valid'] else None})
        for xyz in sorted(x['cells']):
            am,an=x['cells'][xyz];bm,bn=y['cells'][xyz]
            ac=am[0]/sum(am) if sum(am)>0 else None;bc=bm[0]/sum(bm) if sum(bm)>0 else None
            spatial.append({'step':step,'time_s':x['time_s'],'ix':xyz[0],'iy':xyz[1],'iz':xyz[2],
                            'a_count':sum(an),'b_count':sum(bn),'a_mass_kg':sum(am),'b_mass_kg':sum(bm),
                            'a_fraction_a':ac,'b_fraction_a':bc,'fraction_b_minus_a':bc-ac if ac is not None and bc is not None else None})
    report={'schema':1,'metric':ma['metric'],'a':str(Path(a).resolve()),'b':str(Path(b).resolve()),
            'condition_differences':{k:{'a':ma.get(k),'b':mb.get(k)} for k in sorted(ma.keys()|mb.keys()) if ma.get(k)!=mb.get(k)},
            'configuration_sha256':{'a':ha,'b':hb},'a_only_steps':sorted(sa.keys()-sb.keys()),'b_only_steps':sorted(sb.keys()-sa.keys()),
            'note':'Grid-dependent mass uniformity; no product quality threshold, interpolation or monotonic mixing assumption.',
            'samples':rows,'spatial_cells':spatial}
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('a',type=Path);p.add_argument('b',type=Path);p.add_argument('--out',type=Path,required=True);args=p.parse_args()
    report=compare(args.a,args.b)
    args.out.mkdir(parents=True,exist_ok=False)
    (args.out/'comparison.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    with (args.out/'comparison.csv').open('w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(report['samples'][0]));w.writeheader();w.writerows(report['samples'])
    with (args.out/'spatial_comparison.csv').open('w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(report['spatial_cells'][0]));w.writeheader();w.writerows(report['spatial_cells'])
    print(args.out)
if __name__=='__main__':main()
