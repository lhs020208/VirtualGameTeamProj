"""Run the copied level in our own offscreen process; never attach to an editor."""
import hashlib
import json
import pathlib
import subprocess
import sys
import time

root=pathlib.Path(__file__).resolve().parents[1]
assert root.name=='VirtualGameTeamProj_CUDA_20261118'
saved=root/'Saved'
report=root/'Docs/CUDA20261118'
exe='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
args=[exe,str(root/'VirtualGameTeamProj.uproject'),
      '/Game/ProcessPlant_CUDA_20261118/L_ProcessPlant_CUDA_1118',
      '-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720',
      '-unattended','-nosound','-nosplash','-NoLiveCoding','-NoMessaging','-notrace',
      '-DisablePlugins=ModelContextProtocol','-PlantSmokeSeconds=0.05','-PlantWallLimit=90',
      '-stdout','-FullStdOutLogOutput','-abslog='+str(report/'unreal_smoke.log')]
start=time.time()
with (report/'unreal_smoke.stdout.txt').open('w',encoding='utf-8') as log:
    process=subprocess.Popen(args,cwd=root,stdout=log,stderr=subprocess.STDOUT,creationflags=0x08000000)
    print('OWN_OFFSCREEN_PID',process.pid,flush=True)
    code=process.wait()
metadata=dict(command=args,own_pid=process.pid,exit_code=code,process_wall_s=time.time()-start,
              benchmark=False,interpretation='Offscreen Unreal integration smoke; not the equal-duration solver benchmark')
for name in ['PlantSmoke.json','PlantSmokeError.txt','PlantSnapshot.csv','PlantFlowPreview.png']:
    path=saved/name
    if path.exists() and path.stat().st_mtime>=start:
        data=path.read_bytes()
        (report/name).write_bytes(data)
        metadata[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
if 'PlantSmoke.json' in metadata:
    metadata['result']=json.loads((report/'PlantSmoke.json').read_text(encoding='utf-8-sig'))
(report/'unreal_smoke_result.json').write_text(json.dumps(metadata,indent=2))
print(json.dumps(metadata,indent=2),flush=True)
sys.exit(code or (0 if 'result' in metadata and not metadata['result']['failed'] else 1))
