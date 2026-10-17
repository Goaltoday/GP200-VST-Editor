#pragma once
// Exact MidiConnection method bodies are included by run_nam2_tests.py.
// Only ports, timing and unrelated state are modeled.
#include "../source/libgp200/GP200Constants.h"
#include "../source/libgp200/GP200SoundClone.h"
#include "../source/libgp200/GP200EffectDatabase.h"
#include "../source/libgp200/GP200ModSync.h"
#include "../source/libgp200/GP200EffectParamDatabase.h"
namespace gp200 {
using EffectParameters=std::array<float,effectParamCount>;
struct GP200PresetCodec{static juce::String blockNameForSlotIndex(int i){static const char* modules[]={"PRE","WAH","DST","AMP","NR","CAB","EQ","MOD","DLY","RVB","VOL"};return modules[i];}};
struct MidiConnection {
 mutable juce::CriticalSection stateLock;
 struct Output{std::vector<juce::MidiMessage> messages;void sendMessageNow(const juce::MidiMessage& m){messages.push_back(m);}}output;
 Output* midiOutput=&output;
 int routingStage=0;bool presetRestoreTransactionActive=false,currentPresetDataIsLive=true;
 juce::MemoryBlock currentPresetDecodedData;
 enum class IRUploadPhase{Idle,WaitingAfterPrepare,SendingChunks,WaitingBeforeCommit,WaitingAfterCommit};
 enum class SoundCloneUploadPhase{Idle,Busy};
 IRUploadPhase irUploadPhase=IRUploadPhase::Idle;SoundCloneUploadPhase soundCloneUploadPhase=SoundCloneUploadPhase::Idle;
 juce::String lastMessageText,irUploadStatusText,irUploadLabel,hot1FactoryAmpSourceFile,assignmentNamesStatusText;
 GP200IRUpload irUpload;int hot1FactoryAmpUploadIndex=-1,irUploadChunkIndex=0;bool hot2FactoryAmpRename=false;double irUploadNextActionMs=0;
 struct Query{int a=0,b=0,c=0;};std::vector<Query> pendingAssignmentNameQueries;Query currentAssignmentNameQuery;
 std::array<juce::String,userIRCount> userIRNames;std::array<juce::String,snapToneCount> snapToneNames;
 std::uint64_t assignmentNamesRevision=0;bool assignmentNameRequestInProgress=false;
 void sendNextAssignmentNameQuery(){}
 bool isFactoryAmpDestinationInactiveLocked(int)const;
 bool startFactoryAmpUpload(const juce::File&,int,const juce::String&);
 bool startFactoryAmpRename(int,const juce::String&);
 void processIRUpload();
 bool sceneRefreshPending=false;double remoteSceneDeadline=0;std::uint64_t presetRevision=0;
 bool sendParamChange(int,int,juce::uint32,float);
 static std::vector<juce::uint8> buildParamChange(int,int,juce::uint32,float);
 static std::vector<juce::uint8> nibbleEncode(const juce::uint8*,int);
 static std::array<juce::uint8,2> encodeDisplayValue(float);
 void updateCurrentPresetEffectParam(int,int,float);
 void updateCurrentPresetEffectId(int,juce::uint32);
};
}
