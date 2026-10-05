#include "../source/libgp200/GP200FlexibleRouting.h"
#include <cassert>
#include <iostream>
int main(){
std::array<int,11> order{0,1,2,3,4,5,6,7,8,9,10};int tests=0;
for(int s=0;s<=11;s++)for(int p=s;p<=11;p++)for(int r=p;r<=11;r++){
assert(gp200::validFlexibleRouting(order,s,p,r));
for(int mode : {0x80,0x90}){auto bytes=gp200::flexibleRoutingModeMessage(mode|p);
assert(bytes.size()==46&&bytes[0]==0xf0&&bytes[45]==0xf7);
assert((bytes[41]<<4|bytes[42])==(mode|p));
for(int i=1;i<45;i++)assert(bytes[i]<128);
++tests;}}
order[0]=1;assert(!gp200::validFlexibleRouting(order,2,5,8));
order[0]=0;assert(!gp200::validFlexibleRouting(order,5,2,8));assert(!gp200::validFlexibleRouting(order,0,12,12));
auto par=gp200::flexibleRoutingModeMessage(0),ser=gp200::flexibleRoutingModeMessage(1);
int changes=0;for(int i=0;i<46;i++)if(par[i]!=ser[i]){assert(i==42);changes++;}assert(changes==1);
std::cout<<tests<<" mode packets passed\n";
}
