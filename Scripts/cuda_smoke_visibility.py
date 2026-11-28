import pathlib
root=pathlib.Path(__file__).resolve().parents[1]
p=root/'Source/VirtualGameTeamProj/PowderPlantTest.cpp'
s=p.read_text(encoding='utf-8')
old='if(SmokeSeconds>0&&++SmokeWarmFrames==10){Running=true;SmokeStart=FPlatformTime::Seconds();}'
assert s.count(old)==1
s=s.replace(old,old[:-1]+'if(Display)Display->HideEquipment();}')
old='FVector Target(0,-140,260);Camera->SetActorLocation(Target+FVector(30,300,60));'
assert s.count(old)==1
s=s.replace(old,'FVector Target(-60,-140,267);Camera->SetActorLocation(Target+FVector(30,300,60));')
p.write_text(s,encoding='utf-8')
