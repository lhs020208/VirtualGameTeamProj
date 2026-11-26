#include "ProcessWebStateSubsystem.h"
#include "ProcessWebSnapshot.h"
#include "ProcessUIStateAdapter.h"
#include "ProcessControlService.h"
#include "ProcessPlantBinding.h"
#include "PowderPlantTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

namespace
{
const FString PublisherId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
int64 PublicationSequence=0; // Game thread only, across all world/run lifetimes in this process.
FString MapName(const UWorld* W){return W?UWorld::RemovePIEPrefix(W->GetOutermost()->GetName()):FString();}
}
bool UProcessWebStateSubsystem::DoesSupportWorldType(EWorldType::Type T) const{return T==EWorldType::PIE||T==EWorldType::Game;}
void UProcessWebStateSubsystem::OnWorldBeginPlay(UWorld& W)
{
 Super::OnWorldBeginPlay(W);RunId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
 ControlEnabled=FParse::Param(FCommandLine::Get(),TEXT("ProcessWebControl"));
 Enabled=(ControlEnabled||FParse::Param(FCommandLine::Get(),TEXT("ProcessWebState")))&&MapName(&W)==ProcessPlantBinding::LowPolyMap;
}
void UProcessWebStateSubsystem::ReadLink()
{
 FString Data;TSharedPtr<FJsonObject> J;
 if(!FFileHelper::LoadFileToString(Data,*(FPaths::ProjectSavedDir()/TEXT("ProcessWeb/link.json")))||Data.Len()>4096||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data),J)||!J){Port=0;Token.Empty();return;}
 double Version=0,Number=0;FString NewToken,NewServer;
 if(!J->TryGetNumberField(TEXT("version"),Version)||Version!=1||!J->TryGetNumberField(TEXT("ingest_port"),Number)||!FMath::IsFinite(Number)||Number<1||Number>65535||Number!=int32(Number)||!J->TryGetStringField(TEXT("token"),NewToken)||NewToken.Len()!=64||!J->TryGetStringField(TEXT("server_id"),NewServer)){Port=0;return;}
 for(TCHAR C:NewToken)if(!FString(TEXT("0123456789abcdef")).Contains(FString::Chr(C))){Port=0;return;}
 if(ServerId!=NewServer)if(auto* S=BoundService.Get())Commands.Release(*S,TEXT("로컬 서버 재시작 · 명시적 제어권 재획득 필요"));
 Token=NewToken;Port=int32(Number);ServerId=NewServer;
}
void UProcessWebStateSubsystem::Tick(float DeltaTime)
{
 Super::Tick(DeltaTime);if(!Enabled)return;check(IsInGameThread());
 const double Now=FPlatformTime::Seconds();if(auto* S=BoundService.Get();S&&!S->WebLeaseId.IsEmpty()&&!S->WebOwnsControl())Commands.Release(*S,TEXT("웹 제어권 만료"));
 if(Now-LastLinkRead>=1.){LastLinkRead=Now;ReadLink();}
 if(Port==0||Pending||Now-LastPublish<.25)return;LastPublish=Now;
 ProcessWebSnapshot::FContext C;C.PublisherId=PublisherId+TEXT(":")+RunId;C.RunId=RunId;C.PublicationSequence=++PublicationSequence;
 C.BindingReason=TEXT("명시적으로 지정된 UI 해석 Actor 연결 대기");C.NextAction=TEXT("대상 레벨의 기존 UI와 지정된 해석 Actor 연결을 확인하세요.");
 int32 Count=0;for(const auto& WC:GEngine->GetWorldContexts())if(WC.World()&&WC.World()->HasBegunPlay()&&(WC.WorldType==EWorldType::PIE||WC.WorldType==EWorldType::Game)&&MapName(WC.World())==ProcessPlantBinding::LowPolyMap)++Count;
 FPlantSimulationSnapshot Snapshot;FProcessAutoSnapshot Automatic;bool Bound=false;
 auto* UI=GetWorld()->GetSubsystem<UProcessUIStateSubsystem>();
 if(Count!=1){C.BindingReason=TEXT("대상 레벨의 실행 인스턴스가 여러 개이거나 확인되지 않습니다.");C.NextAction=TEXT("대상 PIE·게임 인스턴스를 하나로 지정하세요.");}
 else if(UI&&UI->ControlService)
 {
  auto* Service=UI->ControlService.Get();auto* Source=Service->GetSource();
  FSoftObjectPath Expected(ProcessPlantBinding::SourcePath(ProcessPlantBinding::LowPolyMap));if(GetWorld()->WorldType==EWorldType::PIE)Expected.FixupForPIE(GetWorld()->GetOutermost()->GetPIEInstanceID());
  if(IsValid(Source)&&Source==Expected.ResolveObject()&&Source->GetWorld()==GetWorld()&&Service->State.Connected)
  {
   // One game-thread copy from the service's already observed worker snapshot. No new worker read
   // between fields. Command delivery is separate and explicitly opt-in on the game thread.
   Snapshot=Service->State.Snapshot;Automatic=Service->GetAutomaticSnapshot();C.AppliedEntries=Service->AutoSettings.Entries;
   if(Snapshot.SourceActorPath==Source->GetPathName())
   {
    if(HadSource&&(PublishedSource.Get()!=Source||PublishedConnection!=Service->State.Connection||BoundService.Get()!=Service)){
     if(auto* Old=BoundService.Get())Commands.Release(*Old,TEXT("해석 서비스 재연결 · 새 실행 식별자"));
     RunId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    }
    PublishedSource=Source;PublishedConnection=Service->State.Connection;HadSource=true;
    C.PublisherId=PublisherId+TEXT(":")+RunId;C.RunId=RunId;Bound=true;BoundService=Service;C.ActualActorPath=Source->GetPathName();C.BindingReason=TEXT("지정된 해석 Actor 상태 연결");C.NextAction=ControlEnabled?TEXT("제어권을 명시적으로 가져온 뒤 수동 조작하세요. 3D 관찰은 언리얼에서 진행하세요."):TEXT("조회 전용 · 운전은 언리얼에서 진행하세요.");
    const TCHAR* StopNames[]={TEXT("none"),TEXT("requested"),TEXT("closing"),TEXT("pausing"),TEXT("complete"),TEXT("failed")};C.StopStage=StopNames[int32(Service->State.StopStage)];C.StopReason=Service->State.StopDescription();
    if(Automatic.Epoch==Snapshot.Epoch&&(Automatic.Stage==EProcessAutoStage::Cancelled||Automatic.Stage==EProcessAutoStage::Failed))C.BlockReason=Automatic.RecoveryReason;
    if(Automatic.Stage==EProcessAutoStage::Idle)C.BlockReason=Service->AutoRun.CanStart(Service->State,Now);
   }
  }
  else C.BindingReason=TEXT("UI에 연결된 Actor가 지정된 레벨·Actor 참조와 일치하지 않습니다.");
 }
 if(!Bound){if(auto* S=BoundService.Get())Commands.Release(*S,TEXT("명시적 Actor 연결 해제"));BoundService.Reset();}
 C.Control=Commands.Describe(Bound?BoundService.Get():nullptr,ControlEnabled,Now);
 const FString Payload=ProcessWebSnapshot::Serialize(C,Bound?&Snapshot:nullptr,Bound?&Automatic:nullptr,Now);
 auto Request=FHttpModule::Get().CreateRequest();Pending=Request;
 Request->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/v1/state"),Port));Request->SetVerb(TEXT("POST"));Request->SetHeader(TEXT("Content-Type"),TEXT("application/json"));Request->SetHeader(TEXT("Authorization"),TEXT("Bearer ")+Token);
 Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);Request->SetTimeout(2.f);Request->SetContentAsString(Payload);
 const FString SentServer=ServerId,SentRun=RunId;
 Request->OnProcessRequestComplete().BindWeakLambda(this,[this,SentServer,SentRun,Now](FHttpRequestPtr R,FHttpResponsePtr Response,bool Success){
  check(IsInGameThread());if(Pending==R)Pending.Reset();
  auto* S=BoundService.Get();if(!ControlEnabled||!S||!Success||!Response||Response->GetResponseCode()!=200||SentServer!=ServerId||SentRun!=RunId||Response->GetContentLength()>32768)return;
  // Re-check the same explicitly bound source at delivery, not the network thread or first actor.
  if(!PublishedSource.IsValid()||S->GetSource()!=PublishedSource.Get()||S->State.Snapshot.SourceActorPath!=PublishedSource->GetPathName())return;
  TSharedPtr<FJsonObject> Frame;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()),Frame)||!Frame)return;
  FString Echo;Frame->TryGetStringField(TEXT("server_id"),Echo);if(Echo!=SentServer)return;
  Commands.Receive(Frame,RunId,*S,FPlatformTime::Seconds()-Now);
 });
 if(!Request->ProcessRequest())Pending.Reset(); // Latest-only: no retry queue and no blocking on the solver.
}
void UProcessWebStateSubsystem::Deinitialize()
{
 if(auto* S=BoundService.Get())Commands.Release(*S,TEXT("웹 연결 종료"));BoundService.Reset();
 Enabled=false;if(Pending){auto R=Pending;Pending.Reset();R->OnProcessRequestComplete().Unbind();R->CancelRequest();}Token.Empty();Port=0;
 // Outstanding manual tokens use the existing stop path; no automatic batch restoration.
 Super::Deinitialize();
}
TStatId UProcessWebStateSubsystem::GetStatId() const{RETURN_QUICK_DECLARE_CYCLE_STAT(UProcessWebStateSubsystem,STATGROUP_Tickables);}
