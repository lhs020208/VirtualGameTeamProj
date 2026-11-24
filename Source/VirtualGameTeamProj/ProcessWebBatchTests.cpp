#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ProcessWebCommands.h"
#include "ProcessControlService.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
namespace {
FProcessRecipe Recipe(){FProcessRecipe R;R.Name=TEXT("CPU isolated recipe");R.Mode=EProcessBatchMode::SequentialAB;R.TargetAKg=.100123456789123;R.ToleranceAKg=.002;R.TargetBKg=.099876543210987;R.ToleranceBKg=.002;R.AdditiveEnabled=R.Scheduled=true;R.TargetCKg=.0399711171234567;R.ToleranceCKg=.002;R.DelaySeconds=0;R.TargetRPM=15;R.MixingSeconds=6;return R;}
UProcessControlService* Service(){auto* S=NewObject<UProcessControlService>();S->State.Connected=true;S->State.Connection=3;auto& P=S->State.Snapshot;P.Epoch=7;P.Sequence=9;P.Validity=EPlantValueValidity::Valid;P.PublishedWallSeconds=FPlatformTime::Seconds();S->WebLeaseEpoch=7;S->WebLeaseId=TEXT("web-test");S->WebLeaseUntil=FPlatformTime::Seconds()+100;return S;}
ProcessWebCommands::FCommand Command(const TCHAR* K,UProcessControlService* S){ProcessWebCommands::FCommand C;C.Id=FGuid::NewGuid();C.Run=TEXT("cpu-run");C.Epoch=7;C.Lease=S->WebLeaseId;C.Equipment=TEXT("plant");C.Kind=K;C.Unit=TEXT("none");C.RecipeRevision=LexToString(S->RecipeRevision);C.RecipeFingerprint=FProcessWebBatch::RecipeFingerprint(*S);return C;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWebRecipeFiles,"ProcessWeb.Batch.RecipesAndConcurrentSave",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWebRecipeFiles::RunTest(const FString&)
{
 auto* S=Service();FProcessWebBatch B;B.Store.Root=FPaths::ProjectSavedDir()/TEXT("WebRecipeCPUTests")/FGuid::NewGuid().ToString();auto R=Recipe();
 auto C=Command(TEXT("recipe_apply"),S);C.RecipeJson=ProcessRecipe::Encode(R);ProcessWebCommands::FResult Result;B.Execute(C,*S,Result);
 TestEqual(TEXT("web full apply"),Result.Record.Status,EProcessCommandStatus::Applied);TestTrue(TEXT("all fields precise"),ProcessRecipe::Same(S->GetAppliedRecipe(),R));TestEqual(TEXT("no device command history"),S->State.History.Num(),0);TestEqual(TEXT("ON0 preserved"),S->AutoSettings.AdditiveDelaySeconds,0.);TestTrue(TEXT("ON0 separate from OFF"),S->AutoSettings.AdditiveScheduled);
 B.Execute(C,*S,Result);TestEqual(TEXT("old config version rejected"),Result.Record.Status,EProcessCommandStatus::Rejected);TestEqual(TEXT("no extra revision"),S->RecipeRevision,int64(1));
 FString Why;TestFalse(TEXT("native UI fenced by web ownership"),S->ApplyRecipe(R,Why));
 C=Command(TEXT("recipe_save"),S);C.RecipeJson=ProcessRecipe::Encode(R);B.Execute(C,*S,Result);TestEqual(TEXT("save reuses store"),Result.Record.Status,EProcessCommandStatus::Applied);
 const FString Id=Result.Data->GetStringField(TEXT("file_id")),Version=Result.Data->GetStringField(TEXT("file_version"));
 C=Command(TEXT("recipe_load"),S);C.FileId=Id;B.Execute(C,*S,Result);FProcessRecipe Loaded;TestTrue(TEXT("round trip through web result"),ProcessRecipe::Decode(Result.Data->GetStringField(TEXT("recipe_json")),Loaded,Why));TestTrue(TEXT("precision preserved"),ProcessRecipe::Same(R,Loaded));TestEqual(TEXT("store/load/apply issue no commands"),S->State.History.Num(),0);
 R.MaterialA=TEXT("second writer");TestTrue(TEXT("external editor version update"),B.Store.ReplaceSelected(B.Files[Id],R,Why,Version));C=Command(TEXT("recipe_save"),S);C.FileId=Id;C.FileVersion=Version;C.Confirm=true;C.RecipeJson=ProcessRecipe::Encode(Recipe());B.Execute(C,*S,Result);TestEqual(TEXT("stale overwrite refused"),Result.Record.Status,EProcessCommandStatus::Rejected);B.Store.Load(B.Files[Id],Loaded,Why);TestEqual(TEXT("other edit preserved"),Loaded.MaterialA,R.MaterialA);
 FFileHelper::SaveStringToFile(TEXT("busy"),*(B.Store.Root/TEXT(".recipe-write.lock")));TestFalse(TEXT("writer lock never waited or removed"),B.Store.ReplaceSelected(B.Files[Id],Recipe(),Why,B.Store.FileVersion(B.Files[Id])));TestTrue(TEXT("lock remains"),IFileManager::Get().FileExists(*(B.Store.Root/TEXT(".recipe-write.lock"))));IFileManager::Get().Delete(*(B.Store.Root/TEXT(".recipe-write.lock")));
 C=Command(TEXT("recipe_load"),S);C.FileId=TEXT("../../outside.json");B.Execute(C,*S,Result);TestEqual(TEXT("file paths cannot be selected"),Result.Record.Status,EProcessCommandStatus::Rejected);
 S->AutoRun.State.Active=true;C=Command(TEXT("recipe_apply"),S);C.RecipeJson=ProcessRecipe::Encode(R);B.Execute(C,*S,Result);TestEqual(TEXT("active batch immutable"),Result.Record.Status,EProcessCommandStatus::Rejected);TestEqual(TEXT("still no device commands"),S->State.History.Num(),0);
 IFileManager::Get().DeleteDirectory(*B.Store.Root,false,true);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWebBatchRecovery,"ProcessWeb.Batch.RecoveryProofAndStopPriority",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWebBatchRecovery::RunTest(const FString&)
{
 auto* S=Service();auto& M=S->State;auto& P=M.Snapshot;auto& A=S->AutoRun.State;
 P.Execution=EPlantExecutionState::Paused;P.RequestedRunning=false;P.RequestedSuppliers.Init(false,6);P.AppliedRPMValid=P.AppliedPressureValid=true;
 P.Recovery.Valid=P.Recovery.Supported=true;P.Recovery.Materials.SetNum(3);P.ModelParticleMassKg=.001;auto& Material=P.Recovery.Materials[0];Material.Supplier=0;Material.Source=5;Material.SupplyConnector=3;Material.Weighing=2;
 auto& B=P.SupplyABatch;B.Valid=B.Supported=true;B.CaptureRevision=8;B.InitialCount=B.TotalCount=10;B.InitialKg=B.TotalKg=.01;B.SourceCount=5;B.ConnectorCount=3;B.WeighingCount=2;
 M.StopStage=EProcessStopStage::Complete;A.RunId=FGuid::NewGuid();A.Epoch=7;A.Connection=3;A.RecoveryPending=true;A.BatchMode=EProcessBatchMode::TargetMass;A.LockedTargetAKg=.006;A.LockedToleranceAKg=.001;
 FProcessWebBatch Web;TestTrue(TEXT("existing recovery judgement"),S->AutoRun.CanRecover(M,FPlatformTime::Seconds()).IsEmpty());TestFalse(TEXT("requires explicit check"),Web.Checked(*S));Web.CheckKey=Web.RecoveryKey(*S);Web.CheckToken=TEXT("proof");TestTrue(TEXT("receipt current"),Web.Checked(*S));
 auto O=Web.Describe(*S,FPlatformTime::Seconds());auto R=O->GetObjectField(TEXT("recovery"))->GetArrayField(TEXT("materials"))[0]->AsObject();TestEqual(TEXT("connector + weighing already passed"),R->GetNumberField(TEXT("already_passed_kg")),.005);TestEqual(TEXT("separate additional supply"),R->GetNumberField(TEXT("additional_supply_kg")),.001);TestEqual(TEXT("in-flight separately shown"),R->GetNumberField(TEXT("transit_kg")),.003);
 ++P.Sequence;TestTrue(TEXT("new paused publication alone does not invalidate proof"),Web.Checked(*S));P.PhysicalSeconds+=.01;TestFalse(TEXT("motion invalidates prior proof"),Web.Checked(*S));P.PhysicalSeconds-=.01;++P.RequestedControlRevision;TestFalse(TEXT("manual request invalidates proof"),Web.Checked(*S));--P.RequestedControlRevision;
 P.Recovery.ForeignIds.Add(991);P.Recovery.ForeignSuppliers.Add(1);P.Recovery.ForeignRegions.Add(TEXT("weigher"));TestFalse(TEXT("sticky foreign evidence blocks"),Web.Checked(*S));TestTrue(TEXT("actual blocker exposed"),Web.Reason(TEXT("batch_resume"),*S,FPlatformTime::Seconds()).Contains(TEXT("991")));
 P.Recovery.ForeignIds.Reset();P.Recovery.ForeignSuppliers.Reset();P.Recovery.ForeignRegions.Reset();++P.Epoch;TestFalse(TEXT("reset cannot resume old batch"),Web.Checked(*S));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWebRecoveryAcknowledgement,"ProcessWeb.Batch.WorkerRequestIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWebRecoveryAcknowledgement::RunTest(const FString&)
{
 auto* S=Service();FProcessWebBatch Web;auto& A=S->AutoRun.State;auto& P=S->State.Snapshot;A.RunId=FGuid::NewGuid();A.Active=true;
 ProcessWebCommands::FResult R;R.Command.Kind=TEXT("batch_resume");R.BatchId=A.RunId.ToString(EGuidFormats::DigitsWithHyphens);R.Record.Epoch=7;R.Record.Connection=3;R.Record.AcceptedAt=100;R.Record.Status=EProcessCommandStatus::Processing;R.RecoveryRevision=5;
 P.Recovery.Valid=true;P.Recovery.AcceptedRevision=6;P.Recovery.RequestId=FGuid::NewGuid();Web.Observe(*S,R);TestTrue(TEXT("revision alone is not current request proof"),R.Record.IsPending());
 FProcessCommandRecord Worker;Worker.RequestId=P.Recovery.RequestId;Worker.Command=EProcessCommand::RecoverBatch;Worker.Epoch=7;Worker.Connection=3;Worker.Revision=6;Worker.Status=EProcessCommandStatus::Applied;Worker.AcceptedAt=99;S->State.History.Add(Worker.RequestId,Worker);Web.Observe(*S,R);TestTrue(TEXT("previous admission response cannot confirm new resume"),R.Record.IsPending());
 S->State.History[Worker.RequestId].AcceptedAt=101;Web.Observe(*S,R);TestEqual(TEXT("matching new request plus worker approval"),R.Record.Status,EProcessCommandStatus::Applied);TestEqual(TEXT("service revision retained"),R.Record.Revision,int64(6));
 R.Record.Status=EProcessCommandStatus::Processing;P.Epoch=8;Web.Observe(*S,R);TestEqual(TEXT("reset invalidates pending resume"),R.Record.Status,EProcessCommandStatus::Failed);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWebBatchReplay,"ProcessWeb.Batch.ReplayAndStopPrecedence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWebBatchReplay::RunTest(const FString&)
{
 auto* S=Service();ProcessWebCommands::FBridge Bridge;auto Frame=MakeShared<FJsonObject>();Frame->SetStringField(TEXT("run_id"),TEXT("cpu-run"));Frame->SetStringField(TEXT("epoch"),TEXT("7"));Frame->SetStringField(TEXT("lease_id"),S->WebLeaseId);Frame->SetNumberField(TEXT("lease_remaining_ms"),3000);
 auto Make=[&](const TCHAR* Kind){auto C=MakeShared<FJsonObject>();C->SetNumberField(TEXT("command_version"),1);C->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));C->SetStringField(TEXT("run_id"),TEXT("cpu-run"));C->SetStringField(TEXT("epoch"),TEXT("7"));C->SetStringField(TEXT("lease_id"),S->WebLeaseId);C->SetStringField(TEXT("equipment"),TEXT("plant"));C->SetStringField(TEXT("kind"),Kind);C->SetStringField(TEXT("unit"),TEXT("none"));C->SetNumberField(TEXT("value"),0);C->SetStringField(TEXT("recipe_json"),ProcessRecipe::Encode(Recipe()));C->SetStringField(TEXT("recipe_revision"),LexToString(S->RecipeRevision));C->SetStringField(TEXT("recipe_fingerprint"),FProcessWebBatch::RecipeFingerprint(*S));return C;};
 auto Apply=Make(TEXT("recipe_apply"));Frame->SetArrayField(TEXT("commands"),{MakeShared<FJsonValueObject>(Apply)});Bridge.Receive(Frame,TEXT("cpu-run"),*S,0);TestEqual(TEXT("first config applies"),S->RecipeRevision,int64(1));Bridge.Receive(Frame,TEXT("cpu-run"),*S,0);TestEqual(TEXT("same ID never re-applies"),S->RecipeRevision,int64(1));TestEqual(TEXT("config has no device requests"),S->State.History.Num(),0);
 auto Next=Make(TEXT("recipe_apply")),Stop=Make(TEXT("stop_all"));Frame->SetArrayField(TEXT("commands"),{MakeShared<FJsonValueObject>(Next),MakeShared<FJsonValueObject>(Stop)});Bridge.Receive(Frame,TEXT("cpu-run"),*S,0);TestEqual(TEXT("stop cancels later mutation regardless of input order"),S->RecipeRevision,int64(1));return true;
}

#endif
