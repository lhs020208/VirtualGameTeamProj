"""Instrument ONLY the disposable host. Never install this benchmark module."""
import pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(root/'Scripts/demo_stage.py')],check=True)
src=root/'Intermediate/Demo20261119/EditorHost/Source/VirtualGameTeamProj'
def replace(s,a,b):
 assert a in s,a[:100]
 return s.replace(a,b)
p=src/'PowderPlantTest.cpp';s=p.read_text(encoding='utf-8')
helpers='''
int ScaleCopies(){static int Copies=[](){int N=1;FParse::Value(FCommandLine::Get(),TEXT("ScaleCopies="),N);return N==10?10:1;}();return Copies;}
double ScaleRadius(){return ScaleCopies()==10?.4641588833612779:1.;}
FVector ScaleOffset(int K){static TArray<FVector> V=[](){TArray<FVector> A;for(int i=0;i<ScaleCopies();++i){double Z=1.-2.*(i+.5)/ScaleCopies(),R=std::sqrt(1-Z*Z),Phi=i*2.399963229728653;A.Add(ScaleCopies()==1?FVector::ZeroVector:FVector(R*std::cos(Phi),R*std::sin(Phi),Z)*(1-ScaleRadius()));}return A;}();return V[K];}
double ScaleShowSeconds=0;int64 ScaleShowUpdates=0;
'''
s=replace(s,'namespace {','namespace {'+helpers)
s=replace(s,'auto P=ReadCharge(Root/TEXT("test_charge.csv"),ParticleRadiusM);','FString ChargePath=Root/TEXT("test_charge.csv");if(DemoRealtime)FParse::Value(FCommandLine::Get(),TEXT("ScaleChargePath="),ChargePath);auto P=ReadCharge(ChargePath,ParticleRadiusM);')
s=replace(s,'for(auto A:P)Groups[int(A.w)]->AddInstance(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector((200.*ParticleRadiusM)/Diameter)));','for(auto A:P)for(int K=0;K<ScaleCopies();++K)Groups[int(A.w)]->AddInstance(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100+ScaleOffset(K)*(100*ParticleRadiusM),FVector((200.*ParticleRadiusM*ScaleRadius())/Diameter)));')
s=replace(s,'void APowderPlantTest::Show(){if(!Runtime)return;','void APowderPlantTest::Show(){double ScaleStart=FPlatformTime::Seconds();if(!Runtime)return;')
s=replace(s,'T[int(A.w)].Add(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector((200.*ParticleRadiusM)/Diameter)));','for(int K=0;K<ScaleCopies();++K)T[int(A.w)].Add(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100+ScaleOffset(K)*(100*ParticleRadiusM),FVector((200.*ParticleRadiusM*ScaleRadius())/Diameter)));')
s=replace(s,'V.Actor->SetActorTransform(X);}}','V.Actor->SetActorTransform(X);}ScaleShowSeconds+=FPlatformTime::Seconds()-ScaleStart;++ScaleShowUpdates;}')
p.write_text(s,encoding='utf-8')
p=src/'PowderPlantRuntime.inl';s=p.read_text(encoding='utf-8')
s=replace(s,'struct FPlantRuntime {','struct FPlantRuntime {\n std::atomic<uint64_t> ScaleStepNs{0},ScaleSteps{0},ScalePublishNs{0},ScalePublishes{0};')
s=replace(s,'auto next=GPU.snapshot();','auto ScalePublishStart=Clock::now();auto next=GPU.snapshot();')
s=replace(s,'LastPublish=now;','LastPublish=now;ScalePublishNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-ScalePublishStart).count();++ScalePublishes;')
s=replace(s,'GPU.advance(demo?1:16,float(speed),false);','auto ScaleStepStart=Clock::now();GPU.advance(demo?1:16,float(speed),false);ScaleStepNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-ScaleStepStart).count();++ScaleSteps;')
p.write_text(s,encoding='utf-8')
p=src/'PowderPlantDemo.inl';s=p.read_text(encoding='utf-8')
s=replace(s,'DemoNextSample=0;','DemoNextSample=0;ScaleShowSeconds=0;ScaleShowUpdates=0;')
s=replace(s,'double Sim=PhysicalSeconds;Stop();','double Sim=PhysicalSeconds;double StepMs=Runtime?double(Runtime->ScaleStepNs.load())/std::max(uint64_t(1),Runtime->ScaleSteps.load())/1e6:0;double PublishMs=Runtime?double(Runtime->ScalePublishNs.load())/std::max(uint64_t(1),Runtime->ScalePublishes.load())/1e6:0;Stop();')
s=replace(s,'ParticleCount==20000','(ParticleCount==20000||ParticleCount==200000)')
s=replace(s,r'\"solver_particles\":2500',r'\"solver_particles\":%d')
s=replace(s,'Percentile(.99),ParticleCount,InWeighingHopper','Percentile(.99),ParticleCount*ScaleCopies(),ParticleCount/8,InWeighingHopper')
s=replace(s,'FFileHelper::SaveStringToFile(Result,',r'''Result.RemoveFromEnd(TEXT("}"));Result+=FString::Printf(TEXT(",\"show_mean_ms\":%.6f,\"worker_step_mean_ms\":%.6f,\"worker_publish_mean_ms\":%.6f,\"physical_input_particles\":%d,\"visual_radius_m\":%.9f}"),ScaleShowSeconds/std::max(int64(1),ScaleShowUpdates)*1000,StepMs,PublishMs,ParticleCount,ParticleRadiusM*ScaleRadius());
 FFileHelper::SaveStringToFile(Result,'''.replace('\\"','\\"'))
# Above string is C++ source, preserving the JSON string escapes.
p.write_text(s,encoding='utf-8')
# Reuse the tested launcher/timer policy, but redirect all outputs and input overrides.
launcher=(root/'Scripts/demo_unreal.py').read_text(encoding='utf-8')
launcher=replace(launcher,"out=root/'Docs/Demo20261119'","out=root/'Docs/DemoScale200k'")
launcher=replace(launcher,"if len(sys.argv)>4:args+=['-ExecCmds='+sys.argv[4]]","if len(sys.argv)>4:args+=['-ExecCmds='+sys.argv[4]]\n        if label.startswith('scaled_'):args+=['-ScaleChargePath='+str(root/'Intermediate/DemoScale200k/charge_200k.csv')]\n        if label.startswith('visual_'):args+=['-ScaleCopies=10']")
launcher=replace(launcher,"subprocess.run([sys.executable,str(root/'Scripts/demo_stage.py')],check=True)","# Already staged and instrumented; do not replace benchmark sources.")
# Launcher lives in Scripts for stable root lookup; it never installs its module.
(root/'Scripts/demo_scale_unreal.py').write_text(launcher,encoding='utf-8')
print('Instrumented disposable host only')
