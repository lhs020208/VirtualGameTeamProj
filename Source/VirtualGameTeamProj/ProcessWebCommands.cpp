#include "ProcessWebCommands.h"
#include "ProcessControlService.h"
#include "ProcessPlantBinding.h"
#include "ProcessWebTransferPressure.h"

namespace {
using J=TSharedPtr<FJsonObject>;
J Object(){return MakeShared<FJsonObject>();}
FString Str(const J& Jv,const TCHAR* Key){FString V;if(Jv)Jv->TryGetStringField(Key,V);return V;}
bool Integer(const FString& V,int64& N){if(V.IsEmpty()||V.Len()>19||(V.Len()>1&&V[0]=='0'))return false;for(TCHAR C:V)if(C<'0'||C>'9')return false;return LexTryParseString(N,*V)&&N>=0&&LexToString(N)==V;}
const TCHAR* Status(EProcessCommandStatus S){switch(S){case EProcessCommandStatus::Accepted:return TEXT("accepted");case EProcessCommandStatus::Processing:return TEXT("processing");case EProcessCommandStatus::Applied:return TEXT("applied");case EProcessCommandStatus::Rejected:return TEXT("rejected");case EProcessCommandStatus::Failed:return TEXT("failed");default:return TEXT("unknown");}}
}
FString ProcessWebCommands::FCommand::Fingerprint() const{return FString::Printf(TEXT("%s|%lld|%s|%s|%s|%.17g"),*Run,Epoch,*Equipment,*Kind,*Unit,InputValue)+TEXT("|")+RecipeJson+TEXT("|")+FileId+TEXT("|")+FileVersion+TEXT("|")+RecipeRevision+TEXT("|")+RecipeFingerprint+TEXT("|")+BatchId+TEXT("|")+CheckToken+LexToString(Confirm)+TEXT("|")+LexToString(TransferId)+TEXT("|")+PressureStage.ToString()+TEXT("|")+SupplierId+TEXT("|")+LexToString(MixRun);}
bool ProcessWebCommands::Decode(const J& O,FCommand& R,FString& Error)
{
 Error=TEXT("명령 형식·ID·단위 또는 값이 잘못되었습니다.");double Version=0;
 if(!O||!O->TryGetNumberField(TEXT("command_version"),Version)||Version!=1||!FGuid::ParseExact(Str(O,TEXT("request_id")),EGuidFormats::DigitsWithHyphens,R.Id)||!R.Id.IsValid()||!Integer(Str(O,TEXT("epoch")),R.Epoch))return false;
 R.Run=Str(O,TEXT("run_id"));R.Lease=Str(O,TEXT("lease_id"));R.Equipment=Str(O,TEXT("equipment"));R.Kind=Str(O,TEXT("kind"));R.Unit=Str(O,TEXT("unit"));
 if(R.Run.IsEmpty()||R.Run.Len()>128||R.Lease.Len()>128||!O->TryGetNumberField(TEXT("value"),R.InputValue)||!FMath::IsFinite(R.InputValue))return false;
 R.Value=R.InputValue;const bool Supplier=ProcessPlantBinding::SupplierIndex(R.Equipment)!=INDEX_NONE;
 if(FProcessWebBatch::Handles(R.Kind)){
  if(R.Equipment!=TEXT("plant")||R.Unit!=TEXT("none")||R.Value!=0)return false;
  R.RecipeJson=Str(O,TEXT("recipe_json"));R.FileId=Str(O,TEXT("file_id"));R.FileVersion=Str(O,TEXT("file_version"));R.RecipeRevision=Str(O,TEXT("recipe_revision"));R.RecipeFingerprint=Str(O,TEXT("recipe_fingerprint"));R.BatchId=Str(O,TEXT("batch_id"));R.CheckToken=Str(O,TEXT("check_token"));O->TryGetBoolField(TEXT("confirm"),R.Confirm);
  if(R.RecipeJson.Len()>8192||R.FileId.Len()>64||R.FileVersion.Len()>64||R.BatchId.Len()>64||R.CheckToken.Len()>64||R.RecipeFingerprint.Len()>64||R.RecipeRevision.Len()>19)return false;
  if((R.Kind==TEXT("recipe_apply")||R.Kind==TEXT("recipe_save"))&&R.RecipeJson.IsEmpty())return false;
  Error.Empty();return true;
 }
 if(R.Kind==TEXT("mix_rpm")){
  FGuid Batch;R.BatchId=Str(O,TEXT("batch_id"));
  if(!O->HasTypedField<EJson::Number>(TEXT("value"))||R.Equipment!=TEXT("mixer")||R.Unit!=TEXT("rpm")||
     R.Value<=0||R.Value>PlantControl::MaxRPM||!FGuid::ParseExact(R.BatchId,EGuidFormats::DigitsWithHyphens,Batch)||
     !Batch.IsValid()||!Integer(Str(O,TEXT("mix_run")),R.MixRun)||R.MixRun<=0)return false;
  R.Command=EProcessCommand::MixerRPM;Error.Empty();return true;
 }
 if(R.Kind==TEXT("transfer_pressure")){
  const auto* RawValue=O->Values.Find(TEXT("value"));const auto* RawVersion=O->Values.Find(TEXT("command_version"));
  if(!RawValue||!RawVersion||(*RawValue)->Type!=EJson::Number||(*RawVersion)->Type!=EJson::Number)return false; // UE otherwise accepts numeric strings.
  FGuid Batch;R.BatchId=Str(O,TEXT("batch_id"));
  const bool Scoped=O->HasField(TEXT("stage_token"))||O->HasField(TEXT("supplier_id"));
  R.PressureStage.Invalidate();R.SupplierId=Str(O,TEXT("supplier_id"));
  if(Scoped&&(!FGuid::ParseExact(Str(O,TEXT("stage_token")),EGuidFormats::DigitsWithHyphens,R.PressureStage)||!R.PressureStage.IsValid()||ProcessPlantBinding::SupplierIndex(R.SupplierId)==INDEX_NONE))return false;
  if((Scoped?PlantControl::PressureTarget(R.Equipment)==INDEX_NONE:R.Equipment!=TEXT("receiver"))||R.Unit!=TEXT("kPa(g)")||R.Value<-1||R.Value>(R.Equipment==TEXT("receiver")?0:1)||!FGuid::ParseExact(R.BatchId,EGuidFormats::DigitsWithHyphens,Batch)||!Batch.IsValid()||!Integer(Str(O,TEXT("transfer_id")),R.TransferId)||(!Scoped&&R.TransferId<=0))return false;
  R.Value=R.InputValue*PlantControl::PaPerKPa;R.Command=EProcessCommand::Pressure;Error.Empty();return true;
 }
 if(R.Kind==TEXT("stop_all")||R.Kind==TEXT("resume")||R.Kind==TEXT("pause")){
  if(R.Equipment!=TEXT("plant")||R.Unit!=TEXT("none")||R.Value!=0)return false;
  R.Command=R.Kind==TEXT("stop_all")?EProcessCommand::StopAll:R.Kind==TEXT("resume")?EProcessCommand::Resume:EProcessCommand::Pause;
 }else if(R.Kind==TEXT("valve")||R.Kind==TEXT("mixer_enabled")||R.Kind==TEXT("purge_enabled")){
  if(R.Unit!=TEXT("bool")||(R.Value!=0&&R.Value!=1))return false;
  if(R.Kind==TEXT("valve")){if(!Supplier&&R.Equipment!=TEXT("weigher")&&R.Equipment!=TEXT("receiver"))return false;R.Command=EProcessCommand::Valve;}
  else if(R.Kind==TEXT("mixer_enabled")){if(R.Equipment!=TEXT("mixer"))return false;R.Command=EProcessCommand::MixerEnabled;}
  else{if(ProcessPlantBinding::SupplierIndex(R.Equipment)<0)return false;R.Command=EProcessCommand::PurgeEnabled;}
 }else if(R.Kind==TEXT("rpm")){
  if(R.Unit!=TEXT("rpm")||R.Equipment!=TEXT("mixer")||R.Value<PlantControl::MinRPM||R.Value>PlantControl::MaxRPM)return false;R.Command=EProcessCommand::MixerRPM;
 }else if(R.Kind==TEXT("pressure")||R.Kind==TEXT("purge_pressure")){
  if(R.Unit!=TEXT("kPa(g)"))return false;
  R.Value=R.InputValue*PlantControl::PaPerKPa;
  if(R.Kind==TEXT("pressure")){if(PlantControl::PressureTarget(R.Equipment)==INDEX_NONE||R.Value<-1000||R.Value>(R.Equipment==TEXT("receiver")?0:1000))return false;R.Command=EProcessCommand::Pressure;}
  else{if((ProcessPlantBinding::SupplierIndex(R.Equipment)<0)||R.Value<0||R.Value>1000)return false;R.Command=EProcessCommand::PurgePressure;}
 }else{Error=TEXT("이번 단계에서 지원하지 않는 명령입니다.");return false;}
 Error.Empty();return true;
}
void ProcessWebCommands::FBridge::Release(UProcessControlService& S,const FString& Reason)
{
 S.WebLeaseId.Empty();S.WebLeaseUntil=0;
 bool Pending=false;
 for(auto& P:Results)if(auto* R=S.State.History.Find(P.Value.Record.RequestId);R&&R->IsPending()&&R->Command!=EProcessCommand::StopAll)Pending=true;
 // Already submitted mailbox effects cannot be silently rolled back. Existing whole-stop cancels
 // waiting openings/purge tokens safely. No batch restart, and no stop for mere observer disconnects.
 if(Pending)S.Request(TEXT("plant"),EProcessCommand::StopAll,0);
 for(auto& P:Results)if(!FProcessWebBatch::Handles(P.Value.Command.Kind)&&P.Value.Record.IsPending()&&!S.State.History.Contains(P.Value.Record.RequestId)){P.Value.Record.Status=EProcessCommandStatus::Unknown;P.Value.Record.Message=Reason;}
}
void ProcessWebCommands::FBridge::Receive(const J& Frame,const FString& Run,UProcessControlService& S,double Elapsed)
{
 check(IsInGameThread());if(!Frame||Elapsed>2.0)return;
 const FString Lease=Str(Frame,TEXT("lease_id"));double Remaining=0;Frame->TryGetNumberField(TEXT("lease_remaining_ms"),Remaining);
 int64 Epoch=-1;const bool Identity=Str(Frame,TEXT("run_id"))==Run&&Integer(Str(Frame,TEXT("epoch")),Epoch)&&Epoch==S.State.Snapshot.Epoch;
 if(!Identity){Release(S,TEXT("실행·초기화 세대 변경"));return;}
 if(Lease.IsEmpty()||Remaining<=Elapsed*1000){if(!S.WebLeaseId.IsEmpty())Release(S,TEXT("웹 연결·제어권 종료"));}
 else if(!S.State.IsStopping()&&(!S.WebOwnsControl()||S.WebLeaseId==Lease)){
  bool LocalPending=false;for(const auto& P:S.State.History)if(P.Value.IsPending()&&!Results.Contains(P.Key)&&!S.AutoRun.State.Active)LocalPending=true;
  if(!LocalPending){S.WebLeaseId=Lease;S.WebLeaseEpoch=Epoch;S.WebLeaseUntil=FPlatformTime::Seconds()+FMath::Min(3.,Remaining/1000.-Elapsed);}
 }else if(S.WebLeaseId==Lease&&S.WebOwnsControl())S.WebLeaseUntil=FPlatformTime::Seconds()+FMath::Min(3.,Remaining/1000.-Elapsed);
 // Presentation acknowledgement preserves the existing UI-confirmation interlock.
 const TSharedPtr<FJsonObject>* Seen=nullptr;
 if(Frame->TryGetObjectField(TEXT("observed_transfer"),Seen)){
  const auto& A=S.AutoRun.State;const auto& P=S.State.Snapshot;int64 Transfer=-1,Sequence=-1,SeenEpoch=-1;
  if(Str(*Seen,TEXT("run_id"))==Run&&Integer(Str(*Seen,TEXT("epoch")),SeenEpoch)&&SeenEpoch==P.Epoch&&Str(*Seen,TEXT("batch_id"))==A.RunId.ToString(EGuidFormats::DigitsWithHyphens)&&
     Integer(Str(*Seen,TEXT("transfer_id")),Transfer)&&Integer(Str(*Seen,TEXT("sequence")),Sequence)&&Sequence<=P.Sequence&&Transfer==A.TransferId&&Transfer==P.Transfer.Id&&
     A.Active&&A.Epoch==P.Epoch&&A.Stage==EProcessAutoStage::Transferring&&P.Transfer.State==EPlantTransferState::Complete&&P.Transfer.Validity==EPlantValueValidity::Valid&&P.Validity==EPlantValueValidity::Valid&&FPlatformTime::Seconds()-P.PublishedWallSeconds<=2.)
   S.ConfirmAutomaticUI(P.Epoch,Transfer,P.PhysicalSeconds);
 }
 const TArray<TSharedPtr<FJsonValue>>* Acks=nullptr;
 if(Frame->TryGetArrayField(TEXT("result_acks"),Acks)&&Acks->Num()<=256)for(const auto& V:*Acks){FString Id;FGuid Guid;if(V->TryGetString(Id)&&FGuid::Parse(Id,Guid))if(auto* R=Results.Find(Guid))R->Data.Reset();}
 const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;if(!Frame->TryGetArrayField(TEXT("commands"),Items)||Items->Num()>16)return;
 TArray<J> Ordered;for(const auto& V:*Items)if(V->Type==EJson::Object)Ordered.Add(V->AsObject());
 Ordered.StableSort([](const J& A,const J& B){return Str(A,TEXT("kind"))==TEXT("stop_all")&&Str(B,TEXT("kind"))!=TEXT("stop_all");});
 bool StopSeen=false;
 for(const auto& O:Ordered){
  FCommand C;FString Error;if(!Decode(O,C,Error))continue;
  if(auto* Existing=Results.Find(C.Id)){continue;} // Immutable IDs, never redispatch after reconnect.
  if(Results.Num()>=256){if(C.Kind!=TEXT("stop_all"))continue;for(auto It=Results.CreateIterator();It;++It)if(!It->Value.Record.IsPending()){It.RemoveCurrent();break;}}
  FResult R;R.Command=C;R.Record.RequestId=C.Id;R.Record.Epoch=C.Epoch;R.Record.EquipmentId=C.Equipment;R.Record.Command=C.Command;R.Record.Value=C.Value;R.Record.Status=EProcessCommandStatus::Rejected;
  if(C.Run!=Run||C.Epoch!=S.State.Snapshot.Epoch)R.Record.Message=TEXT("이전 실행·초기화 세대 명령 거부");
  else if(StopSeen&&C.Kind!=TEXT("stop_all")&&!FProcessWebBatch::ReadOnly(C.Kind))R.Record.Message=TEXT("전체 정지 우선 · 새 사용자 요청 필요");
  else if(FProcessWebBatch::Handles(C.Kind))Batch.Execute(C,S,R);
  else if(C.Kind==TEXT("mix_rpm")){
   FGuid BatchId;FGuid::ParseExact(C.BatchId,EGuidFormats::DigitsWithHyphens,BatchId);
   R.BatchId=C.BatchId;R.Record=S.RequestMixSpeed(C.Id,C.Epoch,BatchId,C.MixRun,C.Value,&C.Lease);
  }
  else if(C.Kind==TEXT("transfer_pressure")){FGuid BatchId;FGuid::ParseExact(C.BatchId,EGuidFormats::DigitsWithHyphens,BatchId);R.BatchId=C.BatchId;if(!C.PressureStage.IsValid()&&S.AutoRun.State.PressureStageToken.IsValid())R.Record.Message=TEXT("현재 단계·공급원 식별자가 필요합니다. 웹 화면을 갱신하세요.");else R.Record=C.PressureStage.IsValid()?S.RequestStagePressure(C.Id,C.Epoch,BatchId,C.PressureStage,ProcessPlantBinding::SupplierIndex(C.SupplierId),C.TransferId,C.Equipment,C.Value,&C.Lease):S.RequestWebTransferReceiverPressure(C.Id,C.Epoch,C.Lease,BatchId,C.TransferId,C.Value);}
  else {R.Record=S.RequestWeb(C.Id,C.Epoch,C.Lease,C.Equipment,C.Command,C.Value);if(C.Command==EProcessCommand::StopAll)StopSeen=true;}
  Results.Add(C.Id,R);
 }
}
TSharedPtr<FJsonObject> ProcessWebCommands::FBridge::Describe(UProcessControlService* S,bool Enabled,double Now)
{
 auto O=Object();O->SetStringField(TEXT("owner"),S&&S->WebOwnsControl()?TEXT("web"):TEXT("unreal"));O->SetBoolField(TEXT("web_read_only"),!Enabled);O->SetBoolField(TEXT("transfer_supported"),Enabled);O->SetNumberField(TEXT("command_version"),1);
 O->SetStringField(TEXT("lease_id"),S&&S->WebOwnsControl()?S->WebLeaseId:FString());O->SetStringField(TEXT("pause_policy"),S?S->ManualPausePolicy:TEXT("unknown"));O->SetStringField(TEXT("unconfirmed"),!S?TEXT("해석 연결 없음"):(S->State.IsStopping()||S->State.StopStage==EProcessStopStage::Failed)?S->State.UnconfirmedEquipment():FString());
 bool Pending=false;if(S)for(const auto& P:S->State.History)if(P.Value.IsPending())Pending=true;
 const FString Acquire=!Enabled?TEXT("언리얼 웹 제어 옵션이 꺼져 있습니다."):!S?TEXT("명시적 해석 연결 대기"):S->State.IsStopping()||(Pending&&!S->AutoRun.State.Active)?TEXT("기존 명령·정지 확인 후 제어권을 가져오세요."):S->State.Snapshot.Validity!=EPlantValueValidity::Valid||Now-S->State.Snapshot.PublishedWallSeconds>2?TEXT("최신 데이터 대기"):FString();O->SetStringField(TEXT("acquire_reason"),Acquire);
 TArray<TSharedPtr<FJsonValue>> Actions;
 auto Add=[&](const FString& Id,const TCHAR* Kind,EProcessCommand Cmd,double Value,const TCHAR* Unit){auto A=Object();A->SetStringField(TEXT("equipment"),Id);A->SetStringField(TEXT("kind"),Kind);A->SetNumberField(TEXT("value"),Value);A->SetStringField(TEXT("unit"),Unit);FString Why=!Enabled?TEXT("조회 전용 연결 · 웹 제어 옵션 필요"):!S?TEXT("해석 연결 대기"):S->State.UnavailableReason(Id,Cmd,Cmd==EProcessCommand::PurgePressure?1000:Value,Now);if(S&&S->AutoRun.State.Active&&Cmd!=EProcessCommand::StopAll)Why=TEXT("자동 배치 실행 중 · 일반 웹 수동 조작 차단");A->SetStringField(TEXT("reason"),Why);Actions.Add(MakeShared<FJsonValueObject>(A));};
 Add(TEXT("plant"),TEXT("stop_all"),EProcessCommand::StopAll,0,TEXT("none"));Add(TEXT("plant"),TEXT("resume"),EProcessCommand::Resume,0,TEXT("none"));Add(TEXT("plant"),TEXT("pause"),EProcessCommand::Pause,0,TEXT("none"));
 for(int32 I=0;I<8;++I){const FString Id=I<6?FString::Printf(TEXT("supply-%c"),'a'+I):I==6?TEXT("weigher"):TEXT("receiver");for(double V:{0.,1.})Add(Id,TEXT("valve"),EProcessCommand::Valve,V,TEXT("bool"));Add(Id,TEXT("pressure"),EProcessCommand::Pressure,0,TEXT("kPa(g)"));if(I<6){Add(Id,TEXT("purge_pressure"),EProcessCommand::PurgePressure,0,TEXT("kPa(g)"));for(double V:{0.,1.})Add(Id,TEXT("purge_enabled"),EProcessCommand::PurgeEnabled,V,TEXT("bool"));}}
 Add(TEXT("mixer"),TEXT("rpm"),EProcessCommand::MixerRPM,S?S->State.Snapshot.RequestedRPM:0,TEXT("rpm"));for(double V:{0.,1.})Add(TEXT("mixer"),TEXT("mixer_enabled"),EProcessCommand::MixerEnabled,V,TEXT("bool"));
 for(const TCHAR* Kind:{TEXT("recipe_list"),TEXT("recipe_load"),TEXT("recipe_save"),TEXT("recipe_apply"),TEXT("recipe_charge"),TEXT("batch_start"),TEXT("batch_check"),TEXT("batch_resume"),TEXT("batch_abandon")}){auto A=Object();A->SetStringField(TEXT("equipment"),TEXT("plant"));A->SetStringField(TEXT("kind"),Kind);A->SetNumberField(TEXT("value"),0);A->SetStringField(TEXT("unit"),TEXT("none"));A->SetStringField(TEXT("reason"),!Enabled?TEXT("조회 전용 연결"):!S?TEXT("해석 연결 대기"):Batch.Reason(Kind,*S,Now));Actions.Add(MakeShared<FJsonValueObject>(A));}
 for(int I=0;I<8;++I){const FString Id=I<6?ProcessPlantBinding::SupplierId(I):I==6?TEXT("weigher"):TEXT("receiver");auto A=Object();A->SetStringField(TEXT("equipment"),Id);A->SetStringField(TEXT("kind"),TEXT("transfer_pressure"));A->SetNumberField(TEXT("value"),0);A->SetStringField(TEXT("unit"),TEXT("kPa(g)"));A->SetStringField(TEXT("reason"),!Enabled?TEXT("조회 전용 연결"):!S?TEXT("해석 연결 대기"):S->AutoRun.PressureUnavailable(S->State,Now,Id));Actions.Add(MakeShared<FJsonValueObject>(A));}
 if(S){
  const auto& A=S->AutoRun.State;const auto& P=S->State.Snapshot;
  const bool Same=A.Active&&A.Epoch==P.Epoch&&A.Connection==S->State.Connection;
  auto Speed=Object();
  Speed->SetStringField(TEXT("epoch"),LexToString(P.Epoch));Speed->SetStringField(TEXT("sequence"),LexToString(P.Sequence));
  Speed->SetStringField(TEXT("batch_id"),Same?A.RunId.ToString(EGuidFormats::DigitsWithHyphens):FString());
  Speed->SetStringField(TEXT("mix_run"),LexToString(Same?A.ConfirmedMixRun:int64(0)));
  if(Same)Speed->SetNumberField(TEXT("recipe_rpm"),A.TargetMixerRPM);
  if(P.Validity==EPlantValueValidity::Valid&&P.AppliedRPMValid&&Now>=P.PublishedWallSeconds&&Now-P.PublishedWallSeconds<=2)
   Speed->SetNumberField(TEXT("applied_rpm"),P.AppliedRPM);
  if(Same)if(const auto* R=S->State.History.Find(A.SpeedRequest)){
   Speed->SetStringField(TEXT("status"),Status(R->Status));Speed->SetStringField(TEXT("message"),R->Message);
   if(R->Revision>0&&R->Revision==P.SpeedAppliedRevision){
    Speed->SetNumberField(TEXT("applied_seconds"),P.SpeedAppliedSeconds);
    Speed->SetNumberField(TEXT("start_angle_rad"),P.SpeedStartAngleRadians);
   }
  }
  O->SetObjectField(TEXT("mix_rpm"),Speed);
  auto Action=Object();Action->SetStringField(TEXT("equipment"),TEXT("mixer"));Action->SetStringField(TEXT("kind"),TEXT("mix_rpm"));
  Action->SetNumberField(TEXT("value"),0);Action->SetStringField(TEXT("unit"),TEXT("rpm"));
  Action->SetStringField(TEXT("reason"),Enabled?S->AutoRun.MixSpeedReason(S->State,Now):FString(TEXT("조회 전용 연결")));
  Actions.Add(MakeShared<FJsonValueObject>(Action));
 }
 if(S)O->SetObjectField(TEXT("transfer_pressure"),ProcessWebTransferPressure::Describe(*S,Now));
 O->SetArrayField(TEXT("actions"),Actions);if(S)O->SetObjectField(TEXT("batch_workflow"),Batch.Describe(*S,Now));
 TArray<TSharedPtr<FJsonValue>> Records;
 for(auto& P:Results){if(S)Batch.Observe(*S,P.Value);auto& R=P.Value.Record;if(S)if(const auto* Latest=S->State.History.Find(R.RequestId))R=*Latest;auto Jv=Object();if(P.Value.Data)Jv->SetObjectField(TEXT("data"),P.Value.Data);if(!P.Value.BatchId.IsEmpty())Jv->SetStringField(TEXT("batch_id"),P.Value.BatchId);Jv->SetStringField(TEXT("request_id"),P.Key.ToString(EGuidFormats::DigitsWithHyphens));Jv->SetStringField(TEXT("service_request_id"),R.RequestId.ToString(EGuidFormats::DigitsWithHyphens));if(P.Value.Command.Kind==TEXT("transfer_pressure"))Jv->SetStringField(TEXT("transfer_id"),LexToString(P.Value.Command.TransferId));Jv->SetStringField(TEXT("status"),Status(R.Status));Jv->SetStringField(TEXT("message"),R.Message);Jv->SetStringField(TEXT("revision"),LexToString(R.Revision));Jv->SetStringField(TEXT("epoch"),LexToString(R.Epoch));Jv->SetStringField(TEXT("connection"),LexToString(R.Connection));Jv->SetStringField(TEXT("sequence"),S?LexToString(S->State.Snapshot.Sequence):TEXT("0"));Jv->SetNumberField(TEXT("simulation_seconds"),S?S->State.Snapshot.PhysicalSeconds:0);Records.Add(MakeShared<FJsonValueObject>(Jv));}O->SetArrayField(TEXT("results"),Records);return O;
}
