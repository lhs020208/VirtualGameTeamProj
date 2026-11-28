"""Read the newest complete controller record, including immutable recovery files."""
import json
from pathlib import Path

def read_latest_report(directory):
 root=Path(directory);valid=[]
 for path in [root/'status.json',*root.glob('status_recovery_*.json')]:
  try:
   data=json.loads(path.read_text(encoding='utf8'))
   if not isinstance(data,dict):continue
   valid.append(((int(data.get('report_revision',0)),path.stat().st_mtime_ns),data,path))
  except (OSError,ValueError,TypeError):continue
 if not valid:raise FileNotFoundError('No readable complete status record in '+str(root))
 _,data,path=max(valid,key=lambda item:item[0]);return data,path
