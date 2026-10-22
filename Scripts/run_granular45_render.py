from pathlib import Path
import subprocess,time,json,sys
r=Path('D:/GitHub/VirtualGameTeamProj');label=sys.argv[1];n=int(sys.argv[2]);dt=float(sys.argv[3]);mx=bool(int(sys.argv[4]));seconds=float(sys.argv[5]) if len(sys.argv)>5 else 12
folder=r/'Saved'/('Opt45_'+label);folder.mkdir(exist_ok=False)
command=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(r/'VirtualGameTeamProj.uproject'),'/Game/L_MX301_Test' if mx else '/Game/L_GranularMixer','-game','-windowed','-ResX=1280','-ResY=720','-nosound','-unattended','-ddc=InstalledNoZenLocalFallback','-ExecCmds=t.MaxFPS 30','-GrainBenchmark',f'-GrainCount={n}',f'-GrainDt={dt}','-GrainBatch=32',f'-GrainSeconds={seconds}',f'-GrainOutput={folder}',f'-abslog={folder}/engine.log']
if mx:command.append('-GrainTestMixer')
settings=dict(count=n,dt=dt,iterations=16,batch=32,test_mixer=mx,requested_rate=2,seconds=seconds,render_size=[1280,720],fps_limit=30)
(folder/'settings.json').write_text(json.dumps(settings,indent=2))
p=subprocess.Popen(command,creationflags=subprocess.CREATE_NO_WINDOW)
try:code=p.wait(timeout=600)
except subprocess.TimeoutExpired:raise RuntimeError('benchmark exceeded 600s; process preserved for diagnosis')
assert code==0,code
result=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'));print(label,result,flush=True)
assert not result['error'],result
assert result['time']>=seconds,result
