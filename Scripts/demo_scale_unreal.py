import pathlib,subprocess,sys,time,json,shutil,ctypes
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'Docs/DemoScale200k';out.mkdir(parents=True,exist_ok=True)
engine=pathlib.Path('C:/Program Files/Epic Games/UE_5.8/Engine')
mode=sys.argv[1];label=sys.argv[2] if len(sys.argv)>2 else mode
host=root/'Intermediate/Demo20261119/EditorHost'
project=host/'VirtualGameTeamProj.uproject'
if mode=='installed':project=root/'VirtualGameTeamProj.uproject'
if mode=='build':
    # Restore already-verified CUDA link dependencies in this checkout too.
    previous=pathlib.Path('D:/GitHub/VirtualGameTeamProj_CUDA_20261118')
    for relative in ['Intermediate/CUDA/powder_plant_cuda.lib','Intermediate/CUDAToolkit/lib/x64/cudart_static.lib','Intermediate/CUDAToolkit/lib/x64/cuda.lib']:
        target=root/relative
        if not target.exists() and (previous/relative).exists(): target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(previous/relative,target)
    # Already staged and instrumented; do not replace benchmark sources.
    args=[str(engine/'Build/BatchFiles/Build.bat'),'VirtualGameTeamProjEditor','Win64','Development',str(project),'-WaitMutex','-NoHotReloadFromIDE','-NoLiveCoding','-MaxParallelActions=2','-NoUBA','-NoUBTMakefiles','-ForceRulesCompile']
else:
    args=[str(engine/'Binaries/Win64/UnrealEditor-Cmd.exe'),str(project)]
    if mode=='level':args+=['-run=pythonscript','-script='+str(root/'Scripts/demo_level.py'),'-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities','-nullrhi']
    elif mode in ('test','installed'):
        seconds=sys.argv[3] if len(sys.argv)>3 else '90'
        if (out/(label+'.json')).exists():raise RuntimeError('refusing to overwrite benchmark')
        args+=['/Game/ProcessPlant_Demo20261119/L_ProcessPlant_Demo30FPS_1119','-game','-RenderOffscreen','-windowed','-ForceRes','-ResX=1920','-ResY=1080','-DemoBenchmarkSeconds='+seconds,'-DemoBenchmarkOutput='+str(out/label)]
        if len(sys.argv)>4:args+=['-ExecCmds='+sys.argv[4]]
        if label.startswith('scaled_'):args+=['-ScaleChargePath='+str(root/'Intermediate/DemoScale200k/charge_200k.csv')]
        if label.startswith('visual_'):args+=['-ScaleCopies=10']
        if 'incremental' in label:args+=['-ScaleIncremental']
    elif mode=='smoke':
        level='/Game/ProcessPlant_CUDA_20261118/L_ProcessPlant_CUDA_1118' if len(sys.argv)>3 and sys.argv[3]=='cuda' else '/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111'
        args+=[level,'-game','-RenderOffscreen','-windowed','-ForceRes','-ResX=1280','-ResY=720','-PlantSmokeSeconds=0.00032','-PlantWallLimit=30']
    else:raise ValueError(mode)
    args+=['-unattended','-nosound','-nosplash','-NoLiveCoding','-NoMessaging','-notrace','-DisablePlugins=ModelContextProtocol','-stdout','-FullStdOutLogOutput','-abslog='+str(out/(label+'.log'))]
start=time.time()
timer_policy=None
with (out/(label+'.stdout.txt')).open('w',encoding='utf-8') as log:
    p=subprocess.Popen(args,cwd=root,stdout=log,stderr=subprocess.STDOUT,creationflags=0x08000000)
    if mode in ('test','installed'):
        # Windows 11 can ignore Unreal's 1 ms timer request for a hidden, silent
        # process. Honor that existing request in OUR offscreen child only.
        # Do not change GPU clocks, OS power plans, or the user's open editor.
        class PowerState(ctypes.Structure):
            _fields_=[('Version',ctypes.c_ulong),('ControlMask',ctypes.c_ulong),('StateMask',ctypes.c_ulong)]
        k=ctypes.WinDLL('kernel32',use_last_error=True)
        for name in ('GetProcessInformation','SetProcessInformation'):
            fn=getattr(k,name);fn.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_void_p,ctypes.c_ulong];fn.restype=ctypes.c_int
        state=PowerState(1,0,0)
        ok=k.GetProcessInformation(int(p._handle),4,ctypes.byref(state),ctypes.sizeof(state))
        before=dict(control=state.ControlMask,state=state.StateMask)
        if not ok:raise ctypes.WinError(ctypes.get_last_error())
        state.ControlMask|=4;state.StateMask&=~4
        ok=k.SetProcessInformation(int(p._handle),4,ctypes.byref(state),ctypes.sizeof(state))
        if not ok:raise ctypes.WinError(ctypes.get_last_error())
        timer_policy=dict(scope='own offscreen child only',before=before,control=state.ControlMask,state=state.StateMask,
            reason='Honor Unreal timer resolution while window is hidden; https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessinformation')
    print('OWN_PROCESS',p.pid,'mode',mode,'log',str(out/(label+'.stdout.txt')),flush=True)
    code=p.wait()
(out/(label+'_process.json')).write_text(json.dumps(dict(pid=p.pid,exit_code=code,wall_s=time.time()-start,command=args,timer_policy=timer_policy),indent=2))
print('exit',code,flush=True)
if mode=='build': print('\n'.join((out/(label+'.stdout.txt')).read_text(errors='replace').splitlines()[-25:]))
elif mode in ('test','installed') and (out/(label+'.json')).exists(): print((out/(label+'.json')).read_text(encoding='utf-8-sig'))
elif mode=='smoke':
    result=host/'Saved/PlantSmoke.json'
    if result.exists() and result.stat().st_mtime>=start:shutil.copy2(result,out/(label+'.json'));print(result.read_text(encoding='utf-8-sig'))
sys.exit(code)
