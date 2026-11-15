#include "ProcessAutoRun.h"
#include "PlantSupplyABatch.h"
#include "PlantDoseValidation.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/Paths.h"
#include <limits>
namespace {
struct FAutoFixture {
 FProcessControlMachine M;FProcessAutoRun A;double Now=100;int64 Revision=0;
 struct FCall{EProcessCommand C;FString Id;double V;int64 Revision;};TArray<FCall> Sent,Queued;
 FProcessControlTransport Transport;FProcessAutoCommand Send;
 FAutoFixture(){
  A.Settings.MixingDurationSeconds=.2; // Explicit protocol trial value, never a production default.
  auto& S=M.Snapshot;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Paused;S.Epoch=1;S.Sequence=1;S.PublishedWallSeconds=Now;
  S.SourceActorPath=TEXT("/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111.L_ProcessPlant_LowPoly_1111:PersistentLevel.PowderPlantTest_0");S.GPUDevice=TEXT("protocol fixture (not GPU)");
  S.RequestedSuppliers.Init(false,6);S.AppliedRPMValid=S.AppliedPressureValid=S.PressureControlEnabled=true;S.RequestedRunning=false;
  S.SupplyAPurge.Supported=S.SupplyAPurge.Valid=true;S.FeedPreparation.Enabled=true;
  S.WeighingMass.Validity=S.PipeMass.Validity=S.ReceiverMass.Validity=EPlantValueValidity::Valid;
  S.SupplyABatch.Supported=S.SupplyABatch.Valid=true;S.SupplyABatch.TotalCount=S.SupplyABatch.SourceCount=3;S.SupplyABatch.TotalKg=.00123;S.ModelParticleMassKg=.00123/3;S.SupplyABatch.InitialKg=.00123;S.SupplyABatch.InitialCount=S.SupplyABatch.InventorySourceCount=3;
  S.Transfer.Criteria.StableSimulationSeconds=.5;S.MixerFeed.Supported=S.MixerFeed.Valid=true;S.RequestedRPM=15;
  auto Initial=S;M.Connect(Initial);M.TimeoutPolicy=EProcessTimeoutPolicy::Freeze;M.PausePolicy=EProcessPausePolicy::CloseThenPause;M.PausedClosePolicy=EProcessPausedClosePolicy::StopAll;
  Transport=[this](EProcessCommand C,const FString& Id,double V,int64,int64& R,FString&){R=++Revision;FCall Call{C,Id,V,R};Sent.Add(Call);Queued.Add(Call);return true;};
  Send=[this](EProcessCommand C,const FString& Id,double V){TGuardValue<bool> Auto(M.AutomationDispatch,true);return M.Submit(FGuid::NewGuid(),A.State.Epoch,Id,C,V,Now,Transport);};
 }
 bool Start(){return A.Start(FGuid::NewGuid(),M,Now);}
 void Tick(double Sim=.02){
  auto S=M.Snapshot;
  auto Pending=MoveTemp(Queued);Queued.Empty();
  for(const auto& C:Pending){S.RequestedControlRevision=S.ProcessedControlRevision=C.Revision;
   switch(C.C){
   case EProcessCommand::StopAll:S.RequestedSuppliers.Init(false,6);S.AppliedSupplierMask=0;S.RequestedHopperOpen=S.AppliedHopperOpen=S.RequestedReceiverOpen=S.AppliedReceiverOpen=false;S.AppliedRPM=0;S.RequestedMixerEnabled=S.WorkerMixerEnabled=false;S.RequestedGaugePressurePa=S.AppliedGaugePressurePa=FVector::ZeroVector;S.PressureAppliedRevision=C.Revision;S.SupplyAPurge.RequestedEnabled=S.SupplyAPurge.AppliedEnabled=false;S.SupplyAPurge.RequestedGaugePa=S.SupplyAPurge.AppliedGaugePa=S.SupplyAPurge.FlowM3s=0;S.SupplyAPurge.AppliedRevision=C.Revision;S.RequestedRunning=true;S.Execution=EPlantExecutionState::Running;break;
   case EProcessCommand::BeginB:{S.SequentialBatch.A.ReceiverCount=S.SequentialBatch.A.SelectedCount;S.SequentialBatch.ActiveSupplier=1;S.SequentialBatch.TransitionRevision=C.Revision;
    S.SupplyABatch=FPlantABatchSnapshot();auto& B=S.SupplyABatch;B.Supported=B.Valid=true;B.CaptureRevision=C.Revision;B.TotalCount=B.InitialCount=B.SourceCount=B.InventorySourceCount=3;B.TotalKg=B.InitialKg=3*S.ModelParticleMassKg;S.Transfer=FPlantTransferSnapshot();S.Transfer.Criteria.StableSimulationSeconds=.5;break;}
   case EProcessCommand::CombineAB:{auto& AB=S.SequentialBatch;AB.Combined=true;AB.TransitionRevision=C.Revision;S.SupplyABatch.CaptureRevision=C.Revision;S.SupplyABatch.TotalCount=S.SupplyABatch.ReceiverCount=AB.A.SelectedCount+AB.B.SelectedCount;S.SupplyABatch.TotalKg=AB.A.FinalKg+AB.B.FinalKg;break;}
   case EProcessCommand::CaptureABatch:S.SupplyABatch.CaptureRevision=C.Revision;S.SupplyABatch.CapturedSimulationSeconds=S.PhysicalSeconds;break;
   case EProcessCommand::Resume:S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;break;
   case EProcessCommand::Pause:S.Execution=EPlantExecutionState::Paused;S.RequestedRunning=false;break;
   case EProcessCommand::Pressure:{int I=PlantControl::PressureTarget(C.Id);S.RequestedGaugePressurePa[I]=S.AppliedGaugePressurePa[I]=C.V;S.PressureAppliedRevision=C.Revision;break;}
   case EProcessCommand::PurgePressure:{auto& Q=C.Id==TEXT("supply-b")?S.SupplyBPurge:S.SupplyAPurge;Q.RequestedGaugePa=Q.AppliedGaugePa=C.V;Q.AppliedRevision=C.Revision;break;}
   case EProcessCommand::PurgeEnabled:{auto& Q=C.Id==TEXT("supply-b")?S.SupplyBPurge:S.SupplyAPurge;Q.RequestedEnabled=Q.AppliedEnabled=C.V!=0;Q.FlowM3s=C.V!=0?.05:0;if(C.V==0)Q.RequestedGaugePa=Q.AppliedGaugePa=0;Q.AppliedRevision=C.Revision;break;}
   case EProcessCommand::MixerRPM:S.RequestedRPM=S.WorkerTargetRPM=C.V;break;
   case EProcessCommand::MixerEnabled:S.RequestedMixerEnabled=S.WorkerMixerEnabled=C.V!=0;S.AppliedRPM=C.V!=0?S.RequestedRPM:0;if(C.V!=0){S.MixerFeed.MixerStartRevision=S.MixerFeed.MixerAppliedRevision=C.Revision;S.MixerFeed.MixerStartSimulationSeconds=S.MixerFeed.MixerAppliedSimulationSeconds=S.PhysicalSeconds;}break;
   case EProcessCommand::FinalizeADose:{auto& B=S.SupplyABatch;B.DoseFinalKg=B.WeighingCount*S.ModelParticleMassKg;B.DoseFinalSimulationSeconds=S.PhysicalSeconds;
    if(FMath::Abs(B.DoseFinalKg-A.State.TargetMassKg)>A.State.WeighingToleranceKg+1e-12){B.DoseRejectedRevision=C.Revision;B.DoseReason=TEXT("계량 범위 벗어남");}
    else{B.DoseFinalized=true;B.DoseFinalizeRevision=C.Revision;B.TotalKg=B.DoseFinalKg;B.TotalCount=B.WeighingCount;B.InventorySourceCount=B.SourceCount;B.SourceCount=0;auto& Material=A.State.ActiveSupplier==1?S.SequentialBatch.B:S.SequentialBatch.A;Material.SelectedCount=B.TotalCount;Material.FinalKg=B.TotalKg;}break;}
   case EProcessCommand::Valve:
    if(C.Id==TEXT("supply-a")||C.Id==TEXT("supply-b")){const int I=C.Id==TEXT("supply-b")?1:0;S.RequestedSuppliers[I]=C.V!=0;S.AppliedSupplierMask=C.V!=0?(1<<I):0;if(C.V==0){S.SupplyABatch.SupplyClosedRevision=C.Revision;S.SupplyABatch.SupplyClosedSeconds=S.PhysicalSeconds;}}
    else if(C.Id==TEXT("weigher")){S.RequestedHopperOpen=S.AppliedHopperOpen=C.V!=0;if(C.V!=0){S.Transfer.Id=S.SequentialBatch.ActiveSupplier==1?12:11;S.Transfer.BaselineKg=S.SupplyABatch.TotalKg;S.Transfer.RemainingKg=S.Transfer.BaselineKg;S.Transfer.StartedSimulationSeconds=S.PhysicalSeconds;S.Transfer.State=EPlantTransferState::Moving;S.Transfer.Validity=EPlantValueValidity::Valid;S.FeedPreparation.RequestRevision=C.Revision;S.FeedPreparation.State=EPlantFeedPreparationState::Opened;S.FeedPreparation.OutletAppliedSimulationSeconds=S.PhysicalSeconds;}}
    else if(C.Id==TEXT("receiver")){S.RequestedReceiverOpen=S.AppliedReceiverOpen=C.V!=0;if(C.V!=0){S.MixerFeed.OpeningRevision=C.Revision;S.MixerFeed.StartedSimulationSeconds=S.PhysicalSeconds;S.MixerFeed.BaselineKg=S.SupplyABatch.TotalKg;S.MixerFeed.ReceiverKg=S.MixerFeed.BaselineKg;S.MixerFeed.State=EPlantTransferState::Moving;}else{S.MixerFeed.CloseRevision=C.Revision;S.MixerFeed.ClosedSimulationSeconds=S.PhysicalSeconds;}}
    break;
   default:break;}
  }
  if(S.Execution==EPlantExecutionState::Running){S.PhysicalSeconds+=Sim;S.SupplyABatch.WeighingStableSeconds+=Sim;}
  if(!Pending.IsEmpty()&&S.Execution==EPlantExecutionState::Running){
   S.AppliedRPMRevision=Pending.Last().Revision;S.AppliedRPMSimulationSeconds=S.PhysicalSeconds;
   for(const auto& C:Pending)if(C.C==EProcessCommand::MixerEnabled&&C.V!=0)S.MixerFeed.MixerAppliedSimulationSeconds=S.PhysicalSeconds;
  }
  S.Sequence++;Now+=.1;S.PublishedWallSeconds=Now;M.Observe(S,Now,Transport);A.Tick(M,Now,Send);
 }
 void ArmMixing(){
  Start();A.State.Stage=EProcessAutoStage::Mixing;A.State.DownstreamStarted=true;A.State.MixerAppliedSimulationSeconds=10;
  A.State.MixingDeadlineSimulationSeconds=10+A.State.MixingDurationSeconds;A.State.MixingElapsedSeconds=.05;
  A.State.MixingRemainingSeconds=A.State.MixingDurationSeconds-.05;A.State.BatchCount=3;A.State.BatchKg=.00123;
  auto& S=M.Snapshot;S.PhysicalSeconds=10.05;S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;
  S.RequestedMixerEnabled=S.WorkerMixerEnabled=true;S.AppliedRPM=S.WorkerTargetRPM=S.RequestedRPM=15;
  S.MixerFeed.State=EPlantTransferState::Complete;S.MixerFeed.MixerKg=.00123;S.SupplyABatch.SourceCount=0;S.SupplyABatch.MixerCount=3;
  Revision=21;S.RequestedControlRevision=S.ProcessedControlRevision=S.AppliedRPMRevision=21;
 }
 bool Until(EProcessAutoStage Stage){for(int I=0;I<150&&A.State.Stage!=Stage&&A.State.Active;++I)Tick();return A.State.Stage==Stage;}
 void FeedOut(){auto& S=M.Snapshot;S.SupplyABatch.SourceCount=0;S.SupplyABatch.ConnectorCount=3;S.SupplyAPurge.ResidualCount=3;S.SupplyAPurge.ResidualKg=S.SupplyABatch.TotalKg;}
 void Cleared(){auto& S=M.Snapshot;S.SupplyABatch.ConnectorCount=0;S.SupplyABatch.WeighingCount=3;S.SupplyAPurge.ResidualCount=0;S.SupplyAPurge.ResidualKg=0;S.WeighingMass.Kg=S.SupplyABatch.TotalKg;}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoIdentity,"ProcessUI.AutoA.BatchIdentityAndGate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAutoIdentity::RunTest(const FString&){try{
 powder::GranularConfig C;C.plant=true;C.radius=.002f;C.count=3;
 const FString Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
 FPlantTransferSettings Limits;FPlantTransferTelemetry Mass(C,Routes,Limits);FPlantFeedPreparation Prep(C,Routes,Mass,.5,true);
 FPlantSupplyABatch B(C,{0,0,1},Mass,Prep,true);std::vector<powder::Grain> P{{-1.2f,1.4f,2.4f,0},{-1.2f,1.4f,2.5f,0},{0,1.4f,2.5f,0}};
 B.Observe(P,0);TestTrue(TEXT("capture arbitrary A count without hardcoded mass"),B.Capture(P,0,8));TestEqual(TEXT("two labelled A IDs"),B.Snapshot().TotalCount,2);TestEqual(TEXT("actual model mass"),B.Snapshot().TotalKg,2*Mass.ParticleMassKg);
 P[0]={0,0,1,0};P[2]={.01f,0,1,0};B.Observe(P,.1);
 TestFalse(TEXT("same mass but wrong IDs cannot open"),B.VerifyOpening(P,.1,8));TestEqual(TEXT("foreign same-colour B detected"),B.Snapshot().ForeignCount,1);
 P[1]={.01f,0,1,0};P[2]={0,1.4f,2.5f,0};TestTrue(TEXT("exact initial A cohort admits"),B.VerifyOpening(P,.2,8));TestFalse(TEXT("old/wrong capture revision rejected"),B.VerifyOpening(P,.2,9));
 P[0]={3,0,2.7f,0};B.Observe(P,.3);TestEqual(TEXT("RH above A gate is receiver, not source"),B.Snapshot().ReceiverCount,1);TestEqual(TEXT("A all exited despite RH height"),B.Snapshot().SourceCount,0);
 TestFalse(TEXT("spent initial batch cannot be recaptured"),B.Capture(P,.3,9));
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoSequence,"ProcessUI.AutoA.SequenceAndUIAck",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAutoSequence::RunTest(const FString&){
 FAutoFixture F;TestTrue(TEXT("one start accepted from untouched paused state"),F.Start());auto Id=F.A.State.RunId;F.Start();TestTrue(TEXT("duplicate start keeps run identity"),Id==F.A.State.RunId);
 TestTrue(TEXT("manual route blocked"),F.M.Submit(FGuid::NewGuid(),1,TEXT("supply-b"),EProcessCommand::Valve,1,F.Now,F.Transport).Status==EProcessCommandStatus::Rejected);
 TestTrue(TEXT("feed reached through pressure/valve responses"),F.Until(EProcessAutoStage::Feeding));TestEqual(TEXT("dynamic three-particle batch"),F.A.State.BatchCount,3);TestEqual(TEXT("dynamic batch kg"),F.A.State.BatchKg,.00123);
 for(int I=0;I<4;++I)F.Tick();TestTrue(TEXT("does not close A until all target IDs exit"),F.A.State.Stage==EProcessAutoStage::Feeding);
 F.FeedOut();TestTrue(TEXT("closed A then independent air"),F.Until(EProcessAutoStage::Clearing));TestEqual(TEXT("A actual closed"),F.M.Snapshot.AppliedSupplierMask,0);TestTrue(TEXT("purge applied"),F.M.Snapshot.SupplyAPurge.AppliedEnabled);
 F.Tick();TestTrue(TEXT("residual prevents progression"),F.A.State.Stage==EProcessAutoStage::Clearing);
 F.Cleared();TestTrue(TEXT("air off before stable wait"),F.Until(EProcessAutoStage::WeighingStable));TestFalse(TEXT("purge actually off"),F.M.Snapshot.SupplyAPurge.AppliedEnabled);
 F.Tick(0);const double Stable=F.A.State.PreparationStableSeconds;for(int I=0;I<4;++I)F.Tick(0);TestEqual(TEXT("repeated time adds no stability"),F.A.State.PreparationStableSeconds,Stable);
 TestTrue(TEXT("existing preparation/open protocol reaches transfer"),F.Until(EProcessAutoStage::Transferring));TestTrue(TEXT("transfer pressures"),F.M.Snapshot.AppliedGaugePressurePa==FVector(0,500,-1000));
 auto& T=F.M.Snapshot.Transfer;T.State=EPlantTransferState::Complete;T.ArrivedKg=T.BaselineKg;T.RemainingKg=T.InTransitKg=T.UnclassifiedKg=0;T.StableSeconds=.5;
 F.M.Snapshot.SupplyABatch.WeighingCount=0;F.M.Snapshot.SupplyABatch.ReceiverCount=3;F.M.Snapshot.WeighingMass.Kg=0;
 F.Tick();TestTrue(TEXT("worker complete still waits for actual UI acknowledgement"),F.A.State.Stage==EProcessAutoStage::Transferring&&F.M.Snapshot.AppliedHopperOpen);
 F.A.ConfirmUI(99,11,F.M.Snapshot.PhysicalSeconds);F.Tick();TestTrue(TEXT("wrong epoch UI does not finish"),F.A.State.Stage==EProcessAutoStage::Transferring);
 F.A.ConfirmUI(1,11,F.M.Snapshot.PhysicalSeconds);TestTrue(TEXT("RH feed only after WH closure and zero pressures"),F.Until(EProcessAutoStage::FeedingMixer));
 TestFalse(TEXT("WH actual closed"),F.M.Snapshot.AppliedHopperOpen);TestEqual(TEXT("gravity feed all pressures zero"),F.M.Snapshot.AppliedGaugePressurePa,FVector::ZeroVector);
 for(int I=0;I<4;++I)F.Tick();TestTrue(TEXT("partial mixer arrival cannot close RH"),F.A.State.Stage==EProcessAutoStage::FeedingMixer);
 auto& MF=F.M.Snapshot.MixerFeed;MF.State=EPlantTransferState::Complete;MF.MixerKg=MF.BaselineKg;MF.ReceiverKg=MF.ConnectorKg=0;MF.StableSeconds=.5;
 F.M.Snapshot.SupplyABatch.ReceiverCount=0;F.M.Snapshot.SupplyABatch.MixerCount=3;
 TestTrue(TEXT("full arrival then closure then mixer application"),F.Until(EProcessAutoStage::Mixing));
 TestFalse(TEXT("RH actual closed before final stage"),F.M.Snapshot.AppliedReceiverOpen);TestTrue(TEXT("timed mixing remains automatic and locks conflicting commands"),F.A.State.Active&&F.M.AutomationActive);
 TestEqual(TEXT("existing target applied"),F.M.Snapshot.AppliedRPM,15.);
 TestTrue(TEXT("closure revision precedes mixer request"),F.A.State.ReceiverCloseRevision>0&&F.A.State.ReceiverCloseRevision<F.A.State.MixerRequestRevision);
 for(const auto& C:F.Sent)TestFalse(TEXT("never opens B-F"),C.C==EProcessCommand::Valve&&C.V==1&&C.Id.StartsWith(TEXT("supply-"))&&C.Id!=TEXT("supply-a"));
 TestTrue(TEXT("timed batch reaches stop-confirmed completion"),F.Until(EProcessAutoStage::Complete));
 TestFalse(TEXT("completed sequence no longer active"),F.A.State.Active);TestEqual(TEXT("batch complete RPM zero"),F.M.Snapshot.AppliedRPM,0.);
 TestTrue(TEXT("stop follows target duration"),F.A.State.MixerStopRequestedSimulationSeconds>=F.A.State.MixingDeadlineSimulationSeconds);
 TestTrue(TEXT("actual stop elapsed recorded"),F.A.State.MixingElapsedSeconds>=F.A.State.MixingDurationSeconds);
 TestFalse(TEXT("same spent batch cannot run again"),F.Start());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoCancel,"ProcessUI.AutoA.CancellationAndFailures",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAutoCancel::RunTest(const FString&){
 FAutoFixture F;F.Start();F.Until(EProcessAutoStage::Feeding);F.FeedOut();F.Until(EProcessAutoStage::Clearing);
 F.A.Cancel(TEXT("user whole stop"));F.M.AutomationActive=false;auto R=F.M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::StopAll,0,F.Now,F.Transport);
 for(int I=0;I<10;++I)F.Tick();TestTrue(TEXT("stop ack complete after gas zero and pause"),F.M.History[R.RequestId].Status==EProcessCommandStatus::Applied&&F.M.Snapshot.Execution==EPlantExecutionState::Paused&&F.M.Snapshot.SupplyAPurge.FlowM3s==0);
 F.M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::Resume,1,F.Now,F.Transport);F.Tick();const int Num=F.Sent.Num();for(int I=0;I<10;++I)F.Tick();TestEqual(TEXT("manual resume sends no cancelled step"),F.Sent.Num(),Num);TestFalse(TEXT("no delayed WH opening"),F.M.Snapshot.AppliedHopperOpen);
 for(int Mode=0;Mode<4;++Mode){FAutoFixture G;G.Start();G.Until(EProcessAutoStage::Feeding);
  if(Mode==0)++G.M.Snapshot.Epoch;else if(Mode==1)G.M.Connected=false;else if(Mode==2)G.M.Snapshot.SupplyABatch.ForeignCount=1;else G.A.Settings.StageWallSeconds=.001;
  G.Tick();if(Mode<2)TestTrue(TEXT("reset/detach invalidates sequence"),G.A.State.Stage==EProcessAutoStage::Cancelled&&!G.A.State.Active);
  else{for(int I=0;I<12;++I)G.Tick();TestTrue(TEXT("foreign/timeout invokes whole stop"),G.A.State.Stage==EProcessAutoStage::Failed&&G.M.Snapshot.Execution==EPlantExecutionState::Paused);}
 }
 FAutoFixture H;H.Start();H.Until(EProcessAutoStage::Feeding);H.FeedOut();H.Until(EProcessAutoStage::Clearing);for(int I=0;I<50&&H.A.State.Active;++I)H.Tick();TestTrue(TEXT("stagnant connector stops instead of silently skipping"),H.A.State.Stage==EProcessAutoStage::Failed||H.A.State.Stage==EProcessAutoStage::Stopping);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoMixerSafety,"ProcessUI.AutoA.MixerClosureAndCancellation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAutoMixerSafety::RunTest(const FString&){
 for(int Mode=0;Mode<3;++Mode){FAutoFixture F;F.Start();auto& S=F.M.Snapshot;
  F.A.State.Stage=EProcessAutoStage::CloseReceiver;F.A.State.DownstreamStarted=true;F.A.State.BatchCount=3;F.A.State.BatchKg=.00123;
  S.AppliedReceiverOpen=S.RequestedReceiverOpen=true;S.SupplyABatch.SourceCount=0;S.SupplyABatch.MixerCount=3;
  S.MixerFeed.State=EPlantTransferState::Complete;S.MixerFeed.OpeningRevision=1;S.MixerFeed.MixerKg=S.MixerFeed.BaselineKg=.00123;
  S.MixerFeed.StableSeconds=.5;S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;
  F.Tick(); // close request queued; no applied response yet
  TestEqual(TEXT("only receiver close sent"),F.Sent.Num(),1);
  TestTrue(TEXT("close command target"),F.Sent[0].C==EProcessCommand::Valve&&F.Sent[0].Id==TEXT("receiver")&&F.Sent[0].V==0);
  if(Mode==0){F.Queued.Empty();for(int I=0;I<6;++I)F.Tick();TestTrue(TEXT("no mixer while close unacknowledged"),F.A.State.Stage==EProcessAutoStage::CloseReceiver);}
  else if(Mode==1){F.A.Cancel(TEXT("whole stop"));F.M.AutomationActive=false;F.M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::StopAll,0,F.Now,F.Transport);for(int I=0;I<10;++I)F.Tick();
   F.M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::Resume,1,F.Now,F.Transport);for(int I=0;I<10;++I)F.Tick();TestEqual(TEXT("resume cancelled batch RPM stays zero"),F.M.Snapshot.AppliedRPM,0.);}
  else{++F.M.Snapshot.Epoch;F.Tick();TestTrue(TEXT("epoch cancels pending mixer route"),F.A.State.Stage==EProcessAutoStage::Cancelled);}
  for(const auto& C:F.Sent)TestFalse(TEXT("unconfirmed/cancelled close never emits mixer start"),C.C==EProcessCommand::MixerEnabled&&C.V==1);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoMixerCohort,"ProcessUI.AutoA.MixerGeometryAndCohort",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAutoMixerCohort::RunTest(const FString&){try{
 powder::GranularConfig C;C.plant=true;C.radius=.002f;C.count=3;
 const FString Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
 FPlantTransferSettings Limits;FPlantTransferTelemetry Mass(C,Routes,Limits);FPlantFeedPreparation Prep(C,Routes,Mass,.5,true);FPlantMixerGeometry G(C,true);
 TestTrue(TEXT("active BVH matches compartment boundaries"),G.Error.IsEmpty());TestTrue(TEXT("inlet neck is connector not mixer"),G.Classify({3,0,1.29f,0})==EPlantMixerRegion::Connector);
 TestTrue(TEXT("outside actual vessel is not arrived"),G.Classify({3,.3f,1.1f,0})==EPlantMixerRegion::Outside);
 FPlantSupplyABatch B(C,{0,0,1},Mass,Prep,true);std::vector<powder::Grain> P{{-1.2f,1.4f,2.4f,0},{-1.2f,1.4f,2.5f,0},{3,.1f,1.15f,0}};
 TestTrue(TEXT("existing mixer inventory allowed but excluded"),B.Capture(P,0,8));TestEqual(TEXT("initial mixer count"),B.Snapshot().InitialMixerCount,1);
 P[0]={3,0,2.7f,0};P[1]={3,.01f,2.7f,0};TestTrue(TEXT("exact RH batch admits at worker boundary"),B.BeginMixerFeed(P,.1,8,9));
 P[0]={3,0,1.29f,0};B.Observe(P,.2);TestEqual(TEXT("neck residue tracked"),B.Snapshot().MixerConnectorCount,1);TestEqual(TEXT("old mixer excluded"),B.MixerSnapshot().MixerKg,0.);
 P[0]={3,0,1.15f,0};P[1]={3,.01f,1.15f,0};B.Observe(P,.3);TestEqual(TEXT("only cohort mixer mass"),B.MixerSnapshot().MixerKg,2*Mass.ParticleMassKg);
 B.Observe(P,.3);TestEqual(TEXT("paused/repeated time no stability accumulation"),B.MixerSnapshot().StableSeconds,0.);
 P[0].z=1.29f;B.Observe(P,.4);TestEqual(TEXT("backcross subtracts residence"),B.MixerSnapshot().MixerKg,Mass.ParticleMassKg);
 P[0].z=1.15f;B.Observe(P,.5);B.Observe(P,1.01);TestTrue(TEXT("full residence and existing stability complete"),B.MixerSnapshot().State==EPlantTransferState::Complete);
 // separate copy verifies rejection is sticky without destroying valid path
 auto Rejected=B;TestFalse(TEXT("no actual close proof denies mixer"),Rejected.AdmitMixerStart(P,1.02,8,10));
 B.ReceiverClosed(1.02,10);TestTrue(TEXT("actual close precedes mixer admission"),B.AdmitMixerStart(P,1.03,8,11));B.MixerApplied(1.04,11,15);
 TestEqual(TEXT("RPM applied revision"),B.MixerSnapshot().MixerAppliedRevision,int64(11));
 P[2].z=1.29f;B.Observe(P,1.05);TestTrue(TEXT("existing mixer material escaping invalidates"),B.MixerSnapshot().State==EPlantTransferState::Indeterminate);
 B.InvalidateCapture();TestEqual(TEXT("reset invalidates prior result"),B.MixerSnapshot().OpeningRevision,int64(0));
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimedMixClock,"ProcessUI.AutoA.TimedMixClockAndStop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTimedMixClock::RunTest(const FString&){
 FAutoFixture F;F.A.Settings.MixingDurationSeconds=0;TestFalse(TEXT("unset duration prevents start"),F.Start());
 FString Why;for(double V:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})TestFalse(TEXT("invalid duration rejected"),F.A.SetMixingDuration(V,Why));
 TestTrue(TEXT("explicit fractional seconds accepted"),F.A.SetMixingDuration(.2,Why));F.ArmMixing();
 TestFalse(TEXT("active duration change rejected"),F.A.SetMixingDuration(9,Why));F.A.Settings.MixingDurationSeconds=9;
 TestEqual(TEXT("batch duration captured once"),F.A.State.MixingDurationSeconds,.2);
 F.Tick(0);const double Before=F.A.State.MixingElapsedSeconds;
 for(int I=0;I<5;++I){F.Now+=.1;F.A.Tick(F.M,F.Now,F.Send);}TestEqual(TEXT("same snapshot adds no elapsed time"),F.A.State.MixingElapsedSeconds,Before);
 F.M.Snapshot.Execution=EPlantExecutionState::Paused;for(int I=0;I<5;++I)F.Tick(0);TestEqual(TEXT("paused simulation clock adds no mixing time"),F.A.State.MixingElapsedSeconds,Before);
 F.M.Snapshot.Execution=EPlantExecutionState::Running;F.Tick(.16);
 TestTrue(TEXT("target reached requests stop, not completion"),F.A.State.Stage==EProcessAutoStage::StopMixer&&F.A.State.Active);
 TestEqual(TEXT("one stop request"),F.Sent.Num(),1);TestTrue(TEXT("existing mixer stop command"),F.Sent[0].C==EProcessCommand::MixerEnabled&&F.Sent[0].V==0);
 const int64 Rev=F.A.State.MixerStopRequestRevision;F.A.Tick(F.M,F.Now,F.Send);TestEqual(TEXT("duplicate tick does not send stop twice"),F.Sent.Num(),1);
 F.Tick(.03);TestTrue(TEXT("new worker ack and zero RPM completes"),F.A.State.Stage==EProcessAutoStage::Complete&&!F.A.State.Active);
 TestEqual(TEXT("worker stop revision"),F.M.Snapshot.AppliedRPMRevision,Rev);TestEqual(TEXT("stopped clock is actual first zero batch"),F.A.State.MixerStoppedSimulationSeconds,F.M.Snapshot.AppliedRPMSimulationSeconds);
 TestEqual(TEXT("elapsed starts at actual RPM application, includes response delay"),F.A.State.MixingElapsedSeconds,.24,1e-9);
 TestEqual(TEXT("target-to-stop overshoot recorded"),F.A.State.MixingStopOverrunSeconds,.04,1e-9);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimedMixFaults,"ProcessUI.AutoA.TimedMixCancellationAndFaults",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTimedMixFaults::RunTest(const FString&){
 FAutoFixture F;F.ArmMixing();F.A.Cancel(TEXT("whole stop during mixing"));F.M.AutomationActive=false;
 F.M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::StopAll,0,F.Now,F.Transport);for(int I=0;I<6;++I)F.Tick();
 TestTrue(TEXT("cancelled batch whole stop acknowledged"),F.M.Snapshot.Execution==EPlantExecutionState::Paused&&F.M.Snapshot.AppliedRPM==0);
 const double Elapsed=F.A.State.MixingElapsedSeconds;F.M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::Resume,1,F.Now,F.Transport);
 const int Calls=F.Sent.Num();for(int I=0;I<20;++I)F.Tick(.1);
 TestTrue(TEXT("resume past old deadline stays cancelled"),F.A.State.Stage==EProcessAutoStage::Cancelled&&!F.A.State.Active&&F.M.Snapshot.AppliedRPM==0);
 TestEqual(TEXT("cancelled timer frozen"),F.A.State.MixingElapsedSeconds,Elapsed);TestEqual(TEXT("no delayed start or finish command"),F.Sent.Num(),Calls);
 for(int Mode=0;Mode<7;++Mode){FAutoFixture G;G.ArmMixing();
  if(Mode==0)G.M.Snapshot.AppliedRPM=14;
  if(Mode==1)G.M.Snapshot.AppliedRPMValid=false;
  if(Mode==2)G.Now+=3;
  if(Mode==3)G.M.Snapshot.Error=TEXT("worker fault");
  if(Mode==4)++G.M.Snapshot.Epoch;
  if(Mode==5)G.M.Connected=false;
  if(Mode==6){G.Tick(.2);G.Queued.Empty();G.Now+=11;G.Tick(0);}
  else G.A.Tick(G.M,G.Now,G.Send);
  TestTrue(TEXT("fault/cancel never completes timed batch"),G.A.State.Stage!=EProcessAutoStage::Complete);
  if(Mode==4||Mode==5)TestTrue(TEXT("epoch/detach cancels timer"),G.A.State.Stage==EProcessAutoStage::Cancelled);
  else TestTrue(TEXT("RPM/stale/error/stop timeout uses existing whole-stop failure"),G.A.State.Stage==EProcessAutoStage::Stopping||G.A.State.Stage==EProcessAutoStage::Failed);
 }
 FAutoFixture H;H.ArmMixing();H.Tick(.2);const int64 Requested=H.A.State.MixerStopRequestRevision;H.Queued.Empty();
 auto S=H.M.Snapshot;S.AppliedRPM=0;S.RequestedMixerEnabled=S.WorkerMixerEnabled=false;S.ProcessedControlRevision=S.RequestedControlRevision=Requested;S.AppliedRPMRevision=Requested-1;S.AppliedRPMSimulationSeconds=S.PhysicalSeconds;++S.Sequence;H.Now+=.1;S.PublishedWallSeconds=H.Now;
 H.M.Observe(S,H.Now,H.Transport);H.A.Tick(H.M,H.Now,H.Send);TestTrue(TEXT("previous revision zero cannot complete current batch"),H.A.State.Stage==EProcessAutoStage::Stopping);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDoseInputs,"ProcessUI.TargetDose.InputQuantization",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDoseInputs::RunTest(const FString&){
 const double M=.000040212391695907;const double Available=10000*M;
 TestTrue(TEXT("explicit exploratory partial target representable with tolerance"),PlantDose::Validate(.2,.002,M,Available).IsEmpty());
 TestTrue(TEXT("exact integral mass accepted"),PlantDose::Validate(5000*M,0,M,Available).IsEmpty());
 TestFalse(TEXT("unrepresentable zero tolerance rejected"),PlantDose::Validate(5000.5*M,0,M,Available).IsEmpty());
 TestFalse(TEXT("subparticle target not silently rounded"),PlantDose::Validate(.5*M,.1*M,M,Available).IsEmpty());
 TestFalse(TEXT("exceeds inventory rejected"),PlantDose::Validate(Available+M,M,M,Available).IsEmpty());
 for(double V:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})TestFalse(TEXT("invalid target rejected"),PlantDose::Validate(V,.0001,M,Available).IsEmpty());
 TestFalse(TEXT("negative tolerance rejected"),PlantDose::Validate(.2,-.001,M,Available).IsEmpty());
 FAutoFixture F;F.A.Settings.BatchMode=EProcessBatchMode::TargetMass;TestFalse(TEXT("unset target cannot start"),F.Start());
 F.A.Settings.TargetMassKg=.00082;F.A.Settings.WeighingToleranceKg=.000001;TestTrue(TEXT("valid target starts"),F.Start());
 F.A.Settings.TargetMassKg=.00123;TestEqual(TEXT("target captured for run"),F.A.State.TargetMassKg,.00082);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDoseWorker,"ProcessUI.TargetDose.WorkerCohortAndClosure",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDoseWorker::RunTest(const FString&){try{
 powder::GranularConfig C;C.plant=true;C.radius=.002f;C.count=4;
 const FString Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");
 C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
 FPlantTransferSettings Limits;FPlantTransferTelemetry Mass(C,Routes,Limits);FPlantFeedPreparation Prep(C,Routes,Mass,.5,true);
 FPlantSupplyABatch B(C,{0,0,0,1},Mass,Prep,true);const double M=Mass.ParticleMassKg;
 std::vector<powder::Grain> P{{-1.2f,1.4f,2.4f,0},{-1.2f,1.4f,2.5f,0},{-1.2f,1.4f,2.6f,0},{3,.1f,1.15f,0}};
 TestTrue(TEXT("original A plus preexisting mixer captured"),B.Capture(P,0,8));
 P[0]={0,0,1,0};B.SupplyCloseRequested(P,.1,9);P[1]={.01f,0,1,0};B.Observe(P,.2);TestEqual(TEXT("pending closure already publishes additional mass"),B.Snapshot().ClosingExtraKg,M);B.SupplyClosed(P,.2,9);
 TestEqual(TEXT("worker records closing extra actual mass"),B.Snapshot().ClosingExtraKg,M);
 TestEqual(TEXT("worker measures request time"),B.Snapshot().SupplyCloseRequestedSeconds,.1);TestEqual(TEXT("worker measures applied close time"),B.Snapshot().SupplyClosedSeconds,.2);
 B.Observe(P,.2);TestFalse(TEXT("no stability from repeated frame"),B.FinalizeDose(P,.2,8,10,2*M,0));
 B.Observe(P,.71);auto Bad=B;TestFalse(TEXT("out of range never selects or opens"),Bad.FinalizeDose(P,.71,8,11,M,0));TestTrue(TEXT("out of range reason"),Bad.Snapshot().DoseReason.Contains(TEXT("계량 범위 벗어남")));
 TestEqual(TEXT("rejected actual kg retained"),Bad.Snapshot().DoseFinalKg,2*M);TestEqual(TEXT("full inventory not silently rebased"),Bad.Snapshot().TotalCount,3);
 TestTrue(TEXT("exact measured subset finalized"),B.FinalizeDose(P,.71,8,11,2*M,0));TestEqual(TEXT("selected two IDs"),B.Snapshot().TotalCount,2);TestEqual(TEXT("unselected upstream kept separate"),B.Snapshot().InventorySourceCount,1);
 TestTrue(TEXT("duplicate immutable finalize"),B.FinalizeDose(P,.71,8,11,M,0));TestEqual(TEXT("duplicate cannot change baseline"),B.Snapshot().TotalKg,2*M);
 TestTrue(TEXT("existing preparation accepts measured cohort"),B.VerifyOpening(P,.72,8));
 P[0]={3,0,2.7f,0};P[1]={3,.01f,2.7f,0};TestTrue(TEXT("measured cohort continues RH feed"),B.BeginMixerFeed(P,.8,8,12));
 P[0]={3,0,1.15f,0};P[1]={3,.01f,1.15f,0};B.Observe(P,.9);B.Observe(P,1.41);
 TestEqual(TEXT("only measured A arrives; previous mixer excluded"),B.MixerSnapshot().MixerKg,2*M);TestTrue(TEXT("measured batch completes original stability"),B.MixerSnapshot().State==EPlantTransferState::Complete);
 P[2]={0,0,1,0};B.Observe(P,1.42);TestEqual(TEXT("late unselected A is foreign"),B.Snapshot().ForeignCount,1);TestTrue(TEXT("late material invalidates independent downstream tracking"),B.MixerSnapshot().State==EPlantTransferState::Indeterminate);
 B.InvalidateCapture();TestFalse(TEXT("reset clears selection"),B.Snapshot().DoseFinalized);TestEqual(TEXT("reset restores inventory cohort"),B.Snapshot().TotalCount,3);
 TestFalse(TEXT("stale capture cannot finalize after reset"),B.FinalizeDose(P,1.43,8,11,2*M,0));
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDoseSequence,"ProcessUI.TargetDose.SequenceAndRejection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDoseSequence::RunTest(const FString&){
 for(int Mode=0;Mode<4;++Mode){FAutoFixture F;F.A.Settings.BatchMode=EProcessBatchMode::TargetMass;F.A.Settings.TargetMassKg=.00082;F.A.Settings.WeighingToleranceKg=.000001;
  TestTrue(TEXT("partial begins"),F.Start());TestTrue(TEXT("supply open"),F.Until(EProcessAutoStage::Feeding));
  F.Tick();auto& S=F.M.Snapshot;S.SupplyABatch.SourceCount=2;S.SupplyABatch.ConnectorCount=1;S.SupplyAPurge.ResidualCount=1;
  F.Tick();TestTrue(TEXT("connector plus latest one-snapshot increment triggers early cutoff"),F.A.State.Stage==EProcessAutoStage::CloseA);
  TestEqual(TEXT("cutoff accounts connector"),F.A.State.CutoffConnectorKg,.00041);TestEqual(TEXT("one-snapshot lead separately recorded"),F.A.State.CutoffLeadKg,.00041);
  // One more particle passes during closure in this fixture; the worker subsequently measures two.
  S.SupplyABatch.SourceCount=1;S.SupplyABatch.ConnectorCount=2;S.SupplyAPurge.ResidualCount=2;
  TestTrue(TEXT("actual closure before air"),F.Until(EProcessAutoStage::Clearing));
  if(Mode>=2){
   if(Mode==2){F.A.Cancel(TEXT("stop"));F.M.AutomationActive=false;F.M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::StopAll,0,F.Now,F.Transport);}
   else ++F.M.Snapshot.Epoch;
   for(int I=0;I<10;++I)F.Tick();const int Num=F.Sent.Num();for(int I=0;I<10;++I)F.Tick();TestEqual(TEXT("cancel/reset cannot send finalization/open"),F.Sent.Num(),Num);TestFalse(TEXT("cancel leaves outlet closed"),F.M.Snapshot.AppliedHopperOpen);continue;
  }
  auto& T=F.M.Snapshot;T.SupplyABatch.ConnectorCount=0;T.SupplyABatch.WeighingCount=Mode==1?1:2;T.SupplyABatch.SourceCount=Mode==1?2:1;T.SupplyAPurge.ResidualCount=0;
  T.SupplyABatch.WeighingSetRevision++;T.SupplyABatch.WeighingStableSeconds=0;T.WeighingMass.Kg=T.SupplyABatch.WeighingCount*.00041;
  TestTrue(TEXT("purge off and stable wait"),F.Until(EProcessAutoStage::WeighingStable));
  for(int I=0;I<5;++I)F.Tick(0);TestTrue(TEXT("same simulation time never finalizes"),F.A.State.Stage==EProcessAutoStage::WeighingStable);
  if(Mode==1){F.Until(EProcessAutoStage::Failed);TestTrue(TEXT("out of range invokes whole stop"),F.A.State.Stage==EProcessAutoStage::Failed);TestFalse(TEXT("out of range WH closed"),F.M.Snapshot.AppliedHopperOpen);TestTrue(TEXT("failure preserves measured difference"),F.A.State.FinalDoseKg>0);}
  else{TestTrue(TEXT("subset accepted then existing transfer"),F.Until(EProcessAutoStage::Transferring));TestEqual(TEXT("baseline only measured kg"),F.A.State.BatchKg,.00082);TestEqual(TEXT("upstream residue retained"),F.M.Snapshot.SupplyABatch.InventorySourceCount,1);TestTrue(TEXT("dose confirmed flag"),F.A.State.DoseFinalized);}
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDoseDrainable,"ProcessUI.TargetDose.DynamicGateDrainReserve",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDoseDrainable::RunTest(const FString&){
 FAutoFixture F;F.A.Settings.BatchMode=EProcessBatchMode::TargetMass;F.A.Settings.TargetMassKg=.00082;F.A.Settings.WeighingToleranceKg=.000001;
 TestTrue(TEXT("partial begins"),F.Start());TestTrue(TEXT("feeding"),F.Until(EProcessAutoStage::Feeding));F.Tick();
 auto& S=F.M.Snapshot;S.SupplyABrake.Supported=S.SupplyABrake.Valid=true;S.SupplyABrake.PredictedSpillKg=.00082;S.SupplyABrake.PredictedSpillCount=2;F.Tick();
 TestTrue(TEXT("drainable current upstream IDs trigger before hopper target"),F.A.State.Stage==EProcessAutoStage::CloseA);
 TestEqual(TEXT("reserve separately visible"),F.A.State.CutoffDrainableKg,.00082);TestEqual(TEXT("no hardcoded gram correction"),F.A.State.CutoffEstimatedKg,.00082);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSequentialProtocol,"ProcessUI.SequentialAB.AutomationAndCancel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSequentialProtocol::RunTest(const FString&){
 FAutoFixture F;auto& Settings=F.A.Settings;Settings.BatchMode=EProcessBatchMode::SequentialAB;
 Settings.TargetMassKg=.00041;Settings.WeighingToleranceKg=0;Settings.TargetMassBKg=.00082;Settings.WeighingToleranceBKg=0;
 auto& Initial=F.M.Snapshot;Initial.SupplyBBrake.Supported=Initial.SupplyBBrake.Valid=true;Initial.SupplyBPurge.Supported=Initial.SupplyBPurge.Valid=true;
 Initial.SequentialBatch.B.InitialCount=Initial.SequentialBatch.B.SourceCount=3;
 TestTrue(TEXT("start freezes separate targets"),F.Start());
 for(int Material=0;Material<2;++Material){
  TestTrue(TEXT("acknowledged feeding"),F.Until(EProcessAutoStage::Feeding));TestEqual(TEXT("correct material active"),F.A.State.ActiveSupplier,Material);
  auto& S=F.M.Snapshot;const int Count=Material+1;S.SupplyABatch.SourceCount=3-Count;S.SupplyABatch.InventorySourceCount=3-Count;S.SupplyABatch.ConnectorCount=Count;
  auto& Q=Material==1?S.SupplyBPurge:S.SupplyAPurge;Q.ResidualCount=Count;Q.ResidualKg=Count*S.ModelParticleMassKg;
  TestTrue(TEXT("close then correct purge"),F.Until(EProcessAutoStage::Clearing));
  if(Material==1){
   TestTrue(TEXT("A held and RH closed through B purge"),F.M.Snapshot.SequentialBatch.A.ReceiverCount==1&&!F.M.Snapshot.AppliedReceiverOpen);
   auto Cancelled=F.A;auto Machine=F.M;Cancelled.Cancel(TEXT("whole stop"));const auto Before=F.Sent.Num();
   Machine.Snapshot.Execution=EPlantExecutionState::Running;Machine.Snapshot.PhysicalSeconds+=10;Cancelled.Tick(Machine,F.Now,F.Send);
   TestEqual(TEXT("cancelled B cannot dispatch downstream after resume"),F.Sent.Num(),Before);TestTrue(TEXT("cancel remains cancel"),Cancelled.State.Stage==EProcessAutoStage::Cancelled);
  }
  auto& Cleared=F.M.Snapshot;Cleared.SupplyABatch.ConnectorCount=0;Cleared.SupplyABatch.WeighingCount=Count;
  auto& Off=Material==1?Cleared.SupplyBPurge:Cleared.SupplyAPurge;Off.ResidualCount=0;Off.ResidualKg=0;
  TestTrue(TEXT("own final dose before transfer"),F.Until(EProcessAutoStage::Transferring));
  auto& T=F.M.Snapshot.Transfer;T.State=EPlantTransferState::Complete;T.ArrivedKg=T.BaselineKg;T.RemainingKg=T.InTransitKg=T.UnclassifiedKg=0;T.StableSeconds=.5;
  F.M.Snapshot.SupplyABatch.WeighingCount=0;F.M.Snapshot.SupplyABatch.ReceiverCount=Count;
  F.A.ConfirmUI(1,T.Id,F.M.Snapshot.PhysicalSeconds);
 }
 TestTrue(TEXT("combined feed after both WH closes"),F.Until(EProcessAutoStage::FeedingMixer));TestEqual(TEXT("sum actual counts"),F.A.State.BatchCount,3);
 int Opens=0;for(const auto& C:F.Sent)if(C.C==EProcessCommand::Valve&&C.Id==TEXT("receiver")&&C.V==1)++Opens;
 TestEqual(TEXT("RH opened exactly once"),Opens,1);
 auto& S=F.M.Snapshot;S.MixerFeed.State=EPlantTransferState::Complete;S.MixerFeed.MixerKg=S.MixerFeed.BaselineKg;S.MixerFeed.ReceiverKg=S.MixerFeed.ConnectorKg=0;S.MixerFeed.StableSeconds=.5;
 S.SupplyABatch.ReceiverCount=0;S.SupplyABatch.MixerCount=3;S.SequentialBatch.A.MixerCount=1;S.SequentialBatch.B.MixerCount=1;
 F.Tick();TestTrue(TEXT("sum alone cannot substitute for B entire arrival"),F.A.State.Stage==EProcessAutoStage::FeedingMixer);
 F.M.Snapshot.SequentialBatch.B.MixerCount=2;
 TestTrue(TEXT("actual close then mixing"),F.Until(EProcessAutoStage::Mixing));TestTrue(TEXT("close ack precedes mix"),F.A.State.ReceiverCloseRevision<F.A.State.MixerRequestRevision);
 TestTrue(TEXT("timed stop confirms batch complete"),F.Until(EProcessAutoStage::Complete));TestEqual(TEXT("stop applied RPM0"),F.M.Snapshot.AppliedRPM,0.);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdditiveDeadline,"ProcessUI.Additive.DeadlineAndCancellation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAdditiveDeadline::RunTest(const FString&){
 FAutoFixture F;F.A.Settings.MixingDurationSeconds=1;F.ArmMixing();auto& A=F.A.State;auto& S=F.M.Snapshot;
 A.AdditiveEnabled=A.AdditiveScheduled=true;A.AdditiveDelaySeconds=.1;A.AdditiveDueSeconds=10.1;A.ActiveSupplier=2;A.Stage=EProcessAutoStage::WaitAdditive;A.DownstreamStarted=false;
 S.SequentialBatch.AdditiveActive=S.SequentialBatch.AdditiveScheduled=true;
 F.Tick(.02);TestTrue(TEXT("not ready before due keeps closed and waits"),A.Stage==EProcessAutoStage::WaitAdditive&&!S.AppliedReceiverOpen);
 F.Tick(.04);TestTrue(TEXT("due without readiness invokes whole stop"),A.Stage==EProcessAutoStage::Stopping);TestTrue(TEXT("deadline reason"),A.Reason.Contains(TEXT("예약 준비 미완료")));
 for(int I=0;I<12;++I)F.Tick();TestTrue(TEXT("stop eventually failed not complete"),A.Stage==EProcessAutoStage::Failed&&!A.Active);
 const int Before=F.Sent.Num();S.SequentialBatch.AdditiveReady=true;S.SequentialBatch.AdditiveReadySeconds=S.PhysicalSeconds;
 S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;for(int I=0;I<5;++I)F.Tick();
 TestEqual(TEXT("late readiness after resume cannot revive reservation"),F.Sent.Num(),Before);
 for(const auto& C:F.Sent)TestFalse(TEXT("no receiver opening issued"),C.C==EProcessCommand::Valve&&C.Id==TEXT("receiver")&&C.V!=0);
 FAutoFixture Reset;Reset.ArmMixing();Reset.A.State.AdditiveEnabled=Reset.A.State.AdditiveScheduled=true;Reset.M.Snapshot.Epoch++;
 Reset.Tick();TestTrue(TEXT("initialization cancels reservation"),Reset.A.State.Stage==EProcessAutoStage::Cancelled);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdditiveClock,"ProcessUI.Additive.IndependentClockAndActualStop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAdditiveClock::RunTest(const FString&){
 FAutoFixture F;F.A.Settings.MixingDurationSeconds=1;F.ArmMixing();auto& A=F.A.State;auto& S=F.M.Snapshot;
 F.M.ManualGranted=true;A.AdditiveEnabled=A.AdditiveScheduled=true;A.AdditiveDelaySeconds=.5;A.AdditiveDueSeconds=10.5;A.ActiveSupplier=2;A.Stage=EProcessAutoStage::WaitAdditive;
 S.SequentialBatch.AdditiveActive=S.SequentialBatch.AdditiveScheduled=true;S.SequentialBatch.AdditiveReady=true;S.SequentialBatch.AdditiveReadySeconds=10.04;
 F.Tick(.1);const double Elapsed=A.MixingElapsedSeconds;TestEqual(TEXT("clock anchored at original actual RPM"),A.MixerAppliedSimulationSeconds,10.);
 S.Execution=EPlantExecutionState::Paused;F.Tick(1);TestEqual(TEXT("paused time does not accumulate"),A.MixingElapsedSeconds,Elapsed);
 S.Execution=EPlantExecutionState::Running;F.A.Tick(F.M,F.Now,F.Send);TestEqual(TEXT("duplicate snapshot does not accumulate"),A.MixingElapsedSeconds,Elapsed);
 S.PhysicalSeconds=10.5;F.Tick(0);TestTrue(TEXT("ready at due advances to opening"),A.Stage==EProcessAutoStage::OpenReceiver);F.Tick(0);F.Tick(0);
 int Open=0;for(const auto& C:F.Sent)if(C.C==EProcessCommand::Valve&&C.Id==TEXT("receiver")&&C.V==1)++Open;
 TestEqual(TEXT("one explicit opening command"),Open,1);TestTrue(TEXT("no early command"),A.AdditiveOpenRequestedSeconds>=A.AdditiveDueSeconds);
 // Directly supply a confirmed all-C-arrived/actual-close worker fixture; no dynamics claimed.
 A.Stage=EProcessAutoStage::Mixing;A.PendingRequest=FGuid();A.AdditiveDelivered=true;A.ReceiverCloseRevision=40;A.BatchRevision=8;
 S.SupplyABatch.CaptureRevision=8;S.MixerFeed.State=EPlantTransferState::Complete;S.MixerFeed.CloseRevision=40;S.MixerFeed.OpeningRevision=22;S.MixerFeed.ReceiverKg=S.MixerFeed.ConnectorKg=0;
 S.AppliedReceiverOpen=S.RequestedReceiverOpen=false;S.PhysicalSeconds=10.99;F.Tick(.02);
 TestTrue(TEXT("deadline issues one mixer stop"),A.Stage==EProcessAutoStage::StopMixer);F.Tick();F.Tick();
 TestTrue(TEXT("only actual zero-RPM response completes"),A.Stage==EProcessAutoStage::Complete);TestEqual(TEXT("calculation RPM stopped"),S.AppliedRPM,0.);
 TestTrue(TEXT("stop application after target recorded"),A.MixerStoppedSimulationSeconds>=A.MixingDeadlineSimulationSeconds);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdditiveEndGuard,"ProcessUI.Additive.EndDeadlineDoesNotExtend",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAdditiveEndGuard::RunTest(const FString&){
 for(bool Delivered:{false,true}){FAutoFixture F;F.ArmMixing();auto& A=F.A.State;auto& S=F.M.Snapshot;
  A.AdditiveEnabled=true;A.ActiveSupplier=2;A.AdditiveDelivered=Delivered;A.Stage=EProcessAutoStage::Mixing;
  S.MixerFeed.ReceiverKg=.00041;S.SupplyABatch.ReceiverCount=1;S.SupplyABatch.MixerCount=2;
  F.Tick(.2);TestTrue(TEXT("unfinished or changed confirmed material fails at original deadline"),A.Stage==EProcessAutoStage::Stopping);
  TestEqual(TEXT("duration not extended"),A.MixingDurationSeconds,.2);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdditiveSettings,"ProcessUI.Additive.AdmissionAndLockedSettings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAdditiveSettings::RunTest(const FString&){
 FAutoFixture F;auto& V=F.A.Settings;auto& S=F.M.Snapshot;V.BatchMode=EProcessBatchMode::SequentialAB;
 V.TargetMassKg=V.TargetMassBKg=V.AdditiveTargetKg=.00041;V.WeighingToleranceKg=V.WeighingToleranceBKg=V.AdditiveToleranceKg=0;
 V.AdditiveEnabled=V.AdditiveScheduled=true;V.AdditiveDelaySeconds=.1;
 S.SupplyBBrake.Supported=S.SupplyBBrake.Valid=S.SupplyBPurge.Supported=S.SupplyBPurge.Valid=true;
 S.SupplyCBrake.Supported=S.SupplyCBrake.Valid=S.SupplyCPurge.Supported=S.SupplyCPurge.Valid=true;
 S.SequentialBatch.B.InitialCount=S.SequentialBatch.B.SourceCount=S.SequentialBatch.Additive.InitialCount=S.SequentialBatch.Additive.SourceCount=3;
 TestTrue(TEXT("verified C stock admits configured batch"),F.A.CanStart(F.M,F.Now).IsEmpty());
 V.AdditiveDelaySeconds=.2;TestFalse(TEXT("N equal mixing duration rejected"),F.A.CanStart(F.M,F.Now).IsEmpty());
 V.AdditiveDelaySeconds=-1;TestFalse(TEXT("negative N rejected"),F.A.CanStart(F.M,F.Now).IsEmpty());
 V.AdditiveDelaySeconds=std::numeric_limits<double>::quiet_NaN();TestFalse(TEXT("NaN rejected"),F.A.CanStart(F.M,F.Now).IsEmpty());
 V.AdditiveDelaySeconds=0;V.AdditiveSupplier=3;TestFalse(TEXT("unverified D rejected"),F.A.CanStart(F.M,F.Now).IsEmpty());
 V.AdditiveSupplier=2;V.AdditiveTargetKg=.004;TestFalse(TEXT("inventory excess rejected"),F.A.CanStart(F.M,F.Now).IsEmpty());
 V.AdditiveTargetKg=.00041;V.AdditiveDelaySeconds=.1;TestTrue(TEXT("start accepts explicit valid settings"),F.Start());
 V.AdditiveTargetKg=.001;V.AdditiveDelaySeconds=0;V.MixingDurationSeconds=99;
 TestEqual(TEXT("batch target immutable"),F.A.State.AdditiveTargetKg,.00041);TestEqual(TEXT("batch N immutable"),F.A.State.AdditiveDelaySeconds,.1);TestEqual(TEXT("batch duration immutable"),F.A.State.MixingDurationSeconds,.2);
 const auto Id=F.A.State.RunId;F.Start();TestEqual(TEXT("duplicate start keeps run"),F.A.State.RunId,Id);return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkerDoseAutoAck,"ProcessUI.WorkerDose.AutoConsumesLatchedCutoff",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorkerDoseAutoAck::RunTest(const FString&){
 FAutoFixture F;F.Start();TestTrue(TEXT("fixture feeding"),F.Until(EProcessAutoStage::Feeding));
 auto& A=F.A.State;auto& S=F.M.Snapshot;A.BatchMode=EProcessBatchMode::TargetMass;A.TargetMassKg=.00082;A.WeighingToleranceKg=.0001;
 auto& D=S.DoseCutoff;D.Supported=D.Valid=D.Armed=D.Triggered=true;D.Epoch=A.Epoch;D.BatchRevision=A.BatchRevision;D.Supplier=0;
 D.TargetKg=A.TargetMassKg;D.ToleranceKg=A.WeighingToleranceKg;D.EstimatedKg=.00082;D.OpenedSeconds=.01;D.GateClosedSeconds=S.PhysicalSeconds;
 // Worker is already physically closed when the delayed UI observes the trigger.
 S.AppliedSupplierMask=0;F.Tick();TestTrue(TEXT("worker cutoff consumed despite closed mask"),A.Stage==EProcessAutoStage::CloseA);
 int Closes=0;for(const auto& C:F.Sent)if(C.C==EProcessCommand::Valve&&C.Id==TEXT("supply-a")&&C.V==0)++Closes;
 TestEqual(TEXT("one service confirmation close"),Closes,1);F.A.Tick(F.M,F.Now,F.Send);
 int After=0;for(const auto& C:F.Sent)if(C.C==EProcessCommand::Valve&&C.Id==TEXT("supply-a")&&C.V==0)++After;
 TestEqual(TEXT("duplicate read sends no second close"),After,Closes);
 F.Tick();TestTrue(TEXT("actual close acknowledgement advances"),A.Stage!=EProcessAutoStage::Failed&&A.Stage!=EProcessAutoStage::Stopping);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkerDoseOpenHistory,"ProcessUI.WorkerDose.ExactOpeningHistory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorkerDoseOpenHistory::RunTest(const FString&){
 for(int Mode=0;Mode<4;++Mode){FAutoFixture F;F.M.ManualGranted=true;auto& S=F.M.Snapshot;S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;
  const FGuid Id=FGuid::NewGuid();auto R=F.M.Submit(Id,1,TEXT("supply-b"),EProcessCommand::Valve,1,F.Now,F.Transport);F.Queued.Empty();
  auto& D=S.DoseCutoff;D.Supported=D.Valid=D.Triggered=true;D.RequestId=Id;D.Epoch=1;D.ArmRevision=R.Revision;D.Supplier=1;D.OpenedSeconds=.01;
  if(Mode==1)D.RequestId=FGuid::NewGuid();if(Mode==2)D.Epoch=0;if(Mode==3)D.Cancelled=true;
  S.AppliedSupplierMask=0;S.RequestedSuppliers[1]=false;S.ProcessedControlRevision=R.Revision;S.Sequence++;S.PublishedWallSeconds=++F.Now;
  F.M.Observe(S,F.Now,F.Transport);
  TestEqual(TEXT("only exact noncancelled worker opening proves command"),F.M.History[Id].Status==EProcessCommandStatus::Applied,Mode==0);
 }
 return true;
}

#endif
