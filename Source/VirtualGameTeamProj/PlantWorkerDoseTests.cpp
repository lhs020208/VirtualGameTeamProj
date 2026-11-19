#include "PlantWorkerDose.h"
#include "Misc/AutomationTest.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
namespace {
FPlantControlTargets DoseControl(int Supplier=1){
 FPlantControlTargets C;C.Revision=C.DoseCutoffRevision=10;C.ObservationConnection=2;C.DoseCutoffEpoch=3;
 C.DoseCutoffBatch=4;C.DoseCutoffSupplier=Supplier;C.DoseCutoffTargetKg=.04;C.DoseCutoffToleranceKg=.002;
 C.DoseCutoffRequestId=FGuid::NewGuid();C.DoseCutoffDeadline=200;C.Running=true;C.Suppliers=1u<<Supplier;
 C.GaugePressurePa=FVector(500,-500,0);return C;
}
void Arm(FPlantWorkerDose& W,const FPlantControlTargets& C){W.Sync(C,C.DoseCutoffSupplier,4,0,100,.00004,.08);W.Opened(C.DoseCutoffSupplier,0);}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkerDoseCadence,"ProcessUI.WorkerDose.CutoffIndependentOfConsumer",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorkerDoseCadence::RunTest(const FString&){
 auto C=DoseControl();FPlantWorkerDose W(true);Arm(W,C);TestTrue(TEXT("locked batch accepted"),W.State.Valid&&W.State.Armed);
 int Triggers=0;double TriggerPassed=0;
 // No game-thread reads or commands during these worker observations. UI would next see .050 kg.
 for(int I=0;I<=50;++I){double Passed=I*.001;if(W.Observe(Passed,.008,2000-I*25,I*.001,100+I*.01,77)){++Triggers;TriggerPassed=Passed;}}
 TestEqual(TEXT("exactly one cutoff despite delayed UI"),Triggers,1);TestTrue(TEXT("cutoff precedes late 50 g UI sample"),TriggerPassed<.04);
 TestEqual(TEXT("worker effective supply request closed"),W.Requests(C),uint32(0));
 W.Sync(C,1,4,.06,101,.00004,.08);TestEqual(TEXT("stale open mailbox cannot undo cutoff"),W.Requests(C),uint32(0));
 W.Closed(1,.07);W.Closed(1,.08);TestEqual(TEXT("first physical closure retained"),W.State.GateClosedSeconds,.07);
 TestEqual(TEXT("publish sequence links trigger"),W.State.TriggerPublicationSequence,int64(77));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkerDoseCancellation,"ProcessUI.WorkerDose.CancelAndReplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorkerDoseCancellation::RunTest(const FString&){
 for(int Mode=0;Mode<8;++Mode){auto C=DoseControl();FPlantWorkerDose W(true);Arm(W,C);auto Old=C;
  int Active=1;int64 Capture=4;
  if(Mode==0){C.DoseCutoffRevision=0;C.Revision=11;C.Suppliers=0;C.Running=false;}
  if(Mode==1)++C.ObservationConnection;
  if(Mode==2)++C.DoseCutoffEpoch;
  if(Mode==3)++Capture;
  if(Mode==4)C.DoseCutoffTargetKg=.05;
  if(Mode==5)C.DoseCutoffRequestId=FGuid::NewGuid();
  if(Mode==6)C.DoseCutoffDeadline=99;
  if(Mode==7)C.Running=false;
  W.Sync(C,Active,Capture,.01,101,.00004,.08);TestTrue(TEXT("stop/reset/detach/mutation/deadline cancels"),W.State.Cancelled);
  W.Sync(Old,1,4,.02,102,.00004,.08);TestEqual(TEXT("replayed old open remains blocked"),W.Requests(Old),uint32(0));
  TestFalse(TEXT("cancelled observer cannot trigger"),W.Observe(.08,.01,0,.03,103,80));
 }
 auto C=DoseControl();FPlantWorkerDose W(true);Arm(W,C);C.DoseCutoffRevision=0;C.Revision=11;C.Suppliers=0;C.Running=false;
 W.Sync(C,1,4,.01,101,.00004,.08);C.Running=true;W.Sync(C,1,4,.02,102,.00004,.08);
 TestEqual(TEXT("manual resume restores no supply"),W.Requests(C),uint32(0));
 auto New=DoseControl(2);New.Revision=New.DoseCutoffRevision=12;W.Sync(New,2,4,.03,103,.00004,.08);
 TestTrue(TEXT("new explicit request can arm next supplier"),W.State.Armed&&!W.State.Cancelled&&W.State.Supplier==2);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkerDoseData,"ProcessUI.WorkerDose.DataAndTime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorkerDoseData::RunTest(const FString&){
 auto C=DoseControl();FPlantWorkerDose W(true);Arm(W,C);W.Observe(.001,.001,1975,.01,101,1);auto Count=W.State.Sample;
 W.Observe(.05,.001,750,.01,102,2);TestEqual(TEXT("same simulation time cannot decide again"),W.State.Sample,Count);TestFalse(TEXT("same snapshot does not trigger"),W.State.Triggered);
 W.Observe(std::numeric_limits<double>::quiet_NaN(),0,1900,.02,103,3);TestTrue(TEXT("bad data fails closed"),W.State.Cancelled);TestEqual(TEXT("invalid sample closes supply"),W.Requests(C),uint32(0));
 FPlantWorkerDose Unsupported(false);auto Manual=C;Manual.DoseCutoffRevision=0;TestEqual(TEXT("manual/full supply unchanged without token"),Unsupported.Requests(Manual),Manual.Suppliers);
 return true;
}
#endif
