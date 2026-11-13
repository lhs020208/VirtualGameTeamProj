#include "ProcessControlTypes.h"
#include "ProcessUIStateAdapter.h"
#include "PlantFeedPreparation.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/Paths.h"
#include "powder/plant_pressure.hpp"
#include <cstring>
#include <limits>
namespace {
struct FPurgeFixture {
 FProcessControlMachine M;double Now=100;int64 Revision=0;TArray<EProcessCommand> Sent;FProcessControlTransport Send;
 FPurgeFixture(){FPlantSimulationSnapshot S;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Running;S.Epoch=1;S.Sequence=1;S.PublishedWallSeconds=Now;
  S.RequestedRunning=true;S.RequestedSuppliers.Init(false,6);S.AppliedRPMValid=true;S.PressureControlEnabled=true;S.AppliedPressureValid=true;S.SupplyAPurge.Supported=S.SupplyAPurge.Valid=true;
  M.Connect(S);M.ManualGranted=true;M.TimeoutPolicy=EProcessTimeoutPolicy::Freeze;M.PausePolicy=EProcessPausePolicy::CloseThenPause;M.PausedClosePolicy=EProcessPausedClosePolicy::StopAll;
  Send=[this](EProcessCommand C,const FString&,double,int64,int64& R,FString&){Sent.Add(C);R=++Revision;return true;};}
 FProcessCommandRecord Request(EProcessCommand C,double V=0,const FString& Id=TEXT("supply-a"),FGuid RequestId=FGuid()){
  return M.Submit(RequestId.IsValid()?RequestId:FGuid::NewGuid(),M.Snapshot.Epoch,Id,C,V,Now,Send);}
 void Publish(){auto S=M.Snapshot;++S.Sequence;S.PublishedWallSeconds=Now;S.ProcessedControlRevision=Revision;S.RequestedControlRevision=Revision;M.Observe(S,Now,Send);}
};
void Load(powder::GranularConfig& C,std::vector<powder::PlantRoute>& Routes){
 const FString Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");
 C.plant=true;C.plant_threaded=true;C.kernel_optimize=true;C.plant_wall_mode=5;C.radius=.002f;C.dt=.00001f;C.iterations=8;
 C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPurgeModelTest,"ProcessUI.SupplyAPurge.ModelIsolationGPU",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPurgeModelTest::RunTest(const FString&){try{
 powder::GranularConfig C;std::vector<powder::PlantRoute> Routes;Load(C,Routes);auto& D=C.plant_data;const auto Path=powder::plant_supply_a_purge_path(D,Routes);
 TestEqual(TEXT("first downstream whole cell"),int32(Path.first_cell),12);TestEqual(TEXT("source z"),Path.inlet.z,2.2f);
 auto S=powder::plant_set_regulated_pressures(D,Routes,{0,-500,0},0,0,false,false,false,{true,500});
 TestTrue(TEXT("independent forward carrier"),S.purge.enabled&&S.purge_m3s>0);
 TestTrue(TEXT("WH regulator exhaust balances injected gas with closed outlet"),std::abs(S.weighing_regulator_m3s+S.purge_m3s)<1e-12&&S.transfer_m3s==0);
 for(size_t ri=0;ri<Routes.size();++ri)for(size_t ci=0;ci<Routes[ri].cells.size();++ci)
  TestTrue(TEXT("no source, gate, B-F or transfer field activation"),(ri==Path.route&&ci>=Path.first_cell)?D[Routes[ri].cells[ci]+2].x>0:D[Routes[ri].cells[ci]+2].x==0);
 auto Gates=powder::plant_supplier_gates(D);for(int Gate:Gates)TestEqual(TEXT("all physical supplier gates stay closed"),D[Gate+1].y,0.f);
 TestEqual(TEXT("WH physical gate stays closed"),D[powder::plant_weighing_gate(D)+1].y,0.f);
 const auto Prior=D;
 for(auto P:{powder::PlantSupplyAPurge{true,NAN},{true,-1},{true,1001},{true,0}}){bool Failed=false;try{powder::plant_set_regulated_pressures(D,Routes,{0,500,0},0,0,false,false,false,P);}catch(const std::invalid_argument&){Failed=true;}
  TestTrue(TEXT("invalid purge field update is atomic"),Failed&&std::memcmp(D.data(),Prior.data(),D.size()*sizeof(D[0]))==0);}
 for(int Conflict=0;Conflict<3;++Conflict){bool Failed=false;try{powder::plant_set_regulated_pressures(D,Routes,{0,-500,0},Conflict==0?1:0,Conflict==1?1:0,Conflict==2,false,false,{true,500});}catch(const std::invalid_argument&){Failed=true;}TestTrue(TEXT("gate/air/outlet conflicts rejected"),Failed);}
 const int Middle=Routes[Path.route].cells[Path.first_cell+22];auto A=D[Middle],B=D[Middle+1];
 std::vector<powder::Grain> Seeds{{-1.2f,1.4f,2.24f,0},{-1.2f,1.4f,2.215f,0},{-1.2f,1.4f,2.19f,0},{(A.x+B.x)/2,(A.y+B.y)/2,(A.z+B.z)/2,0}};
 C.count=unsigned(Seeds.size());powder::GranularGPU GPU(C,Seeds,std::vector<powder::Grain>(Seeds.size()));
 auto Air=GPU.plant_air_snapshot();auto Length=[](powder::Grain G){return std::sqrt(G.x*G.x+G.y*G.y+G.z*G.z);};
 TestEqual(TEXT("GPU no air above gate"),Length(Air[0]),0.f);TestEqual(TEXT("GPU no air between gate and new inlet"),Length(Air[1]),0.f);
 TestTrue(TEXT("GPU downward air below separate inlet"),Air[2].z<0);TestTrue(TEXT("GPU downstream A horizontal air"),Air[3].x>0&&Air[3].y<0);
 GPU.advance(16,0,false);auto V=GPU.velocity_snapshot();TestTrue(TEXT("GPU pressure drag moves downstream grain"),V[3].x>0&&V[3].y<0);
 auto Off=powder::plant_set_regulated_pressures(D,Routes,{0,-500,0},0,0,false,false,false,{});GPU.update_plant_field(D);GPU.set_state(Seeds,std::vector<powder::Grain>(Seeds.size()));
 for(auto G:GPU.plant_air_snapshot())TestEqual(TEXT("purge stop clears GPU field"),Length(G),0.f);
 TestTrue(TEXT("stopped target/flow zero"),!Off.purge.enabled&&Off.purge.gauge_pa==0&&Off.purge_m3s==0);
 AddInfo(FString::Printf(TEXT("A inlet=(-1.2,1.4,2.2)m; purge +500Pa / WH -500Pa; q=%.12g m3/s; actual GPU %hs"),S.purge_m3s,GPU.device().c_str()));
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPurgeProtocolTest,"ProcessUI.SupplyAPurge.ProtocolAndStop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPurgeProtocolTest::RunTest(const FString&){
 FPurgeFixture F;
 for(double V:{-1.,1001.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})TestTrue(TEXT("invalid input rejected"),F.Request(EProcessCommand::PurgePressure,V).Status==EProcessCommandStatus::Rejected);
 TestTrue(TEXT("B unsupported"),F.Request(EProcessCommand::PurgePressure,500,TEXT("supply-b")).Status==EProcessCommandStatus::Rejected);
 auto R=F.Request(EProcessCommand::PurgePressure,.5*PlantControl::PaPerKPa);F.Request(EProcessCommand::PurgePressure,500,TEXT("supply-a"),R.RequestId);TestEqual(TEXT("duplicate sent once"),F.Sent.Num(),1);
 auto& P=F.M.Snapshot.SupplyAPurge;P.RequestedGaugePa=P.AppliedGaugePa=500;F.Publish();TestTrue(TEXT("old apply revision insufficient"),F.M.History[R.RequestId].IsPending());
 P.AppliedRevision=R.Revision;F.Publish();TestTrue(TEXT("pressure confirmed"),F.M.History[R.RequestId].Status==EProcessCommandStatus::Applied);
 F.M.Snapshot.AppliedSupplierMask=1;TestTrue(TEXT("actual A closure required"),F.Request(EProcessCommand::PurgeEnabled,1).Status==EProcessCommandStatus::Rejected);F.M.Snapshot.AppliedSupplierMask=0;
 R=F.Request(EProcessCommand::PurgeEnabled,1);P.RequestedEnabled=P.AppliedEnabled=true;P.AppliedRevision=R.Revision;F.Publish();TestTrue(TEXT("enabled but zero flow not completed"),F.M.History[R.RequestId].IsPending());P.FlowM3s=.01;F.Publish();TestTrue(TEXT("worker gas flow confirmed"),F.M.History[R.RequestId].Status==EProcessCommandStatus::Applied);
 TestTrue(TEXT("A cannot open against purge"),F.Request(EProcessCommand::Valve,1).Status==EProcessCommandStatus::Rejected);
 TestTrue(TEXT("reverse pressure rejected"),F.Request(EProcessCommand::Pressure,500,TEXT("weigher")).Status==EProcessCommandStatus::Rejected);
 TestFalse(TEXT("stale inventory cannot look valid"),UProcessUIStateAdapter::BuildEquipment(TEXT("supply-a"),&F.M.Snapshot,F.Now+3).SupplyAPurge.Valid);
 const auto Stop=F.Request(EProcessCommand::StopAll,0,TEXT("plant"));F.Publish();TestTrue(TEXT("purge prevents early pause"),F.Sent.Last()==EProcessCommand::StopAll);TestTrue(TEXT("failed gas state named"),F.M.UnconfirmedEquipment().Contains(TEXT("연결부 공기")));
 P.RequestedEnabled=P.AppliedEnabled=false;P.RequestedGaugePa=P.AppliedGaugePa=0;F.Publish();TestTrue(TEXT("nonzero gas flow still prevents pause"),F.Sent.Last()==EProcessCommand::StopAll);
 P.FlowM3s=0;F.Publish();TestTrue(TEXT("zero purge permits pause request"),F.Sent.Last()==EProcessCommand::Pause);
 F.M.Snapshot.Execution=EPlantExecutionState::Paused;F.M.Snapshot.RequestedRunning=false;F.Publish();TestTrue(TEXT("stop completes after pause ack"),F.M.History[Stop.RequestId].Status==EProcessCommandStatus::Applied);
 TestTrue(TEXT("no queued purge while paused"),F.Request(EProcessCommand::PurgePressure,500).Status==EProcessCommandStatus::Rejected);
 F.Request(EProcessCommand::Resume,1,TEXT("plant"));F.M.Snapshot.Execution=EPlantExecutionState::Running;F.M.Snapshot.RequestedRunning=true;F.Publish();TestFalse(TEXT("resume does not restore purge"),F.M.Snapshot.SupplyAPurge.RequestedEnabled);
 FPurgeFixture G;auto Pending=G.Request(EProcessCommand::PurgePressure,500);auto Reset=G.M.Snapshot;++Reset.Epoch;Reset.Sequence=1;G.M.Observe(Reset,G.Now,G.Send);
 TestTrue(TEXT("reset invalidates old purge command"),G.M.History[Pending.RequestId].Status==EProcessCommandStatus::Unknown);
 TestTrue(TEXT("old epoch rejected"),G.M.Submit(FGuid::NewGuid(),1,TEXT("supply-a"),EProcessCommand::PurgeEnabled,1,G.Now,G.Send).Status==EProcessCommandStatus::Rejected);
 FPurgeFixture H;auto Cancelled=H.Request(EProcessCommand::PurgePressure,500);H.M.Snapshot.SupplyAPurge.CancelledRevision=Cancelled.Revision;H.Publish();TestTrue(TEXT("worker cancellation is terminal"),H.M.History[Cancelled.RequestId].Status==EProcessCommandStatus::Failed);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPurgeReadinessTest,"ProcessUI.SupplyAPurge.OpeningInterlock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPurgeReadinessTest::RunTest(const FString&){try{
 powder::GranularConfig C;std::vector<powder::PlantRoute> Routes;Load(C,Routes);C.count=1;FPlantTransferSettings Limits;
 FPlantTransferTelemetry Mass(C,Routes,Limits);FPlantFeedPreparation Prepare(C,Routes,Mass,.5,true);std::vector<powder::Grain> P{{0,0,1,0}};
 FPlantControlTargets T;T.Running=true;T.Revision=T.WeighingOpenRevision=2;T.WeighingOpen=true;T.WeighingOpenDeadline=100;T.WeighingOpenLeaseUntil=100;T.SupplyAPurgeEnabled=true;T.SupplyAPurgeGaugePa=500;
 TestFalse(TEXT("empty connector cannot admit while purge on"),Prepare.Evaluate(P,1,1,T,0,false,true,.01,500));TestTrue(TEXT("purge stop reason"),Prepare.Snapshot().State==EPlantFeedPreparationState::WaitingPurgeStop);
 T.SupplyAPurgeEnabled=false;T.SupplyAPurgeGaugePa=0;++T.Revision;TestFalse(TEXT("requested off insufficient"),Prepare.Evaluate(P,1.1,1.1,T,0,false,true,.01,500));
 TestFalse(TEXT("stable interval starts after actual off"),Prepare.Evaluate(P,1.2,1.2,T,0,false));
 TestFalse(TEXT("repeated snapshot no stability"),Prepare.Evaluate(P,1.2,1.5,T,0,false));TestEqual(TEXT("zero repeated stability"),Prepare.Snapshot().StableSeconds,0.);
 TestTrue(TEXT("unchanged composition after gas off admits"),Prepare.Evaluate(P,1.71,1.71,T,0,false));
 TestEqual(TEXT("live A inventory is empty"),Prepare.SupplyAInventory(P),0);
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;}
#endif
