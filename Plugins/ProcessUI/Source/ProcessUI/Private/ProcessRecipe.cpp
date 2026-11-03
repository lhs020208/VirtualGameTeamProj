#include "ProcessContract.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
namespace ProcessUI
{
FString Display(const FReading& R,double Scale,const FString& Unit)
{
    if(R.Valid()) return FString::Printf(TEXT("%.4g %s"),R.Value*Scale,Unit.IsEmpty()?*R.Unit:*Unit);
    switch(R.Validity) {case EValidity::Unsupported:return TEXT("미지원");case EValidity::Stale:return TEXT("오래된 데이터");case EValidity::Error:return TEXT("데이터 오류");default:return TEXT("데이터 없음");}
}
FRecipe DemoRecipe()
{
    FRecipe R; R.Name=TEXT("테스트 예제 — 공정 사양 아님"); R.RPM=20; R.MixSeconds=30;
    R.Hoppers={{TEXT("supply-1"),TEXT("가상 A"),false,0.0001,0.025},{TEXT("supply-2"),TEXT("가상 B"),false,0.0001,0.025},{TEXT("supply-3"),TEXT("가상 첨가제"),true,0.0001,0.025}};
    FStep A; A.Name=TEXT("A 개별 칭량·수취 이송"); A.HopperId=R.Hoppers[0].Id; A.TargetKg=0.05; R.Steps.Add(A);
    FStep B; B.Name=TEXT("B 개별 칭량·수취 이송"); B.HopperId=R.Hoppers[1].Id; B.TargetKg=0.05; R.Steps.Add(B);
    FStep C; C.Name=TEXT("초기 믹서 투입"); C.Kind=EStepKind::MixerCharge; R.Steps.Add(C);
    FStep D; D.Name=TEXT("첨가제 개별 칭량·수취 이송"); D.HopperId=R.Hoppers[2].Id; D.TargetKg=0.025; R.Steps.Add(D);
    FStep E; E.Name=TEXT("혼합 12초 믹서 투입"); E.Kind=EStepKind::MixerCharge; E.Timing=ETiming::AfterMixing; E.AtSeconds=12; R.Steps.Add(E);
    return R;
}
static bool SafeName(const FString& S)
{
    if(S.TrimStartAndEnd().IsEmpty()||S.Len()>100) return false;
    for(TCHAR C:S) if(C<TCHAR(32)||FString(TEXT("/\\:*?\"<>|.")).Contains(FString::Chr(C))) return false;
    return true;
}
bool SaveRecipe(const FRecipe& R,const FString& Directory,FString& Error)
{
    if(!SafeName(R.Name)) { Error=TEXT("저장 이름에 경로·특수문자를 사용할 수 없습니다."); return false; }
    auto Root=MakeShared<FJsonObject>(); Root->SetNumberField(TEXT("version"),R.Version); Root->SetStringField(TEXT("name"),R.Name); Root->SetNumberField(TEXT("rpm"),R.RPM); Root->SetNumberField(TEXT("mixSeconds"),R.MixSeconds);
    TArray<TSharedPtr<FJsonValue>> Hs,Ss;
    for(const auto& H:R.Hoppers)
    {
        auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("id"),H.Id); O->SetStringField(TEXT("material"),H.Material); O->SetBoolField(TEXT("additive"),H.Additive); O->SetNumberField(TEXT("diameterM"),H.DiameterM); O->SetNumberField(TEXT("rateKgS"),H.RateKgS); Hs.Add(MakeShared<FJsonValueObject>(O));
    }
    for(const auto& S:R.Steps)
    {
        auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("id"),S.Id); O->SetStringField(TEXT("name"),S.Name); O->SetStringField(TEXT("kind"),S.Kind==EStepKind::WeighTransfer?TEXT("weighTransfer"):TEXT("mixerCharge")); O->SetStringField(TEXT("hopper"),S.HopperId); O->SetNumberField(TEXT("targetKg"),S.TargetKg); O->SetStringField(TEXT("timing"),S.Timing==ETiming::Initial?TEXT("initial"):TEXT("afterMixing")); O->SetNumberField(TEXT("atSeconds"),S.AtSeconds); Ss.Add(MakeShared<FJsonValueObject>(O));
    }
    Root->SetArrayField(TEXT("hoppers"),Hs); Root->SetArrayField(TEXT("steps"),Ss);
    FString Json; FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json)); IFileManager::Get().MakeDirectory(*Directory,true);
    const FString Path=Directory/(R.Name+TEXT(".json")),Temp=Path+TEXT(".tmp");
    if(!FFileHelper::SaveStringToFile(Json,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)||!IFileManager::Get().Move(*Path,*Temp,true,true)) { Error=TEXT("저장 실패: 파일 권한 확인 필요"); return false; }
    Error=TEXT("이름을 붙인 설정 저장 완료"); return true;
}
bool LoadRecipe(const FString& Name,const FString& Directory,FRecipe& Out,FString& Error)
{
    if(!SafeName(Name)) { Error=TEXT("유효하지 않은 저장 이름"); return false; }
    FString Json; TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Json,*(Directory/(Name+TEXT(".json"))))||Json.Len()>1024*1024||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)||!Root.IsValid()) { Error=TEXT("설정 파일 읽기/JSON 검증 실패"); return false; }
    FRecipe R; double Version=0; const TArray<TSharedPtr<FJsonValue>> *Hs=nullptr,*Ss=nullptr;
    bool Ok=Root->TryGetNumberField(TEXT("version"),Version)&&Version==1&&Root->TryGetStringField(TEXT("name"),R.Name)&&Root->TryGetNumberField(TEXT("rpm"),R.RPM)&&Root->TryGetNumberField(TEXT("mixSeconds"),R.MixSeconds)&&Root->TryGetArrayField(TEXT("hoppers"),Hs)&&Root->TryGetArrayField(TEXT("steps"),Ss);
    if(!Ok||!Hs||!Ss||Hs->Num()>256||Ss->Num()>4096) { Error=TEXT("저장 스키마/버전 검증 실패"); return false; }
    for(const auto& V:*Hs)
    {
        const TSharedPtr<FJsonObject>* O=nullptr; FHopper H;
        if(!V->TryGetObject(O)||!O||!O->IsValid()) { Ok=false; break; }
        Ok=(*O)->TryGetStringField(TEXT("id"),H.Id)&&(*O)->TryGetStringField(TEXT("material"),H.Material)&&(*O)->TryGetBoolField(TEXT("additive"),H.Additive)&&(*O)->TryGetNumberField(TEXT("diameterM"),H.DiameterM)&&(*O)->TryGetNumberField(TEXT("rateKgS"),H.RateKgS);
        if(!Ok) break; R.Hoppers.Add(H);
    }
    for(const auto& V:*Ss)
    {
        const TSharedPtr<FJsonObject>* O=nullptr; FStep S; FString Kind,Timing;
        if(!Ok||!V->TryGetObject(O)||!O||!O->IsValid()) { Ok=false; break; }
        Ok=(*O)->TryGetStringField(TEXT("id"),S.Id)&&(*O)->TryGetStringField(TEXT("name"),S.Name)&&(*O)->TryGetStringField(TEXT("kind"),Kind)&&(*O)->TryGetStringField(TEXT("hopper"),S.HopperId)&&(*O)->TryGetNumberField(TEXT("targetKg"),S.TargetKg)&&(*O)->TryGetStringField(TEXT("timing"),Timing)&&(*O)->TryGetNumberField(TEXT("atSeconds"),S.AtSeconds);
        if((Kind!=TEXT("weighTransfer")&&Kind!=TEXT("mixerCharge"))||(Timing!=TEXT("initial")&&Timing!=TEXT("afterMixing"))) Ok=false;
        if(!Ok) break; S.Kind=Kind==TEXT("weighTransfer")?EStepKind::WeighTransfer:EStepKind::MixerCharge; S.Timing=Timing==TEXT("initial")?ETiming::Initial:ETiming::AfterMixing; R.Steps.Add(S);
    }
    if(!Ok) { Error=TEXT("잘못된 단계 또는 누락 필드: 기존 설정 유지"); return false; }
    Out=MoveTemp(R); Error=TEXT("불러오기 완료 · 실행 전 의미 검증 필요 · 자동 실행 안 함"); return true;
}
}
