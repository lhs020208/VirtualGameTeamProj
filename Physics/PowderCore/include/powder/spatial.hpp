#pragma once
#include "powder/core.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <vector>
#include <stdexcept>
namespace powder {
using NodePair=std::pair<std::size_t,std::size_t>;
// Input indices, not particle IDs. Both snapshots are searched independently.
// Validated particles are supplied by ClumpWorld. Extreme coordinates fall back
// to exhaustive enumeration rather than narrowing the supported physical domain.
inline std::vector<NodePair> clump_candidates(const std::vector<Particle>& nodes,const std::vector<std::size_t>& owner,bool grid=true){
 if(nodes.size()!=owner.size())throw std::invalid_argument("candidate owner size mismatch");
 std::vector<NodePair> pairs;
 auto exhaustive=[&]{for(std::size_t i=0;i<nodes.size();++i)for(std::size_t j=i+1;j<nodes.size();++j)if(owner[i]!=owner[j])pairs.emplace_back(i,j);};
 if(!grid){exhaustive();return pairs;}
 if(nodes.size()<2)return pairs;
 double width=0;for(const auto& n:nodes)width=std::max(width,n.diameter_m);
 // 1% slack dwarfs quotient rounding at the bounded cell coordinates.
 width*=1.01;
 using Cell=std::array<long long,3>;
 struct Hash {std::size_t operator()(const Cell& c)const{std::size_t h=0;for(auto v:c)h^=std::hash<long long>{}(v)+std::size_t(0x9e3779b9)+(h<<6)+(h>>2);return h;}};
 std::vector<Cell> keys;keys.reserve(nodes.size());
 for(const auto& n:nodes){Cell key{};const double xyz[]={n.position_m.x,n.position_m.y,n.position_m.z};
  for(int d=0;d<3;++d){const double v=std::floor(xyz[d]/width);if(!std::isfinite(width)||width<=0||!std::isfinite(v)||std::abs(v)>1e12){exhaustive();return pairs;}key[d]=static_cast<long long>(v);}keys.push_back(key);}
 // Compact grids use flat linked buckets; sparse/extreme spans use hashing.
 // Bound memory by node count rather than allocating the entire empty domain.
 Cell lo=keys.front(),hi=lo;for(const auto& k:keys)for(int d=0;d<3;++d){lo[d]=std::min(lo[d],k[d]);hi[d]=std::max(hi[d],k[d]);}
 const double volume=double(hi[0]-lo[0]+1)*double(hi[1]-lo[1]+1)*double(hi[2]-lo[2]+1);
 if(volume<=16.*nodes.size()+64.){
  const auto nx=static_cast<std::size_t>(hi[0]-lo[0]+1),ny=static_cast<std::size_t>(hi[1]-lo[1]+1);
  const auto count=static_cast<std::size_t>(volume),none=nodes.size();
  std::vector<std::size_t> heads(count,none),links(nodes.size(),none);
  auto index=[&](const Cell& k){return static_cast<std::size_t>(k[0]-lo[0])+nx*(static_cast<std::size_t>(k[1]-lo[1])+ny*static_cast<std::size_t>(k[2]-lo[2]));};
  for(std::size_t i=0;i<nodes.size();++i){auto cell=index(keys[i]);links[i]=heads[cell];heads[cell]=i;}
  auto emit=[&](std::size_t i,std::size_t j){if(owner[i]!=owner[j])pairs.emplace_back(std::min(i,j),std::max(i,j));};
  for(std::size_t root=0;root<nodes.size();++root){const auto& c=keys[root];if(heads[index(c)]!=root)continue;
   for(auto i=root;i!=none;i=links[i])for(auto j=links[i];j!=none;j=links[j])emit(i,j);
   for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z){if(x<0||(x==0&&y<0)||(x==0&&y==0&&z<=0))continue;
    Cell other{c[0]+x,c[1]+y,c[2]+z};if(other[0]<lo[0]||other[0]>hi[0]||other[1]<lo[1]||other[1]>hi[1]||other[2]<lo[2]||other[2]>hi[2])continue;
    for(auto i=root;i!=none;i=links[i])for(auto j=heads[index(other)];j!=none;j=links[j])emit(i,j);
   }
  }
  std::sort(pairs.begin(),pairs.end());return pairs;
 }
 std::unordered_map<Cell,std::vector<std::size_t>,Hash> cells;cells.reserve(nodes.size());
 for(std::size_t i=0;i<nodes.size();++i)cells[keys[i]].push_back(i);
 // Visit each occupied cell and each unordered neighboring cell pair once.
 auto emit=[&](std::size_t i,std::size_t j){if(owner[i]!=owner[j])pairs.emplace_back(std::min(i,j),std::max(i,j));};
 for(const auto& bucket:cells){const auto& c=bucket.first;const auto& local=bucket.second;
  for(std::size_t a=0;a<local.size();++a)for(std::size_t b=a+1;b<local.size();++b)emit(local[a],local[b]);
  for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z){if(x<0||(x==0&&y<0)||(x==0&&y==0&&z<=0))continue;
   auto it=cells.find({c[0]+x,c[1]+y,c[2]+z});if(it!=cells.end())for(auto i:local)for(auto j:it->second)emit(i,j);
  }
 }
 std::sort(pairs.begin(),pairs.end());return pairs;
}
}
