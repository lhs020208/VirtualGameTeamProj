from pathlib import Path
import csv,json,subprocess,sys
r=Path('D:/GitHub/VirtualGameTeamProj');d=r/'Docs/DEM/optimization45'
rows=list(csv.DictReader((d/'reference12/trace.csv').open()));first=rows[0];last=rows[-1]
p=d/'synthetic_tool_selfcheck.csv'
with p.open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['split','metric','time_s','value','sigma'])
 w.writerow(['fit','receiver_kg',last['time'],float(last['receiver_count'])*64.8/50000,.01])
 w.writerow(['holdout','mean_z_m',first['time'],first['mean_z'],.001])
subprocess.run([sys.executable,str(r/'Scripts/calibrate_granular_material.py'),str(p)],check=True,stdout=subprocess.DEVNULL)
result=json.loads((d/'material_calibration.json').read_text());assert result['selected_friction']==.4 and not result['physical_accuracy_validated']
result['synthetic_tool_test_only']=True;result['not_independent_physical_validation']=True
(d/'material_tool_selfcheck.json').write_text(json.dumps(result,indent=2))
subprocess.run([sys.executable,str(r/'Scripts/calibrate_granular_material.py')],check=True)
print('Synthetic tool self-check passed; actual calibration remains awaiting measurements.')
