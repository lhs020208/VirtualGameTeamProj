#pragma once
#include <cuda_runtime.h>
#include <math_constants.h>
#include <math.h>
namespace powder { namespace cuda_detail {
// float4 is the sole global-memory vector ABI (16 bytes, matching Grain).
template<class T,int N> struct Vec {
 T x,y,z,w;
 __host__ __device__ Vec():x(0),y(0),z(0),w(0){}
 __host__ __device__ explicit Vec(T a):x(a),y(a),z(a),w(a){}
 __host__ __device__ Vec(T a,T b):x(a),y(b),z(0),w(0){}
 __host__ __device__ Vec(T a,T b,T c):x(a),y(b),z(c),w(0){}
 __host__ __device__ Vec(T a,T b,T c,T d):x(a),y(b),z(c),w(d){}
 __host__ __device__ Vec(Vec<T,2> a,T b):x(a.x),y(a.y),z(b),w(0){}
 __host__ __device__ Vec(Vec<T,3> a,T b):x(a.x),y(a.y),z(a.z),w(b){}
 __device__ T& operator[](int i){return (&x)[i];}
 __device__ T operator[](int i)const{return (&x)[i];}
 __device__ Vec<T,2> xy()const{return Vec<T,2>(x,y);}
 __device__ Vec<T,2> yz()const{return Vec<T,2>(y,z);}
 __device__ Vec<T,2> zw()const{return Vec<T,2>(z,w);}
 __device__ Vec<T,3> xyz()const{return Vec<T,3>(x,y,z);}
 __device__ Vec<T,3> yzw()const{return Vec<T,3>(y,z,w);}
 __device__ void set_xyz(Vec<T,3> v){x=v.x;y=v.y;z=v.z;}
};
using float2=Vec<float,2>;using float3=Vec<float,3>;using float4=Vec<float,4>;
using int2=Vec<int,2>;using int3=Vec<int,3>;using int4=Vec<int,4>;using uint=unsigned;
static_assert(sizeof(float4)==16,"Grain ABI");
#define VEC_OP(OP) \
template<class T,int N> __device__ Vec<T,N> operator OP(Vec<T,N> a,Vec<T,N> b){Vec<T,N> r;for(int i=0;i<N;++i)r[i]=a[i] OP b[i];return r;} \
template<class T,int N,class S> __device__ Vec<T,N> operator OP(Vec<T,N> a,S b){return a OP Vec<T,N>(T(b));} \
template<class T,int N,class S> __device__ Vec<T,N> operator OP(S a,Vec<T,N> b){return Vec<T,N>(T(a)) OP b;}
VEC_OP(+) VEC_OP(-) VEC_OP(*)
#undef VEC_OP
__device__ float cl_div(float a,float b){float r;asm("div.full.f32 %0, %1, %2;":"=f"(r):"f"(a),"f"(b));return r;}
template<class T,int N> __device__ Vec<T,N> operator/(Vec<T,N>a,Vec<T,N>b){Vec<T,N>r;for(int i=0;i<N;++i)r[i]=cl_div(a[i],b[i]);return r;}
template<class T,int N,class S> __device__ Vec<T,N> operator/(Vec<T,N>a,S b){return a/Vec<T,N>(T(b));}
template<class T,int N,class S> __device__ Vec<T,N> operator/(S a,Vec<T,N>b){return Vec<T,N>(T(a))/b;}
#define VEC_ASSIGN(OP,BIN) template<class T,int N,class S> __device__ Vec<T,N>& operator OP(Vec<T,N>&a,S b){a=a BIN b;return a;}
VEC_ASSIGN(+=,+) VEC_ASSIGN(-=,-) VEC_ASSIGN(*=,*) VEC_ASSIGN(/=,/)
#undef VEC_ASSIGN
template<class T,int N> __device__ Vec<T,N> operator-(Vec<T,N>a){return Vec<T,N>(T(0))-a;}
#define VEC_CMP(OP) \
template<class T,int N> __device__ Vec<int,N> operator OP(Vec<T,N> a,Vec<T,N> b){Vec<int,N> r;for(int i=0;i<N;++i)r[i]=a[i] OP b[i];return r;} \
template<class T,int N,class S> __device__ Vec<int,N> operator OP(Vec<T,N>a,S b){return a OP Vec<T,N>(T(b));}
VEC_CMP(<) VEC_CMP(>) VEC_CMP(<=) VEC_CMP(>=) VEC_CMP(==) VEC_CMP(!=)
#undef VEC_CMP
template<int N> __device__ bool any(Vec<int,N> a){for(int i=0;i<N;++i)if(a[i])return true;return false;}
template<int N> __device__ bool all(Vec<int,N> a){for(int i=0;i<N;++i)if(!a[i])return false;return true;}
// Preserve NVIDIA OpenCL's dot association and scaled length/normalization.
// A plain sqrt(dot(a,a)) changed contact corrections and amplified velocity error.
template<int N> __device__ float dot(Vec<float,N>a,Vec<float,N>b){float s=__fmaf_rn(a[0],b[0],__fmul_rn(a[1],b[1]));for(int i=2;i<N;++i)s=__fmaf_rn(a[i],b[i],s);return s;}
template<int N> __device__ float length(Vec<float,N>a){
 float scale=0;for(int i=0;i<N;++i){a[i]=fabsf(a[i]);scale=fmaxf(scale,a[i]);}
 if(scale==0||isinf(scale))return scale;
 if constexpr(N==2){float t=cl_div(fminf(a.x,a.y),scale);return scale*sqrtf(__fmaf_rn(t,t,1.f));}
 float sum=0;for(int i=0;i<N;++i){float t=cl_div(a[i],scale);sum=__fmaf_rn(t,t,sum);}return scale*sqrtf(sum);
}
template<int N> __device__ Vec<float,N> normalize(Vec<float,N>a){
 float scale=0;for(int i=0;i<N;++i){if(isnan(a[i]))return Vec<float,N>(CUDART_NAN_F);scale=fmaxf(scale,fabsf(a[i]));}
 if(scale==0)return Vec<float,N>(0);if(isinf(scale))return a/scale;
 auto scaled=a/scale;float norm=sqrtf(dot(scaled,scaled)),denominator=scale*norm;
 return isinf(denominator)?(a/norm)/scale:a/denominator;
}
__device__ float3 cross(float3 a,float3 b){return float3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
#define VEC_UNARY(NAME,FN) \
__device__ float cl_##NAME(float a){return FN(a);} \
template<int N> __device__ Vec<float,N> cl_##NAME(Vec<float,N>a){for(int i=0;i<N;++i)a[i]=FN(a[i]);return a;}
VEC_UNARY(sqrt,sqrtf) VEC_UNARY(sin,sinf) VEC_UNARY(cos,cosf) VEC_UNARY(fabs,fabsf) VEC_UNARY(floor,floorf)
#undef VEC_UNARY
#define VEC_BINARY(NAME,FN) \
__device__ float cl_##NAME(float a,float b){return FN(a,b);} \
template<int N> __device__ Vec<float,N> cl_##NAME(Vec<float,N>a,Vec<float,N>b){for(int i=0;i<N;++i)a[i]=FN(a[i],b[i]);return a;}
VEC_BINARY(fmin,fminf) VEC_BINARY(fmax,fmaxf) VEC_BINARY(copysign,copysignf) VEC_BINARY(pow,powf)
#undef VEC_BINARY
__device__ float clamp(float a,float lo,float hi){return fminf(fmaxf(a,lo),hi);}
template<int N> __device__ Vec<float,N> clamp(Vec<float,N>a,Vec<float,N>lo,Vec<float,N>hi){for(int i=0;i<N;++i)a[i]=clamp(a[i],lo[i],hi[i]);return a;}
template<int N> __device__ Vec<int,N> isfinite(Vec<float,N>a){Vec<int,N>b;for(int i=0;i<N;++i)b[i]=::isfinite(a[i]);return b;}
__device__ bool isfinite(float a){return ::isfinite(a);}
__device__ int3 convert_int3(float3 a){return int3(int(a.x),int(a.y),int(a.z));}
__device__ float mix(float a,float b,float t){return a+(b-a)*t;}
__device__ unsigned get_global_id(int){return blockIdx.x*blockDim.x+threadIdx.x;}
__device__ int atomic_or(int*p,int v){return atomicOr(p,v);}
__device__ int atomic_xchg(int*p,int v){return atomicExch(p,v);}
} }
