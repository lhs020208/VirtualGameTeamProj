#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ProcessWebSnapshot.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessWebProjection,"ProcessWeb.ReadOnly.Projection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessWebProjection::RunTest(const FString&)
{
 ProcessWebSnapshot::FContext C;C.PublisherId=TEXT("cpu-test-publisher");C.RunId=TEXT("cpu-test-run");C.PublicationSequence=1;
 FPlantSimulationSnapshot S;S.Epoch=9007199254740993LL;S.Sequence=10;S.PhysicalSeconds=2.5;S.PublishedWallSeconds=100.;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Paused;
 S.RequestedSuppliers.Init(false,6);S.InitialSupplierCounts.Init(2000,6);S.PressureControlEnabled=true;S.AppliedPressureValid=true;S.RequestedGaugePressurePa=S.AppliedGaugePressurePa=FVector(500,-500,0);
 S.WeighingMass.Kg=.0000001;S.WeighingMass.Validity=EPlantValueValidity::Valid;S.InWeighingHopper=1;S.AppliedRPMValid=true;
 FPlantSupplyValvePressure P;P.Supplier=0;P.Mapped=true;P.Valid=true;P.BeforeGaugePa=400;P.AfterGaugePa=350;P.DifferencePa=50;S.SupplyValvePressures.Add(P);
 FProcessAutoSnapshot A;A.Epoch=S.Epoch;
 auto Parse=[](const FString& Text){TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O);return O;};
 const FString Body=ProcessWebSnapshot::Serialize(C,&S,&A,100.);auto Root=Parse(Body);TestTrue(TEXT("JSON serializes"),Root.IsValid());if(!Root)return false;
 auto Snap=Root->GetObjectField(TEXT("snapshot"));TestEqual(TEXT("64bit epoch lossless"),Snap->GetStringField(TEXT("epoch")),TEXT("9007199254740993"));TestEqual(TEXT("paused is data, not disconnect"),Snap->GetStringField(TEXT("execution")),TEXT("paused"));
 const auto& E=Snap->GetArrayField(TEXT("equipment"));TestEqual(TEXT("six suppliers and three devices"),E.Num(),9);auto M=E[0]->AsObject()->GetObjectField(TEXT("metrics"));
 TestTrue(TEXT("closed valve clears previous estimate"),M->GetObjectField(TEXT("valve_difference"))->GetField<EJson::Null>(TEXT("value")).IsValid());
 TestEqual(TEXT("Pa gauge retained without UI rounding"),M->GetObjectField(TEXT("pressure_applied"))->GetNumberField(TEXT("value")),500.);
 TestEqual(TEXT("tiny mass retained"),E[6]->AsObject()->GetObjectField(TEXT("metrics"))->GetObjectField(TEXT("mass"))->GetNumberField(TEXT("value")),.0000001);
 auto Stale=Parse(ProcessWebSnapshot::Serialize(C,&S,&A,103.))->GetObjectField(TEXT("snapshot"));TestEqual(TEXT("wall freshness independent of simulation time"),Stale->GetStringField(TEXT("validity")),TEXT("stale"));
 TestTrue(TEXT("stale numeric values never emitted as zero"),Stale->GetArrayField(TEXT("equipment"))[6]->AsObject()->GetObjectField(TEXT("metrics"))->GetObjectField(TEXT("mass"))->GetField<EJson::Null>(TEXT("value")).IsValid());
 auto Initial=S;Initial.Validity=EPlantValueValidity::Initializing;auto InitialJson=Parse(ProcessWebSnapshot::Serialize(C,&Initial,&A,100.));TestTrue(TEXT("unconfirmed simulation time is null"),InitialJson->GetObjectField(TEXT("snapshot"))->GetField<EJson::Null>(TEXT("simulation_seconds")).IsValid());
 auto Waiting=Parse(ProcessWebSnapshot::Serialize(C,nullptr,nullptr,103.));TestTrue(TEXT("unbound clears all previous data"),Waiting->GetField<EJson::Null>(TEXT("snapshot")).IsValid());
 TestTrue(TEXT("read-only capability"),Root->GetObjectField(TEXT("control"))->GetBoolField(TEXT("web_read_only")));
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("ProcessWebTests");IFileManager::Get().MakeDirectory(*Dir,true);TestTrue(TEXT("cross-language contract fixture saved"),FFileHelper::SaveStringToFile(Body,*(Dir/TEXT("serialized.json"))));return true;
}
#endif
