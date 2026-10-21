#include "PowderMeshGeometry.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include <map>
#include <stdexcept>
powder::StaticTriangleMesh ExtractDEMStaticMesh(const UStaticMeshComponent& Component,FVector Origin,double MinDiagonal,double MaxDiagonal){
 UStaticMesh* Mesh=Component.GetStaticMesh();
 if(!Mesh||!Mesh->GetRenderData()||Mesh->GetRenderData()->LODResources.Num()==0)throw std::invalid_argument("analysis mesh has no rendered LOD0");
#if !WITH_EDITOR
 if(!Mesh->bAllowCPUAccess)throw std::invalid_argument("analysis mesh needs CPU access in packaged builds");
#endif
 const auto& LOD=Mesh->GetRenderData()->LODResources[0];const auto& Positions=LOD.VertexBuffers.PositionVertexBuffer;const auto& Indices=LOD.IndexBuffer;
 if(Indices.GetNumIndices()==0||Indices.GetNumIndices()%3!=0||Indices.GetNumIndices()/3>200000)throw std::invalid_argument("invalid analysis triangle count");
 const auto Transform=Component.GetComponentTransform();const auto Scale=Transform.GetScale3D();if(Scale.X==0||Scale.Y==0||Scale.Z==0)throw std::invalid_argument("zero analysis mesh scale");
 powder::MeshInput Input;Input.closed_solid=true;Input.metres_per_unit=.01;Input.minimum_diagonal_m=MinDiagonal;Input.maximum_diagonal_m=MaxDiagonal;
 std::map<std::array<double,3>,std::size_t> Weld;
 // Rendering duplicates vertices at UV/normal seams. Weld exact local positions only: never bridge a gap.
 std::map<uint32,std::size_t> Remap;
 for(int32 I=0;I<Indices.GetNumIndices();++I){uint32 Index=Indices.GetIndex(I);if(Index>=Positions.GetNumVertices())throw std::invalid_argument("render mesh index out of range");if(Remap.count(Index))continue;auto P=Positions.VertexPosition(Index);std::array<double,3> Key{P.X,P.Y,P.Z};auto Found=Weld.find(Key);if(Found!=Weld.end()){Remap[Index]=Found->second;continue;}FVector V=Transform.TransformPosition(FVector(P))-Origin;std::size_t NewIndex=Input.vertices.size();Weld.emplace(Key,NewIndex);Remap[Index]=NewIndex;Input.vertices.push_back({V.X,V.Y,V.Z});}
 for(int32 I=0;I<Indices.GetNumIndices();I+=3){std::array<std::size_t,3> T{Remap.at(Indices.GetIndex(I)),Remap.at(Indices.GetIndex(I+1)),Remap.at(Indices.GetIndex(I+2))};Input.triangles.push_back(T);}
 // Unreal render winding is not a solver convention. Normalize the entire closed shell, never individual faces.
 double Volume=0;auto OriginVertex=Input.vertices.front();for(auto T:Input.triangles){auto A=Input.vertices[T[0]],B=Input.vertices[T[1]],C=Input.vertices[T[2]];FVector AV(A.x-OriginVertex.x,A.y-OriginVertex.y,A.z-OriginVertex.z),BV(B.x-OriginVertex.x,B.y-OriginVertex.y,B.z-OriginVertex.z),CV(C.x-OriginVertex.x,C.y-OriginVertex.y,C.z-OriginVertex.z);Volume+=FVector::DotProduct(AV,FVector::CrossProduct(BV,CV));}if(Volume<0)for(auto& T:Input.triangles)std::swap(T[1],T[2]);
 return powder::StaticTriangleMesh(Input);
}
