"""Independent retained-species mass reconstruction from exported pebble radii.
Only the fixed 1..6 virtual presets (rho=1000/1100/1200) are supported.
"""
from pathlib import Path
import csv,json,math,argparse

def audit(folder):
 folder=Path(folder);p=json.loads((folder/'protocol.json').read_text(encoding='utf-8-sig'))
 assert p['experiment'] in range(1,7)
 actual={0:0.,1:0.,2:0.}
 for row in csv.DictReader((folder/'particles.csv').open(encoding='utf-8-sig')):
  kind=int(row['type']);radius=float(row['radius_m']);assert kind in actual and radius>0 and math.isfinite(radius)
  actual[kind]+=4*math.pi/3*radius**3*(1000+kind*100)
 errors={str(s['type']):actual[s['type']]-s['retained_kg'] for s in p['by_type']}
 assert max(abs(e) for e in errors.values())<1e-15
 assert abs(sum(actual.values())-p['retained_kg'])<1e-15
 result={'passed':True,'actual_retained_kg_from_pebbles':actual,'ledger_error_by_type_kg':errors,'scope':'fixed virtual preset density; independent of supplied-minus-exited bookkeeping'}
 (folder/'species_audit.json').write_text(json.dumps(result,indent=2));return result
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('folder');a=p.parse_args();print(json.dumps(audit(a.folder),indent=2))
