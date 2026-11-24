#include "ProcessRecipe.h"
#include "ProcessPlantBinding.h"
#include "ProcessControlTypes.h"
#include "ProcessStagePressure.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#else
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace {
using Obj=TSharedPtr<FJsonObject>;
FString ModeId(EProcessBatchMode M){return M==EProcessBatchMode::Full?TEXT("a_full"):M==EProcessBatchMode::TargetMass?TEXT("a_target"):M==EProcessBatchMode::SequentialAB?TEXT("sequential_ab"):TEXT("unsupported");}
bool TextOK(const FString& S,int32 Max){if(S.IsEmpty()||S.Len()>Max||S!=S.TrimStartAndEnd())return false;for(TCHAR C:S)if(C<32||C==127)return false;return true;}
bool Keys(const Obj& O,std::initializer_list<const TCHAR*> Expected){if(!O||O->Values.Num()!=int32(Expected.size()))return false;for(auto K:Expected)if(!O->HasField(K))return false;return true;}
bool Str(const Obj& O,const TCHAR* K,FString& Out){const auto* V=O?O->Values.Find(K):nullptr;return V&&(*V)->Type==EJson::String&&(*V)->TryGetString(Out);}
bool Num(const Obj& O,const TCHAR* K,double& Out){const auto* V=O?O->Values.Find(K):nullptr;return V&&(*V)->Type==EJson::Number&&(*V)->TryGetNumber(Out)&&FMath::IsFinite(Out);}
bool Bool(const Obj& O,const TCHAR* K,bool& Out){const auto* V=O?O->Values.Find(K):nullptr;return V&&(*V)->Type==EJson::Boolean&&(*V)->TryGetBool(Out);}
Obj Sub(const Obj& O,const TCHAR* K){const auto* V=O?O->Values.Find(K):nullptr;return V&&(*V)->Type==EJson::Object?(*V)->AsObject():nullptr;}
TArray<FString> Order(const FProcessRecipe& R){TArray<FString> A{TEXT("supply-a")};if(R.Mode==EProcessBatchMode::SequentialAB)A.Add(TEXT("supply-b"));if(R.AdditiveEnabled)A.Add(TEXT("supply-c"));return A;}
FString KeyFor(const FString& Name){FTCHARToUTF8 Bytes(*Name.ToLower());FMD5 Hash;Hash.Update(reinterpret_cast<const uint8*>(Bytes.Get()),Bytes.Length());uint8 Digest[16];Hash.Final(Digest);return BytesToHex(Digest,16).ToLower()+TEXT(".json");}
}

FString ProcessRecipe::ValidateEntries(const TArray<FProcessFeedEntry>& Entries,bool WithAdditive)
{
  int32 Used=0,Regular=0,Additives=0;
  for(const auto& E:Entries){
   if(E.Supplier<0||E.Supplier>=6||(Used&(1<<E.Supplier)))return TEXT("공급호퍼 ID가 중복되었거나 지원 범위를 벗어납니다.");
   Used|=1<<E.Supplier;
   if(!TextOK(E.DisplayName,80)||!FMath::IsFinite(E.TargetKg)||!FMath::IsFinite(E.ToleranceKg)||E.TargetKg<0||E.ToleranceKg<0)return TEXT("원료 표시명·목표량·오차를 확인하세요.");
   if(!E.Enabled)continue;
   if(E.TargetKg<=0||E.ToleranceKg>=E.TargetKg)return TEXT("사용 원료의 목표량은 양수, 오차는 목표량 미만이어야 합니다.");
   if(E.Additive)++Additives;
   else {if(Additives)return TEXT("첨가제는 일반 원료 뒤에 배치하세요.");++Regular;}
  }
  if(!Regular||Additives>1)return TEXT("일반 원료가 한 개 이상 필요하며 첨가제는 한 종류만 지원합니다.");
  return WithAdditive==(Additives==1)?FString():FString(TEXT("첨가제 역할과 사용 설정이 일치하지 않습니다."));
}

