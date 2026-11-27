#include "PlantMixerObstacles.h"
#include "powder/plant_mixer_obstacle.hpp"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void ConfigureFixedMixerObstacles(UWorld* World,powder::GranularConfig& Config)
{
 std::vector<powder::MixerObstacleBox> Boxes;
 for(TActorIterator<AActor> It(World);It;++It){
  if(!It->ActorHasTag(TEXT("MixerFixedObstacle"))&&!It->ActorHasTag(TEXT("MixerObstructionTest")))continue;
  auto* Component=It->GetComponentByClass<UStaticMeshComponent>();
  if(!Component||!Component->GetStaticMesh()||
     Component->GetStaticMesh()->GetPathName()!=TEXT("/Engine/BasicShapes/Cube.Cube")||
     Component->Mobility!=EComponentMobility::Static||Component->IsSimulatingPhysics())
   throw std::runtime_error("MixerFixedObstacle requires a fixed Engine/BasicShapes/Cube (not a free rigid body)");
  const auto T=Component->GetComponentTransform();
  const auto Bounds=Component->GetStaticMesh()->GetBounds();
  const FVector Centre=T.TransformPosition(Bounds.Origin),Half=Bounds.BoxExtent*T.GetScale3D().GetAbs()/100.;
  powder::MixerObstacleBox Box;
  // Unreal cm / left-handed Y -> solver metres / right-handed Y.
  Box.centre={Centre.X/100.,-Centre.Y/100.,Centre.Z/100.};Box.half={Half.X,Half.Y,Half.Z};
  const FVector Basis[]{FVector::XAxisVector,FVector::YAxisVector,FVector::ZAxisVector};
  for(int I=0;I<3;++I){auto Axis=T.GetRotation().RotateVector(Basis[I]);Box.axes[I]={Axis.X,-Axis.Y,Axis.Z};}
  Boxes.push_back(Box);
 }
 if(!Boxes.empty()&&!Config.plant_powered_mixer)throw std::runtime_error("Fixed mixer obstacles require the powered mixer drive");
 Config.mixer_stops=powder::build_mixer_obstacle_stops(Config.plant_data,Boxes);
 if(!Boxes.empty())UE_LOG(LogTemp,Display,TEXT("Mixer fixed obstacles: %d; contact limits enabled=%d, angle=[%.6f, %.6f] rad"),
  int(Boxes.size()),int(Config.mixer_stops.enabled),Config.mixer_stops.lower_rad,Config.mixer_stops.upper_rad);
}
