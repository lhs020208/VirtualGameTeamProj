#include "ProcessWebSnapshot.h"
#include "ProcessUIStateAdapter.h"
#include "ProcessAutoRun.h"
#include "ProcessPlantBinding.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace
{
using J=TSharedPtr<FJsonObject>;
J Obj(){return MakeShared<FJsonObject>();}
void Text(J O,const TCHAR* K,const FString& V){O->SetStringField(K,V);}
void Num(J O,const TCHAR* K,double V){if(FMath::IsFinite(V))O->SetNumberField(K,V);else O->SetField(K,MakeShared<FJsonValueNull>());}
void Child(J O,const TCHAR* K,J V){O->SetObjectField(K,V);}
FString Validity(EPlantValueValidity V)
{
 switch(V){case EPlantValueValidity::Valid:return TEXT("valid");case EPlantValueValidity::Initializing:return TEXT("initializing");case EPlantValueValidity::Stale:return TEXT("stale");case EPlantValueValidity::SourceMissing:return TEXT("source_missing");case EPlantValueValidity::Unsupported:return TEXT("unsupported");case EPlantValueValidity::Error:return TEXT("error");default:return TEXT("unknown");}
}
J Metric(double V,const TCHAR* Unit,const FString& Status,const FString& Reason=FString(),const TCHAR* Source=TEXT("모델 계산값"))
{
 auto O=Obj();const bool Valid=Status==TEXT("valid")&&FMath::IsFinite(V)&&(!(FCString::Strcmp(Unit,TEXT("kg"))==0||FCString::Strcmp(Unit,TEXT("particles"))==0)||V>=0);
 if(Valid)Num(O,TEXT("value"),V);else O->SetField(TEXT("value"),MakeShared<FJsonValueNull>());
 Text(O,TEXT("unit"),Unit);Text(O,TEXT("validity"),Status==TEXT("valid")&&!Valid?TEXT("error"):Status);Text(O,TEXT("reason"),Reason);Text(O,TEXT("source"),Source);return O;
}
J Metric(const FProcessStateValue& V,const TCHAR* Unit){return Metric(V.Value,Unit,Validity(V.Validity));}
J Entry(const FString& Id,const FString& Label,J Metrics)
{auto O=Obj();Text(O,TEXT("id"),Id);Text(O,TEXT("label"),Label);Child(O,TEXT("metrics"),Metrics);return O;}
TSharedPtr<FJsonValue> Val(J V){return MakeShared<FJsonValueObject>(V);}
FString Execution(EPlantExecutionState V){return StaticEnum<EPlantExecutionState>()->GetNameStringByValue(int64(V)).ToLower();}
}