FString ProcessRecipe::Validate(const FProcessRecipe& R)
{
 const auto PressureWhy=ProcessStagePressure::Validate(R.FeedPressurePa,R.TransferPressurePa,R.SupplierFeedPa);if(!PressureWhy.IsEmpty())return PressureWhy;
 if(!TextOK(R.Name,80))return TEXT("레시피 이름은 공백으로 시작/끝나지 않는 1~80자여야 합니다. 제어 문자는 사용할 수 없습니다.");
 if(R.Mode!=EProcessBatchMode::HopperSequence&&R.Mode!=EProcessBatchMode::Full&&R.Mode!=EProcessBatchMode::TargetMass&&R.Mode!=EProcessBatchMode::SequentialAB)return TEXT("지원하지 않는 배치 모드입니다.");
 if(R.Mode!=EProcessBatchMode::HopperSequence&&(!TextOK(R.MaterialA,80)||(R.Mode==EProcessBatchMode::SequentialAB&&!TextOK(R.MaterialB,80))||(R.AdditiveEnabled&&!TextOK(R.MaterialC,80))))return TEXT("원료 표시명은 제어 문자 없는 1~80자여야 합니다.");
 for(double V:{R.TargetAKg,R.ToleranceAKg,R.TargetBKg,R.ToleranceBKg,R.TargetCKg,R.ToleranceCKg,R.DelaySeconds,R.TargetRPM,R.MixingSeconds})
  if(!FMath::IsFinite(V)||V<0)return TEXT("질량·오차·시간·RPM은 유한한 0 이상 수치여야 합니다.");
 if(R.TargetRPM<=0||R.TargetRPM>PlantControl::MaxRPM)return TEXT("혼합 목표 RPM은 0 초과 30 이하여야 합니다.");
 if(R.MixingSeconds<=0)return TEXT("혼합 시간은 양수 해석초여야 합니다.");
 if(R.Mode==EProcessBatchMode::HopperSequence){
  const auto EntryWhy=ValidateEntries(R.Entries,R.AdditiveEnabled);if(!EntryWhy.IsEmpty())return EntryWhy;
  if((!R.AdditiveEnabled&&R.Scheduled)||(!R.Scheduled&&R.DelaySeconds!=0)||(R.Scheduled&&R.DelaySeconds>=R.MixingSeconds))return TEXT("첨가제 사용 여부와 혼합 종료 전 예약 시각을 확인하세요.");
  return FString();
 }
 if(!R.Entries.IsEmpty())return TEXT("호퍼별 투입 목록은 선택 순차 모드에서만 적용됩니다.");
 auto Dose=[](double T,double E){return T>0&&E<T;};
 if(R.Mode!=EProcessBatchMode::Full&&!Dose(R.TargetAKg,R.ToleranceAKg))return TEXT("A 목표량은 양수, 계량 오차는 목표량 미만이어야 합니다.");
 if(R.Mode==EProcessBatchMode::SequentialAB&&!Dose(R.TargetBKg,R.ToleranceBKg))return TEXT("B 목표량은 양수, 계량 오차는 목표량 미만이어야 합니다.");
 if(R.Mode==EProcessBatchMode::Full&&(R.TargetAKg!=0||R.ToleranceAKg!=0))return TEXT("A 전량 모드에는 목표 계량값을 저장할 수 없습니다.");
 if(R.Mode!=EProcessBatchMode::SequentialAB&&(R.AdditiveEnabled||R.TargetBKg!=0||R.ToleranceBKg!=0))return TEXT("B와 C는 A→B 순차 모드에서만 지원합니다.");
 if(R.AdditiveEnabled){
  if(!Dose(R.TargetCKg,R.ToleranceCKg))return TEXT("C 목표량은 양수, 계량 오차는 목표량 미만이어야 합니다.");
  if(R.Scheduled&&R.DelaySeconds>=R.MixingSeconds)return TEXT("예약 N은 혼합 종료보다 앞서야 합니다. ON·0초는 지원합니다.");
 }else if(R.TargetCKg!=0||R.ToleranceCKg!=0||R.Scheduled||R.DelaySeconds!=0)return TEXT("첨가제 미사용 레시피에 첨가제 투입값이 있습니다.");
 if(!R.Scheduled&&R.DelaySeconds!=0)return TEXT("예약 OFF의 N은 0이어야 합니다. 초기 투입은 ON·0초와 구분됩니다.");
 return FString();
}

