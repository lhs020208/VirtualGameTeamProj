#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ProcessWebCommands.h"
#include "ProcessControlService.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWebManualContract,"ProcessWeb.Manual.Contract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWebManualContract::RunTest(const FString&)
{
 auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("command_version"),1);J->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));J->SetStringField(TEXT("run_id"),TEXT("cpu-run"));J->SetStringField(TEXT("epoch"),TEXT("9007199254740993"));J->SetStringField(TEXT("lease_id"),TEXT("cpu-lease"));J->SetStringField(TEXT("equipment"),TEXT("receiver"));J->SetStringField(TEXT("kind"),TEXT("pressure"));J->SetStringField(TEXT("unit"),TEXT("kPa(g)"));J->SetNumberField(TEXT("value"),-.5);
 ProcessWebCommands::FCommand C;FString Why;TestTrue(TEXT("valid command decoded"),ProcessWebCommands::Decode(J,C,Why));TestEqual(TEXT("single exact pressure conversion"),C.Value,-500.);TestEqual(TEXT("epoch lossless"),C.Epoch,9007199254740993LL);
 J->SetNumberField(TEXT("value"),.1);TestFalse(TEXT("positive receiver gauge refused"),ProcessWebCommands::Decode(J,C,Why));J->SetNumberField(TEXT("value"),-.5);J->SetStringField(TEXT("unit"),TEXT("Pa"));TestFalse(TEXT("wrong unit refused"),ProcessWebCommands::Decode(J,C,Why));J->SetStringField(TEXT("unit"),TEXT("kPa(g)"));J->SetStringField(TEXT("kind"),TEXT("automatic_start"));TestFalse(TEXT("future commands refused"),ProcessWebCommands::Decode(J,C,Why));
 auto* S=NewObject<UProcessControlService>();const double Now=FPlatformTime::Seconds();S->State.Connected=true;S->State.Snapshot.Epoch=5;S->WebLeaseEpoch=5;S->WebLeaseId=TEXT("lease");S->WebLeaseUntil=Now+10;
 ProcessWebCommands::FBridge Bridge;
 S->State.StopStage=EProcessStopStage::Complete;
 TestTrue(TEXT("confirmed stop has no pending-device diagnostic"),Bridge.Describe(S,true,Now)->GetStringField(TEXT("unconfirmed")).IsEmpty());
 S->State.StopStage=EProcessStopStage::Pausing;
 TestFalse(TEXT("pending stop still reports unconfirmed devices"),Bridge.Describe(S,true,Now)->GetStringField(TEXT("unconfirmed")).IsEmpty());
 S->State.StopStage=EProcessStopStage::None;
 TestTrue(TEXT("lease owns one operator"),S->WebOwnsControl());auto R=S->Request(TEXT("receiver"),EProcessCommand::Valve,1);TestEqual(TEXT("local command fenced at service"),R.Status,EProcessCommandStatus::Rejected);TestTrue(TEXT("visible owner reason"),R.Message.Contains(TEXT("웹")));
 S->AutoRun.State.Active=true;auto Stale=S->RequestWeb(FGuid::NewGuid(),4,TEXT("lease"),TEXT("plant"),EProcessCommand::StopAll,0);TestEqual(TEXT("old epoch stop rejected without cancellation"),Stale.Status,EProcessCommandStatus::Rejected);TestTrue(TEXT("stale stop does not cancel batch"),S->AutoRun.State.Active);
 auto Auto=S->RequestWeb(FGuid::NewGuid(),5,TEXT("lease"),TEXT("receiver"),EProcessCommand::Pressure,-500);TestEqual(TEXT("stage3 cannot use stage5 automatic pressure path"),Auto.Status,EProcessCommandStatus::Rejected);S->AutoRun.State.Active=false;
 auto WrongLease=S->RequestWeb(FGuid::NewGuid(),5,TEXT("other"),TEXT("plant"),EProcessCommand::Resume,0);TestEqual(TEXT("wrong lease rejected"),WrongLease.Status,EProcessCommandStatus::Rejected);
 S->WebLeaseUntil=Now-1;TestFalse(TEXT("wall lease expiry independent of sim time"),S->WebOwnsControl());S->WebLeaseUntil=Now+10;S->State.Snapshot.Epoch=6;TestFalse(TEXT("reset invalidates ownership"),S->WebOwnsControl());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWebManualApplied,"ProcessWeb.Manual.AppliedAcknowledgement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWebManualApplied::RunTest(const FString&)
{
 FPlantSimulationSnapshot S;S.Epoch=1;S.Sequence=1;S.Validity=EPlantValueValidity::Valid;S.PublishedWallSeconds=100;S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;S.RequestedSuppliers.Init(false,6);S.PressureControlEnabled=true;S.AppliedPressureValid=true;
 FProcessControlMachine M;M.Connect(S);M.ManualGranted=true;int Calls=0;auto Send=[&](EProcessCommand,const FString&,double,int64,int64& Revision,FString&){++Calls;Revision=7;return true;};
 const auto Id=FGuid::NewGuid();auto R=M.Submit(Id,1,TEXT("receiver"),EProcessCommand::Pressure,-500,100,Send);TestTrue(TEXT("acceptance is not application"),R.IsPending());M.Submit(Id,1,TEXT("receiver"),EProcessCommand::Pressure,-500,100,Send);TestEqual(TEXT("duplicate dispatch once"),Calls,1);
 S.Sequence=2;S.ProcessedControlRevision=7;S.RequestedGaugePressurePa.Z=-500;S.PressureAppliedRevision=6;S.PublishedWallSeconds=100.1;M.Observe(S,100.1,Send);TestTrue(TEXT("worker reception without actual pressure is pending"),M.History[Id].IsPending());
 S.Sequence=3;S.PressureAppliedRevision=7;S.AppliedGaugePressurePa.Z=-500;S.PublishedWallSeconds=100.2;M.Observe(S,100.2,Send);TestEqual(TEXT("worker confirmed result applied"),M.History[Id].Status,EProcessCommandStatus::Applied);
 S.Epoch=2;S.Sequence=1;S.PublishedWallSeconds=100.3;M.Observe(S,100.3,Send);M.Submit(Id,1,TEXT("receiver"),EProcessCommand::Pressure,-500,100.3,Send);TestEqual(TEXT("old id never replays after reset"),Calls,1);return true;
}
#endif
