"""Collect reproducible hardware diagnostics; do not treat occupancy as FLOP utilization."""
from pathlib import Path
import json,hashlib,statistics,shutil
ROOT=Path(__file__).resolve().parents[1];src=ROOT/'Saved/HardwareDiagnosis';out=ROOT/'Docs/DEM/hardware_diagnosis';out.mkdir(parents=True,exist_ok=True)
report={'scope':'Current virtual binary 50000 fixture only; fixed dt/contact iterations; editor left open in background. Core timing is not a frame-exact UE replay. GPU samples are whole adapter, not SM occupancy or per-process GPU counters.','runs':{}}
for suite in ['screen','long','unreal']:
 for folder in (src/suite).iterdir():
  if not folder.is_dir() or not (folder/'result.json').exists():continue
  key=suite+'/'+folder.name;result=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'));timing=json.loads((folder/'timing.json').read_text());monitor=folder/'monitor.json' if suite=='unreal' else folder.parent/(folder.name+'_monitor.json');samples=json.loads(monitor.read_text());values=[[float(v.strip()) for v in s['gpu'].split(',')] for s in samples if s['gpu']]
  metrics={'gpu_utilization_median_percent':statistics.median(v[0] for v in values),'max_temperature_c':max(v[5] for v in values),'max_power_w':max(v[2] for v in values),'median_power_w':statistics.median(v[2] for v in values),'gpu_memory_used_max_mib':max(v[3] for v in values),'scope':'sampled process lifetime including startup/exit, whole GPU'}
  if len(samples)>1:metrics['process_average_logical_cores']=(samples[-1]['process_cpu_s']-samples[0]['process_cpu_s'])/(samples[-1]['elapsed_s']-samples[0]['elapsed_s'])
  report['runs'][key]={'result':result,'timing':timing,'monitor_summary':metrics}
  dest=out/suite/folder.name;dest.mkdir(parents=True,exist_ok=True)
  for name in ['result.json','timing.json','command.json','worker_timing.json','display_timing.json','audit_report.json']:
   if (folder/name).exists():shutil.copy2(folder/name,dest/name)
  shutil.copy2(monitor,dest/'monitor.json')
 for name in ['geometry_audit.json','fixture.json']:
  if (src/suite/name).exists():shutil.copy2(src/suite/name,out/(suite+'_'+name))
for f in ['regression.log']:
 if (src/f).exists():shutil.copy2(src/f,out/f)
report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'Physics/PowderCore/src/granular.cpp',ROOT/'Physics/PowderCore/src/granular_kernels.inc',ROOT/'Physics/PowderCore/app/granular_diagnose.cpp',ROOT/'Intermediate/PowderCore/Release/granular_diagnose.exe',ROOT/'Binaries/Win64/UnrealEditor-VirtualGameTeamProj.dll']}
(out/'summary.json').write_text(json.dumps(report,indent=2));print(out)
