import pathlib
p=pathlib.Path(__file__).resolve().parents[1]/'Source/VirtualGameTeamProj/PowderPlantTest.cpp'
s=p.read_text(encoding='utf-8');start=s.index('void APowderPlantTest::Show()');end=s.index('void APowderPlantTest::Tick(',start);part=s[start:end]
if 'const auto DisplayPoints=Runtime->Points' in part:
    print('Display lock optimization already present');raise SystemExit(0)
part=part.replace('std::lock_guard<std::mutex>L(Runtime->Mutex);','std::unique_lock<std::mutex>L(Runtime->Mutex);',1)
marker=' TArray<TArray<FTransform>> T;'
assert marker in part
part=part.replace(marker,' const auto DisplayPoints=Runtime->Points;const float DisplayAngle=Runtime->Angle;L.unlock();\n'+marker,1)
part=part.replace('for(auto A:Runtime->Points)','for(auto A:DisplayPoints)')
after=part.index(' for(auto V:ValveVisuals)')
tail=part[after:].replace('Runtime->Phase','CurrentPhase').replace('Runtime->OpenSuppliers','AppliedSupplierMask').replace('Runtime->HopperOpen','AppliedHopperOpen').replace('Runtime->ReceiverOpen','AppliedReceiverOpen').replace('Runtime->Angle','DisplayAngle')
part=part[:after]+tail
p.write_text(s[:start]+part+s[end:],encoding='utf-8')
