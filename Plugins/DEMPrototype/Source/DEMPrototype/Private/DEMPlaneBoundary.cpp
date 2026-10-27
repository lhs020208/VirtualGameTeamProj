#include "DEMPlaneBoundary.h"
#include "DEMWorld.h"
#include "DEMCoordinates.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include <cmath>
UDEMPlaneBoundaryComponent::UDEMPlaneBoundaryComponent(){PrimaryComponentTick.bCanEverTick=false;}
bool UDEMPlaneBoundaryComponent::Snapshot(dem::Plane& F,FString& Error) const
{
 const FTransform T=GetComponentTransform();
 if(BoundaryID<0 || !std::isfinite(Width)||!std::isfinite(Height)||Width<=0||Height<=0||Width>20000||Height>20000||
    T.ContainsNaN()||!T.GetScale3D().Equals(FVector::OneVector,1e-8)||!T.GetRotation().IsNormalized())
 {Error=TEXT("Boundary ID/size/transform invalid: positive finite width/height <=20000m and unit world Scale required.");return false;}
 F.id=uint32(BoundaryID);F.center=DEMCoordinates::PositionToDEM(T.GetLocation());
 F.u=DEMCoordinates::PolarToDEM(T.GetRotation().RotateVector(FVector::ForwardVector));
 // Flip the second tangent so cross(u,v) equals the polar-transformed local +Z normal.
 F.v=DEMCoordinates::PolarToDEM(T.GetRotation().RotateVector(-FVector::RightVector));
 F.halfU=Width*.5;F.halfV=Height*.5;F.measureSupport=bMeasureSupport;return true;
}
void UDEMPlaneBoundaryComponent::BeginPlay()
{
 Super::BeginPlay();FString Error;
 if(ADEMWorld* W=ADEMWorld::Resolve(GetWorld(),TargetSimulation,Error)) W->RegisterBoundary(this);
 else UE_LOG(LogTemp,Warning,TEXT("DEM boundary %s: %s"),*GetPathName(),*Error);
}
ADEMPlaneBoundary::ADEMPlaneBoundary()
{
 PrimaryActorTick.bCanEverTick=false;Boundary=CreateDefaultSubobject<UDEMPlaneBoundaryComponent>(TEXT("DEMFiniteFace"));SetRootComponent(Boundary);
 Outline=CreateDefaultSubobject<UBoxComponent>(TEXT("FaceOutline"));Outline->SetupAttachment(Boundary);Outline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Outline->SetCanEverAffectNavigation(false);Outline->SetHiddenInGame(false);Outline->ShapeColor=FColor::Cyan;
 Normal=CreateDefaultSubobject<UArrowComponent>(TEXT("NormalPlusZ"));Normal->SetupAttachment(Boundary);Normal->SetRelativeRotation(FRotator(90,0,0));Normal->SetHiddenInGame(true);
}
void ADEMPlaneBoundary::OnConstruction(const FTransform& T)
{
 Super::OnConstruction(T);
 if(FMath::IsFinite(Boundary->Width)&&FMath::IsFinite(Boundary->Height))
 Outline->SetBoxExtent(FVector(FMath::Clamp(Boundary->Width,0.,20000.)*50,FMath::Clamp(Boundary->Height,0.,20000.)*50,.05));
}
