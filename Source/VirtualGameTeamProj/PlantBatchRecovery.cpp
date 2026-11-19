#include "PlantSupplyABatch.h"

void FPlantSupplyABatch::Foreign(int32 Id,const FString& Region)
{
 if(State.CaptureRevision<=0||!ForeignSeen.IsValidIndex(Id)||ForeignSeen[Id])return;
 ForeignSeen[Id]=1;Recovery.ForeignIds.Add(Id);Recovery.ForeignSuppliers.Add(Labels[Id]);Recovery.ForeignRegions.Add(Region);
 if(Recovery.ForeignIds.Num()<=16)UE_LOG(LogTemp,Warning,TEXT("BATCH_FOREIGN id=%d supplier=%d region=%s sim=%.9f"),Id,Labels[Id],*Region,State.ObservedSimulationSeconds);
}
void FPlantSupplyABatch::ObserveMix(double Time,bool Automatic,double RPM)
{
 if(MixLastTime>=0&&Time>=MixLastTime&&MixWasAutomatic)Recovery.AutomaticMixSeconds+=Time-MixLastTime;
 MixLastTime=Time;MixWasAutomatic=Automatic&&RPM>0;
}
bool FPlantSupplyABatch::AdmitRecovery(const std::vector<powder::Grain>& P,double Time,int64 Capture,int64 Revision,FGuid Request)
{
 if(Revision<=LastRecoveryAttempt)return Recovery.AcceptedRevision==Revision&&Recovery.Ticket==Revision;
 LastRecoveryAttempt=Revision;Recovery.Ticket=0;Observe(P,Time);
 Recovery.RequestId=Request;
 if(!Request.IsValid()||Capture<=0||State.CaptureRevision!=Capture||!Recovery.Valid||!Recovery.ForeignIds.IsEmpty()||State.ForeignCount||State.UnknownCount){
  Recovery.RejectedRevision=Revision;Recovery.Reason=TEXT("배치 재개 차단 · 입자 ID/공급원/질량 보존 또는 오원료 유입 기록 확인 필요");return false;
 }
 if(State.DoseFinalized){
  if(State.SourceCount||State.ConnectorCount||State.WeighingCount+State.PipeCount+State.ReceiverCount+State.MixerConnectorCount+State.MixerCount!=State.TotalCount||
     FMath::Abs(State.TotalCount*Mass.ParticleMassKg-State.TotalKg)>Criteria.MassErrorKg){
   Recovery.RejectedRevision=Revision;Recovery.Reason=TEXT("확정 입자 누락·역류·질량 불일치 · 재개 금지");return false;
  }
 }
 Recovery.AcceptedRevision=Recovery.Ticket=Revision;Recovery.AcceptedSeconds=Time;Recovery.Reason=TEXT("작업자 배치 이력·현재 위치 대조 확인 · 새 요청만 허용");
 // Reopening a partially dosed source requires fresh closure observations.
 if(!State.DoseFinalized){State.SupplyCloseRequestedRevision=State.SupplyClosedRevision=0;State.SupplyCloseRequestedSeconds=State.SupplyClosedSeconds=-1;}
 UE_LOG(LogTemp,Display,TEXT("BATCH_RECOVERY_ADMITTED revision=%lld batch=%lld sim=%.9f source=%d connector=%d wh=%d pipe=%d rh=%d mixer=%d mix_elapsed=%.9f"),
  Revision,Capture,Time,State.SourceCount,State.ConnectorCount,State.WeighingCount,State.PipeCount,State.ReceiverCount,State.MixerCount,Recovery.AutomaticMixSeconds);
 return true;
}
