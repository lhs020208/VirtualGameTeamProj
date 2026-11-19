import pathlib,subprocess,sys,json,hashlib
root=pathlib.Path(__file__).resolve().parents[1];out=root/'Docs/Demo200kSetup'
module=root/'Intermediate/Demo20261119/EditorHost/Binaries/Win64/UnrealEditor-VirtualGameTeamProj.dll'
sha=hashlib.sha256(module.read_bytes()).hexdigest()
for args in [('test','verify_staged','150'),('smoke','original_opencl'),('smoke','original_cuda','cuda')]:
 code=subprocess.call([sys.executable,str(root/'Scripts/demo_publish_unreal.py'),*args],cwd=root)
 if code:raise SystemExit(code)
 result=json.loads((out/(args[1]+'.json')).read_text(encoding='utf-8-sig'))
 if args[0]=='test':
  assert result['configuration_pass'] and not result['error']
  assert .98<=result['rate']<=1.02 and result['MX']==20000
 else:assert not result['failed']
assert hashlib.sha256(module.read_bytes()).hexdigest()==sha
(out/'verified_module.json').write_text(json.dumps(dict(sha256=sha,display_particles=200000,solver_particles=2500,tests=['verify_staged','original_opencl','original_cuda']),indent=2))
print('VERIFIED_200K_MODULE',sha,flush=True)
