import pathlib
root=pathlib.Path(__file__).resolve().parents[1]
p=root/'Source/VirtualGameTeamProj/PowderPlantTest.cpp'
s=p.read_text(encoding='utf-8')
changes={
 'SmokeStart=FPlatformTime::Seconds();CacheRotor();':'SmokeStart=FPlatformTime::Seconds();if(SmokeSeconds>0)Running=false;CacheRotor();',
 'void APowderPlantTest::Tick(float D){Super::Tick(D);':'void APowderPlantTest::Tick(float D){Super::Tick(D);if(SmokeSeconds>0&&++SmokeWarmFrames==10){Running=true;SmokeStart=FPlatformTime::Seconds();}',
 'FPlatformMisc::RequestExitWithStatus(false,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?0:1);':'Stop();Running=false;FPlatformMisc::RequestExitWithStatus(false,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?0:1);'
}
for old,new in changes.items():
    assert s.count(old)==1,old
    s=s.replace(old,new)
p.write_text(s,encoding='utf-8')
