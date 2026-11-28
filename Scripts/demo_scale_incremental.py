"""Test-only instance update toggle; leaves production sources/module intact."""
import pathlib
root=pathlib.Path(__file__).resolve().parents[1]
p=root/'Intermediate/Demo20261119/EditorHost/Source/VirtualGameTeamProj/PowderPlantTest.cpp'
s=p.read_text(encoding='utf-8');a='BatchUpdateInstancesTransforms(0,T[I],false,true,true)';b='BatchUpdateInstancesTransforms(0,T[I],false,!FParse::Param(FCommandLine::Get(),TEXT("ScaleIncremental")),true)'
assert a in s or b in s
p.write_text(s.replace(a,b),encoding='utf-8')
p=root/'Scripts/demo_scale_unreal.py';s=p.read_text(encoding='utf-8')
a="if label.startswith('visual_'):args+=['-ScaleCopies=10']"
b=a+"\n        if 'incremental' in label:args+=['-ScaleIncremental']"
if b not in s:
 assert a in s;s=s.replace(a,b)
p.write_text(s,encoding='utf-8')
print('Prepared incremental instance-data update experiment')
