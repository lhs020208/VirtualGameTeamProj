#include "ProcessAutoRun.h"
#include "ProcessPlantBinding.h"

namespace {
FVector Boundary(const FPlantSimulationSnapshot& S,int32 Supplier,bool Applied)
{
 FVector Pa=Applied?S.AppliedGaugePressurePa:S.RequestedGaugePressurePa;
 Pa.X=S.SupplierPressure(Supplier,Applied);
 return Pa;
}
}
const FProcessCommandRecord* FProcessAutoRun::LatestStagePressure(const FProcessControlMachine& M,const FString& Equipment) const
{
 if(!State.Active||!State.PressureStageToken.IsValid())return nullptr;
 const FProcessCommandRecord* Latest=nullptr;
 for(const auto& Pair:M.History){
  const auto& R=Pair.Value;
  if(R.PressureStage==State.PressureStageToken&&R.PressureBatch==State.RunId&&R.EquipmentId==Equipment&&
     R.Epoch==State.Epoch&&R.Connection==M.Connection&&R.Revision>0&&(!Latest||R.Revision>Latest->Revision))Latest=&R;
 }
 return Latest;
}
FVector FProcessAutoRun::StageBaseline() const
{
 return State.Stage==EProcessAutoStage::Feeding?State.FeedBaseline():State.LockedTransferPressurePa;
}
FString FProcessAutoRun::PressureSection() const
{
 if(State.Active&&State.Stage==EProcessAutoStage::Feeding)return FString::Printf(TEXT("%c 공급 → 칭량 중"),'A'+State.ActiveSupplier);
 if(State.Active&&State.Stage==EProcessAutoStage::Transferring)return TEXT("칭량 → 수취 이송 중");
 return StageText(State.Stage);
}
bool FProcessAutoRun::PressureFlow(const FPlantSimulationSnapshot& S,double& Flow) const
{
 if(!State.Active||State.Epoch!=S.Epoch)return false;
 if(State.Stage==EProcessAutoStage::Feeding&&S.SupplyValvePressures.IsValidIndex(State.ActiveSupplier)){
  const auto& Valve=S.SupplyValvePressures[State.ActiveSupplier];
  if(Valve.FlowValid||Valve.Valid){Flow=Valve.FlowM3s;return true;}
 }
 if(State.Stage==EProcessAutoStage::Transferring&&State.TransferId==S.Transfer.Id&&S.Transfer.Validity==EPlantValueValidity::Valid&&
    S.AppliedHopperOpen&&!S.AppliedReceiverOpen&&S.Transfer.CarrierFlowValid){Flow=S.Transfer.CalculatedCarrierFlowM3s;return true;}
 return false;
}
FString FProcessAutoRun::PressureUnavailable(const FProcessControlMachine& M,double Now,const FString& Equipment,bool CheckPending) const
{
 const auto& S=M.Snapshot;
 if(M.IsStopping())return TEXT("전체 정지 확인 중 · 압력은 자동으로 대기압으로 복귀합니다.");
 const bool Feed=State.Stage==EProcessAutoStage::Feeding;
 if(!State.Active||(!Feed&&State.Stage!=EProcessAutoStage::Transferring))return StageText(State.Stage)+TEXT(" · 공급 또는 칭량→수취 이송 중에만 조절 가능합니다.");
 if(!M.Connected||State.Connection!=M.Connection||State.Epoch!=S.Epoch||S.Validity!=EPlantValueValidity::Valid||!S.Error.IsEmpty()||
    Now<S.PublishedWallSeconds||Now-S.PublishedWallSeconds>2.||!S.AppliedPressureValid)return TEXT("최신 작업자 압력과 유로 상태 확인 중");
 if(S.Execution!=EPlantExecutionState::Running||!S.RequestedRunning)return TEXT("계산 실행 확인 중");
 if(S.AppliedReceiverOpen||S.RequestedReceiverOpen)return TEXT("수취 출구 실제 닫힘 확인 중");
 if(AnyPlantPurge(S,[](const auto& P){return P.Supported&&(!P.Valid||P.AppliedEnabled||P.RequestedEnabled||P.FlowM3s!=0||P.RequestedGaugePa!=0||P.AppliedGaugePa!=0);}))return TEXT("연결부 공기 차단 확인 중");
 if(Feed){
  if(Equipment!=TEXT("weigher")&&Equipment!=ProcessPlantBinding::SupplierId(State.ActiveSupplier))return PressureSection()+TEXT(" · 현재 공급원과 칭량호퍼만 조절 가능합니다.");
  if(!S.RequestedSuppliers.IsValidIndex(State.ActiveSupplier)||!S.RequestedSuppliers[State.ActiveSupplier]||
     S.AppliedSupplierMask!=(1<<State.ActiveSupplier)||S.RequestedHopperOpen||S.AppliedHopperOpen||
     (State.BatchMode==EProcessBatchMode::Full&&S.SupplyABatch.SourceCount==0)||
     (S.DoseCutoff.Supported&&State.BatchMode!=EProcessBatchMode::Full&&(!S.DoseCutoff.Armed||S.DoseCutoff.Triggered||S.DoseCutoff.Cancelled)))
   return TEXT("공급 종료 또는 게이트 보호 닫힘 · 압력 조절 잠김");
 }else{
  if(Equipment!=TEXT("weigher")&&Equipment!=TEXT("receiver"))return TEXT("칭량→수취 이송 · 칭량·수취 압력만 조절 가능합니다.");
  if(State.TransferId<=0||S.Transfer.Id!=State.TransferId||S.Transfer.Validity!=EPlantValueValidity::Valid||
     S.Transfer.State==EPlantTransferState::Complete||S.Transfer.State==EPlantTransferState::Indeterminate||S.Transfer.AdditionalInflowCount)
   return TEXT("이송 완료 또는 이송 기준 미확인");
  if(!S.AppliedHopperOpen||!S.RequestedHopperOpen||S.AppliedSupplierMask)return TEXT("칭량 출구 열림과 공급 밸브 닫힘 확인 중");
  for(bool Open:S.RequestedSuppliers)if(Open)return TEXT("공급 밸브 닫힘 요청 확인 중");
  if(State.BatchMode!=EProcessBatchMode::Full&&!S.SupplyABatch.DoseFinalized)return TEXT("작업자 최종 계량 확정 확인 중");
 }
 if(State.ReceiverPressurePending)return CheckPending?FString(TEXT("적용 확인 중 · 작업자 응답을 기다리세요.")):FString();
 const FVector Expected=State.PressureStageToken.IsValid()?State.ExpectedPressurePa:StageBaseline();
 if(Boundary(S,State.ActiveSupplier,true)!=Expected)return TEXT("단계 압력 적용 확인 중");
 return FString();
}
FString FProcessAutoRun::ReceiverPressureUnavailable(const FProcessControlMachine& M,double Now) const
{
 return PressureUnavailable(M,Now,TEXT("receiver"));
}
FProcessCommandRecord FProcessAutoRun::RequestReceiverPressure(FGuid Id,int64 Epoch,int64 TransferId,double Pa,FProcessControlMachine& M,double Now,const FProcessControlTransport& Send)
{
 return RequestStagePressure(Id,Epoch,TransferId,Pa,M,Now,Send,TEXT("receiver"));
}
FProcessCommandRecord FProcessAutoRun::RequestStagePressure(FGuid Id,int64 Epoch,int64 TransferId,double Pa,FProcessControlMachine& M,double Now,const FProcessControlTransport& Send,const FString& Equipment)
{
 if(M.History.Contains(Id))return M.Submit(Id,Epoch,Equipment,EProcessCommand::Pressure,Pa,Now,Send);
 const bool Feed=State.Stage==EProcessAutoStage::Feeding;
 FString Why=PressureUnavailable(M,Now,Equipment);
 if(State.ReceiverPressurePending&&PressureUnavailable(M,Now,Equipment,false).IsEmpty()&&
    State.PressureEquipment==Equipment&&State.ReceiverPressureRequestedPa==Pa&&Epoch==State.Epoch&&TransferId==State.ReceiverPressureTransferId){
  if(const auto* Prior=M.History.Find(State.ReceiverPressureRequest))return *Prior;
 }
 if(TransferId!=(Feed?0:State.TransferId)||Epoch!=State.Epoch)Why=TEXT("이송 또는 초기화 세대 변경");
 const int32 Axis=PlantControl::PressureTarget(Equipment);
 if(Axis==INDEX_NONE||!FMath::IsFinite(Pa)||Pa < -1000||Pa>(Axis==2?0:1000))Why=TEXT("잘못된 압력 대상 또는 모델 범위 밖 입력");
 TGuardValue<bool> Dispatch(M.AutomationDispatch,true);
 TGuardValue<bool> WorkerAck(M.StagePressureDispatch,true);
 const auto R=M.Submit(Id,Epoch,Equipment,EProcessCommand::Pressure,Pa,Now,Send,Why);
 if(auto* Record=M.History.Find(Id)){
  Record->PressureStage=State.PressureStageToken;Record->PressureBatch=State.RunId;
  Record->PressureSupplier=State.ActiveSupplier;Record->PressureTransfer=TransferId;
 }
 if(!R.IsPending()&&R.Status!=EProcessCommandStatus::Applied)return R;
 if(!State.PressureStageToken.IsValid())State.ExpectedPressurePa=StageBaseline();
 State.PressureEquipment=Equipment;
 State.ReceiverPressureRequest=R.RequestId;State.ReceiverPressureTransferId=TransferId;State.ReceiverPressureRevision=R.Revision;
 State.ReceiverPressurePreviousPa=Boundary(M.Snapshot,State.ActiveSupplier,true)[Axis];State.ReceiverPressureRequestedPa=Pa;
 State.ReceiverPressureRequestedSeconds=M.Snapshot.PhysicalSeconds;State.ReceiverPressurePending=R.IsPending();
 State.ReceiverPressureAppliedSeconds=R.IsPending()?-1:M.Snapshot.PhysicalSeconds;
 State.ReceiverPressureStatus=R.IsPending()?TEXT("적용 확인 중"):TEXT("적용 완료 · 작업자 설정 일치");
 if(!R.IsPending()){State.ReceiverPressureConfirmedPa=Pa;State.ExpectedPressurePa[Axis]=Pa;}
 return R;
}
bool FProcessAutoRun::ObserveReceiverPressure(FProcessControlMachine& M,FString& Why)
{
 const auto& S=M.Snapshot;
 const FVector Actual=Boundary(S,State.ActiveSupplier,true);
 if(!State.ReceiverPressurePending)return Actual==(State.PressureStageToken.IsValid()||State.ReceiverPressureRequest.IsValid()?State.ExpectedPressurePa:StageBaseline());
 const auto* R=M.History.Find(State.ReceiverPressureRequest);
 if(!R||R->Epoch!=State.Epoch||R->Connection!=State.Connection){Why=TEXT("압력 요청 세대·연결 불일치");return false;}
 const int32 Axis=PlantControl::PressureTarget(R->EquipmentId);
 FVector Next=State.ExpectedPressurePa;Next[Axis]=R->Value;
 if(R->Status==EProcessCommandStatus::Applied&&S.Sequence>R->BaselineSequence&&S.PressureAppliedRevision>=R->Revision&&Actual==Next&&Boundary(S,State.ActiveSupplier,false)[Axis]==R->Value){
  State.ExpectedPressurePa=Next;State.ReceiverPressureConfirmedPa=R->Value;State.ReceiverPressurePending=false;
  State.ReceiverPressureAppliedSeconds=S.PressureAppliedSimulationSeconds;State.ReceiverPressureStatus=TEXT("적용 완료 · 작업자 압력 설정 확인");
  return true;
 }
 if(State.Stage==EProcessAutoStage::Transferring&&S.Transfer.State==EPlantTransferState::Complete)return Actual==State.ExpectedPressurePa||Actual==Next;
 if(R->Status==EProcessCommandStatus::Rejected&&Actual==State.ExpectedPressurePa){
  State.ReceiverPressurePending=false;State.ReceiverPressureStatus=R->Message;return true;
 }
 if(!R->IsPending()){Why=State.ReceiverPressureStatus=TEXT("압력 적용 실패 · ")+R->Message;return false;}
 return Actual==State.ExpectedPressurePa||(Actual==Next&&S.PressureAppliedRevision>=R->Revision);
}
void FProcessAutoRun::EndReceiverPressure()
{
 if(State.ReceiverPressurePending)State.ReceiverPressureStatus=TEXT("취소 · 단계 종료 또는 전체 정지");
 State.ReceiverPressurePending=false;
}
