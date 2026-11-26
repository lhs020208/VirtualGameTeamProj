#include "PlantWorkerDose.h"
#include "PlantDoseValidation.h"
void FPlantWorkerDose::Cancel(const TCHAR* Why){State.Armed=false;State.Cancelled=true;State.Reason=Why;}
void FPlantWorkerDose::Sync(const FPlantControlTargets& C,int32 Active,int64 Capture,double Sim,double Now,double ParticleKg,double AvailableKg)
{
 if(!Supported)return;
 if(C.DoseCutoffRevision<=0){
  HighWater=FMath::Max(HighWater,C.Revision);
  if(State.Armed){if(State.Triggered)State.Armed=false;else Cancel(TEXT("계량 차단 요청 취소 · 재개 후 자동 복원 안 함"));}
  return;
 }
 if(State.ArmRevision!=C.DoseCutoffRevision){
  if(C.DoseCutoffRevision<=HighWater){Cancel(TEXT("이전 계량 차단 요청은 다시 실행할 수 없습니다."));return;}
  HighWater=C.DoseCutoffRevision;State=FPlantDoseCutoffSnapshot();State.Supported=true;
  State.ArmRevision=C.DoseCutoffRevision;State.RequestId=C.DoseCutoffRequestId;State.BatchRevision=C.DoseCutoffBatch;
  State.Epoch=C.DoseCutoffEpoch;State.Connection=C.ObservationConnection;State.Supplier=C.DoseCutoffSupplier;
  State.TargetKg=C.DoseCutoffTargetKg;State.ToleranceKg=C.DoseCutoffToleranceKg;State.RequestedWall=C.DoseCutoffRequestedWall;
  State.ReceivedWall=Now;State.ReceivedSeconds=Sim;PreviousTime=-1;PreviousPassed=0;
  State.Valid=State.RequestId.IsValid()&&State.Epoch>=0&&State.Supplier>=0&&State.Supplier<6&&
   PlantDose::Validate(State.TargetKg,State.ToleranceKg,ParticleKg,AvailableKg).IsEmpty();
  State.Armed=State.Valid;
  if(!State.Valid){Cancel(TEXT("작업자 계량 목표·재고·입자 분해능 또는 요청 번호 오류"));return;}
 }
 if(State.Cancelled||!State.Armed)return;
 const uint32 Bit=1u<<State.Supplier;
 if(C.ObservationConnection!=State.Connection||C.DoseCutoffEpoch!=State.Epoch||C.DoseCutoffRequestId!=State.RequestId||
    Active!=State.Supplier||Capture!=State.BatchRevision||C.DoseCutoffBatch!=State.BatchRevision||
    C.DoseCutoffTargetKg!=State.TargetKg||C.DoseCutoffToleranceKg!=State.ToleranceKg||C.DoseCutoffSupplier!=State.Supplier)
 {Cancel(TEXT("계량 차단의 연결·배치·초기화 세대·목표가 변경됐습니다."));return;}
 if(!C.Running||C.Suppliers!=Bit||C.WeighingOpen||C.ReceiverOpen||C.SupplyAPurgeEnabled||C.SupplyAPurgeGaugePa!=0||
    !FMath::IsFinite(C.DoseCutoffDeadline)||Now>C.DoseCutoffDeadline)
 {Cancel(TEXT("계량 차단 취소/만료 또는 공급 조건 변경 · 출구 개방 금지"));}
}
uint32 FPlantWorkerDose::Requests(const FPlantControlTargets& C) const
{
 if(C.DoseCutoffRevision<=0)return C.Suppliers;
 const int I=C.DoseCutoffSupplier;
 if(I<0||I>=6)return 0;
 if(State.ArmRevision!=C.DoseCutoffRevision||!State.Valid||State.Cancelled||State.Triggered||!State.Armed)return C.Suppliers&~(1u<<I);
 return C.Suppliers;
}
void FPlantWorkerDose::Opened(int32 Supplier,double Sim)
{if(State.Armed&&!State.Cancelled&&State.Supplier==Supplier&&State.OpenedSeconds<0)State.OpenedSeconds=Sim;}
bool FPlantWorkerDose::Observe(double Passed,double Spill,int32 Source,double Sim,double Now,int64 Sequence)
{
 if(!State.Armed||State.Cancelled||State.Triggered||State.OpenedSeconds<0||Sim<=PreviousTime)return false;
 if(!FMath::IsFinite(Passed)||!FMath::IsFinite(Spill)||Passed<0||Spill<0||Source<0){Cancel(TEXT("작업자 계량 관찰 값 오류"));return false;}
 // Same estimator as the original controller, evaluated on each completed GPU batch.
 State.LeadKg=PreviousTime>=0?FMath::Max(0.,Passed-PreviousPassed):0;
 PreviousTime=Sim;PreviousPassed=Passed;++State.Sample;State.ObservedSeconds=Sim;
 State.PassedKg=Passed;State.SpillKg=Spill;State.EstimatedKg=Passed+Spill+State.LeadKg;
 if(State.EstimatedKg<State.TargetKg&&Source>0)return false;
 State.Triggered=true;State.TriggerSeconds=Sim;State.TriggerWall=Now;State.TriggerSample=State.Sample;State.TriggerPublicationSequence=Sequence;
 State.Reason=TEXT("작업자 목표량 차단 확정 · 제동·점유 해소·실제 닫힘 확인 중");return true;
}
void FPlantWorkerDose::Closed(int32 Supplier,double Sim)
{if(State.Triggered&&State.Supplier==Supplier&&State.GateClosedSeconds<0)State.GateClosedSeconds=Sim;}
