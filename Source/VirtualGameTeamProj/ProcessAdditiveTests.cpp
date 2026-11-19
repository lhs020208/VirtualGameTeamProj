#include "PlantSupplyABatch.h"
#include "ProcessControlTypes.h"
#include "powder/plant_pressure.hpp"
#include "powder/plant_feed_brake.hpp"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
// Synthetic coordinates exercise the real worker ledger, not particle dynamics.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdditiveLedger,"ProcessUI.Additive.WorkerCohortsAndReservationModes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAdditiveLedger::RunTest(const FString&){try{
 for(int Mode=0;Mode<3;++Mode){ // OFF initial, ON0 prepared first, ONpositive concurrent
  powder::GranularConfig C;C.plant=true;C.radius=.002f;C.count=9;
  const FString Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");
  C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
  FPlantTransferSettings Limits;FPlantTransferTelemetry Mass(C,Routes,Limits);FPlantFeedPreparation Prep(C,Routes,Mass,.5,true);
  FPlantSupplyABatch B(C,{0,0,0,1,1,1,2,2,2},Mass,Prep,true);const double M=Mass.ParticleMassKg;
  auto Gates=powder::plant_supplier_gates(C.plant_data);std::vector<powder::Grain> P(9);
  for(int I=0;I<9;++I){P[I]=C.plant_data[Gates[I/3]];P[I].z+=.15f+.01f*I;}
  TestTrue(TEXT("capture initial A with B and C inventory"),B.Capture(P,0,1));
  P[0]={0,0,1,0};B.Observe(P,.1);B.Observe(P,.61);TestTrue(TEXT("A dose"),B.FinalizeDose(P,.61,1,2,M,0));
  P[0]={3,0,2.7f,0};B.Observe(P,.7);TestTrue(TEXT("B begins after A receiver"),B.Transition(P,.7,1,4,false));
  P[3]={0,0,1,0};B.Observe(P,.8);B.Observe(P,1.31);TestTrue(TEXT("B dose"),B.FinalizeDose(P,1.31,4,5,M,0));
  P[3]={3,.02f,2.7f,0};B.Observe(P,1.5);TestTrue(TEXT("AB union"),B.Transition(P,1.5,4,7,true));
  TestTrue(TEXT("AB receiver opens"),B.BeginMixerFeed(P,1.5,7,8));P[0]={3,0,1.15f,0};P[3]={3,.02f,1.15f,0};B.Observe(P,1.6);B.Observe(P,2.11);
  B.ReceiverClosed(2.12,9);const int64 AHash=B.SequentialSnapshot().A.CohortHash,BHash=B.SequentialSnapshot().B.CohortHash;
  if(Mode==2){TestTrue(TEXT("concurrent preparation starts after mixing"),B.AdmitMixerStart(P,2.13,7,10));B.MixerApplied(2.13016,10,15);B.ObserveMix(2.13016,true,15);}
  TestTrue(TEXT("C preparation after AB complete and actual close"),B.BeginAdditive(P,2.14,7,11,Mode!=0,Mode==2?2:0));
  TestTrue(TEXT("duplicate does not replace cohort"),B.BeginAdditive(P,2.14,7,11,Mode!=0,Mode==2?2:0));
  TestEqual(TEXT("AB retained in mixer, excluded from C baseline"),B.MixerSnapshot().InitialMixerKg,2*M);
  P[6]={0,0,1,0};B.Observe(P,2.2);B.Observe(P,2.71);TestTrue(TEXT("C own dose"),B.FinalizeDose(P,2.71,11,12,M,0));
  P[6]={3,-.02f,2.7f,0};B.Observe(P,2.8);B.ObserveAdditiveReady(2.8,true);
  TestTrue(TEXT("C all RH ready"),B.SequentialSnapshot().AdditiveReady);TestEqual(TEXT("C source remains"),B.SequentialSnapshot().Additive.SourceCount,2);
  auto Changed=B;auto Wrong=P;Wrong[6]={0,0,1,0};Changed.Observe(Wrong,2.81);Changed.ObserveAdditiveReady(2.81,true);
  TestFalse(TEXT("changed waiting inventory invalidates readiness"),Changed.SequentialSnapshot().AdditiveReady);
  TestEqual(TEXT("readiness time invalidated"),Changed.SequentialSnapshot().AdditiveReadySeconds,-1.);
  double OpenTime=2.9;
  if(Mode==1){TestTrue(TEXT("ON0 mix only after C ready"),B.AdmitMixerStart(P,2.9,11,13));B.MixerApplied(2.90016,13,15);B.ObserveMix(2.90016,true,15);OpenTime=2.90016;}
  if(Mode==2){TestEqual(TEXT("C prep preserves actual AB mix clock"),B.InitialMixerSnapshot().MixerAppliedSimulationSeconds,2.13016);OpenTime=4.13016;}
  if(Mode==2){auto Late=B;Late.ObserveMix(OpenTime+.001,true,15);Late.ObserveAdditiveReady(OpenTime+.001,false);Late.ObserveAdditiveReady(OpenTime+.002,true);
   TestFalse(TEXT("worker rejects readiness acquired after due"),Late.BeginMixerFeed(P,OpenTime+.003,11,14));}
  if(Mode!=0){auto Early=B;if(Mode==2)Early.ObserveMix(OpenTime-.00001,true,15);TestFalse(TEXT("worker independently rejects early opening"),Early.BeginMixerFeed(P,OpenTime-.00001,11,14));}
  if(Mode==2)B.ObserveMix(OpenTime,true,15);
  TestTrue(TEXT("OFF opens before mix, ON opens at due"),B.BeginMixerFeed(P,OpenTime,11,14));
  P[6]={3,-.02f,1.15f,0};B.Observe(P,OpenTime+.1);B.Observe(P,OpenTime+.61);
  TestEqual(TEXT("only additive arrived mass"),B.MixerSnapshot().MixerKg,M);TestEqual(TEXT("AB not additional inflow"),B.MixerSnapshot().AdditionalInflowCount,0);
  TestTrue(TEXT("C stable all arrival"),B.MixerSnapshot().State==EPlantTransferState::Complete);
  B.ReceiverClosed(OpenTime+.62,15);
  if(Mode==0){TestTrue(TEXT("OFF starts mix only after C full arrival and actual closure"),B.AdmitMixerStart(P,OpenTime+.63,11,16));B.MixerApplied(OpenTime+.63016,16,15);}
  TestEqual(TEXT("A cohort unchanged"),B.SequentialSnapshot().A.CohortHash,AHash);TestEqual(TEXT("B cohort unchanged"),B.SequentialSnapshot().B.CohortHash,BHash);
  TestEqual(TEXT("C own selected mass"),B.SequentialSnapshot().Additive.FinalKg,M);
  B.InvalidateCapture();TestFalse(TEXT("reset clears reservation"),B.SequentialSnapshot().AdditiveActive);TestEqual(TEXT("reset retires original clock"),B.InitialMixerSnapshot().MixerAppliedRevision,int64(0));
  TestFalse(TEXT("reset rejects old C opening"),B.BeginMixerFeed(P,OpenTime+1,11,14));
 }
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdditivePath,"ProcessUI.Additive.CGeometryAndAirIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAdditivePath::RunTest(const FString&){try{
 const FString Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");
 auto Data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
 auto Gates=powder::plant_supplier_gates(Data);auto Path=powder::plant_supply_purge_path(Data,Routes,2);auto Gate=Data[Gates[2]];
 TestEqual(TEXT("C purge follows real C gate x"),Path.inlet.x,Gate.x);TestEqual(TEXT("C purge follows real C gate y"),Path.inlet.y,Gate.y);TestTrue(TEXT("C downstream only"),Path.inlet.z<Gate.z);
 const auto State=powder::plant_set_regulated_pressures(Data,Routes,{0,-500,0},0,0,false,false,false,{true,500,2});TestTrue(TEXT("C independent flow"),State.purge_m3s>0);
 for(size_t R=0;R<Routes.size();++R)for(size_t I=0;I<Routes[R].cells.size();++I){const float V=Data[Routes[R].cells[I]+2].x;
  if(R==Path.route&&I>=Path.first_cell)TestTrue(TEXT("C downstream air positive"),V>0);else TestEqual(TEXT("A B and all other cells remain zero"),V,0.f);}
 auto Brake=powder::plant_feed_brake(Data,2,.002f);
 for(int I=0;I<2;++I){auto Other=Data[Gates[I]];Other.z+=.5f;TestFalse(TEXT("C brake does not act on A or B"),powder::plant_feed_brake_contains(Other,Brake));}
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdditiveStop,"ProcessUI.Additive.StopConfirmsCAirAndBrake",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAdditiveStop::RunTest(const FString&){
 FProcessControlMachine M;FPlantSimulationSnapshot S;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;S.Epoch=1;S.Sequence=1;S.PublishedWallSeconds=100;S.RequestedSuppliers.Init(false,6);S.AppliedRPMValid=true;
 S.SupplyCBrake.Supported=S.SupplyCBrake.Valid=S.SupplyCBrake.Required=true;S.SupplyCPurge.Supported=S.SupplyCPurge.Valid=S.SupplyCPurge.AppliedEnabled=true;
 M.Connect(S);M.TimeoutPolicy=EProcessTimeoutPolicy::Freeze;int64 Rev=0;FProcessControlTransport Send=[&](EProcessCommand,const FString&,double,int64,int64& R,FString&){R=++Rev;return true;};
 M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::StopAll,0,100,Send);
 auto Tick=[&](){++S.Sequence;S.ProcessedControlRevision=Rev;M.Observe(S,100,Send);};Tick();TestTrue(TEXT("C air/brake prevent premature pause"),M.StopStage==EProcessStopStage::Closing);
 S.SupplyCBrake.Applied=true;Tick();TestTrue(TEXT("C air alone prevents pause"),M.StopStage==EProcessStopStage::Closing);
 S.SupplyCPurge.AppliedEnabled=false;Tick();TestTrue(TEXT("C both confirmed before pause"),M.StopStage==EProcessStopStage::Pausing);
 S.Execution=EPlantExecutionState::Paused;S.RequestedRunning=false;Tick();TestTrue(TEXT("worker pause confirmed"),M.StopStage==EProcessStopStage::Complete);return true;}
#endif
