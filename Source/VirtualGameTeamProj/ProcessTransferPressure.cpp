#include "ProcessAutoRun.h"

FString FProcessAutoRun::ReceiverPressureUnavailable(const FProcessControlMachine& M,double Now) const
{
 const auto& S=M.Snapshot;
 if(!State.Active||State.Stage!=EProcessAutoStage::Transferring||M.IsStopping())
  return TEXT("칭량→수취 자동 이송 중에만 조절할 수 있습니다. 계량 확정과 출구 열림을 기다리세요.");
 if(!M.Connected||State.Connection!=M.Connection||State.Epoch!=S.Epoch||S.Validity!=EPlantValueValidity::Valid||
    !S.Error.IsEmpty()||Now<S.PublishedWallSeconds||Now-S.PublishedWallSeconds>2.)return TEXT("최신 작업자 이송 상태 확인 중입니다.");
 if(S.Execution!=EPlantExecutionState::Running||!S.RequestedRunning)return TEXT("계산이 진행 중일 때만 현재 이송 압력을 조절할 수 있습니다.");
 if(State.TransferId<=0||S.Transfer.Id!=State.TransferId||S.Transfer.Validity!=EPlantValueValidity::Valid||
    S.Transfer.State==EPlantTransferState::Complete||S.Transfer.State==EPlantTransferState::Indeterminate||S.Transfer.AdditionalInflowCount)
  return TEXT("현재 이송이 완료되었거나 이송 기준을 확인할 수 없습니다.");
 if(!S.AppliedHopperOpen||!S.RequestedHopperOpen||S.AppliedReceiverOpen||S.RequestedReceiverOpen)
  return TEXT("칭량 출구 실제 열림과 수취 출구 실제 닫힘을 확인해야 합니다.");
 if(State.BatchMode!=EProcessBatchMode::Full&&!S.SupplyABatch.DoseFinalized)return TEXT("작업자의 최종 계량 확정이 필요합니다.");
 if(!S.AppliedPressureValid||S.AppliedGaugePressurePa.X!=State.LockedTransferPressurePa.X||S.AppliedGaugePressurePa.Y!=State.LockedTransferPressurePa.Y||S.AppliedSupplierMask)
  return TEXT("공급·칭량 압력 또는 공급 밸브 적용 조건이 일치하지 않습니다.");
 for(bool Open:S.RequestedSuppliers)if(Open)return TEXT("공급 밸브 닫힘 요청을 확인해야 합니다.");
 for(const auto& P:{S.SupplyAPurge,S.SupplyBPurge,S.SupplyCPurge})if(P.Supported&&(!P.Valid||P.AppliedEnabled||P.RequestedEnabled||P.FlowM3s!=0||P.RequestedGaugePa!=0||P.AppliedGaugePa!=0))
  return TEXT("연결부 공기 차단 확인이 필요합니다.");
 if(State.ReceiverPressurePending)return TEXT("적용 확인 중 · 현재 요청의 작업자 응답을 기다리세요.");
 return FString();
}

FProcessCommandRecord FProcessAutoRun::RequestReceiverPressure(FGuid Id,int64 Epoch,int64 TransferId,double Pa,FProcessControlMachine& M,double Now,const FProcessControlTransport& Send)
{
 // The generic history owns idempotency. Do not mint a second worker command for a repeated click.
 if(const auto* R=M.History.Find(Id)){
  if(R->Epoch==Epoch&&R->Connection==M.Connection&&R->EquipmentId==TEXT("receiver")&&R->Command==EProcessCommand::Pressure&&R->Value==Pa)return *R;
  return M.Submit(Id,Epoch,TEXT("receiver"),EProcessCommand::Pressure,Pa,Now,Send,TEXT("동일 요청 번호의 내용이 달라졌습니다."));
 }
 const bool SameTransfer=State.Active&&State.Stage==EProcessAutoStage::Transferring&&State.TransferId==TransferId&&State.Epoch==Epoch&&State.Connection==M.Connection;
 if(SameTransfer&&State.ReceiverPressurePending&&State.ReceiverPressureRequestedPa==Pa)
  if(const auto* R=M.History.Find(State.ReceiverPressureRequest))return *R;
 FString Why=ReceiverPressureUnavailable(M,Now);
 if(!SameTransfer)Why=TEXT("이송 단계·초기화 세대가 변경되었습니다. 종료된 단계의 압력 요청은 재사용하지 않습니다.");
 if(!FMath::IsFinite(Pa)||Pa<-1000||Pa>0)Why=TEXT("수취 압력은 유한한 −1.0~0 kPa(g)만 입력하세요. 범위 밖 값은 적용하지 않습니다.");
 TGuardValue<bool> Dispatch(M.AutomationDispatch,true);
 const auto R=M.Submit(Id,Epoch,TEXT("receiver"),EProcessCommand::Pressure,Pa,Now,Send,Why);
 if(R.IsPending()||R.Status==EProcessCommandStatus::Applied){
  State.ReceiverPressureRequest=R.RequestId;State.ReceiverPressureTransferId=TransferId;State.ReceiverPressureRevision=R.Revision;
  State.ReceiverPressurePreviousPa=M.Snapshot.AppliedGaugePressurePa.Z;State.ReceiverPressureRequestedPa=Pa;
  State.ReceiverPressureRequestedSeconds=M.Snapshot.PhysicalSeconds;State.ReceiverPressureAppliedSeconds=-1;
  State.ReceiverPressurePending=R.IsPending();State.ReceiverPressureStatus=R.IsPending()?TEXT("적용 확인 중"):TEXT("적용 완료 · 이미 같은 설정");
  if(!R.IsPending()){State.ReceiverPressureConfirmedPa=Pa;State.ReceiverPressureAppliedSeconds=M.Snapshot.PhysicalSeconds;}
  UE_LOG(LogTemp,Display,TEXT("TRANSFER_PRESSURE_REQUEST request=%s epoch=%lld transfer=%lld revision=%lld sequence=%lld time=%.9f old_pa=%.9g requested_pa=%.9g"),*R.RequestId.ToString(),Epoch,TransferId,R.Revision,M.Snapshot.Sequence,M.Snapshot.PhysicalSeconds,State.ReceiverPressurePreviousPa,Pa);
 }
 return R;
}

