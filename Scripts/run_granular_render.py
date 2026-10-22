from pathlib import Path
import subprocess,time,json,sys
r=Path('D:/GitHub/VirtualGameTeamProj');n=int(sys.argv[1]);label=sys.argv[2]
out=r/'Saved'/('Opt44_'+label);assert not out.exists();out.mkdir()
cmd=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(r/'VirtualGameTeamProj.uproject'),'/Game/L_GranularMixer','-game','-windowed','-ResX=1280','-ResY=720','-nosound','-unattended','-ddc=InstalledNoZenLocalFallback','-ExecCmds=t.MaxFPS 30','-GrainBenchmark',f'-GrainCount={n}',f'-GrainOutput={out}',f'-abslog={out}/engine.log']
p=subprocess.Popen(cmd,creationflags=subprocess.CREATE_NO_WINDOW)
start=time.monotonic()
while not (out/'result.json').exists():
 if time.monotonic()-start>240:raise RuntimeError('render timeout; preserve running process for diagnosis')
 time.sleep(1)
print((out/'result.json').read_text(encoding='utf-8-sig'),flush=True)
time.sleep(3)

