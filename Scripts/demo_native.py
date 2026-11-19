import pathlib,subprocess,os,sys
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'Intermediate/Demo20261119';out.mkdir(parents=True,exist_ok=True)
vc=pathlib.Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
bat=out/'environment.bat';bat.write_text('@echo off\ncall "'+str(vc)+'" >nul\nset\n')
result=subprocess.run(['cmd.exe','/d','/c',str(bat)],capture_output=True,check=True)
env=os.environ.copy()
for line in result.stdout.decode('utf-8',errors='replace').splitlines():
    if '=' in line:
        k,v=line.split('=',1);env[k]=v
cl=pathlib.Path(env['VCToolsInstallDir'])/'bin/Hostx64/x64/cl.exe'
name=sys.argv[1] if len(sys.argv)>1 else 'demo_plant_bench'
exe='demo_bench' if name=='demo_plant_bench' else name
args=[str(cl),'/nologo','/std:c++17','/EHsc','/O2','/MD','/I'+str(root/'Physics/PowderCore/include'),str(root/'Physics/PowderCore/tests'/(name+'.cpp')),'/Fe:'+str(out/(exe+'.exe')),'/Fo:'+str(out/(exe+'.obj'))]
p=subprocess.run(args,env=env,cwd=out,capture_output=True)
print(p.stdout.decode('utf-8',errors='replace'));print(p.stderr.decode('utf-8',errors='replace'))
sys.exit(p.returncode)
