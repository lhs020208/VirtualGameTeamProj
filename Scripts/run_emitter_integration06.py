from pathlib import Path
import subprocess,json,sys,time
r=Path(__file__).resolve().parents[1]
label=sys.argv[1];level=sys.argv[2];seconds=float(sys.argv[3])
out=r/'Saved'/('Emitter06_'+label);out.mkdir(exist_ok=False)
cmd=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(r/'VirtualGameTeamProj.uproject'),level,'-game','-windowed','-ResX=1280','-ResY=720','-nosound','-unattended','-ddc=InstalledNoZenLocalFallback','-ExecCmds=t.MaxFPS 30','-GrainBenchmark','-GrainCount=50000','-GrainDt=0.000375','-GrainBatch=32',f'-GrainSeconds={seconds}',f'-GrainOutput={out}',f'-abslog={out}/engine.log']
(out/'settings.json').write_text(json.dumps(dict(count=50000,dt=.000375,batch=32,iterations=16,seconds=seconds,level=level,requested_rate=2,resolution=[1280,720],fps_cap=30),indent=2))
p=subprocess.Popen(cmd,creationflags=subprocess.CREATE_NO_WINDOW)
try:code=p.wait(timeout=600)
except subprocess.TimeoutExpired:raise RuntimeError(f'Benchmark timeout PID={p.pid}; process retained for inspection')
assert code==0,code
result=json.loads((out/'result.json').read_text(encoding='utf-8-sig'));print(label,result,flush=True)
assert not result['error'] and result['time']>=seconds,result
