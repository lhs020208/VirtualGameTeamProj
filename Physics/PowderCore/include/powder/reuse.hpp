#pragma once
#include "powder/spatial.hpp"
#include "powder/profile.hpp"
namespace powder {
// Sorted, contiguous contact histories. Double buffering keeps failed steps atomic.
template<class Key,class Value>class FlatHistory {
 using Entry=std::pair<Key,Value>;
 std::vector<Entry> data_;
public:
 auto find(const Key& k)const{auto it=std::lower_bound(data_.begin(),data_.end(),k,[](const Entry& e,const Key& key){return e.first<key;});return it!=data_.end()&&it->first==k?it:data_.end();}
 auto end()const{return data_.end();}
 std::size_t count(const Key& k)const{return find(k)!=end();}
 const Value& at(const Key& k)const{auto it=find(k);if(it==end())throw std::out_of_range("contact history key");return it->second;}
 void emplace(const Key& k,const Value& v){if(data_.empty()||data_.back().first<k){data_.emplace_back(k,v);return;}auto it=std::lower_bound(data_.begin(),data_.end(),k,[](const Entry& e,const Key& key){return e.first<key;});if(it==data_.end()||it->first!=k)data_.insert(it,{k,v});}
 void clear(){data_.clear();}
 std::size_t size()const{return data_.size();}
 std::size_t capacity_bytes()const{return data_.capacity()*sizeof(Entry);}
 void swap(FlatHistory& other)noexcept{data_.swap(other.data_);}
};
class ClumpNeighbors {
 std::vector<Particle> reference_,expanded_;
 std::vector<std::size_t> owner_;
 std::vector<NodePair> pairs_;
 double skin_=0;
 bool valid_=false;
public:
 void invalidate(){valid_=false;}
 std::size_t capacity_bytes()const{return (reference_.capacity()+expanded_.capacity())*sizeof(Particle)+owner_.capacity()*sizeof(std::size_t)+pairs_.capacity()*sizeof(NodePair);}
 const std::vector<NodePair>& query(const std::vector<Particle>& nodes,const std::vector<std::size_t>& owner,bool grid,bool reuse,ClumpProfile* profile){
  bool hit=grid&&reuse&&valid_&&nodes.size()==reference_.size()&&owner==owner_;
  if(hit)for(std::size_t i=0;i<nodes.size();++i){const auto& a=nodes[i];const auto& b=reference_[i];
   if(a.id!=b.id||a.diameter_m!=b.diameter_m||std::hypot(a.position_m.x-b.position_m.x,a.position_m.y-b.position_m.y,a.position_m.z-b.position_m.z)>skin_*.25){hit=false;break;}}
  if(!hit){
   valid_=false;
   if(grid&&reuse&&!nodes.empty()){
    double smallest=nodes.front().diameter_m;for(const auto& n:nodes)smallest=std::min(smallest,n.diameter_m);
    skin_=.2*smallest;expanded_=nodes;for(auto& n:expanded_)n.diameter_m+=skin_;
    auto candidates=clump_candidates(expanded_,owner,true);pairs_.clear();
    for(auto pair:candidates){const auto& a=nodes[pair.first];const auto& b=nodes[pair.second];if(std::hypot(a.position_m.x-b.position_m.x,a.position_m.y-b.position_m.y,a.position_m.z-b.position_m.z)<=(a.diameter_m+b.diameter_m)/2+skin_)pairs_.push_back(pair);}
    reference_=nodes;owner_=owner;valid_=true;
   }else{pairs_=clump_candidates(nodes,owner,grid);reference_.clear();owner_.clear();}
   if(profile)++profile->neighbor_rebuilds;
  }else if(profile)++profile->neighbor_reuses;
  if(profile)profile->neighbor_capacity_bytes=std::max<std::uint64_t>(profile->neighbor_capacity_bytes,capacity_bytes());
  return pairs_;
 }
};
}
