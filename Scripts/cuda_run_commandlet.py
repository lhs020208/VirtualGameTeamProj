import pathlib,shutil,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[1]
original=pathlib.Path('D:/GitHub/VirtualGameTeamProj')
if not (root/'Binaries/Win64/UnrealEditor.modules').exists():
    shutil.copytree(original/'Binaries',root/'Binaries',dirs_exist_ok=True)
    for p in (original/'Plugins').glob('*/Binaries'):
        shutil.copytree(p,root/p.relative_to(original),dirs_exist_ok=True)
exe='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
script=(root/'Scripts'/sys.argv[1]).resolve()
log=root/'Docs/CUDA20261118'/('commandlet_'+script.stem+'.log')
args=[exe,str(root/'VirtualGameTeamProj.uproject'),'-run=pythonscript','-script='+str(script),'-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities','-DisablePlugins=ModelContextProtocol','-unattended','-nullrhi','-nosound','-nosplash','-NoLiveCoding','-NoMessaging','-notrace','-stdout','-FullStdOutLogOutput','-abslog='+str(log)]
with (log.with_suffix('.stdout.txt')).open('w',encoding='utf-8') as f:
    p=subprocess.Popen(args,cwd=root,stdout=f,stderr=subprocess.STDOUT,creationflags=0x08000000)
    print('OWN_COMMANDLET_PID',p.pid,flush=True)
    code=p.wait()
print('exit',code,'log',log,flush=True)
sys.exit(code)