FString ProcessRecipe::Encode(const FProcessRecipe& R)
{
 Obj O=MakeShared<FJsonObject>();O->SetStringField(TEXT("schema"),TEXT("virtual-game-team.batch-recipe"));O->SetNumberField(TEXT("version"),!R.SupplierFeedPa.IsEmpty()?4:R.Mode==EProcessBatchMode::HopperSequence?3:Version);O->SetStringField(TEXT("name"),R.Name);O->SetStringField(TEXT("mode"),R.Mode==EProcessBatchMode::HopperSequence?TEXT("hopper_sequence"):ModeId(R.Mode));
 Obj U=MakeShared<FJsonObject>();U->SetStringField(TEXT("mass"),TEXT("kg"));U->SetStringField(TEXT("time"),TEXT("simulation_s"));U->SetStringField(TEXT("speed"),TEXT("rpm"));U->SetStringField(TEXT("pressure"),TEXT("Pa_g"));O->SetObjectField(TEXT("units"),U);
 TArray<TSharedPtr<FJsonValue>> Steps,Materials;auto Ids=Order(R);
 const FString Labels[]={R.MaterialA,R.MaterialB,R.MaterialC};const double Targets[]={R.TargetAKg,R.TargetBKg,R.TargetCKg},Tolerances[]={R.ToleranceAKg,R.ToleranceBKg,R.ToleranceCKg};
 FString AdditiveId=TEXT("supply-c");
 if(R.Mode==EProcessBatchMode::HopperSequence){
  AdditiveId.Empty();
  for(const auto& E:R.Entries){
   const FString Id=ProcessPlantBinding::SupplierId(E.Supplier);
   if(E.Enabled){Steps.Add(MakeShared<FJsonValueString>(Id));if(E.Additive)AdditiveId=Id;}
   Obj M=MakeShared<FJsonObject>();M->SetStringField(TEXT("hopper_id"),Id);M->SetStringField(TEXT("display_name"),E.DisplayName);
   M->SetBoolField(TEXT("enabled"),E.Enabled);M->SetStringField(TEXT("role"),E.Additive?TEXT("additive"):TEXT("material"));
   M->SetNumberField(TEXT("target_kg"),E.TargetKg);M->SetNumberField(TEXT("tolerance_kg"),E.ToleranceKg);Materials.Add(MakeShared<FJsonValueObject>(M));
  }
 }else for(int I=0;I<Ids.Num();++I){Steps.Add(MakeShared<FJsonValueString>(Ids[I]));Obj M=MakeShared<FJsonObject>();M->SetStringField(TEXT("hopper_id"),Ids[I]);M->SetStringField(TEXT("display_name"),Labels[I]);M->SetNumberField(TEXT("target_kg"),Targets[I]);M->SetNumberField(TEXT("tolerance_kg"),Tolerances[I]);Materials.Add(MakeShared<FJsonValueObject>(M));}
 O->SetArrayField(TEXT("feed_order"),Steps);O->SetArrayField(TEXT("materials"),Materials);
 Obj Mix=MakeShared<FJsonObject>();Mix->SetNumberField(TEXT("target_rpm"),R.TargetRPM);Mix->SetNumberField(TEXT("duration_s"),R.MixingSeconds);O->SetObjectField(TEXT("mixing"),Mix);
 Obj C=MakeShared<FJsonObject>();C->SetBoolField(TEXT("enabled"),R.AdditiveEnabled);C->SetStringField(TEXT("hopper_id"),AdditiveId);C->SetBoolField(TEXT("scheduled"),R.Scheduled);C->SetNumberField(TEXT("delay_s"),R.DelaySeconds);O->SetObjectField(TEXT("additive"),C);
 Obj Press=MakeShared<FJsonObject>();
 auto Stage=[](const FVector& V){Obj P=MakeShared<FJsonObject>();P->SetNumberField(TEXT("supply-common"),V.X);P->SetNumberField(TEXT("weigher"),V.Y);P->SetNumberField(TEXT("receiver"),V.Z);return P;};
 if(!R.SupplierFeedPa.IsEmpty()){
  Obj Sources=MakeShared<FJsonObject>();
  for(int32 I=0;I<R.SupplierFeedPa.Num();++I)Sources->SetNumberField(ProcessPlantBinding::SupplierId(I),R.SupplierFeedPa[I]);
  Press->SetObjectField(TEXT("supplier_feed_pa"),Sources);
 }
 Press->SetObjectField(TEXT("supply_to_weighing"),Stage(R.FeedPressurePa));Press->SetObjectField(TEXT("weighing_to_receiver"),Stage(R.TransferPressurePa));O->SetObjectField(TEXT("hopper_pressure_targets"),Press);
 FString Text;FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));return Text;
}