bool FProcessAutoRun::ObserveReceiverPressure(FProcessControlMachine& M,FString& Why)
{
 const auto& S=M.Snapshot;
 if(S.AppliedGaugePressurePa.X!=State.LockedTransferPressurePa.X||S.AppliedGaugePressurePa.Y!=State.LockedTransferPressurePa.Y)return false;
 const bool Scoped=State.ReceiverPressureTransferId>0&&State.ReceiverPressureTransferId==State.TransferId;
 const double Expected=Scoped?State.ReceiverPressureConfirmedPa:State.LockedTransferPressurePa.Z;
 if(!State.ReceiverPressurePending)return S.AppliedGaugePressurePa.Z==Expected;
 const auto* R=M.History.Find(State.ReceiverPressureRequest);
 if(!Scoped||!R||R->Epoch!=State.Epoch||R->Connection!=State.Connection){Why=TEXT("압력 요청 이송·세대·연결 불일치");return false;}
 if(S.Transfer.State==EPlantTransferState::Complete){
  // Arrival and closure take precedence; no waiting for a late adjustment.
  return S.AppliedGaugePressurePa.Z==State.ReceiverPressurePreviousPa||S.AppliedGaugePressurePa.Z==State.ReceiverPressureRequestedPa;
 }
 if(R->Status==EProcessCommandStatus::Applied&&S.Sequence>R->BaselineSequence&&S.PressureAppliedRevision>=R->Revision&&
    S.RequestedGaugePressurePa.Z==R->Value&&S.AppliedGaugePressurePa.Z==R->Value){
  State.ReceiverPressurePending=false;State.ReceiverPressureConfirmedPa=R->Value;
  State.ReceiverPressureAppliedSeconds=S.PressureAppliedSimulationSeconds;State.ReceiverPressureStatus=TEXT("적용 완료 · 작업자 압력 설정 확인");
  UE_LOG(LogTemp,Display,TEXT("TRANSFER_PRESSURE_CONFIRMED request=%s transfer=%lld revision=%lld sequence=%lld applied_s=%.9f observed_s=%.9f receiver_pa=%.9g flow=%.12g"),*R->RequestId.ToString(),State.TransferId,R->Revision,S.Sequence,State.ReceiverPressureAppliedSeconds,S.PhysicalSeconds,R->Value,S.Transfer.CalculatedCarrierFlowM3s);
  return true;
 }
 if(!R->IsPending()){State.ReceiverPressureStatus=TEXT("압력 적용 실패 · ")+R->Message;Why=State.ReceiverPressureStatus;return false;}
 return S.AppliedGaugePressurePa.Z==State.ReceiverPressurePreviousPa||
  (S.PressureAppliedRevision>=R->Revision&&S.AppliedGaugePressurePa.Z==State.ReceiverPressureRequestedPa);
}

void FProcessAutoRun::EndReceiverPressure()
{
 if(State.ReceiverPressurePending)State.ReceiverPressureStatus=TEXT("취소 · 이송 종료/정지 우선 · 자동 재적용 안 함");
 State.ReceiverPressurePending=false;
}
