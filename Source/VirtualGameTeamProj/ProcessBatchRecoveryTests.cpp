#include "ProcessAutoRun.h"
#include "PlantSupplyABatch.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace {
struct FRecoveryFixture {
 FProcessControlMachine M;FProcessAutoRun A;
 FRecoveryFixture(){
  auto& S=M.Snapshot;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Paused;S.Epoch=7;S.Sequence=20;S.PublishedWallSeconds=100;S.RequestedRunning=false;
  S.RequestedSuppliers.Init(false,6);S.AppliedRPMValid=S.AppliedPressureValid=true;S.SupplyAPurge.Valid=S.SupplyAPurge.Supported=true;
  S.Recovery.Supported=S.Recovery.Valid=true;S.Recovery.Materials.SetNum(3);S.ModelParticleMassKg=.001;
  auto& B=S.SupplyABatch;B.Valid=B.Supported=true;B.CaptureRevision=8;B.InitialCount=B.TotalCount=10;B.InitialKg=B.TotalKg=.01;B.SourceCount=5;B.ConnectorCount=3;B.WeighingCount=2;
  M.Connected=true;M.Connection=3;M.ManualGranted=true;M.StopStage=EProcessStopStage::Complete;
  A.State.RunId=FGuid::NewGuid();A.State.Epoch=7;A.State.Connection=3;A.State.RecoveryPending=true;A.State.BatchRevision=8;A.State.BatchMode=EProcessBatchMode::TargetMass;
  A.State.LockedTargetAKg=.006;A.State.LockedToleranceAKg=.001;A.State.MixingDurationSeconds=6;A.State.TargetMixerRPM=15;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecoveryRemaining,"ProcessUI.Recovery.RemainingSupplyAndExplicitResume",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecoveryRemaining::RunTest(const FString&){
 FRecoveryFixture F;TestTrue(TEXT("closed stopped batch is recoverable"),F.A.CanRecover(F.M,100).IsEmpty());
 int Calls=0;auto Send=[&](EProcessCommand,const FString&,double){++Calls;return FProcessCommandRecord();};
 F.A.Tick(F.M,100,Send);TestEqual(TEXT("query does not command"),Calls,0);
 F.M.Snapshot.Execution=EPlantExecutionState::Running;F.M.Snapshot.RequestedRunning=true;F.A.Tick(F.M,100,Send);TestEqual(TEXT("calculation resume never restores automation"),Calls,0);TestFalse(TEXT("must stop again after manual action"),F.A.CanRecover(F.M,100).IsEmpty());
 F.M.Snapshot.Execution=EPlantExecutionState::Paused;F.M.Snapshot.RequestedRunning=false;
 F.A.State.LockedTransferPressurePa.Z=-500;F.A.Settings.TransferPressurePa.Z=-1000;
 F.A.Settings.TargetMassKg=.009;F.A.Settings.MixingDurationSeconds=123;
 TestTrue(TEXT("explicit resume accepted"),F.A.Recover(F.M,100));TestEqual(TEXT("locked original target retained after next-recipe editing"),F.A.State.TargetMassKg,.006);TestEqual(TEXT("locked mix duration retained"),F.A.State.MixingDurationSeconds,6.);TestTrue(TEXT("passed connector included; no reopen"),F.A.State.RecoveryNext==EProcessAutoStage::RecoveryClearPressure);
 TestEqual(TEXT("recovery preserves the original pressure recipe"),F.A.State.LockedTransferPressurePa,FVector(0,500,-500));
 TestEqual(TEXT("remaining target from WH plus connector"),F.A.State.RemainingSupplyKg,.001);
 TestFalse(TEXT("duplicate resume no new run"),F.A.Recover(F.M,100));TestEqual(TEXT("one recovery"),F.A.State.RecoveryCount,1);
 FRecoveryFixture TooMuch;TooMuch.M.Snapshot.SupplyABatch.WeighingCount=8;TooMuch.M.Snapshot.SupplyABatch.ConnectorCount=0;TooMuch.M.Snapshot.SupplyABatch.SourceCount=2;
 TestFalse(TEXT("out-of-tolerance material blocked before outlet opening"),TooMuch.A.CanRecover(TooMuch.M,100).IsEmpty());
 FRecoveryFixture Reset;Reset.M.Snapshot.Epoch++;TestFalse(TEXT("old epoch cannot recover"),Reset.A.Recover(Reset.M,100));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecoveryPartial,"ProcessUI.Recovery.PartialArrivalKeepsCohort",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecoveryPartial::RunTest(const FString&){try{
 powder::GranularConfig C;C.plant=true;C.radius=.002f;C.count=4;
 const auto Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
 FPlantTransferSettings Limits;FPlantTransferTelemetry Mass(C,Routes,Limits);FPlantFeedPreparation Prep(C,Routes,Mass,.5,true);
 FPlantSupplyABatch B(C,{0,0,0,1},Mass,Prep,true);std::vector<powder::Grain> P{{-1.2f,1.4f,2.4f,0},{-1.2f,1.4f,2.5f,0},{-1.2f,1.4f,2.6f,0},{0,1.4f,2.5f,0}};
 TestTrue(TEXT("initial cohort captured"),B.Capture(P,0,8));P[0]={0,0,1,0};P[1]={.01f,0,1,0};B.Observe(P,.1);B.Observe(P,.7);
 TestTrue(TEXT("actual dose finalized"),B.FinalizeDose(P,.7,8,9,Mass.ParticleMassKg*2,Mass.ParticleMassKg*.5));
 Mass.Begin(P,.7,10,false);P[0]={3,0,2.7f,0};Mass.Observe(P,.8,false,false);B.Observe(P,.8);
 FPlantSimulationSnapshot Before;Mass.Publish(Before);const auto IDs=B.Recovery.Materials[0].SelectedIds;
 TestTrue(TEXT("worker admits stopped partial arrival"),B.AdmitRecovery(P,.8,8,11,FGuid::NewGuid()));
 TestTrue(TEXT("partial WH plus arrived RH keeps original cohort"),B.VerifyOpening(P,.8,8));Mass.Begin(P,.8,12,false);
 FPlantSimulationSnapshot After;Mass.Publish(After);TestEqual(TEXT("same transfer id"),After.Transfer.Id,Before.Transfer.Id);TestEqual(TEXT("same baseline"),After.Transfer.BaselineKg,Before.Transfer.BaselineKg);TestEqual(TEXT("existing arrival retained"),After.Transfer.ArrivedKg,Mass.ParticleMassKg);
 TestTrue(TEXT("same immutable IDs"),IDs==B.Recovery.Materials[0].SelectedIds);B.RecoveryRevoked();TestFalse(TEXT("cancelled ticket cannot reopen partial cohort"),B.VerifyOpening(P,.9,8));
 auto Mixer=B;auto MP=P;MP[1]={3,.01f,2.7f,0};Mixer.Observe(MP,.95);
 TestTrue(TEXT("first mixer feed admits intact RH dose"),Mixer.BeginMixerFeed(MP,.95,8,20));MP[0]={3,0,1.15f,0};Mixer.Observe(MP,1);Mixer.ReceiverClosed(1.01,21);
 TestTrue(TEXT("partial mixer arrival admitted after actual close"),Mixer.AdmitRecovery(MP,1.02,8,22,FGuid::NewGuid()));
 TestTrue(TEXT("receiver reopened only for remaining cohort"),Mixer.BeginMixerFeed(MP,1.02,8,23));TestEqual(TEXT("mixer baseline unchanged"),Mixer.MixerSnapshot().BaselineKg,2*Mass.ParticleMassKg);
 MP[1]={3,.01f,1.15f,0};Mixer.Observe(MP,1.1);Mixer.Observe(MP,1.61);Mixer.ReceiverClosed(1.62,24);
 TestTrue(TEXT("full mixer dose stable"),Mixer.MixerSnapshot().State==EPlantTransferState::Complete);TestTrue(TEXT("mix only after actual reclosure"),Mixer.AdmitMixerStart(MP,1.63,8,25));
 P[3]={.02f,0,1,0};B.Observe(P,1);TestEqual(TEXT("actual foreign position recorded with immutable source"),B.Recovery.ForeignSuppliers[0],1);TestEqual(TEXT("actual foreign ID"),B.Recovery.ForeignIds[0],3);
 P[3]={0,1.4f,2.5f,0};B.Observe(P,1.1);TestEqual(TEXT("foreign evidence sticky after leaving"),B.Recovery.ForeignIds.Num(),1);TestFalse(TEXT("recovery rejects contaminated history"),B.AdmitRecovery(P,1.1,8,13,FGuid::NewGuid()));
 B.ObserveMix(2,true,15);B.ObserveMix(2.2,true,15);B.ObserveMix(2.3,false,0);B.ObserveMix(10,false,15);B.ObserveMix(11,true,15);B.ObserveMix(11.1,true,15);
 TestTrue(TEXT("only actual automatic mix intervals accrued"),FMath::Abs(B.Recovery.AutomaticMixSeconds-.4)<1e-9);B.ObserveMix(11.1,true,15);TestTrue(TEXT("duplicate sim time not counted"),FMath::Abs(B.Recovery.AutomaticMixSeconds-.4)<1e-9);
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecoveryReservation,"ProcessUI.Recovery.ExpiredReservationAndMissingIds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecoveryReservation::RunTest(const FString&){
 FRecoveryFixture F;F.A.State.AdditiveEnabled=F.A.State.AdditiveScheduled=true;F.A.State.MixerAppliedSimulationSeconds=1;F.A.State.AdditiveDueSeconds=5;F.A.State.AdditiveDelaySeconds=4;F.M.Snapshot.PhysicalSeconds=6;
 TestFalse(TEXT("late unresolved reservation cannot auto inject"),F.A.CanRecover(F.M,100).IsEmpty());
 FRecoveryFixture Missing;Missing.M.Snapshot.SupplyABatch.DoseFinalized=true;Missing.M.Snapshot.SupplyABatch.SourceCount=Missing.M.Snapshot.SupplyABatch.ConnectorCount=0;
 TestFalse(TEXT("missing finalized IDs blocks recovery"),Missing.A.CanRecover(Missing.M,100).IsEmpty());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecoveryStopAck,"ProcessUI.Recovery.FreshZeroRPMAcknowledgement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecoveryStopAck::RunTest(const FString&){
 FRecoveryFixture F;auto& S=F.M.Snapshot;S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;
 F.M.AutomationDispatch=true;int Calls=0;FProcessControlTransport Send=[&](EProcessCommand,const FString&,double,int64,int64& R,FString&){++Calls;R=22;return true;};
 auto R=F.M.Submit(FGuid::NewGuid(),S.Epoch,TEXT("mixer"),EProcessCommand::MixerEnabled,0,100,Send);
 TestEqual(TEXT("already-zero automatic stop still issues this request"),Calls,1);TestTrue(TEXT("waits for new worker ack"),R.Status==EProcessCommandStatus::Processing);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecoveryArrivalRace,"ProcessUI.Recovery.ArrivedDuringPreparationKeepsStabilityAndUIAck",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecoveryArrivalRace::RunTest(const FString&){
 FRecoveryFixture F;auto& S=F.M.Snapshot;auto& A=F.A.State;auto& B=S.SupplyABatch;auto& T=S.Transfer;
 A.Active=true;A.Stage=EProcessAutoStage::OpenWeigher;A.RecoveryCount=1;A.BatchCount=10;A.BatchKg=.01;A.TransferId=7;
 B.DoseFinalized=true;B.SourceCount=B.ConnectorCount=B.WeighingCount=B.PipeCount=0;B.ReceiverCount=10;
 S.Recovery.Ticket=11;S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;S.RequestedHopperOpen=true;
 T.Validity=EPlantValueValidity::Valid;T.Id=7;T.BaselineKg=T.ArrivedKg=.01;T.State=EPlantTransferState::Moving;T.StableSeconds=.2;T.Criteria.StableSimulationSeconds=.5;
 int Calls=0;FProcessAutoCommand Send=[&](EProcessCommand,const FString&,double){++Calls;return FProcessCommandRecord();};
 F.A.Tick(F.M,100,Send);TestTrue(TEXT("arrival cancels unnecessary open, even with empty weigher"),A.Stage==EProcessAutoStage::RecoveryCloseOutlet);TestTrue(TEXT("batch remains active"),A.Active);TestEqual(TEXT("no reopen issued"),Calls,0);
 A.Stage=EProcessAutoStage::Transferring;S.RequestedHopperOpen=false;F.A.Tick(F.M,100,Send);
 TestTrue(TEXT("closed-outlet arrival waits existing stable duration"),A.Stage==EProcessAutoStage::Transferring&&A.Active);
 T.State=EPlantTransferState::Complete;T.StableSeconds=.5;F.A.Tick(F.M,100,Send);TestTrue(TEXT("still requires actual UI completion ack"),A.Stage==EProcessAutoStage::Transferring);
 F.A.ConfirmUI(S.Epoch,T.Id,S.PhysicalSeconds);F.A.Tick(F.M,100,Send);TestTrue(TEXT("then progresses without recapturing baseline"),A.Stage==EProcessAutoStage::CloseWeigher);TestEqual(TEXT("original id kept"),T.Id,int64(7));TestEqual(TEXT("original mass kept"),T.BaselineKg,.01);return true;
}
#endif
