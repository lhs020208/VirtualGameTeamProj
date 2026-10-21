import json, math, subprocess, sys
exe=sys.argv[1]
args=['--diameter','1','--density',str(6/math.pi),'--plane-z','0','--stiffness','10000','--damping','40','--pz','0.6','--dt','0.00001','--steps','500000']
r=subprocess.run([exe,*args],capture_output=True,text=True)
assert r.returncode==0,r.stderr
d=json.loads(r.stdout)
assert d['plane_enabled'] and d['stiffness_n_m']==10000 and d['schema_version']==3
z=d['particle']['position_m'][2]; v=d['particle']['velocity_m_s'][2]
assert abs(z-(.5-9.81/10000))<1e-7,(z,v)
assert abs(v)<1e-6,(z,v)
for bad in [['--plane-z','0','--dt','1'],['--plane-z','nan'],['--plane-z','0','--stiffness','0'],['--plane-z','0','--damping','-1'],['--damping','1']]:
    r=subprocess.run([exe,*bad],capture_output=True,text=True)
    assert r.returncode!=0 and 'ERROR:' in r.stderr
print(f'PASS plane CLI and dynamic settling: z={z:.14g} m v={v:.14g} m/s; 5 invalid configurations rejected')
