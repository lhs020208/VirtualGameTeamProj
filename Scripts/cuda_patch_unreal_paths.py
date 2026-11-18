import pathlib
root=pathlib.Path(__file__).resolve().parents[1]
p=root/'Source/VirtualGameTeamProj/PowderPlantTest.cpp'
s=p.read_text(encoding='utf-8')
old='C.backend=static_cast<powder::GranularBackend>(ComputeBackend);'
new=old+'C.cuda_ptx_file=TCHAR_TO_UTF8(*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("SourceAssets/PowderCUDA/plant_sm89.ptx")));'
assert s.count(old)==1 and 'C.cuda_ptx_file=' not in s
p.write_text(s.replace(old,new),encoding='utf-8')
