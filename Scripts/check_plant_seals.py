"""Check generated joint sleeves against independently exported body bores."""
import json
from pathlib import Path
import numpy as np
root=Path(__file__).resolve().parents[1];data=root/'SourceAssets/ProcessPlantPhysics'
m=json.loads((data/'runtime_manifest.json').read_text(encoding='utf-8'));results=[]
for prefix,axis,centre in [('BV122',0,[0,2.682]),('BV272',2,[3,0])]:
    seal=next(x for x in m['test_overrides'] if x.get('id')=='Test_'+prefix+'_seal')
    radial=[j for j in range(3) if j!=axis];ranges=[]
    for suffix in ['', '.001']:
        v=np.array(json.loads((data/(prefix+'_110_Split_body_half'+suffix+'.json')).read_text())['vertices'])
        bore=float(np.min(np.linalg.norm(v[:,radial]-centre,axis=1)))
        error=max(abs(seal['radius_a_m']-bore),abs(seal['radius_b_m']-bore))
        assert error<1e-6,(prefix,'discontinuous bore',error)
        ranges.append((float(v[:,axis].min()),float(v[:,axis].max())))
    ranges.sort();assert seal['a'][axis]<ranges[0][1] and seal['b'][axis]>ranges[1][0]
    results.append({'joint':prefix,'body_bore_radius_m':bore,'matched':True,'overlaps_both_body_ends':True})
report={'passed':True,'joints':results,'note':'Bore/axial overlap check. Does not certify every source surface is watertight.'}
(root/'Docs/ProcessPlantPhysics/seal_fit.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
