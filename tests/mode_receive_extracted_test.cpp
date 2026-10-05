
#include "../source/libgp200/GP200FlexibleRouting.h"
#include <cassert>
#include <iostream>
#include <vector>
namespace juce{using uint8=std::uint8_t;}
using gp200::routingModeResponse;
struct MidiConnection{int currentSlot=2,refreshes=0;bool currentPresetDataIsLive=false;struct Snapshot{int mode=-1,slot=2;int revision=0;}routingModeSnapshot;bool handleModSyncResponse(const juce::uint8*,int){return false;}void scheduleLivePresetRefresh(){refreshes++;}void parseGP200SysEx(const juce::uint8*,int);};
std::vector<std::uint8_t> reply(int v){std::vector<std::uint8_t>b{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,8,0,0,0};for(int x:{6,0,4,0,5,0,v,0}){b.push_back(x>>4);b.push_back(x&15);}b.push_back(0xf7);return b;}
void MidiConnection::parseGP200SysEx (const juce::uint8* data, int size)
{
    if (size < 15)
        return;

    const bool isGP200Header = data[0] == 0xF0 && data[1] == 0x21 && data[2] == 0x25 && data[3] == 0x7E &&
                               data[4] == 0x47 && data[5] == 0x50 && data[6] == 0x2D && data[7] == 0x32;

    if (!isGP200Header)
        return;

    if (handleModSyncResponse (data, size)) return;

    const auto routeMode = routingModeResponse (data, size);
    if (routeMode >= 0)
    {
        // The parameter notification carries no preset-slot identifier.
        // During a slot load, do not relabel a queued old reply as the new slot.
        // A later query after the complete live read will recover the current mode.
        if (currentSlot < 0 || !currentPresetDataIsLive) return;
        const bool changed = routingModeSnapshot.mode != routeMode || routingModeSnapshot.slot != currentSlot;
        routingModeSnapshot.mode = routeMode;
        routingModeSnapshot.slot = currentSlot;
        ++routingModeSnapshot.revision;
        if (changed) scheduleLivePresetRefresh ();
        return;
    }

}

int main(){MidiConnection m;auto b=reply(0x83);m.parseGP200SysEx(b.data(),b.size());assert(m.routingModeSnapshot.mode==-1&&m.refreshes==0);m.currentPresetDataIsLive=true;m.parseGP200SysEx(b.data(),b.size());assert(m.routingModeSnapshot.mode==0x83&&m.routingModeSnapshot.slot==2&&m.routingModeSnapshot.revision==1&&m.refreshes==1);m.parseGP200SysEx(b.data(),b.size());assert(m.routingModeSnapshot.revision==2&&m.refreshes==1);m.currentSlot=3;m.currentPresetDataIsLive=false;m.parseGP200SysEx(b.data(),b.size());assert(m.routingModeSnapshot.slot==2&&m.routingModeSnapshot.revision==2);m.currentPresetDataIsLive=true;b=reply(0x93);m.parseGP200SysEx(b.data(),b.size());assert(m.routingModeSnapshot.slot==3&&m.routingModeSnapshot.mode==0x93);std::cout<<"Actual native mode receive branch: loading-slot discard, device adoption, duplicate readback freshness and no duplicate refresh passed\n";}
