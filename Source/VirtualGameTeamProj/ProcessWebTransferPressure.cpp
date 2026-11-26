#include "ProcessWebTransferPressure.h"
#include "ProcessControlService.h"
#include "ProcessPlantBinding.h"
namespace {
void Number(const TSharedPtr<FJsonObject>& O,const TCHAR* K,double V,bool Known){if(Known&&FMath::IsFinite(V))O->SetNumberField(K,V);else O->SetField(K,MakeShared<FJsonValueNull>());}
}
FString ProcessWebTransferPressure::Reason(const UProcessControlService& S,double Now)
{
 const FString Why=S.AutoRun.ReceiverPressureUnavailable(S.State,Now);
 if(Why.IsEmpty())return Why; // Never broaden the original service's admission.
 if(S.State.IsStopping())return TEXT("전체 정지 확인 중 · 취소된 압력은 자동 복원되지 않습니다.");
 if(S.State.StopStage==EProcessStopStage::Failed)return TEXT("전체 정지 실패 · 미확인 장비를 확인하고 정지를 재시도하세요.");
 if(!S.AutoRun.State.Active)return S.AutoRun.State.RunId.IsValid()?TEXT("자동 이송 종료 또는 중단 · 새 이송 중에만 조절할 수 있습니다."):TEXT("이송 준비 전 · 레시피 적용 후 자동 배치를 시작하세요.");
 return Why;
}
TSharedPtr<FJsonObject> ProcessWebTransferPressure::Describe(const UProcessControlService& S,double Now)
{
 const auto& M=S.State;const auto& P=M.Snapshot;const auto& A=S.AutoRun.State;auto O=MakeShared<FJsonObject>();
 const bool Fresh=M.Connected&&P.PublishedWallSeconds>0&&P.Validity==EPlantValueValidity::Valid&&P.Error.IsEmpty()&&Now>=P.PublishedWallSeconds&&Now-P.PublishedWallSeconds<=2.;
 const bool SameBatch=A.RunId.IsValid()&&A.Epoch==P.Epoch&&A.Connection==M.Connection;
 const bool SameTransfer=SameBatch&&A.TransferId>0&&P.Transfer.Id==A.TransferId;
 const auto* Request=M.History.Find(A.ReceiverPressureRequest);
 const bool Scoped=SameTransfer&&A.ReceiverPressureTransferId==A.TransferId&&Request&&Request->Epoch==P.Epoch&&Request->Connection==M.Connection&&Request->Command==EProcessCommand::Pressure&&Request->EquipmentId==TEXT("receiver");
 const bool Flow=Fresh&&SameTransfer&&P.AppliedHopperOpen&&!P.AppliedReceiverOpen&&P.Transfer.Validity==EPlantValueValidity::Valid&&P.Transfer.CarrierFlowValid;
 O->SetStringField(TEXT("batch_id"),SameBatch?A.RunId.ToString(EGuidFormats::DigitsWithHyphens):FString());O->SetStringField(TEXT("transfer_id"),SameTransfer&&A.Stage==EProcessAutoStage::Transferring?LexToString(A.TransferId):TEXT("0"));
 O->SetStringField(TEXT("epoch"),LexToString(P.Epoch));O->SetStringField(TEXT("sequence"),LexToString(P.Sequence));Number(O,TEXT("simulation_seconds"),P.PhysicalSeconds,P.Validity==EPlantValueValidity::Valid||P.Validity==EPlantValueValidity::Stale);
 O->SetStringField(TEXT("validity"),Fresh?TEXT("valid"):P.Validity==EPlantValueValidity::Error||!P.Error.IsEmpty()?TEXT("error"):TEXT("stale"));
 const auto Why=Reason(S,Now);O->SetBoolField(TEXT("available"),Why.IsEmpty());O->SetStringField(TEXT("reason"),Why);O->SetStringField(TEXT("stage"),FProcessAutoRun::StageText(A.Stage));
 Number(O,TEXT("recipe_pa"),A.LockedTransferPressurePa.Z,SameBatch);Number(O,TEXT("requested_pa"),A.ReceiverPressureRequestedPa,Fresh&&Scoped);
 Number(O,TEXT("applied_pa"),P.AppliedGaugePressurePa.Z,Fresh&&P.AppliedPressureValid);Number(O,TEXT("flow_m3_s"),P.Transfer.CalculatedCarrierFlowM3s,Flow);
 O->SetStringField(TEXT("flow_reason"),Flow?FString():TEXT("미산출 · 현재 이송 유로와 최신 계산 유량 확인 필요"));
 O->SetStringField(TEXT("service_request_id"),Scoped?Request->RequestId.ToString(EGuidFormats::DigitsWithHyphens):FString());O->SetBoolField(TEXT("pending"),Scoped&&A.ReceiverPressurePending);
 O->SetStringField(TEXT("status"),Scoped?A.ReceiverPressureStatus:TEXT("이번 이송의 조절 요청 없음"));
 Number(O,TEXT("requested_s"),A.ReceiverPressureRequestedSeconds,Scoped&&A.ReceiverPressureRequestedSeconds>=0);Number(O,TEXT("applied_s"),A.ReceiverPressureAppliedSeconds,Scoped&&A.ReceiverPressureAppliedSeconds>=0);
 O->SetStringField(TEXT("stage_token"),A.PressureStageToken.IsValid()?A.PressureStageToken.ToString(EGuidFormats::DigitsWithHyphens):FString());
 O->SetStringField(TEXT("supplier_id"),ProcessPlantBinding::SupplierId(A.ActiveSupplier));
 O->SetStringField(TEXT("section"),S.AutoRun.PressureSection());
 auto Targets=MakeShared<FJsonObject>();
 for(int I=0;I<8;++I){
  const FString Id=I<6?ProcessPlantBinding::SupplierId(I):I==6?TEXT("weigher"):TEXT("receiver");
  const int Axis=PlantControl::PressureTarget(Id);
  const bool Feed=A.Stage==EProcessAutoStage::Feeding;
  const bool Participates=SameBatch&&A.Active&&(Feed?(I==A.ActiveSupplier||Axis==1):(A.Stage==EProcessAutoStage::Transferring&&Axis>0));
  const auto* TargetRequest=Participates?S.AutoRun.LatestStagePressure(M,Id):nullptr;
  auto Value=MakeShared<FJsonObject>();const FString Reason=S.AutoRun.PressureUnavailable(M,Now,Id);
  Value->SetBoolField(TEXT("available"),Reason.IsEmpty());Value->SetStringField(TEXT("reason"),Reason);
  Value->SetStringField(TEXT("status"),TargetRequest?FProcessControlMachine::StatusText(TargetRequest->Status):FString());
  Number(Value,TEXT("recipe_pa"),S.AutoRun.StageBaseline()[Axis],Participates);
  Number(Value,TEXT("requested_pa"),TargetRequest?TargetRequest->Value:0,Fresh&&TargetRequest);
  Number(Value,TEXT("applied_pa"),Axis==0?P.SupplierPressure(I,true):P.AppliedGaugePressurePa[Axis],Fresh&&P.AppliedPressureValid);
  double Q=0;const bool FlowKnown=Participates&&Fresh&&S.AutoRun.PressureFlow(P,Q);Number(Value,TEXT("flow_m3_s"),Q,FlowKnown);
  Targets->SetObjectField(Id,Value);
 }
 O->SetObjectField(TEXT("targets"),Targets);
 O->SetStringField(TEXT("pressure_applied_revision"),LexToString(P.PressureAppliedRevision));return O;
}