bool ProcessRecipe::Decode(const FString& Text,FProcessRecipe& Out,FString& Error)
{
 auto Fail=[&](const TCHAR* Why){Error=Why;return false;};Obj O;
 if(Text.Len()>65536||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O)||!O)return Fail(TEXT("레시피 JSON이 손상됐거나 크기 제한(64 KiB)을 초과했습니다."));
 FString Schema,Mode;double V=0;FProcessRecipe R;
 if(!Str(O,TEXT("schema"),Schema)||Schema!=TEXT("virtual-game-team.batch-recipe")||!Num(O,TEXT("version"),V)||(V!=1&&V!=Version&&V!=3&&V!=4))return Fail(TEXT("지원하지 않는 레시피 형식 또는 버전입니다."));
 const bool Modern=V>=2;
 const bool RootKeys=Modern?Keys(O,{TEXT("schema"),TEXT("version"),TEXT("name"),TEXT("mode"),TEXT("units"),TEXT("feed_order"),TEXT("materials"),TEXT("mixing"),TEXT("additive"),TEXT("hopper_pressure_targets")}):Keys(O,{TEXT("schema"),TEXT("version"),TEXT("name"),TEXT("mode"),TEXT("units"),TEXT("feed_order"),TEXT("materials"),TEXT("mixing"),TEXT("additive")});
 if(!RootKeys)return Fail(TEXT("레시피 필드가 누락됐거나 지원하지 않는 필드가 있습니다. 실행 상태/명령은 레시피가 아닙니다."));
 if(!Str(O,TEXT("name"),R.Name)||!Str(O,TEXT("mode"),Mode))return Fail(TEXT("레시피 이름/모드 형식 오류"));
 if(V==3&&Mode!=TEXT("hopper_sequence"))return Fail(TEXT("버전 3 모드 오류"));
 if(V>=3&&Mode==TEXT("hopper_sequence"))R.Mode=EProcessBatchMode::HopperSequence;else if(Mode==TEXT("a_full"))R.Mode=EProcessBatchMode::Full;else if(Mode==TEXT("a_target"))R.Mode=EProcessBatchMode::TargetMass;else if(Mode==TEXT("sequential_ab"))R.Mode=EProcessBatchMode::SequentialAB;else return Fail(TEXT("미지원 배치 모드입니다."));
 Obj U=Sub(O,TEXT("units")),Mix=Sub(O,TEXT("mixing")),C=Sub(O,TEXT("additive"));FString Mass,Time,Speed,Hopper,PressureUnit;
 const bool UnitKeys=Modern?Keys(U,{TEXT("mass"),TEXT("time"),TEXT("speed"),TEXT("pressure")}):Keys(U,{TEXT("mass"),TEXT("time"),TEXT("speed")});
 if(!UnitKeys||!Str(U,TEXT("mass"),Mass)||Mass!=TEXT("kg")||!Str(U,TEXT("time"),Time)||Time!=TEXT("simulation_s")||!Str(U,TEXT("speed"),Speed)||Speed!=TEXT("rpm"))return Fail(TEXT("레시피 단위는 kg / simulation_s / rpm이어야 합니다."));
 if(Modern){
  if(!Str(U,TEXT("pressure"),PressureUnit)||PressureUnit!=TEXT("Pa_g"))return Fail(TEXT("호퍼 목표 압력 저장 단위는 게이지압 Pa_g여야 합니다."));
  Obj P=Sub(O,TEXT("hopper_pressure_targets"));
  auto ReadStage=[&](const TCHAR* Id,FVector& OutPressure){Obj T=Sub(P,Id);return Keys(T,{TEXT("supply-common"),TEXT("weigher"),TEXT("receiver")})&&Num(T,TEXT("supply-common"),OutPressure.X)&&Num(T,TEXT("weigher"),OutPressure.Y)&&Num(T,TEXT("receiver"),OutPressure.Z);};
  bool Fields=Keys(P,{TEXT("supply_to_weighing"),TEXT("weighing_to_receiver")});
  if(V==4){
   Fields=Keys(P,{TEXT("supply_to_weighing"),TEXT("weighing_to_receiver"),TEXT("supplier_feed_pa")});
   const Obj Sources=Sub(P,TEXT("supplier_feed_pa"));
   if(!Sources||Sources->Values.Num()!=6)return Fail(TEXT("공급원별 압력 목록 오류"));
   for(int32 I=0;I<6;++I){double Pa;if(!Num(Sources,ProcessPlantBinding::SupplierId(I),Pa))return Fail(TEXT("공급원별 압력 ID 또는 수치 오류"));R.SupplierFeedPa.Add(Pa);}
  }
  if(!Fields||!ReadStage(TEXT("supply_to_weighing"),R.FeedPressurePa)||!ReadStage(TEXT("weighing_to_receiver"),R.TransferPressurePa))return Fail(TEXT("단계별 호퍼 목표 압력 형식/ID 오류"));
 } // v1 has no pressure fields: the struct's original verified stage defaults remain intact.
 if(!Keys(Mix,{TEXT("target_rpm"),TEXT("duration_s")})||!Num(Mix,TEXT("target_rpm"),R.TargetRPM)||!Num(Mix,TEXT("duration_s"),R.MixingSeconds))return Fail(TEXT("혼합 RPM/시간 수치 형식 오류"));
 if(!Keys(C,{TEXT("enabled"),TEXT("hopper_id"),TEXT("scheduled"),TEXT("delay_s")})||!Bool(C,TEXT("enabled"),R.AdditiveEnabled)||!Bool(C,TEXT("scheduled"),R.Scheduled)||!Num(C,TEXT("delay_s"),R.DelaySeconds)||!Str(C,TEXT("hopper_id"),Hopper)||(R.Mode!=EProcessBatchMode::HopperSequence&&Hopper!=TEXT("supply-c")))return Fail(TEXT("첨가제는 안정 ID supply-c만 지원하며 예약 여부는 boolean이어야 합니다."));
 const TArray<TSharedPtr<FJsonValue>> *Steps=nullptr,*Materials=nullptr;auto Expected=Order(R);
 if(R.Mode==EProcessBatchMode::HopperSequence){
  if(!O->TryGetArrayField(TEXT("feed_order"),Steps)||!O->TryGetArrayField(TEXT("materials"),Materials)||Materials->Num()>6)return Fail(TEXT("호퍼 목록 오류"));
  int32 Step=0;FString AdditiveId;
  for(const auto& Item:*Materials){
   Obj M=Item->Type==EJson::Object?Item->AsObject():nullptr;FProcessFeedEntry E;FString Id,Role;
   if(!Keys(M,{TEXT("enabled"),TEXT("role"),TEXT("hopper_id"),TEXT("display_name"),TEXT("target_kg"),TEXT("tolerance_kg")})||
    !Str(M,TEXT("hopper_id"),Id)||!Str(M,TEXT("role"),Role)||(Role!=TEXT("material")&&Role!=TEXT("additive"))||
    !Bool(M,TEXT("enabled"),E.Enabled)||!Str(M,TEXT("display_name"),E.DisplayName)||!Num(M,TEXT("target_kg"),E.TargetKg)||!Num(M,TEXT("tolerance_kg"),E.ToleranceKg))return Fail(TEXT("투입 항목 형식 오류"));
   E.Supplier=ProcessPlantBinding::SupplierIndex(Id);E.Additive=Role==TEXT("additive");
   if(E.Enabled){
    FString Ordered;if(!Steps->IsValidIndex(Step)||!(*Steps)[Step++]->TryGetString(Ordered)||Ordered!=Id)return Fail(TEXT("투입 순서 불일치"));
    if(E.Additive)AdditiveId=Id;
   }
   R.Entries.Add(E);
  }
  if(Step!=Steps->Num()||AdditiveId!=Hopper)return Fail(TEXT("첨가제 또는 사용 순서 불일치"));
  Error=Validate(R);if(!Error.IsEmpty())return false;Out=R;return true;
 }
 if(!O->TryGetArrayField(TEXT("feed_order"),Steps)||!O->TryGetArrayField(TEXT("materials"),Materials)||Steps->Num()!=Expected.Num()||Materials->Num()!=Expected.Num())return Fail(TEXT("원료 목록/투입 순서가 지원 모드와 맞지 않습니다."));
 FString* Labels[]={&R.MaterialA,&R.MaterialB,&R.MaterialC};double* Targets[]={&R.TargetAKg,&R.TargetBKg,&R.TargetCKg};double* Tolerances[]={&R.ToleranceAKg,&R.ToleranceBKg,&R.ToleranceCKg};
 for(int I=0;I<Expected.Num();++I){FString Id,Step;const auto& MV=(*Materials)[I];Obj M=MV->Type==EJson::Object?MV->AsObject():nullptr;
  if((*Steps)[I]->Type!=EJson::String||!(*Steps)[I]->TryGetString(Step)||Step!=Expected[I]||!Keys(M,{TEXT("hopper_id"),TEXT("display_name"),TEXT("target_kg"),TEXT("tolerance_kg")})||!Str(M,TEXT("hopper_id"),Id)||Id!=Expected[I])return Fail(TEXT("투입 순서/호퍼 ID 오류: A→B와 그 이후 C만 지원합니다. 표시명으로 호퍼를 선택하지 않습니다."));
  if(!Str(M,TEXT("display_name"),*Labels[I])||!Num(M,TEXT("target_kg"),*Targets[I])||!Num(M,TEXT("tolerance_kg"),*Tolerances[I]))return Fail(TEXT("원료 표시명/목표량/계량 오차 형식 오류"));
 }
 Error=Validate(R);if(!Error.IsEmpty())return false;Out=R;return true;
}
bool ProcessRecipe::Same(const FProcessRecipe& A,const FProcessRecipe& B){return Encode(A)==Encode(B);}
FString ProcessRecipe::Summary(const FProcessRecipe& R)
{
 FString PressureText=TEXT("\n공급 기준 ");
 if(R.SupplierFeedPa.IsEmpty())PressureText+=FString::Printf(TEXT("A~F 공통 %g"),R.FeedPressurePa.X/1000.);
 else for(int I=0;I<R.SupplierFeedPa.Num();++I)PressureText+=FString::Printf(TEXT(" %c %g"),'A'+I,R.SupplierFeedPa[I]/1000.);
 PressureText+=FString::Printf(TEXT(" kPa(g)\n공급 중 칭량 %g / 수취 %g · 수취 이송 중 칭량 %g / 수취 %g kPa(g)"),R.FeedPressurePa.Y/1000.,R.FeedPressurePa.Z/1000.,R.TransferPressurePa.Y/1000.,R.TransferPressurePa.Z/1000.);
 if(R.Mode==EProcessBatchMode::HopperSequence){
  FString Result=R.Name;
  for(const auto& E:R.Entries)if(E.Enabled)Result+=FString::Printf(TEXT("\n%s %s · %s · %g ± %g g"),ProcessPlantBinding::SupplierId(E.Supplier),*E.DisplayName,E.Additive?TEXT("첨가제"):TEXT("원료"),E.TargetKg*1000,E.ToleranceKg*1000);
  return Result+PressureText+FString::Printf(TEXT("\n%g RPM · %g 해석초"),R.TargetRPM,R.MixingSeconds);
 }
 FString S=R.Name+TEXT(" · ")+(R.Mode==EProcessBatchMode::Full?TEXT("A 전량"):R.Mode==EProcessBatchMode::TargetMass?TEXT("A 목표량"):TEXT("A→B 순차 계량 (순서 고정)"));
 S+=FString::Printf(TEXT("\nA [%s] %s"),*R.MaterialA,R.Mode==EProcessBatchMode::Full?TEXT("전량"):*FString::Printf(TEXT("%.6f g ± %.6f g"),R.TargetAKg*1000,R.ToleranceAKg*1000));
 if(R.Mode==EProcessBatchMode::SequentialAB)S+=FString::Printf(TEXT("\nB [%s] %.6f g ± %.6f g"),*R.MaterialB,R.TargetBKg*1000,R.ToleranceBKg*1000);
 if(R.AdditiveEnabled)S+=FString::Printf(TEXT("\nC [%s] %.6f g ± %.6f g · %s"),*R.MaterialC,R.TargetCKg*1000,R.ToleranceCKg*1000,R.Scheduled?*FString::Printf(TEXT("예약 ON · N=%.6f 해석초"),R.DelaySeconds):TEXT("예약 OFF · 초기 투입"));
 S+=PressureText;
 return S+FString::Printf(TEXT("\n목표 %.6g RPM · 혼합 %.6g 해석초"),R.TargetRPM,R.MixingSeconds);
}

