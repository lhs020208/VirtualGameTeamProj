#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "ProcessControlTypes.h"
#include "powder/plant_feed_brake.hpp"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeedBrakeGPU,"ProcessUI.FeedBrake.GPUForceAndGateProtection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFeedBrakeGPU::RunTest(const FString&){try{
 const FString Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");
 powder::GranularConfig C;C.plant=true;C.plant_threaded=true;C.kernel_optimize=true;C.plant_wall_mode=5;C.radius=.002f;C.dt=.00001f;C.iterations=8;
 C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
 auto Gates=powder::plant_supplier_gates(C.plant_data);auto Gate=C.plant_data[Gates[0]],Other=C.plant_data[Gates[1]];auto Brake=powder::plant_feed_brake(C.plant_data,0,C.radius);Brake.enabled=true;
 TestTrue(TEXT("support face above unchanged closure exclusion"),Brake.region.z>Gate.z+.003+C.radius+.00002);
 TestFalse(TEXT("other supplier outside support volume"),powder::plant_feed_brake_contains({Other.x,Other.y,Other.z+.1f,0},Brake));
 powder::plant_set_manual_valves(C.plant_data,Routes,0,3,3,false,false);
 std::vector<powder::Grain> P{{Gate.x,Gate.y,Brake.region.z+.03f,0},{Gate.x,Gate.y,Gate.z,0},{Other.x,Other.y,Other.z+.1f,0}};
 std::vector<powder::Grain> V{{0,0,-.4f,0},{0,0,-.4f,0},{0,0,-.4f,0}};C.count=P.size();powder::GranularGPU GPU(C,P,V);
 GPU.set_plant_feed_brake(Brake);auto Before=GPU.snapshot(),BeforeV=GPU.velocity_snapshot();
 for(unsigned I=0;I<P.size();++I){TestEqual(TEXT("setter does not teleport"),Before[I].z,P[I].z);TestEqual(TEXT("setter does not reset velocity"),BeforeV[I].z,V[I].z);}
 TestFalse(TEXT("occupied gate protection still blocks close"),powder::plant_supplier_closure_clear(Gate,Before,C.radius));TestEqual(TEXT("diagnostic uses same occupancy predicate"),powder::plant_closure_occupancy(Gate,Before,C.radius),1u);
 GPU.advance(800,0,false);GPU.advance(800,0,false);auto After=GPU.snapshot(),AfterV=GPU.velocity_snapshot();
 TestEqual(TEXT("all IDs/count preserved"),After.size(),P.size());
 TestTrue(TEXT("finite drag brakes upstream grain through integration"),AfterV[0].z>-.01f&&After[0].z<Before[0].z);
 TestTrue(TEXT("grain already in gate continues downward"),After[1].z<Before[1].z-.004f);
 TestTrue(TEXT("other supply remains gravity driven"),AfterV[2].z<-.4f);
 TestTrue(TEXT("only empty gate admits close"),powder::plant_supplier_closure_clear(Gate,After,C.radius));
 std::vector<powder::Grain> Probe{{Gate.x,Gate.y,Gate.z+.0035f,0}};auto Open=GPU.inspect_boundaries(Probe,0,false,false);
 TestTrue(TEXT("open physical disk has no collision"),FMath::Abs(Open[0].position.z-Probe[0].z)<1.e-6f);
 powder::plant_set_manual_valves(C.plant_data,Routes,0,2,2,false,false);GPU.update_plant_field(C.plant_data);auto Closed=GPU.inspect_boundaries(Probe,0,false,false);
 TestTrue(TEXT("closed disk collision is actually uploaded"),Closed[0].position.z-Probe[0].z>.001f);
 powder::plant_set_manual_valves(C.plant_data,Routes,0,3,3,false,false);GPU.update_plant_field(C.plant_data);
 GPU.set_state(P,V);GPU.advance(16,0,false);TestTrue(TEXT("reset removes previous force boundary"),GPU.velocity_snapshot()[0].z<-.4f);
 auto Bad=Brake;Bad.response_time_s=0;bool Rejected=false;try{GPU.set_plant_feed_brake(Bad);}catch(const std::exception&){Rejected=true;}TestTrue(TEXT("invalid response time rejected"),Rejected);
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeedBrakeConfirm,"ProcessUI.FeedBrake.ConfirmedStop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFeedBrakeConfirm::RunTest(const FString&){
 FProcessControlMachine M;FPlantSimulationSnapshot S;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;S.Epoch=1;S.Sequence=1;S.PublishedWallSeconds=100;
 S.RequestedSuppliers.Init(false,6);S.AppliedRPMValid=true;S.SupplyABrake.Supported=true;S.SupplyABrake.Valid=true;S.SupplyABrake.Required=true;M.Connect(S);M.TimeoutPolicy=EProcessTimeoutPolicy::Freeze;M.PausePolicy=EProcessPausePolicy::CloseThenPause;M.PausedClosePolicy=EProcessPausedClosePolicy::StopAll;
 int64 Revision=0;FProcessControlTransport Send=[&](EProcessCommand,const FString&,double,int64,int64& R,FString&){R=++Revision;return true;};
 M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::StopAll,0,100,Send);
 auto Observe=[&](){++S.Sequence;S.ProcessedControlRevision=Revision;M.Observe(S,100,Send);};Observe();
 TestTrue(TEXT("closed flags alone cannot confirm unapplied brake"),M.StopStage==EProcessStopStage::Closing);
 S.SupplyABrake.Applied=true;S.AppliedSupplierMask=1;Observe();TestTrue(TEXT("brake cannot substitute for physical gate closure"),M.StopStage==EProcessStopStage::Closing);
 S.AppliedSupplierMask=0;S.SupplyABrake.Valid=false;Observe();TestTrue(TEXT("unknown brake rejected"),M.StopStage==EProcessStopStage::Closing);
 S.SupplyABrake.Valid=true;Observe();TestTrue(TEXT("actual gate plus applied holding force admits pause"),M.StopStage==EProcessStopStage::Pausing);
 S.Execution=EPlantExecutionState::Paused;S.RequestedRunning=false;Observe();TestTrue(TEXT("worker pause confirms whole stop"),M.StopStage==EProcessStopStage::Complete);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFeedBrakeDeadline,"ProcessUI.FeedBrake.ProtectedClosureDeadline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFeedBrakeDeadline::RunTest(const FString&){
 FProcessControlMachine M;FPlantSimulationSnapshot S;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Running;S.RequestedRunning=true;S.Epoch=1;S.Sequence=1;S.PublishedWallSeconds=100;
 S.RequestedSuppliers.Init(false,6);S.RequestedSuppliers[0]=true;S.AppliedSupplierMask=1;S.AppliedRPMValid=true;S.SupplyABrake.Supported=S.SupplyABrake.Valid=true;M.Connect(S);M.ManualGranted=true;M.TimeoutPolicy=EProcessTimeoutPolicy::Freeze;
 int64 Revision=0;FProcessControlTransport Send=[&](EProcessCommand,const FString&,double,int64,int64& R,FString&){R=++Revision;return true;};
 auto R=M.Submit(FGuid::NewGuid(),1,TEXT("supply-a"),EProcessCommand::Valve,0,100,Send);TestEqual(TEXT("protected A close uses existing whole-stop bound"),R.Deadline,130.);
 S.RequestedSuppliers[0]=false;S.SupplyABrake.Required=S.SupplyABrake.Applied=true;S.ProcessedControlRevision=R.Revision;S.Sequence=2;S.PublishedWallSeconds=111;M.Observe(S,111,Send);
 TestTrue(TEXT("fresh protected close remains pending after generic mailbox budget"),M.History[R.RequestId].IsPending());
 S.AppliedSupplierMask=0;S.Sequence=3;S.PublishedWallSeconds=112;M.Observe(S,112,Send);TestTrue(TEXT("actual closed snapshot confirms the same request"),M.History[R.RequestId].Status==EProcessCommandStatus::Applied);
 S.AppliedSupplierMask=1;S.RequestedSuppliers[0]=true;S.Sequence=4;S.PublishedWallSeconds=120;M.Observe(S,120,Send);
 auto Late=M.Submit(FGuid::NewGuid(),1,TEXT("supply-a"),EProcessCommand::Valve,0,120,Send);S.RequestedSuppliers[0]=false;S.Sequence=5;S.PublishedWallSeconds=151;S.ProcessedControlRevision=Late.Revision;M.Observe(S,151,Send);
 TestFalse(TEXT("protected wait still has a finite timeout"),M.History[Late.RequestId].IsPending());
 return true;
}
#endif
