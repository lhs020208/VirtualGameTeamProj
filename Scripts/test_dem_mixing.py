import json
from pathlib import Path
import shutil
import sys
import tempfile
from compare_dem_mixing import compare,load

def rejects(f):
    try:f()
    except (ValueError,KeyError,FileNotFoundError):return
    raise AssertionError('expected invalid export rejection')

root=Path(sys.argv[1]);a=root/'separated';b=root/'premixed'
r=compare(a,b);assert abs(r['samples'][0]['b_minus_a']-1)<1e-12
assert 'premixed' in r['condition_differences']
assert len(r['spatial_cells'])==4
meta,series,_=load(root/'dynamic');assert meta['run_finished'] is True and len(series)==11
assert series[250000]['valid'] and 0<series[250000]['uniformity']<1
r=compare(a,root/'outside');assert r['samples'][0]['b_minus_a'] is None
with tempfile.TemporaryDirectory(prefix='dem_mixing_') as tmp:
    d=Path(tmp)/'copy';shutil.copytree(b,d)
    p=d/'mixing_metadata.json';original=p.read_text();m=json.loads(original);m['dt_s']*=2;p.write_text(json.dumps(m));rejects(lambda:compare(a,d));p.write_text(original)
    m=json.loads(original);m['grid']=[4,2,1];p.write_text(json.dumps(m));rejects(lambda:load(d));p.write_text(original)
    s=d/'mixing_timeseries.csv';text=s.read_text();s.write_text(text.replace(',valid,1,',',valid,nan,'));rejects(lambda:load(d));s.write_text(text)
    # A plausible but fabricated score must fail recomputation against cell masses.
    s.write_text(text.replace(',valid,1,',',valid,0.5,'));rejects(lambda:load(d));s.write_text(text)
    c=d/'mixing_cells.csv';ct=c.read_text();c.write_text('\n'.join(ct.splitlines()[:-1])+'\n');rejects(lambda:load(d));c.write_text(ct)
    (d/'complete.json').unlink();rejects(lambda:load(d))
print('PASS reference comparisons, invalid samples, metadata/time/grid compatibility, nonfinite/tampered scores, incomplete spatial ledger/export')
