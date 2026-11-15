#include "ProcessAutoRun.h"

// This clock is independent of the current material/pressure command stage.
// Only a new committed solver snapshot can advance elapsed simulation time.
bool FProcessAutoRun::TickAdditiveClock(FProcessControlMachine& M,double Now,const FProcessAutoCommand& Send)
{
 if(!State.AdditiveEnabled||State.MixerAppliedSimulationSeconds<0||State.Stage==EProcessAutoStage::StopMixer)return false;
 const auto& S=M.Snapshot;
 if(!S.AppliedRPMValid||!S.WorkerMixerEnabled||!S.RequestedMixerEnabled||
    S.AppliedRPM!=double(float(State.TargetMixerRPM))||S.WorkerTargetRPM!=State.TargetMixerRPM||S.RequestedRPM!=State.TargetMixerRPM){
  Fail(TEXT("첨가제 준비/투입 중 목표 혼합 RPM 이탈 또는 적용값 미확인"),M,Now,Send);return true;
 }
 if(S.Sequence>LastClockSequence&&S.Execution==EPlantExecutionState::Running){
  const double Elapsed=MixElapsed(S);
  if(!FMath::IsFinite(Elapsed)||Elapsed<State.MixingElapsedSeconds){Fail(TEXT("예약/혼합 해석 시간 연속성 오류"),M,Now,Send);return true;}
  LastClockSequence=S.Sequence;State.MixingElapsedSeconds=Elapsed;State.MixingRemainingSeconds=FMath::Max(0.,State.MixingDurationSeconds-Elapsed);
 }
 if(State.MixingElapsedSeconds>=State.MixingDurationSeconds){
  State.MixingTargetReachedSimulationSeconds=S.PhysicalSeconds;
  const auto& B=S.SupplyABatch;const auto& F=S.MixerFeed;const auto& AB=S.SequentialBatch;
  const bool Intact=B.Valid&&B.CaptureRevision==State.BatchRevision&&B.TotalCount==State.BatchCount&&B.MixerCount==State.BatchCount&&
   B.ForeignCount==0&&B.UnknownCount==0&&AB.A.MixerCount==AB.A.SelectedCount&&AB.B.MixerCount==AB.B.SelectedCount&&
   AB.Additive.MixerCount==AB.Additive.SelectedCount&&F.Valid&&F.State==EPlantTransferState::Complete&&F.AdditionalInflowCount==0&&
   F.ReceiverKg==0&&F.ConnectorKg==0&&F.CloseRevision==State.ReceiverCloseRevision&&F.CloseRevision>F.OpeningRevision;
  if(!State.AdditiveDelivered||!Intact||S.AppliedReceiverOpen||S.RequestedReceiverOpen){
   Fail(FString::Printf(TEXT("혼합 종료 시 첨가제 투입/닫힘 미완료 · %s · 수취 %.6f g / 연결부 %.6f g / 도착 %.6f g · 시간 자동 연장 없음"),
    *StageText(State.Stage),S.MixerFeed.ReceiverKg*1000,S.MixerFeed.ConnectorKg*1000,S.MixerFeed.MixerKg*1000),M,Now,Send);return true;
  }
  Enter(EProcessAutoStage::StopMixer,S,Now);Command(EProcessCommand::MixerEnabled,TEXT("mixer"),0,EProcessAutoStage::Complete,M,Now,Send);return true;
 }
 if(State.AdditiveScheduled&&State.AdditiveOpenRevision==0&&S.PhysicalSeconds>=State.AdditiveDueSeconds){
  const bool ReadyByDue=S.SequentialBatch.AdditiveReady&&S.SequentialBatch.AdditiveReadySeconds>=0&&S.SequentialBatch.AdditiveReadySeconds<=State.AdditiveDueSeconds;
  if(!ReadyByDue){
   State.AdditiveStatus=TEXT("예약 준비 미완료 · 늦은 자동 투입 취소");
   Fail(FString::Printf(TEXT("예약 준비 미완료 · N=%.6f초 / 예약시각 %.9f / 현재 %.9f · %s · 연결부 %d개 / 칭량 %.6f g / 수취 %.6f g · 출구 닫힘·전체 정지"),
    State.AdditiveDelaySeconds,State.AdditiveDueSeconds,S.PhysicalSeconds,*StageText(State.Stage),S.SupplyABatch.ConnectorCount,
    S.WeighingMass.Kg*1000,S.ReceiverMass.Kg*1000),M,Now,Send);return true;
  }
 }
 return false;
}
