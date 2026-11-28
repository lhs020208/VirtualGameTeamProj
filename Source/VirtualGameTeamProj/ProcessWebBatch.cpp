#include "ProcessWebBatch.h"
#include "ProcessWebCommands.h"
#include "ProcessControlService.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
namespace {
using J=TSharedPtr<FJsonObject>;
J Obj(){return MakeShared<FJsonObject>();}
FString Hash(const FString& Text){FTCHARToUTF8 B(*Text);FMD5 M;M.Update(reinterpret_cast<const uint8*>(B.Get()),B.Length());uint8 D[16];M.Final(D);return BytesToHex(D,16);}
void Number(J O,const TCHAR* K,double V,bool Known=true){if(Known&&FMath::IsFinite(V))O->SetNumberField(K,V);else O->SetField(K,MakeShared<FJsonValueNull>());}
FString Stage(EProcessAutoStage S){return FProcessAutoRun::StageText(S);}
}
bool FProcessWebBatch::Handles(const FString& K){return K==TEXT("recipe_list")||K==TEXT("recipe_load")||K==TEXT("recipe_save")||K==TEXT("recipe_apply")||K==TEXT("recipe_charge")||K==TEXT("batch_start")||K==TEXT("batch_check")||K==TEXT("batch_resume")||K==TEXT("batch_abandon");}
bool FProcessWebBatch::ReadOnly(const FString& K){return K==TEXT("recipe_list")||K==TEXT("recipe_load");}
FString FProcessWebBatch::RecipeFingerprint(const UProcessControlService& S){return Hash(ProcessRecipe::Encode(S.GetAppliedRecipe()));}
FString FProcessWebBatch::RecoveryKey(const UProcessControlService& S) const
{
 const auto& A=S.AutoRun.State;const auto& M=S.State;const auto& P=M.Snapshot;
 return FString::Printf(TEXT("%s|%s|%s|%lld|%lld|%lld|%.17g|%lld"),*A.RunId.ToString(),*M.StopRequest.ToString(),*M.LastRequest.ToString(),M.Connection,P.Epoch,P.RequestedControlRevision,P.PhysicalSeconds,P.Recovery.AcceptedRevision);
}
bool FProcessWebBatch::Checked(const UProcessControlService& S) const{return !CheckToken.IsEmpty()&&CheckKey==RecoveryKey(S)&&S.AutoRun.CanRecover(S.State,FPlatformTime::Seconds()).IsEmpty();}
FString FProcessWebBatch::Reason(const FString& K,UProcessControlService& S,double Now) const
{
 const auto& M=S.State;const auto& A=S.AutoRun.State;
 if(!M.Connected||M.Snapshot.Validity!=EPlantValueValidity::Valid||Now<M.Snapshot.PublishedWallSeconds||Now-M.Snapshot.PublishedWallSeconds>2)return TEXT("최신 공정 데이터 대기 · 연결과 작업자 상태를 확인하세요.");
 if(K==TEXT("recipe_list"))return FString();
 if(K==TEXT("recipe_load")||K==TEXT("recipe_save")||K==TEXT("recipe_apply")){
  if(A.Active||M.AutomationActive||M.IsStopping())return TEXT("자동 배치·정지 확인 중 · 이번 실행 설정을 유지합니다.");return FString();
 }
 if(K==TEXT("recipe_charge"))return S.NewChargeReason();
 if(K==TEXT("batch_start")){
  if(A.RecoveryPending)return TEXT("복구할 배치가 있습니다. 상태 확인 후 남은 양 재개 또는 배치 포기를 선택하세요.");
  if(S.RecipeRevision<=0)return TEXT("레시피 미적용 · 레시피 설정에서 전체 적용하세요.");
  return S.AutoRun.CanStart(M,Now);
 }
 if(K==TEXT("batch_check")||K==TEXT("batch_resume")){
  const auto Why=S.AutoRun.CanRecover(M,Now);if(!Why.IsEmpty())return Why;
  if(K==TEXT("batch_resume")&&!Checked(S))return TEXT("배치 상태 확인 필요 · 최신 위치와 남은 양을 확인하세요.");return FString();
 }
 if(K==TEXT("batch_abandon"))return A.Active||!A.RecoveryPending||A.RecoveryAbandoned?TEXT("전체 정지 후 보존된 중단 배치를 선택하세요."):FString();
 return TEXT("미지원 배치 조작");
}
void FProcessWebBatch::Execute(const ProcessWebCommands::FCommand& C,UProcessControlService& S,ProcessWebCommands::FResult& R)
{
 check(IsInGameThread());auto& M=S.State;auto& A=S.AutoRun.State;auto& Rec=R.Record;
 Rec.Status=EProcessCommandStatus::Rejected;Rec.Connection=M.Connection;Rec.AcceptedAt=FPlatformTime::Seconds();
 if(!ReadOnly(C.Kind)&&(!S.WebOwnsControl()||C.Lease!=S.WebLeaseId)){Rec.Message=TEXT("웹 제어권 없음 · 명시적으로 다시 가져오세요.");return;}
 Rec.Message=Reason(C.Kind,S,FPlatformTime::Seconds());if(!Rec.Message.IsEmpty())return;
 if(C.Kind.StartsWith(TEXT("batch_"))&&C.Kind!=TEXT("batch_start")&&C.BatchId!=A.RunId.ToString(EGuidFormats::DigitsWithHyphens)){Rec.Message=TEXT("대상 배치 ID 변경 · 현재 배치를 다시 확인하세요.");return;}
 if((C.Kind==TEXT("recipe_apply")||C.Kind==TEXT("recipe_charge")||C.Kind==TEXT("batch_start"))&&(C.RecipeRevision!=LexToString(S.RecipeRevision)||C.RecipeFingerprint!=RecipeFingerprint(S))){Rec.Message=TEXT("적용 설정 버전 충돌 · 최신 적용값을 확인하세요.");return;}
 TGuardValue<bool> Guard(S.WebDispatch,true);R.Data=Obj();bool OK=false;
 if(C.Kind==TEXT("recipe_list")){
  TArray<TSharedPtr<FJsonValue>> List;const auto Entries=Store.List();
  for(const auto& F:Entries){if(List.Num()>=128)break;FString Id;for(const auto& Pair:Files)if(Pair.Value==F.File){Id=Pair.Key;break;}if(Id.IsEmpty()){Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);Files.Add(Id,F.File);}
   auto V=Obj();V->SetStringField(TEXT("file_id"),Id);V->SetStringField(TEXT("name"),F.Name);V->SetStringField(TEXT("error"),F.Error);V->SetStringField(TEXT("file_version"),F.Version);List.Add(MakeShared<FJsonValueObject>(V));}
  R.Data->SetArrayField(TEXT("files"),List);R.Data->SetBoolField(TEXT("truncated"),Entries.Num()>128);OK=true;
 }else if(C.Kind==TEXT("recipe_load")){
  const auto* File=Files.Find(C.FileId);FProcessRecipe Recipe;
  if(!File){Rec.Message=TEXT("서버가 발급한 레시피 ID가 아닙니다. 목록을 갱신하세요.");return;}
  const FString Before=Store.FileVersion(*File);OK=Store.Load(*File,Recipe,Rec.Message);
  if(OK&&(Before.IsEmpty()||Before!=Store.FileVersion(*File))){OK=false;Rec.Message=TEXT("불러오는 동안 파일 변경 · 다시 불러오세요.");}
  if(OK){R.Data->SetStringField(TEXT("recipe_json"),ProcessRecipe::Encode(Recipe));R.Data->SetStringField(TEXT("file_version"),Before);R.Data->SetStringField(TEXT("file_id"),C.FileId);}
 }else if(C.Kind==TEXT("recipe_apply")||C.Kind==TEXT("recipe_save")){
  FProcessRecipe Recipe;if(!ProcessRecipe::Decode(C.RecipeJson,Recipe,Rec.Message))return;
  if(C.Kind==TEXT("recipe_apply")){OK=S.ApplyRecipe(Recipe,Rec.Message);if(OK){R.Data->SetStringField(TEXT("recipe_revision"),LexToString(S.RecipeRevision));R.Data->SetStringField(TEXT("recipe_fingerprint"),RecipeFingerprint(S));}}
  else {FString File;
   if(C.FileId.IsEmpty())OK=Store.SaveNew(Recipe,File,Rec.Message);
   else{const auto* Found=Files.Find(C.FileId);if(!Found||!C.Confirm||C.FileVersion.IsEmpty()){Rec.Message=TEXT("덮어쓸 대상·파일 버전·명시적 확인 필요");return;}File=*Found;OK=Store.ReplaceSelected(File,Recipe,Rec.Message,C.FileVersion);}
   if(OK){FString Id=C.FileId;if(Id.IsEmpty()){Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);Files.Add(Id,File);}R.Data->SetStringField(TEXT("file_id"),Id);R.Data->SetStringField(TEXT("file_version"),Store.FileVersion(File));}
  }
 }else if(C.Kind==TEXT("recipe_charge")){
  OK=S.LoadNewCharge(Rec.Message);
 }else if(C.Kind==TEXT("batch_start")){
  OK=S.StartAutomaticA();if(OK){ExecutionBatch=A.RunId.ToString(EGuidFormats::DigitsWithHyphens);ExecutionRecipeRevision=C.RecipeRevision;R.Data->SetStringField(TEXT("recipe_revision"),ExecutionRecipeRevision);}if(!OK)Rec.Message=S.AutoRun.CanStart(M,FPlatformTime::Seconds());
 }else if(C.Kind==TEXT("batch_check")){
  S.CheckAutomaticBatch();Rec.Message=S.AutoRun.CanRecover(M,FPlatformTime::Seconds());OK=Rec.Message.IsEmpty();
  if(OK){CheckKey=RecoveryKey(S);CheckToken=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);R.Data->SetStringField(TEXT("check_token"),CheckToken);}
 }else if(C.Kind==TEXT("batch_resume")){
  if(C.CheckToken!=CheckToken||!Checked(S)){Rec.Message=TEXT("상태 확인 이후 조건 변경 · 다시 상태 확인하세요.");return;}
  R.RecoveryRevision=M.Snapshot.Recovery.AcceptedRevision;OK=S.ResumeAutomaticBatch();CheckToken.Empty();
  if(!OK)Rec.Message=A.RecoveryReason;
 }else if(C.Kind==TEXT("batch_abandon")){
  if(!C.Confirm){Rec.Message=TEXT("대상 배치 포기를 명시적으로 확인하세요.");return;}
  S.AbandonAutomaticBatch();CheckToken.Empty();OK=A.RecoveryAbandoned;
 }
 if(OK){Rec.Status=C.Kind==TEXT("batch_resume")?EProcessCommandStatus::Processing:EProcessCommandStatus::Applied;Rec.Message=C.Kind==TEXT("recipe_charge")?TEXT("새 장입 초기화 · 새 작업자 상태 확인 필요"):C.Kind==TEXT("batch_resume")?TEXT("남은 양 재개 접수 · 기존 작업자 재개 승인 확인 중"):C.Kind==TEXT("batch_start")?TEXT("자동 시작 접수 · 확정 실행값으로 배치 생성. 배치 완료는 진행 상태에서 별도 확인"):TEXT("처리 완료 · 설정/조회는 장비 명령을 발행하지 않습니다.");
  if(C.Kind.StartsWith(TEXT("batch_"))){R.BatchId=A.RunId.ToString(EGuidFormats::DigitsWithHyphens);R.Data->SetStringField(TEXT("batch_id"),R.BatchId);}
 }else if(Rec.Message.IsEmpty())Rec.Message=TEXT("처리 실패 · 기존 설정·파일을 유지합니다.");
}
void FProcessWebBatch::Observe(UProcessControlService& S,ProcessWebCommands::FResult& R)
{
 if(R.Command.Kind!=TEXT("batch_resume")||!R.Record.IsPending())return;
 const auto& A=S.AutoRun.State;const auto& P=S.State.Snapshot;
 const auto* Worker=S.State.History.Find(P.Recovery.RequestId);
 const bool Identity=P.Epoch==R.Record.Epoch&&S.State.Connection==R.Record.Connection&&A.RunId.ToString(EGuidFormats::DigitsWithHyphens)==R.BatchId;
 if(Identity&&P.Recovery.Valid&&P.Recovery.AcceptedRevision>R.RecoveryRevision&&Worker&&Worker->Command==EProcessCommand::RecoverBatch&&Worker->Status==EProcessCommandStatus::Applied&&Worker->Revision==P.Recovery.AcceptedRevision&&Worker->Epoch==R.Record.Epoch&&Worker->Connection==R.Record.Connection&&Worker->AcceptedAt>=R.Record.AcceptedAt){R.Record.Status=EProcessCommandStatus::Applied;R.Record.Revision=Worker->Revision;R.Record.Message=TEXT("이번 작업자 요청 ID·배치 재개 승인 확인 · 기존 입자 이력으로 남은 단계 진행");if(!R.Data)R.Data=Obj();R.Data->SetStringField(TEXT("worker_request_id"),Worker->RequestId.ToString(EGuidFormats::DigitsWithHyphens));}
 else if(!Identity||!A.Active){R.Record.Status=EProcessCommandStatus::Failed;R.Record.Message=TEXT("재개 취소·실패 · 현재 정지/배치 상태를 확인하세요.");}
}
TSharedPtr<FJsonObject> FProcessWebBatch::Describe(UProcessControlService& S,double Now)
{
 auto O=Obj();auto& A=S.AutoRun.State;const auto& P=S.State.Snapshot;const bool Same=A.RunId.IsValid()&&A.Epoch==P.Epoch&&A.Connection==S.State.Connection;
 O->SetStringField(TEXT("applied_json"),ProcessRecipe::Encode(S.GetAppliedRecipe()));O->SetStringField(TEXT("recipe_revision"),LexToString(S.RecipeRevision));O->SetStringField(TEXT("recipe_fingerprint"),RecipeFingerprint(S));O->SetStringField(TEXT("execution_json"),Same?A.LockedRecipeJson:FString());O->SetStringField(TEXT("execution_recipe_revision"),Same&&ExecutionBatch==A.RunId.ToString(EGuidFormats::DigitsWithHyphens)?ExecutionRecipeRevision:FString());O->SetStringField(TEXT("execution_recipe_fingerprint"),Same?Hash(A.LockedRecipeJson):FString());
 O->SetStringField(TEXT("batch_id"),Same?A.RunId.ToString(EGuidFormats::DigitsWithHyphens):FString());O->SetNumberField(TEXT("active_supplier"),A.ActiveSupplier);
 Number(O,TEXT("mixing_elapsed_s"),A.MixingElapsedSeconds,Same);Number(O,TEXT("mixing_remaining_s"),A.MixingRemainingSeconds,Same);Number(O,TEXT("mixing_duration_s"),A.MixingDurationSeconds,Same);
 auto T=Obj();T->SetStringField(TEXT("id"),LexToString(P.Transfer.Id));T->SetBoolField(TEXT("complete"),A.Active&&A.Stage==EProcessAutoStage::Transferring&&A.Epoch==P.Epoch&&P.Transfer.Id==A.TransferId&&P.Transfer.State==EPlantTransferState::Complete&&P.Transfer.Validity==EPlantValueValidity::Valid);Number(T,TEXT("baseline_kg"),P.Transfer.BaselineKg,P.Transfer.Validity==EPlantValueValidity::Valid);Number(T,TEXT("remaining_kg"),P.Transfer.RemainingKg,P.Transfer.Validity==EPlantValueValidity::Valid);Number(T,TEXT("transit_kg"),P.Transfer.InTransitKg,P.Transfer.Validity==EPlantValueValidity::Valid);Number(T,TEXT("arrived_kg"),P.Transfer.ArrivedKg,P.Transfer.Validity==EPlantValueValidity::Valid);O->SetObjectField(TEXT("transfer"),T);
 O->SetStringField(TEXT("additive_status"),A.AdditiveStatus);Number(O,TEXT("additive_due_s"),A.AdditiveDueSeconds,A.AdditiveDueSeconds>=0);
 auto R=Obj();const bool Ready=Checked(S);const auto Why=S.AutoRun.CanRecover(S.State,Now);
 R->SetBoolField(TEXT("pending"),Same&&A.RecoveryPending);R->SetBoolField(TEXT("abandoned"),Same&&A.RecoveryAbandoned);R->SetBoolField(TEXT("checked"),Ready);R->SetStringField(TEXT("check_token"),Ready?CheckToken:FString());
 R->SetStringField(TEXT("reason"),Ready?TEXT("현재 배치 확인 완료 · 남은 양 이어서 투입을 명시적으로 선택하세요."):!Why.IsEmpty()?Why:TEXT("수동 조치 완료 후 배치 상태 확인을 선택하세요."));
 R->SetStringField(TEXT("next_stage"),Stage(A.RecoveryNext));R->SetStringField(TEXT("stop_reason"),S.State.StopDescription());
 TArray<TSharedPtr<FJsonValue>> Materials;TArray<FString> Lines;
 const bool Proven=Ready&&P.Recovery.Valid&&P.ModelParticleMassKg>0;

 for(int I=0;I<6;++I){if(!A.UsesSupplier(I))continue;const auto* Entry=A.EntryFor(I);auto V=Obj();const FString Id=FString::Printf(TEXT("supply-%c"),'a'+I);V->SetStringField(TEXT("id"),Id);V->SetStringField(TEXT("label"),Entry?Entry->DisplayName:I==0?A.RecipeMaterialA:I==1?A.RecipeMaterialB:A.RecipeMaterialC);
  const auto* M=P.Recovery.Materials.FindByPredicate([I](const auto& X){return X.Supplier==I;});const bool Known=Proven&&M;double Passed=0,Transit=0,Remaining=0,Supply=0;
  if(Known){Passed=(M->SupplyConnector+M->Weighing+M->Pipe+M->Receiver+M->MixerConnector+M->Mixer)*P.SourceKg(M->Supplier);Transit=(M->SupplyConnector+M->Pipe+M->MixerConnector)*P.SourceKg(M->Supplier);
   const double Target=A.DoseFor(I,false);
   const bool Final=!M->SelectedIds.IsEmpty();Supply=Final?0:A.BatchMode==EProcessBatchMode::Full?M->Source*P.SourceKg(M->Supplier):FMath::Max(0.,Target-Passed);
   Remaining=Final?FMath::Max(0,M->SelectedIds.Num()-M->Mixer)*P.SourceKg(M->Supplier):Passed-M->Mixer*P.SourceKg(M->Supplier);
   if(Supply>0)Lines.Add(FString::Printf(TEXT("%c 공급호퍼 → 칭량호퍼 추가 공급 %.6g g"),'A'+I,Supply*1000));
   if(Remaining>0)Lines.Add(FString::Printf(TEXT("%c 칭량·배관·수취 경로 → 혼합기 남은 이송 %.6g g (연결부/배관 %.6g g 포함)"),'A'+I,Remaining*1000,Transit*1000));
  }
  Number(V,TEXT("already_passed_kg"),Passed,Known);Number(V,TEXT("transit_kg"),Transit,Known);Number(V,TEXT("additional_supply_kg"),Supply,Known);Number(V,TEXT("remaining_transfer_kg"),Remaining,Known);Number(V,TEXT("receiver_kg"),Known?M->Receiver*P.SourceKg(M->Supplier):0,Known);Number(V,TEXT("mixer_kg"),Known?M->Mixer*P.SourceKg(M->Supplier):0,Known);Materials.Add(MakeShared<FJsonValueObject>(V));
 }
 if(Ready&&A.MixerAppliedSimulationSeconds>=0&&A.MixingElapsedSeconds<A.MixingDurationSeconds)Lines.Add(FString::Printf(TEXT("%g RPM으로 남은 혼합 %.2g초 재개"),A.TargetMixerRPM,FMath::Max(0.,A.MixingDurationSeconds-A.MixingElapsedSeconds)));
 R->SetArrayField(TEXT("materials"),Materials);R->SetStringField(TEXT("movement_summary"),Ready?(Lines.IsEmpty()?TEXT("추가 투입 없음 · 확인된 남은 혼합/종료 단계만 진행"):FString::Join(Lines,TEXT(" / "))):TEXT("남은 양 확인 중 · 전체 정지 확인 후 배치 상태 확인을 선택하세요."));O->SetObjectField(TEXT("recovery"),R);return O;
}
