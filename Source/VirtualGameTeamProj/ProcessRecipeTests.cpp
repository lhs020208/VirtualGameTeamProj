#include "ProcessRecipe.h"
#include "ProcessControlService.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include <limits>
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
#if WITH_DEV_AUTOMATION_TESTS
namespace {
FProcessRecipe Example(){FProcessRecipe R;R.Name=TEXT("레시피 시험 · 원료 표시명");R.Mode=EProcessBatchMode::SequentialAB;R.MaterialA=TEXT("원료 알파");R.MaterialB=TEXT("원료 베타");R.MaterialC=TEXT("첨가제 감마");R.TargetAKg=.04;R.ToleranceAKg=.002;R.TargetBKg=.04;R.ToleranceBKg=.002;R.AdditiveEnabled=true;R.TargetCKg=.02;R.ToleranceCKg=.002;R.Scheduled=true;R.DelaySeconds=4;R.TargetRPM=15;R.MixingSeconds=6;return R;}
TSharedPtr<FJsonObject> Object(const FString& S){TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),O);return O;}
FString String(const TSharedPtr<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecipeRoundTrip,"ProcessUI.Recipe.VersionedRoundTripAndModes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecipeRoundTrip::RunTest(const FString&)
{
 for(int Mode=0;Mode<5;++Mode){auto R=Example();
  if(Mode<2){R.Mode=Mode==0?EProcessBatchMode::Full:EProcessBatchMode::TargetMass;R.TargetBKg=R.ToleranceBKg=0;R.AdditiveEnabled=R.Scheduled=false;R.TargetCKg=R.ToleranceCKg=R.DelaySeconds=0;if(Mode==0)R.TargetAKg=R.ToleranceAKg=0;}
  if(Mode==2){R.Scheduled=false;R.DelaySeconds=0;}if(Mode==3)R.DelaySeconds=0;
  FString Why;FProcessRecipe Restored;TestTrue(TEXT("valid explicit recipe"),ProcessRecipe::Validate(R).IsEmpty());TestTrue(TEXT("round trip"),ProcessRecipe::Decode(ProcessRecipe::Encode(R),Restored,Why));
  TestTrue(TEXT("all active settings and names identical"),ProcessRecipe::Same(R,Restored));TestEqual(TEXT("OFF and ON0 boolean retained"),Restored.Scheduled,R.Scheduled);TestEqual(TEXT("N preserved"),Restored.DelaySeconds,R.DelaySeconds);
  const auto Text=ProcessRecipe::Encode(R);for(const TCHAR* Forbidden:{TEXT("valve"),TEXT("particle_ids"),TEXT("before_gauge"),TEXT("RequestId"),TEXT("AppliedRPM")})TestFalse(TEXT("execution state not serialized"),Text.Contains(Forbidden));
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecipeBadFiles,"ProcessUI.Recipe.RejectInvalidFileWithoutPartialLoad",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecipeBadFiles::RunTest(const FString&)
{
 const auto Good=Example();const FString Original=ProcessRecipe::Encode(Good);TArray<FString> Bad{TEXT("{ broken"),TEXT("[]"),TEXT("null")};
 auto Add=[&](TFunction<void(TSharedPtr<FJsonObject>)> Change){auto O=Object(Original);Change(O);Bad.Add(String(O));};
 Add([](auto O){O->SetNumberField(TEXT("version"),99);});Add([](auto O){O->SetStringField(TEXT("mode"),TEXT("b_then_a"));});
 Add([](auto O){O->GetObjectField(TEXT("units"))->SetStringField(TEXT("mass"),TEXT("g"));});
 Add([](auto O){O->GetObjectField(TEXT("units"))->SetStringField(TEXT("time"),TEXT("wall_s"));});
 Add([](auto O){O->GetObjectField(TEXT("mixing"))->SetNumberField(TEXT("target_rpm"),31);});
 Add([](auto O){O->GetObjectField(TEXT("mixing"))->SetStringField(TEXT("duration_s"),TEXT("6"));});
 Add([](auto O){O->GetObjectField(TEXT("additive"))->SetStringField(TEXT("hopper_id"),TEXT("supply-d"));});
 Add([](auto O){O->GetObjectField(TEXT("additive"))->SetNumberField(TEXT("delay_s"),6);});
 Add([](auto O){O->SetArrayField(TEXT("feed_order"),{MakeShared<FJsonValueString>(TEXT("supply-b")),MakeShared<FJsonValueString>(TEXT("supply-a")),MakeShared<FJsonValueString>(TEXT("supply-c"))});});
 Add([](auto O){O->GetArrayField(TEXT("materials"))[1]->AsObject()->SetStringField(TEXT("hopper_id"),TEXT("supply-a"));});
 Add([](auto O){O->GetArrayField(TEXT("materials"))[2]->AsObject()->SetNumberField(TEXT("target_kg"),-.02);});
 Add([](auto O){O->SetBoolField(TEXT("receiver_open"),true);});
 Add([](auto O){O->GetArrayField(TEXT("materials"))[0]->AsObject()->SetNumberField(TEXT("tolerance_kg"),.04);});
 for(const auto& Text:Bad){FProcessRecipe Out=Good;FString Why;TestFalse(TEXT("reject malformed/unsupported recipe"),ProcessRecipe::Decode(Text,Out,Why));TestFalse(TEXT("reason provided"),Why.IsEmpty());TestEqual(TEXT("no partial mutation"),ProcessRecipe::Encode(Out),Original);}
 auto R=Good;R.TargetAKg=std::numeric_limits<double>::quiet_NaN();TestFalse(TEXT("NaN refused"),ProcessRecipe::Validate(R).IsEmpty());R=Good;R.TargetRPM=std::numeric_limits<double>::infinity();TestFalse(TEXT("infinity refused"),ProcessRecipe::Validate(R).IsEmpty());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecipeFiles,"ProcessUI.Recipe.PersistenceAndExplicitReplacement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecipeFiles::RunTest(const FString&)
{
 const FString Root=FPaths::ProjectSavedDir()/TEXT("RecipeAutomatedTests")/FGuid::NewGuid().ToString(EGuidFormats::Digits);FProcessRecipeStore Store(Root);auto R=Example();FString File,Why;
 TestTrue(TEXT("new named save"),Store.SaveNew(R,File,Why));const auto Original=ProcessRecipe::Encode(R);R.MaterialA=TEXT("표시명 변경 · 호퍼 ID 유지");
 FString Other;TestFalse(TEXT("duplicate save never overwrites"),Store.SaveNew(R,Other,Why));FProcessRecipe Reloaded;
 TestTrue(TEXT("new store instance loads from disk"),FProcessRecipeStore(Root).Load(File,Reloaded,Why));TestEqual(TEXT("previous save intact"),ProcessRecipe::Encode(Reloaded),Original);
 TestTrue(TEXT("explicit selected replacement"),Store.ReplaceSelected(File,R,Why));TestTrue(TEXT("replacement reload"),Store.Load(File,Reloaded,Why));TestTrue(TEXT("new data saved"),ProcessRecipe::Same(R,Reloaded));
 auto Different=R;Different.Name=TEXT("다른 이름");TestFalse(TEXT("selection/name mismatch refused"),Store.ReplaceSelected(File,Different,Why));
 TestFalse(TEXT("path escape refused"),Store.Load(TEXT("../elsewhere.json"),Reloaded,Why));
 FFileHelper::SaveStringToFile(TEXT("broken"),*(Root/TEXT("broken.json")));TestEqual(TEXT("bad file listed with error"),Store.List().Num(),2);
 TestFalse(TEXT("bad file load refused"),Store.Load(TEXT("broken.json"),Reloaded,Why));TestTrue(TEXT("old output preserved"),ProcessRecipe::Same(R,Reloaded));
 IFileManager::Get().DeleteDirectory(*Root,false,true);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecipeAtomic,"ProcessUI.Recipe.FailedReplacementPreservesPreviousBytes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecipeAtomic::RunTest(const FString&)
{
 const FString Root=FPaths::ProjectSavedDir()/TEXT("RecipeAutomatedTests")/FGuid::NewGuid().ToString(EGuidFormats::Digits);FProcessRecipeStore Store(Root);auto R=Example();FString File,Why;
 if(!TestTrue(TEXT("seed previous file"),Store.SaveNew(R,File,Why)))return false;const FString Path=FPaths::ConvertRelativePathToFull(Root/File);TArray<uint8> Before,After;FFileHelper::LoadFileToArray(Before,*Path);R.TargetAKg=.041;
#if PLATFORM_WINDOWS
 HANDLE Lock=::CreateFileW(*Path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(!TestTrue(TEXT("deny-delete lock for real commit failure"),Lock!=INVALID_HANDLE_VALUE))return false;
 TestFalse(TEXT("atomic replace fails while destination locked"),Store.ReplaceSelected(File,R,Why));::CloseHandle(Lock);
#else
 AddInfo(TEXT("Windows deny-delete replacement case is exercised on the project's Win64 build."));
#endif
 FFileHelper::LoadFileToArray(After,*Path);TestTrue(TEXT("previous bytes unchanged on failure"),Before==After);
 TArray<FString> Temps;IFileManager::Get().FindFiles(Temps,*(Root/TEXT("*.tmp")),true,false);TestEqual(TEXT("owned temporary cleaned"),Temps.Num(),0);
 IFileManager::Get().DeleteDirectory(*Root,false,true);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecipeApply,"ProcessUI.Recipe.AtomicConfigurationWithoutCommands",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecipeApply::RunTest(const FString&)
{
 auto* Service=NewObject<UProcessControlService>();Service->State.Snapshot.RequestedControlRevision=42;Service->State.Snapshot.RequestedRPM=0;Service->AutoSettings.StageWallSeconds=1234;
 auto R=Example();FString Why;TestTrue(TEXT("apply without worker inventory or commands"),Service->ApplyRecipe(R,Why));TestTrue(TEXT("all fields assigned together"),ProcessRecipe::Same(R,Service->GetAppliedRecipe()));
 TestEqual(TEXT("request revision untouched"),Service->State.Snapshot.RequestedControlRevision,int64(42));TestEqual(TEXT("worker RPM not sent"),Service->State.Snapshot.RequestedRPM,0.);TestEqual(TEXT("no control history created"),Service->State.History.Num(),0);TestEqual(TEXT("trial timeout not a recipe field"),Service->AutoSettings.StageWallSeconds,1234.);
 const auto Applied=ProcessRecipe::Encode(Service->GetAppliedRecipe());auto Bad=R;Bad.TargetBKg=-1;TestFalse(TEXT("invalid whole recipe rejected"),Service->ApplyRecipe(Bad,Why));TestEqual(TEXT("valid A not partially applied"),ProcessRecipe::Encode(Service->GetAppliedRecipe()),Applied);
 Service->AutoRun.State.Active=true;Service->AutoRun.State.TargetMixerRPM=15;Service->AutoRun.State.LockedRecipeJson=Applied;R.TargetRPM=20;
 TestFalse(TEXT("active batch rejects changed recipe"),Service->ApplyRecipe(R,Why));TestEqual(TEXT("current batch RPM unchanged"),Service->AutoRun.State.TargetMixerRPM,15.);TestEqual(TEXT("locked configuration unchanged"),Service->AutoRun.State.LockedRecipeJson,Applied);TestEqual(TEXT("applied values unchanged"),ProcessRecipe::Encode(Service->GetAppliedRecipe()),Applied);
 Service->AutoRun.State.Active=false;Service->State.StopStage=EProcessStopStage::Closing;TestFalse(TEXT("full stop rejects apply"),Service->ApplyRecipe(R,Why));TestEqual(TEXT("only one recipe revision"),Service->RecipeRevision,int64(1));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecipeStart,"ProcessUI.Recipe.StartUsesConfiguredRPMAndRechecksInventory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecipeStart::RunTest(const FString&)
{
 FProcessAutoRun A;FProcessControlMachine M;auto& S=M.Snapshot;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Paused;S.Epoch=1;S.Sequence=1;S.PublishedWallSeconds=100;
 S.SourceActorPath=TEXT("/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111.L_ProcessPlant_LowPoly_1111:PersistentLevel.PowderPlantTest_0");S.GPUDevice=TEXT("protocol fixture; not GPU");S.RequestedSuppliers.Init(false,6);
 S.SupplyAPurge.Supported=S.SupplyAPurge.Valid=S.FeedPreparation.Enabled=true;S.MixerFeed.Supported=S.MixerFeed.Valid=true;
 S.WeighingMass.Validity=S.PipeMass.Validity=S.ReceiverMass.Validity=EPlantValueValidity::Valid;S.SupplyABatch.Supported=S.SupplyABatch.Valid=true;S.SupplyABatch.TotalCount=S.SupplyABatch.SourceCount=100;S.SupplyABatch.TotalKg=.1;S.ModelParticleMassKg=.001;S.RequestedRPM=0;M.Connected=true;
 A.Settings.MixingDurationSeconds=2;A.Settings.TargetMixerRPM=15;TestTrue(TEXT("recipe RPM admits without prior motor command"),A.CanStart(M,100).IsEmpty());TestTrue(TEXT("explicit start captures values"),A.Start(FGuid::NewGuid(),M,100));TestEqual(TEXT("start locks configured RPM"),A.State.TargetMixerRPM,15.);TestEqual(TEXT("start only enters initial stop; no worker targets written"),S.RequestedRPM,0.);
 A.Cancel(TEXT("test reset"));M.AutomationActive=false;A.Settings.BatchMode=EProcessBatchMode::TargetMass;A.Settings.TargetMassKg=.2;A.Settings.WeighingToleranceKg=.002;TestFalse(TEXT("start rejects current inventory shortage"),A.CanStart(M,100).IsEmpty());
 A.Settings.TargetMassKg=.0505;A.Settings.WeighingToleranceKg=0;TestFalse(TEXT("start rejects unrepresentable mass"),A.CanStart(M,100).IsEmpty());
 A.Settings.BatchMode=EProcessBatchMode::Full;A.Settings.TargetMixerRPM=0;S.RequestedRPM=17;TestTrue(TEXT("legacy individual RPM still supported"),A.CanStart(M,100).IsEmpty());TestEqual(TEXT("legacy RPM chosen"),A.ConfiguredRPM(S),17.);
 A.Settings.TargetMixerRPM=std::numeric_limits<double>::quiet_NaN();TestFalse(TEXT("NaN setting cannot fall back silently"),A.CanStart(M,100).IsEmpty());return true;
}
#endif