FString ProcessWebSnapshot::Serialize(const FContext& C,const FPlantSimulationSnapshot* S,const FProcessAutoSnapshot* A,double Now)
{
 auto Root=Obj();Num(Root,TEXT("schema_version"),1);Text(Root,TEXT("publisher_id"),C.PublisherId);Text(Root,TEXT("run_id"),C.RunId);
 Text(Root,TEXT("publication_sequence"),LexToString(C.PublicationSequence));Text(Root,TEXT("level"),ProcessPlantBinding::LowPolyMap);Text(Root,TEXT("actor"),ProcessPlantBinding::SourcePath(ProcessPlantBinding::LowPolyMap));
 auto Binding=Obj();Text(Binding,TEXT("status"),S?TEXT("connected"):C.BindingStatus);Text(Binding,TEXT("reason"),C.BindingReason);Text(Binding,TEXT("next_action"),C.NextAction);Child(Root,TEXT("binding"),Binding);
 auto Control=Obj();Text(Control,TEXT("owner"),TEXT("unreal"));Control->SetBoolField(TEXT("web_read_only"),true);Control->SetBoolField(TEXT("transfer_supported"),false);Child(Root,TEXT("control"),C.Control?C.Control:Control);
 if(!S)Root->SetField(TEXT("snapshot"),MakeShared<FJsonValueNull>());
 else
 {
  const double Age=FMath::Max(0.,Now-S->PublishedWallSeconds);
  auto V=S->Validity;if(V==EPlantValueValidity::Valid&&(S->PublishedWallSeconds<=0||Age>2.))V=EPlantValueValidity::Stale;
  const FString Status=Validity(V);auto Snap=Obj();Text(Snap,TEXT("epoch"),LexToString(S->Epoch));Text(Snap,TEXT("sequence"),LexToString(S->Sequence));
  if(V==EPlantValueValidity::Valid||V==EPlantValueValidity::Stale)Num(Snap,TEXT("simulation_seconds"),S->PhysicalSeconds);else Snap->SetField(TEXT("simulation_seconds"),MakeShared<FJsonValueNull>());Num(Snap,TEXT("age_ms"),Age*1000.);Text(Snap,TEXT("validity"),Status);Text(Snap,TEXT("execution"),Execution(S->Execution));Text(Snap,TEXT("error"),S->Error);Text(Snap,TEXT("actual_actor_path"),C.ActualActorPath);
  Text(Snap,TEXT("updated_utc"),S->UpdatedUtc.ToIso8601());Text(Snap,TEXT("controller_observed_sequence"),LexToString(S->Sequence));
  auto Revisions=Obj();Text(Revisions,TEXT("requested"),LexToString(S->RequestedControlRevision));Text(Revisions,TEXT("processed"),LexToString(S->ProcessedControlRevision));Text(Revisions,TEXT("pressure_applied"),LexToString(S->PressureAppliedRevision));Text(Revisions,TEXT("rpm_applied"),LexToString(S->AppliedRPMRevision));Child(Snap,TEXT("control_revisions"),Revisions);
  TArray<TSharedPtr<FJsonValue>> Equipment;
  const int32 Suppliers=FMath::Clamp(FMath::Max(6,S->RequestedSuppliers.Num()),6,26);
  for(int32 I=0;I<Suppliers+3;++I)
  {
   const bool Supply=I<Suppliers;const FString Id=Supply?FString::Printf(TEXT("supply-%c"),TCHAR('a'+I)):I==Suppliers?TEXT("weigher"):I==Suppliers+1?TEXT("receiver"):TEXT("mixer");
   const bool HasBatch=A&&A->RunId.IsValid()&&A->Epoch==S->Epoch;
   const auto* Material=HasBatch?A->EntryFor(I):C.AppliedEntries.FindByPredicate([I](const auto& E){return E.Enabled&&E.Supplier==I;});
   const FString BaseLabel=Supply?FString::Printf(TEXT("공급 %c"),TCHAR('A'+I)):I==Suppliers?TEXT("칭량호퍼 WH101"):I==Suppliers+1?TEXT("수취호퍼 RH-201"):TEXT("혼합기 MX-301");
   const FString Label=Material?BaseLabel+TEXT(" · ")+Material->DisplayName:BaseLabel;
   const auto E=UProcessUIStateAdapter::BuildEquipment(Id,S,Now);auto M=Obj();
   Child(M,TEXT("valve_requested"),Metric(E.ValveRequestedOpen,TEXT("bool")));Child(M,TEXT("valve_applied"),Metric(E.ValveAppliedOpen,TEXT("bool")));
   Child(M,TEXT("particles"),Metric(E.RegionParticleCount,TEXT("particles")));Child(M,TEXT("mass"),Metric(E.CurrentMass.Kg,TEXT("kg"),Validity(E.CurrentMass.Validity)));
   Child(M,TEXT("pressure_requested"),Metric(E.RequestedPressurePa.Value,TEXT("Pa(g)"),Validity(E.RequestedPressurePa.Validity),E.PressureScope,TEXT("호퍼 목표 압력")));
   Child(M,TEXT("pressure_applied"),Metric(E.AppliedPressurePa.Value,TEXT("Pa(g)"),Validity(E.AppliedPressurePa.Validity),E.PressureScope,TEXT("적용 압력 설정 · 센서값 아님")));
   if(Supply)
   {
    if(Material){
     Child(M,TEXT("target"),Metric(Material->TargetKg,TEXT("kg"),Status));
     const auto* D=HasBatch&&S->SequentialBatch.Materials.IsValidIndex(I)?&S->SequentialBatch.Materials[I]:nullptr;
     Child(M,TEXT("final"),Metric(D?D->FinalKg:0,TEXT("kg"),D&&D->FinalSimulationSeconds>=0?Status:TEXT("unknown")));
    }
    Child(M,TEXT("initial_particles"),Metric(E.InitialParticleCount,TEXT("particles")));
    const auto* R=S->Recovery.Materials.FindByPredicate([I](const FPlantRecoveryMaterial& P){return P.Supplier==I;});
    const FString Inventory=S->Recovery.Valid&&R&&!R->Unknown?Status:TEXT("unknown");
    Child(M,TEXT("particles"),Metric(R?R->Source:0,TEXT("particles"),Inventory,TEXT("공급원 ID 기준 현재 상류 재고")));
    Child(M,TEXT("mass"),Metric(R?R->Source*S->SourceKg(R->Supplier):0,TEXT("kg"),S->ModelParticleMassKg>0?Inventory:TEXT("unknown")));
    const auto* P=S->SupplyValvePressures.FindByPredicate([I](const FPlantSupplyValvePressure& Q){return Q.Supplier==I;});
    const bool Open=E.ValveAppliedOpen.Validity==EPlantValueValidity::Valid&&E.ValveAppliedOpen.Value!=0;
    const FString PV=V!=EPlantValueValidity::Valid?Status:P&&P->Mapped&&P->Valid&&Open?TEXT("valid"):TEXT("not_computed");
    FString Why=P?P->Reason:TEXT("밸브 경로 위치 미확인");if(E.ValveAppliedOpen.Validity!=EPlantValueValidity::Valid)Why=TEXT("밸브 실제 적용 상태 미확인");else if(!Open)Why=TEXT("닫힘 · 차압 미산출");
    if(I<6&&V==EPlantValueValidity::Valid&&PlantPurgeFor(*S,I).Valid&&PlantPurgeFor(*S,I).AppliedEnabled)Why+=TEXT(" · 하류 독립 공기 작동 · 공급 밸브 통과 유량과 구분");
    Child(M,TEXT("valve_before"),Metric(P?P->BeforeGaugePa:0,TEXT("Pa(g)"),PV,Why,TEXT("모델 추정값")));
    Child(M,TEXT("valve_after"),Metric(P?P->AfterGaugePa:0,TEXT("Pa(g)"),PV,Why,TEXT("모델 추정값")));
    Child(M,TEXT("valve_difference"),Metric(P?P->DifferencePa:0,TEXT("Pa"),PV,Why,TEXT("모델 추정값 · 직전 − 직후")));
    Child(M,TEXT("flow"),Metric(P?P->FlowM3s:0,TEXT("m3/s"),Open&&P&&P->FlowValid?Status:PV,Why,TEXT("계산 유량")));
    Child(M,TEXT("weigher_pressure_applied"),Metric(S->AppliedGaugePressurePa.Y,TEXT("Pa(g)"),S->AppliedPressureValid?Status:TEXT("unknown"),FString(),TEXT("칭량호퍼 적용 압력 설정")));
    if(I<6){const auto& Purg=PlantPurgeFor(*S,I);const FString PurgeStatus=Purg.Supported?(Purg.Valid?Status:TEXT("unknown")):TEXT("unsupported");Child(M,TEXT("connector_mass"),Metric(Purg.ResidualKg,TEXT("kg"),PurgeStatus));Child(M,TEXT("connector_particles"),Metric(Purg.ResidualCount,TEXT("particles"),PurgeStatus));Child(M,TEXT("purge_requested"),Metric(Purg.RequestedEnabled?1:0,TEXT("bool"),PurgeStatus));Child(M,TEXT("purge_applied"),Metric(Purg.AppliedEnabled?1:0,TEXT("bool"),PurgeStatus));Child(M,TEXT("purge_pressure_requested"),Metric(Purg.RequestedGaugePa,TEXT("Pa(g)"),PurgeStatus));Child(M,TEXT("purge_pressure_applied"),Metric(Purg.AppliedGaugePa,TEXT("Pa(g)"),PurgeStatus));Child(M,TEXT("purge_flow"),Metric(Purg.FlowM3s,TEXT("m3/s"),Purg.Supported?(Purg.Valid?Status:TEXT("unknown")):TEXT("unsupported"),Purg.Diagnostic,TEXT("하류 독립 공기 계산 유량")));}
   }
   else if(Id==TEXT("mixer"))
   {
    M->RemoveField(TEXT("valve_requested"));M->RemoveField(TEXT("valve_applied"));M->RemoveField(TEXT("pressure_requested"));M->RemoveField(TEXT("pressure_applied"));
    Child(M,TEXT("rpm_target"),Metric(E.RequestedRPM,TEXT("rpm")));Child(M,TEXT("rpm_applied"),Metric(E.AppliedRPM,TEXT("rpm")));
    Child(M,TEXT("mass"),Metric(S->MixerMassKg,TEXT("kg"),S->ModelParticleMassKg>0?Status:TEXT("unknown"),FString(),TEXT("현재 모델 입자 질량 합계")));
   }
   else Child(M,TEXT("flow"),Metric(S->Transfer.CalculatedCarrierFlowM3s,TEXT("m3/s"),S->Transfer.CarrierFlowValid?Status:TEXT("not_computed"),S->Transfer.Reason,TEXT("계산 이송 유량")));
   auto Q=Entry(Id,Label,M);if(Material)Text(Q,TEXT("role"),Material->Additive?TEXT("첨가제"):TEXT("일반 원료"));Text(Q,TEXT("kind"),Supply?TEXT("supplier"):Id);Equipment.Add(Val(Q));
  }
  Snap->SetArrayField(TEXT("equipment"),Equipment);TArray<TSharedPtr<FJsonValue>> Regions;
  auto Pipe=Obj();Child(Pipe,TEXT("mass"),Metric(S->PipeMass.Kg,TEXT("kg"),V==EPlantValueValidity::Valid?Validity(S->PipeMass.Validity):Status));
  Child(Pipe,TEXT("particles"),Metric(S->ModelParticleMassKg>0?S->InTransferPipe:0,TEXT("particles"),S->ModelParticleMassKg>0&&S->PipeMass.Validity==EPlantValueValidity::Valid?Status:TEXT("unknown"),FString(),TEXT("이송 배관 내부 입자 수")));Regions.Add(Val(Entry(TEXT("transfer-pipe"),TEXT("칭량–수취 배관"),Pipe)));
  auto Connector=Obj();Child(Connector,TEXT("mass"),Metric(S->MixerFeed.ConnectorKg,TEXT("kg"),S->MixerFeed.Valid?Status:TEXT("unknown"),FString(),TEXT("이번 확정 배치 연결부 잔량")));Regions.Add(Val(Entry(TEXT("mixer-connector"),TEXT("수취–혼합 연결부 · 이번 배치"),Connector)));Snap->SetArrayField(TEXT("regions"),Regions);
  auto Batch=Obj();const bool AutoValid=A&&(A->Epoch==S->Epoch||(!A->Active&&!A->RunId.IsValid()&&A->Stage==EProcessAutoStage::Idle));Text(Batch,TEXT("stage"),AutoValid?StaticEnum<EProcessAutoStage>()->GetNameStringByValue(int64(A->Stage)).ToLower():TEXT("unknown"));
  Text(Batch,TEXT("label"),AutoValid?(A->Active&&!A->Entries.IsEmpty()?FString::Printf(TEXT("%c · %s"),'A'+A->ActiveSupplier,*FProcessAutoRun::StageText(A->Stage)):FProcessAutoRun::StageText(A->Stage)):TEXT("배치 상태 확인 대기"));Batch->SetBoolField(TEXT("active"),AutoValid&&A->Active);
  Text(Batch,TEXT("reason"),AutoValid?A->Reason:TEXT("현재 초기화 세대의 자동 배치 정보 없음"));Text(Batch,TEXT("block_reason"),C.BlockReason);Text(Batch,TEXT("stop_stage"),C.StopStage);Text(Batch,TEXT("stop_reason"),C.StopReason);
  Text(Batch,TEXT("validity"),AutoValid?Status:TEXT("unknown"));Text(Batch,TEXT("run_id"),AutoValid?A->RunId.ToString():FString());
  TArray<TSharedPtr<FJsonValue>> Materials;
  const FPlantMaterialBatchSnapshot* Batches[]={&S->SequentialBatch.A,&S->SequentialBatch.B,&S->SequentialBatch.Additive};
  for(int32 I=0;I<(S->SequentialBatch.Materials.IsEmpty()?3:S->SequentialBatch.Materials.Num());++I){
   if(AutoValid&&A->RunId.IsValid()&&!A->UsesSupplier(I))continue;
   const auto* Plan=AutoValid?A->EntryFor(I):nullptr;
   const auto& B=S->SequentialBatch.Materials.IsValidIndex(I)?S->SequentialBatch.Materials[I]:*Batches[I];auto M=Obj();const FString Final=B.FinalSimulationSeconds>=0?Status:TEXT("unknown");
   Child(M,TEXT("target"),Metric(Plan?Plan->TargetKg:B.TargetKg,TEXT("kg"),(Plan||B.TargetKg>0)?Status:TEXT("unknown")));Child(M,TEXT("final"),Metric(B.FinalKg,TEXT("kg"),Final));
   Child(M,TEXT("receiver"),Metric(S->MixerFeed.ReceiverKg,TEXT("kg"),B.InitialCount>0&&S->ModelParticleMassKg>0?Status:TEXT("unknown")));
   Child(M,TEXT("mixer"),Metric(S->MixerFeed.MixerKg,TEXT("kg"),B.InitialCount>0&&S->ModelParticleMassKg>0?Status:TEXT("unknown")));
   Materials.Add(Val(Entry(FString::Printf(TEXT("supply-%c"),TCHAR('a'+I)),Plan?Plan->DisplayName:FString::Printf(TEXT("원료 %c"),TCHAR('A'+I)),M)));}
  Batch->SetArrayField(TEXT("materials"),Materials);Child(Snap,TEXT("batch"),Batch);Child(Root,TEXT("snapshot"),Snap);
 }
 FString Out;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out));return Out;
}
