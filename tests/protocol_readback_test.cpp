#include "../source/libgp200/GP200FlexibleRouting.h"
#include <cassert>
#include <iostream>
#include <vector>
// Independent reproduction of the native serializer at V180 file 0x48C8D..0x48CEB:
// total decoded length and chunk offset are encoded in header bytes 9..12.
std::vector<std::uint8_t> nativeReply (int mode) {
 std::vector<std::uint8_t> b{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,8,0,0,0};
 for(int value : {6,0,4,0,5,0,mode,0}){b.push_back(value>>4);b.push_back(value&15);}b.push_back(0xf7);return b;
}
int main(){
for(int mode=0;mode<256;mode++){
 auto b=nativeReply(mode);assert(b.size()==30);
 assert(gp200::routingModeResponse(b.data(),b.size())==(gp200::validRoutingModeValue(mode)?mode:-1));
 auto full=gp200::flexibleRoutingModeMessage(mode);
 assert(gp200::routingModeResponse(full.data(),46)==(gp200::validRoutingModeValue(mode)?mode:-1));
 b[9]=16;assert(gp200::routingModeResponse(b.data(),b.size())==-1);
 b=nativeReply(mode);b[14]=7;assert(gp200::routingModeResponse(b.data(),b.size())==-1);
 b=nativeReply(mode);b[11]=1;assert(gp200::routingModeResponse(b.data(),b.size())==-1);
}
auto q=gp200::routingModeQueryMessage();assert(q[8]==0x11&&q[9]==16);assert(gp200::routingModeResponse(q.data(),46)==-1);
assert(gp200::routingModeResponse(nullptr,30)==-1);
std::cout<<"FIX4: 512 native short/full mode frames passed; wrong length, wrong path, nonzero offset and outgoing query rejected\n";
}
