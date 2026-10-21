"""Run with Unreal Python commandlet; validates real Unreal Transform conversion."""
import math
import runpy
from pathlib import Path
import unreal as u
api=runpy.run_path(str(Path(u.Paths.project_dir())/'Scripts/export_dem_geometry.py'))
convert=api['transform_box']
t=u.Transform(location=u.Vector(100,200,300),rotation=u.Rotator(pitch=0,yaw=90,roll=0),scale=u.Vector(2,-3,.5))
centre,half,axes=convert(t)
def close(a,b):assert abs(a-b)<1e-10,(a,b)
for a,b in zip(centre,[1,2,3]):close(a,b)
for a,b in zip(half,[1,1.5,.25]):close(a,b)
for axis,expected in zip(axes,[[0,1,0],[1,0,0],[0,0,1]]):
 for a,b in zip(axis,expected):close(a,b)
t=u.Transform(location=u.Vector(-50,30,200),rotation=u.Rotator(pitch=31,yaw=27,roll=19),scale=u.Vector(.2,3,1.5))
centre,half,axes=convert(t)
for signs in ((1,1,1),(-1,1,-1),(1,-1,-1)):
 expected=api['xyz'](t.transform_location(u.Vector(*(s*50 for s in signs))))
 actual=[centre[k]+sum(signs[i]*half[i]*axes[i][k] for i in range(3)) for k in range(3)]
 for a,b in zip(actual,expected):close(a,b*.01)
print('DEM_TRANSFORM_TEST_PASS: cm-to-m, nonuniform and negative scale, yaw, combined pitch/yaw/roll')
