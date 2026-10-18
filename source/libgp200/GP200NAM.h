#pragma once
#include "GP200IR.h"
#include <array>
namespace gp200 {
class GP200NAM final {
public:
 static constexpr unsigned weightBytes=7484, binding=0x44423084, ackMagic=0x3130484e, nameAckMagic=0x3230484e, genericAckMagic=0x3330484e;
 static juce::Result readWeights(const juce::File&, std::vector<juce::uint8>&);
 static GP200IRUpload query(int logicalIndex,unsigned nonce,bool names=false,bool generic=false);
 static GP200IRUpload upload(const std::vector<juce::uint8>&,int logicalIndex,unsigned nonce,const juce::String& name={},int physical=-1);
 struct Ack {unsigned nonce=0,status=0,id=0,physical=0;bool names=false,generic=false;};
 // Returns true for our protocol, including malformed/late replies, to prevent
 // stock parsers interpreting a NAM acknowledgement as another command.
 static bool decodeAck(const juce::uint8*,int,Ack&);
};
}
