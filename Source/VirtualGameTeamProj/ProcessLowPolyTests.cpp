#include "ProcessPlantBinding.h"
#include "ProcessUIStateAdapter.h"
#include "ProcessControlTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessLowPolyMappingTest,"ProcessUI.LowPoly.Mapping",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessLowPolyMappingTest::RunTest(const FString&)
{
    FPlantSimulationSnapshot S;
    S.SourceActorPath=ProcessPlantBinding::SourcePath(ProcessPlantBinding::LowPolyMap);
    TestTrue(TEXT("explicit LowPoly object, not FlowTest or first actor"),S.SourceActorPath.Contains(TEXT("L_ProcessPlant_LowPoly_1111:PersistentLevel.PowderPlantTest_0")));
    TestTrue(TEXT("unknown maps never auto bind"),ProcessPlantBinding::SourcePath(TEXT("/Game/Unknown")).IsEmpty());
    S.Validity=EPlantValueValidity::Valid;S.PublishedWallSeconds=100;S.PhysicalSeconds=3;
    S.RequestedSuppliers.Init(false,6);S.InitialSupplierCounts={10000,10000,0,0,0,0};
    for(int32 I=0;I<6;++I)
    {
        const FString Id=FString::Printf(TEXT("supply-%c"),TCHAR('a'+I));
        S.RequestedSuppliers[I]=true;S.AppliedSupplierMask=1<<I;
        const auto E=UProcessUIStateAdapter::BuildEquipment(Id,&S,101);
        TestTrue(Id,E.Validity==EPlantValueValidity::Valid);
        TestEqual(TEXT("requested gate"),E.ValveRequestedOpen.Value,1.0);
        TestEqual(TEXT("applied gate"),E.ValveAppliedOpen.Value,1.0);
        TestEqual(TEXT("initial charge, including valid empty D-F"),E.InitialParticleCount.Value,double(S.InitialSupplierCounts[I]));
        TestTrue(TEXT("supplier current inventory remains unknown"),E.RegionParticleCount.Validity==EPlantValueValidity::Unknown);
        S.RequestedSuppliers[I]=false;
    }
    S.InWeighingHopper=123;
    FText Status,Quantity,Valve;
    UProcessUIStateAdapter::Format(UProcessUIStateAdapter::BuildEquipment(TEXT("weigher"),&S,101),Status,Quantity,Valve);
    TestTrue(TEXT("new level displays region count"),Quantity.ToString().Contains(TEXT("123개")));
    for(auto V:{EPlantValueValidity::Initializing,EPlantValueValidity::Error})
    {
        S.Validity=V;
        UProcessUIStateAdapter::Format(UProcessUIStateAdapter::BuildEquipment(TEXT("supply-f"),&S,101),Status,Quantity,Valve);
        TestEqual(TEXT("reset/error clears prior value"),Quantity.ToString(),FString(TEXT("미확인")));
    }
    S.Validity=EPlantValueValidity::Valid;
    TestTrue(TEXT("F stale heartbeat distinguished"),UProcessUIStateAdapter::BuildEquipment(TEXT("supply-f"),&S,103).Validity==EPlantValueValidity::Stale);
    S.RequestedSuppliers.SetNum(3);
    TestTrue(TEXT("legacy D remains unsupported"),UProcessUIStateAdapter::BuildEquipment(TEXT("supply-d"),&S,101).Validity==EPlantValueValidity::Unsupported);
    S.RequestedSuppliers.Empty();S.Validity=EPlantValueValidity::Error;
    TestTrue(TEXT("missing data exposes error rather than unsupported or old values"),UProcessUIStateAdapter::BuildEquipment(TEXT("supply-f"),&S,101).Validity==EPlantValueValidity::Error);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessSixGateStopTest,"ProcessUI.LowPoly.SixGateStop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessSixGateStopTest::RunTest(const FString&)
{
    FPlantSimulationSnapshot S;
    S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Running;
    S.Epoch=1;S.Sequence=1;S.PublishedWallSeconds=100;S.RequestedRunning=true;
    S.RequestedSuppliers.Init(false,6);S.AppliedSupplierMask=32;S.AppliedRPMValid=true;
    FProcessControlMachine M;M.Connect(S);M.ManualGranted=true;
    M.TimeoutPolicy=EProcessTimeoutPolicy::Freeze;
    int64 Rev=0;TArray<EProcessCommand> Sent;
    FProcessControlTransport Send=[&](EProcessCommand C,const FString&,double,int64,int64& R,FString&){Sent.Add(C);R=++Rev;return true;};
    const auto Stop=M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::StopAll,0,100,Send);
    TestTrue(TEXT("six-gate full stop accepted"),Stop.IsPending());
    S.Sequence++;S.ProcessedControlRevision=Rev;M.Observe(S,100.1,Send);
    TestFalse(TEXT("open F prevents stop completion"),M.StopStage==EProcessStopStage::Complete);
    TestEqual(TEXT("pause not dispatched while F remains open"),Sent.Num(),1);
    TestTrue(TEXT("unconfirmed F named"),M.UnconfirmedEquipment().Contains(TEXT("공급 F")));
    S.AppliedSupplierMask=0;S.Sequence++;M.Observe(S,100.2,Send);
    TestTrue(TEXT("pause only after all six applied closed and RPM zero"),Sent.Last()==EProcessCommand::Pause);
    TestFalse(TEXT("pause request is not pause confirmation"),M.StopStage==EProcessStopStage::Complete);
    S.Execution=EPlantExecutionState::Paused;S.RequestedRunning=false;S.ProcessedControlRevision=Rev;S.Sequence++;
    M.Observe(S,100.3,Send);
    TestTrue(TEXT("worker response completes stop"),M.StopStage==EProcessStopStage::Complete);
    return true;
}
#endif
