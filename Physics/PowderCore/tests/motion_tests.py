"""Analytic task 02 tests; tolerances fixed before execution."""
import json, math, subprocess, sys
exe=sys.argv[1]
def run(dt,steps,*args):
    r=subprocess.run([exe,'--dt',str(dt),'--steps',str(steps),*args],capture_output=True,text=True)
    assert r.returncode==0,r.stderr
    return json.loads(r.stdout)
def close(a,b,tol=1e-10):
    assert abs(a-b)<=tol,(a,b,abs(a-b))
# Zero gravity, 3D initial position/velocity; absolute error <= 1e-10 SI.
r=run(.001,1000,'--gz','0','--px','1','--py','-2','--pz','3','--vx','2','--vy','-3','--vz','4')
for a,b in zip(r['particle']['position_m'],[3,-5,7]): close(a,b)
assert r['particle']['velocity_m_s']==[2,-3,4]
# Constant gravity: v=v0+gt, x=x0+v0*t+g*t^2/2.
# Symplectic Euler position error is |g|*T*dt/2; ratio 2 within 1e-8.
errors=[]
for dt,n in [(.02,50),(.01,100),(.005,200)]:
    r=run(dt,n,'--pz','10','--vz','2')
    close(r['particle']['velocity_m_s'][2],-7.81)
    err=abs(r['particle']['position_m'][2]-7.095)
    close(err,9.81*dt/2)
    errors.append(err)
for a,b in zip(errors,errors[1:]): close(a/b,2,1e-8)
# Gravity is independent of mass; gravity in all axes.
base=run(.001,1000,'--gx','1','--gy','2','--gz','-3')
heavy=run(.001,1000,'--gx','1','--gy','2','--gz','-3','--density','5000','--diameter','0.02')
for a,b in zip(base['particle']['velocity_m_s'],[1,2,-3]): close(a,b)
for a,b in zip(base['particle']['position_m'],heavy['particle']['position_m']): close(a,b)
# Constant external force, superposed with gravity: mass=pi/6, Fx=pi/3 => ax=2.
r=run(.001,1000,'--diameter','1','--density','1','--fx',str(math.pi/3))
close(r['particle']['velocity_m_s'][0],2)
close(r['particle']['velocity_m_s'][2],-9.81)
# No time elapsed, no state change.
r=run(.01,0,'--pz','4','--vz','2')
assert r['particle']['position_m']==[0,0,4] and r['particle']['velocity_m_s']==[0,0,2]
for option in ['--gx','--fz','--px','--vz']:
    r=subprocess.run([exe,option,'nan'],capture_output=True,text=True)
    assert r.returncode!=0 and 'ERROR:' in r.stderr
print(json.dumps({'result':'PASS','position_errors_m':errors,'error_ratios':[errors[i]/errors[i+1] for i in range(2)],'absolute_tolerance_SI':1e-10,'ratio_tolerance':1e-8},indent=2))
