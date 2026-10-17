#include "/workspace/scratch/2054cfd4f93a/effectbank/GP200-VST-Editor/source/libgp200/GP200ModSync.h"
#include "/workspace/scratch/2054cfd4f93a/effectbank/GP200-VST-Editor/source/libgp200/GP200EffectDatabase.h"
#include "/workspace/scratch/2054cfd4f93a/effectbank/GP200-VST-Editor/source/libgp200/GP200EffectParamDatabase.h"
#include <chrono>
#include <iostream>
int main(){gp200::GP200ModSync::reloadIfChanged();const auto stats=juce::File::statCalls,reads=juce::File::readCalls;std::size_t sink=0;auto start=std::chrono::steady_clock::now();for(int i=0;i<20000;++i){const auto* p=gp200::GP200EffectParamDatabase::findParamsForEffect(10,"PRE");sink+=p?std::size_t(p->count):0;auto n=gp200::GP200EffectDatabase::getEffectName(10,"DST");sink+=std::size_t(n.length());(void)gp200::GP200ModSync::getRevision();(void)gp200::GP200ModSync::isNamAmp(0x07000001);}auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();std::cout<<"{\"iterations\":20000,\"microseconds\":"<<us<<",\"file_stats\":"<<(juce::File::statCalls-stats)<<",\"file_reads\":"<<(juce::File::readCalls-reads)<<",\"sink\":"<<sink<<"}\n";}
