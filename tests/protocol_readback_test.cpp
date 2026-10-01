#include "../source/libgp200/GP200FlexibleRouting.h"
#include <cassert>
#include <iostream>
#include <vector>
int main(){
for(int mode=0;mode<256;mode++){
 auto frame=gp200::flexibleRoutingModeMessage(mode);
 assert(gp200::routingModeResponse(frame.data(),46)==(gp200::validRoutingModeValue(mode)?mode:-1));
 std::vector<std::uint8_t> shortFrame(frame.begin(),frame.begin()+13);
 for(int b:{6,0,4,0,5,0,mode,0}){shortFrame.push_back(b>>4);shortFrame.push_back(b&15);}shortFrame.push_back(0xf7);
 assert(shortFrame.size()==30);
 assert(gp200::routingModeResponse(shortFrame.data(),30)==(gp200::validRoutingModeValue(mode)?mode:-1));
 shortFrame[14]=7;assert(gp200::routingModeResponse(shortFrame.data(),30)==-1);
}
auto q=gp200::routingModeQueryMessage();assert(q[8]==0x11&&gp200::routingModeResponse(q.data(),46)==-1);
assert(gp200::routingModeResponse(nullptr,30)==-1);
assert(gp200::routingModeIsParallel(0x83)&&!gp200::routingModeIsParallel(0x93));
std::cout<<"Protocol FIX3: 512 full/short mode replies, wrong paths, query and invalid data passed\n";
}
