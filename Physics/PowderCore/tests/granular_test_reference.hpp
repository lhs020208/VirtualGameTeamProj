#pragma once
#include "powder/granular.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
using namespace powder;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
using V=std::array<float,3>;
V sub(V a,V b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
V mul(V a,float s){return {a[0]*s,a[1]*s,a[2]*s};}
V add(V a,V b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
float dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
V xyz(Grain a){return {a.x,a.y,a.z};}
// Independent O(N^2) Jacobi reference, deliberately away from any wall/rotor.
std::vector<Grain> reference(GranularConfig c,std::vector<Grain> p,const std::vector<Grain>& vel){
 auto old=p;for(unsigned i=0;i<c.count;++i){p[i].x+=vel[i].x*c.dt;p[i].y+=vel[i].y*c.dt;p[i].z+=(vel[i].z-9.81f*c.dt)*c.dt;}
 for(unsigned it=0;it<c.iterations;++it){auto next=p;
  for(unsigned i=0;i<c.count;++i){V correction{};for(unsigned j=0;j<c.count;++j)if(j!=i){auto delta=sub(xyz(p[i]),xyz(p[j]));float d2=dot(delta,delta);if(d2<4*c.radius*c.radius){float d=std::sqrt(d2);V normal=d>1.e-9f?mul(delta,1/d):V{i>j?1.f:-1.f,0,0};float overlap=2*c.radius-d;auto relative=sub(sub(xyz(p[i]),xyz(old[i])),sub(xyz(p[j]),xyz(old[j])));auto tangent=sub(relative,mul(normal,dot(relative,normal)));float length=std::sqrt(dot(tangent,tangent));correction=add(correction,mul(normal,overlap*.25f));if(length>1.e-9f)correction=sub(correction,mul(tangent,std::min(.25f,c.friction*overlap*.25f/length)));}}
   float length=std::sqrt(dot(correction,correction));if(length>c.radius*.35f)correction=mul(correction,c.radius*.35f/length);next[i].x+=correction[0];next[i].y+=correction[1];next[i].z+=correction[2];
  }p=next;
 }return p;
}
void compare(const std::vector<Grain>& a,const std::vector<Grain>& b,float tolerance){check(a.size()==b.size(),"count changed");for(unsigned i=0;i<a.size();++i){check(a[i].w==b[i].w,"group changed");for(float d:sub(xyz(a[i]),xyz(b[i])))check(std::abs(d)<=tolerance,"trajectory differs from reference");}}
