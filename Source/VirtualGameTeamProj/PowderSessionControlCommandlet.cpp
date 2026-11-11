#include "PowderSessionControlCommandlet.h"
#include "EngineServiceMessages.h"
#include "SessionServiceMessages.h"
#include "MessageEndpointBuilder.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
UPowderSessionControlCommandlet::UPowderSessionControlCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UPowderSessionControlCommandlet::Main(const FString& Params){
 FString Target,CommandFile;FParse::Value(*Params,TEXT("Target="),Target);FParse::Value(*Params,TEXT("CommandFile="),CommandFile);
 TMap<FGuid,FMessageAddress> Addresses;bool denied=false;
 auto endpoint=FMessageEndpoint::Builder("PowderSessionControl")
 .Handling<FEngineServicePong>([&](const FEngineServicePong& M,const TSharedRef<IMessageContext,ESPMode::ThreadSafe>& C){Addresses.Add(M.InstanceId,C->GetSender());UE_LOG(LogTemp,Display,TEXT("PROBE instance=%s level=%s begun=%d world_seconds=%.3f type=%s"),*M.InstanceId.ToString(),*M.CurrentLevel,M.HasBegunPlay,M.WorldTimeSeconds,*M.InstanceType);})
 .Handling<FSessionServicePong>([&](const FSessionServicePong& M,const TSharedRef<IMessageContext,ESPMode::ThreadSafe>&){UE_LOG(LogTemp,Display,TEXT("SESSION instance=%s name=%s authorized=%d owner=%s"),*M.InstanceId.ToString(),*M.InstanceName,M.Authorized,*M.SessionOwner);})
 .Handling<FEngineServiceNotification>([&](const FEngineServiceNotification&M,const TSharedRef<IMessageContext,ESPMode::ThreadSafe>&){UE_LOG(LogTemp,Display,TEXT("NOTIFICATION %s"),*M.Text);denied=true;})
 .ReceivingOnThread(ENamedThreads::GameThread).Build();
 if(!endpoint.IsValid())return 1;
 auto tick=[](){FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);FTSTicker::GetCoreTicker().Tick(.01f);FPlatformProcess::Sleep(.01f);};
 for(int n=0;n<600;++n){if(n%100==0){endpoint->Publish(FMessageEndpoint::MakeMessage<FEngineServicePing>(),EMessageScope::Network);endpoint->Publish(FMessageEndpoint::MakeMessage<FSessionServicePing>(FPlatformProcess::UserName()),EMessageScope::Network);}tick();}
 if(Target.IsEmpty()){endpoint.Reset();return 0;}
 FGuid Id;if(!FGuid::Parse(Target,Id)||!Addresses.Contains(Id)){UE_LOG(LogTemp,Error,TEXT("Exact target instance not discovered; no command sent"));return 2;}
 FString Command;if(CommandFile.IsEmpty()||!FFileHelper::LoadFileToString(Command,*CommandFile)||Command.IsEmpty())return 3;
 // No authorization grants, termination, broadcast commands, or fallback target.
 endpoint->Send(FMessageEndpoint::MakeMessage<FEngineServiceExecuteCommand>(Command.TrimStartAndEnd(),FPlatformProcess::UserName()),Addresses[Id]);
 UE_LOG(LogTemp,Display,TEXT("SENT_ONCE instance=%s"),*Target);
 for(int n=0;n<300;++n)tick();endpoint.Reset();return denied?4:0;
}
