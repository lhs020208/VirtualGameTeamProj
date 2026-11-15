#include "PlantSupplyABatch.h"
#include "ProcessControlTypes.h"
#include "powder/plant_pressure.hpp"
#include "powder/plant_feed_brake.hpp"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace {
struct FSequentialFixture {
 powder::GranularConfig C;std::vector<powder::PlantRoute> Routes;
 FSequentialFixture(){C.plant=true;C.radius=.002f;C.count=6;
  const FString Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");
  C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSequentialIDs,"ProcessUI.SequentialAB.MaterialIsolationAndUnion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSequentialIDs::RunTest(const FString&){try{
 FSequentialFixture F;FPlantTransferSettings Limits;FPlantTransferTelemetry Mass(F.C,F.Routes,Limits);FPlantFeedPreparation Prep(F.C,F.Routes,Mass,.5,true);
 FPlantSupplyABatch B(F.C,{0,0,0,1,1,1},Mass,Prep,true);const double M=Mass.ParticleMassKg;
 auto Gates=powder::plant_supplier_gates(F.C.plant_data);auto A=F.C.plant_data[Gates[0]],PB=F.C.plant_data[Gates[1]];
 std::vector<powder::Grain> P(6);for(int I=0;I<6;++I){P[I]=I<3?A:PB;P[I].z+=.15f+.01f*I;}
 TestTrue(TEXT("capture A with separate B inventory"),B.Capture(P,0,1));TestEqual(TEXT("B original source count"),B.SequentialSnapshot().B.SourceCount,3);
 P[0]={0,0,1,0};B.Observe(P,.1);B.Observe(P,.61);
 TestTrue(TEXT("A own target accepted"),B.FinalizeDose(P,.61,1,2,M,0));const int64 AHash=B.SequentialSnapshot().A.CohortHash;
 TestFalse(TEXT("B cannot start until A entirely RH"),B.Transition(P,.61,1,3,false));
 P[0]={3,0,2.7f,0};B.Observe(P,.7);
 TestTrue(TEXT("B starts with A held in RH"),B.Transition(P,.7,1,4,false));TestEqual(TEXT("active B"),B.ActiveSupplier(),1);
 TestEqual(TEXT("A not foreign for B"),B.Snapshot().ForeignCount,0);TestEqual(TEXT("B initial count excludes held A"),B.Snapshot().TotalCount,3);
 TestTrue(TEXT("duplicate beginB does not overwrite cohort"),B.Transition(P,.7,1,4,false));
 auto Escaped=B;auto Wrong=P;Wrong[0]={0,0,1,0};Escaped.Observe(Wrong,.8);TestEqual(TEXT("A escaping RH detected"),Escaped.Snapshot().ForeignCount,1);
 P[3]={0,0,1,0};P[4]={.02f,0,1,0};B.Observe(P,.8);B.Observe(P,1.31);
 auto OutsideTolerance=B;TestFalse(TEXT("B overfill cannot offset A"),OutsideTolerance.FinalizeDose(P,1.31,4,5,M,0));
 TestTrue(TEXT("B own target accepted"),B.FinalizeDose(P,1.31,4,5,2*M,0));
 Mass.Begin(P,1.31,6,false);FPlantSimulationSnapshot Snap;Mass.Publish(Snap);
 TestEqual(TEXT("B transfer initial RH excludes A"),Snap.Transfer.InitialReceiverKg,M);TestEqual(TEXT("B baseline only B"),Snap.Transfer.BaselineKg,2*M);
 P[3]={3,.02f,2.7f,0};P[4]={3,-.02f,2.7f,0};Mass.Observe(P,1.5,true,false);Mass.Publish(Snap);
 TestEqual(TEXT("arrival excludes A even same material colour"),Snap.Transfer.ArrivedKg,2*M);TestEqual(TEXT("no foreign influx"),Snap.Transfer.AdditionalInflowCount,0);
 B.Observe(P,1.5);TestEqual(TEXT("A IDs unchanged during B transfer"),B.SequentialSnapshot().A.CohortHash,AHash);
 TestTrue(TEXT("union after exact A and B RH"),B.Transition(P,1.5,4,7,true));TestEqual(TEXT("union actual masses"),B.Snapshot().TotalKg,3*M);TestEqual(TEXT("union stable IDs count"),B.Snapshot().TotalCount,3);
 TestTrue(TEXT("union receiver opening admitted"),B.BeginMixerFeed(P,1.5,7,8));
 P[0]={3,0,1.15f,0};P[3]={3,.02f,1.15f,0};P[4]={3,-.02f,1.15f,0};B.Observe(P,1.6);B.Observe(P,2.11);
 TestEqual(TEXT("A mixer count"),B.SequentialSnapshot().A.MixerCount,1);TestEqual(TEXT("B mixer count"),B.SequentialSnapshot().B.MixerCount,2);
 TestEqual(TEXT("zero extra"),B.MixerSnapshot().AdditionalInflowCount,0);TestTrue(TEXT("stable union complete"),B.MixerSnapshot().State==EPlantTransferState::Complete);
 TestFalse(TEXT("mix cannot start without actual RH close"),B.AdmitMixerStart(P,2.11,7,9));
 B.InvalidateCapture();TestEqual(TEXT("reset clears A cohort"),B.SequentialSnapshot().A.SelectedCount,0);TestFalse(TEXT("reset rejects old combine"),B.Transition(P,2.2,4,7,true));
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBPath,"ProcessUI.SequentialAB.BGeometryAndAirIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBPath::RunTest(const FString&){try{
 FSequentialFixture F;auto Gates=powder::plant_supplier_gates(F.C.plant_data);auto A=powder::plant_supply_purge_path(F.C.plant_data,F.Routes,0),B=powder::plant_supply_purge_path(F.C.plant_data,F.Routes,1);
 TestTrue(TEXT("independent routes"),A.route!=B.route);auto Gate=F.C.plant_data[Gates[1]];
 TestEqual(TEXT("B inlet follows B gate X"),B.inlet.x,Gate.x);TestEqual(TEXT("B inlet follows B gate Y"),B.inlet.y,Gate.y);TestTrue(TEXT("below B physical gate"),B.inlet.z<Gate.z);
 const auto State=powder::plant_set_regulated_pressures(F.C.plant_data,F.Routes,{0,-500,0},0,0,false,false,false,{true,500,1});
 TestTrue(TEXT("positive B downstream flow"),State.purge_m3s>0);
 for(size_t R=0;R<F.Routes.size();++R)for(size_t I=0;I<F.Routes[R].cells.size();++I){auto Flow=F.C.plant_data[F.Routes[R].cells[I]+2].x;
  if(R==B.route&&I>=B.first_cell)TestTrue(TEXT("only B downstream air"),Flow>0);else TestEqual(TEXT("A and upstream remain zero"),Flow,0.f);}
 auto BrakeA=powder::plant_feed_brake(F.C.plant_data,0,F.C.radius),BrakeB=powder::plant_feed_brake(F.C.plant_data,1,F.C.radius);
 TestFalse(TEXT("B brake does not act on A"),powder::plant_feed_brake_contains({BrakeA.region.x,BrakeA.region.y,3,0},BrakeB));
 auto Off=powder::plant_set_regulated_pressures(F.C.plant_data,F.Routes,{},0,0,false,false,false,{});TestEqual(TEXT("stop air flow zero"),Off.purge_m3s,0.);
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBStop,"ProcessUI.SequentialAB.StopConfirmsBothAirAndBrakes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBStop::RunTest(const FString&){
 FProcessControlMachine M;FPlantSimulationSnapshot S;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;S.Epoch=1;S.Sequence=1;S.PublishedWallSeconds=100;S.RequestedSuppliers.Init(false,6);S.AppliedRPMValid=true;
 S.SupplyBBrake.Supported=S.SupplyBBrake.Valid=S.SupplyBBrake.Required=true;S.SupplyBPurge.Supported=S.SupplyBPurge.Valid=S.SupplyBPurge.AppliedEnabled=true;
 M.Connect(S);M.TimeoutPolicy=EProcessTimeoutPolicy::Freeze;int64 Rev=0;FProcessControlTransport Send=[&](EProcessCommand,const FString&,double,int64,int64& R,FString&){R=++Rev;return true;};
 M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::StopAll,0,100,Send);
 auto Tick=[&](){++S.Sequence;S.ProcessedControlRevision=Rev;M.Observe(S,100,Send);};Tick();TestTrue(TEXT("B air and brake prevent premature stop"),M.StopStage==EProcessStopStage::Closing);
 S.SupplyBBrake.Applied=true;Tick();TestTrue(TEXT("B air alone prevents pause"),M.StopStage==EProcessStopStage::Closing);
 S.SupplyBPurge.AppliedEnabled=false;Tick();TestTrue(TEXT("both confirmed before pause"),M.StopStage==EProcessStopStage::Pausing);
 S.Execution=EPlantExecutionState::Paused;S.RequestedRunning=false;Tick();TestTrue(TEXT("worker paused"),M.StopStage==EProcessStopStage::Complete);return true;}
#endif