FProcessRecipeStore::FProcessRecipeStore(FString Directory):Root(Directory.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("BatchRecipes"):MoveTemp(Directory)){}
bool FProcessRecipeStore::SafeFile(const FString& F) const{return !F.IsEmpty()&&F.Len()<=120&&F.EndsWith(TEXT(".json"))&&F==FPaths::GetCleanFilename(F)&&!F.Contains(TEXT(".."))&&!F.Contains(TEXT(":"))&&!F.Contains(TEXT("/"))&&!F.Contains(TEXT("\\"));}
bool FProcessRecipeStore::Load(const FString& File,FProcessRecipe& Out,FString& Error) const
{
 if(!SafeFile(File)){Error=TEXT("레시피 저장 폴더 밖의 경로는 사용할 수 없습니다.");return false;}
 const FString Path=Root/File;const int64 Size=IFileManager::Get().FileSize(*Path);FString Text;
 if(Size<0||Size>65536||!FFileHelper::LoadFileToString(Text,*Path)){Error=TEXT("레시피 파일 읽기 실패 또는 크기 초과. 기존 편집값/적용값을 유지합니다.");return false;}
 return ProcessRecipe::Decode(Text,Out,Error);
}
TArray<FProcessRecipeFile> FProcessRecipeStore::List() const
{
 TArray<FString> Files;IFileManager::Get().FindFiles(Files,*(Root/TEXT("*.json")),true,false);TArray<FProcessRecipeFile> Out;
 for(const auto& File:Files){if(!SafeFile(File))continue;FProcessRecipe R;FString Why;const bool OK=Load(File,R,Why);Out.Add({File,OK?R.Name:File,Why,FileVersion(File)});}
 Out.Sort([](const auto& A,const auto& B){return A.Name<B.Name;});return Out;
}
bool FProcessRecipeStore::SaveNew(const FProcessRecipe& R,FString& SavedFile,FString& Error) const
{
 Error=ProcessRecipe::Validate(R);if(!Error.IsEmpty())return false;const FString File=KeyFor(R.Name);
 for(const auto& Existing:List())if(Existing.Name.Equals(R.Name,ESearchCase::IgnoreCase)){Error=TEXT("같은 이름이 이미 있습니다. 목록에서 선택하고 명시적으로 덮어쓰세요.");return false;}
 if(!Commit(File,R,false,Error))return false;SavedFile=File;return true;
}
bool FProcessRecipeStore::ReplaceSelected(const FString& File,const FProcessRecipe& R,FString& Error,const FString& ExpectedVersion) const
{
 FProcessRecipe Existing;if(!Load(File,Existing,Error))return false;
 if(!Existing.Name.Equals(R.Name,ESearchCase::IgnoreCase)){Error=TEXT("선택한 레시피와 편집 이름이 다릅니다. 다른 이름은 새로 저장하세요.");return false;}
 return Commit(File,R,true,Error,ExpectedVersion);
}
FString FProcessRecipeStore::FileVersion(const FString& File) const
{
 TArray<uint8> Bytes;if(!SafeFile(File)||IFileManager::Get().FileSize(*(Root/File))>65536||!FFileHelper::LoadFileToArray(Bytes,*(Root/File)))return FString();
 FMD5 Hash;Hash.Update(Bytes.GetData(),Bytes.Num());uint8 Digest[16];Hash.Final(Digest);return BytesToHex(Digest,16);
}
bool FProcessRecipeStore::Commit(const FString& File,const FProcessRecipe& R,bool Replace,FString& Error,const FString& ExpectedVersion) const
{
 Error=ProcessRecipe::Validate(R);if(!Error.IsEmpty()||!SafeFile(File))return false;
 auto& FM=IFileManager::Get();if(!FM.MakeDirectory(*Root,true)){Error=TEXT("레시피 저장 폴더 생성 실패. 이전 파일을 유지합니다.");return false;}
 // Shared by every editor/web writer. Never wait on a lock or remove another writer's lock.
 struct FLock {
  FString Path;bool Held=false;
  explicit FLock(FString P):Path(MoveTemp(P)){
#if PLATFORM_WINDOWS
   HANDLE H=::CreateFileW(*Path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);Held=H!=INVALID_HANDLE_VALUE;if(Held)::CloseHandle(H);
#else
   int F=::open(TCHAR_TO_UTF8(*Path),O_CREAT|O_EXCL|O_WRONLY,0600);Held=F>=0;if(Held)::close(F);
#endif
  }
  ~FLock(){if(Held)IFileManager::Get().Delete(*Path);}
 } Lock(FPaths::ConvertRelativePathToFull(Root/TEXT(".recipe-write.lock")));
 if(!Lock.Held){Error=TEXT("다른 저장 작업 진행 중 · 목록을 새로 확인하고 다시 저장하세요. 잠금 파일이 남았다면 다른 편집기 종료 후 확인하세요.");return false;}
 if(Replace&&!ExpectedVersion.IsEmpty()&&FileVersion(File)!=ExpectedVersion){Error=TEXT("레시피 동시 수정 충돌 · 기존 파일 유지. 다시 불러와 비교한 뒤 덮어쓰세요.");return false;}
 const FString Dest=FPaths::ConvertRelativePathToFull(Root/File),Temp=Dest+TEXT(".")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".tmp");
 if(!FFileHelper::SaveStringToFile(ProcessRecipe::Encode(R),*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)){FM.Delete(*Temp);Error=TEXT("레시피 임시 파일 쓰기 실패. 이전 파일을 유지합니다.");return false;}
 bool OK=false;
#if PLATFORM_WINDOWS
 OK=::MoveFileExW(*Temp,*Dest,MOVEFILE_WRITE_THROUGH|(Replace?MOVEFILE_REPLACE_EXISTING:0))!=0;
#else
 if(Replace)OK=::rename(TCHAR_TO_UTF8(*Temp),TCHAR_TO_UTF8(*Dest))==0;
 else {OK=::link(TCHAR_TO_UTF8(*Temp),TCHAR_TO_UTF8(*Dest))==0;if(OK)::unlink(TCHAR_TO_UTF8(*Temp));}
#endif
 if(!OK){FM.Delete(*Temp);Error=Replace?TEXT("레시피 교체 실패. 이전 저장 파일을 유지합니다."):TEXT("저장 실패 또는 같은 이름 파일이 이미 존재합니다. 덮어쓰기는 별도 선택이 필요합니다.");return false;}
 Error.Empty();return true;
}
