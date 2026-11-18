"""Compare all repeats; include both unchanged-baseline measurement rounds."""
from cuda_measure import *
import itertools

result=[]
for scene in ('initial','transfer','mixing'):
    groups={
        'baseline':[read_state(REPORT/folder/f'{scene}_r{r}_s0.bin')
                    for folder in ('benchmark_graph','benchmark_final') for r in range(3)],
        'cuda':[read_state(REPORT/'benchmark_final'/f'{scene}_r{r}_s1.bin') for r in range(3)],
        'optimized':[read_state(REPORT/'benchmark_final'/f'{scene}_r{r}_s2.bin') for r in range(3)]}
    comparisons=[]
    for label,pairs in [('baseline_repeat',itertools.combinations(groups['baseline'],2)),
                         ('cuda_vs_baseline',itertools.product(groups['cuda'],groups['baseline'])),
                         ('optimized_vs_cuda',itertools.product(groups['optimized'],groups['cuda'])),
                         ('optimized_vs_baseline',itertools.product(groups['optimized'],groups['baseline']))]:
        measures=[]
        for a,b in pairs:
            delta=np.linalg.norm(a[:,:,:3]-b[:,:,:3],axis=2)
            measures.append([float(delta[0].max()),float(np.sqrt(np.mean(delta[0]**2))),
                             float(np.sqrt(np.mean(delta[1]**2))),
                             float(np.linalg.norm(a[0,:,:3].mean(0)-b[0,:,:3].mean(0)))])
        names=['position_max_m','position_rms_m','velocity_rms_m_s','centroid_difference_m']
        a=np.asarray(measures)
        comparisons.append(dict(comparison=label,pairs=len(measures),
            metrics={n:dict(min=float(a[:,i].min()),max=float(a[:,i].max()),median=float(np.median(a[:,i]))) for i,n in enumerate(names)}))
    result.append(dict(scene=scene,comparisons=comparisons))
(REPORT/'repeatability.json').write_text(json.dumps(dict(results=result,
    note='Six unchanged baseline repeats from two measurement rounds. Descriptive uncertainty evidence; not a newly relaxed acceptance threshold or experimental validation.'),indent=2))
print(json.dumps(result,indent=2))
