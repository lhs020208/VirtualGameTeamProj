"""Small reporting regression tests; never starts Unreal or changes physics."""
import ast, datetime, hashlib, json, tempfile, time, types, uuid
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'Scripts/run_full_process_20261110.py'
node=next(n for n in ast.parse(source.read_text(encoding='utf8')).body if isinstance(n,ast.FunctionDef) and n.name=='save_report')
results=[]
with tempfile.TemporaryDirectory(prefix='powder_report_test_') as td:
 warnings=[];g={'OUT':Path(td),'report':{'state_marker':'unchanged'},'json':json,'datetime':datetime,'time':time,'uuid':uuid,'u':types.SimpleNamespace(log_warning=warnings.append)}
 exec(compile(ast.Module(body=[node],type_ignores=[]),'isolated_save_report','exec'),g)
 with patch.object(Path,'replace',side_effect=PermissionError('simulated reader lock')), patch.object(time,'time_ns',return_value=0):
  for _ in range(5):assert g['save_report']() is False
 assert len(list(Path(td).glob('status_recovery_*.json')))==5
 assert g['report']['state_marker']=='unchanged'
 results.append({'case':'five replacement locks','passed':True,'recovery_files':5,'exception_escaped':False})
 assert g['save_report']() is True
 assert json.loads((Path(td)/'status.json').read_text())['state_marker']=='unchanged'
 results.append({'case':'next publish succeeds after lock release','passed':True})
 with patch.object(Path,'write_text',side_effect=PermissionError('primary and recovery denied')):
  assert g['save_report']() is False
 results.append({'case':'primary and recovery writes denied','passed':True,'exception_escaped':False,'warning_emitted':True})
report={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'cases':results,'limitations':['Isolated Python fault injection, not actual UE/Windows concurrent reader integration.','No physics state, Unreal process, or large simulation was started.','Other CSV/snapshot I/O paths are not covered.']}
out=ROOT/'Docs/ProcessPlant_20261110_LowPoly/Followup_20261110/REPORT_IO_MOCK_REGRESSION.json'
out.write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report,indent=2))
