from pathlib import Path
import subprocess,tempfile
source=(Path(__file__).resolve().parent.parent/'source/libgp200/MidiConnection.cpp').read_text()
a=source.index('void MidiConnection::handleIncomingMidiMessage')
b=source.index('bool MidiConnection::handleRoutingChangeNotification',a)
methods=source[a:b]
h=r'''
#include <cassert>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>
static int depth=0,formatCalls=0;
namespace juce {
using uint8=unsigned char;template<class T>T jmin(T x,T y){return std::min(x,y);}
struct String:std::string{using std::string::string;using std::string::operator=;
 static String toHexString(int n){assert(depth==0);++formatCalls;std::ostringstream s;s<<std::hex<<n;return String(s.str());}
 String(std::string s):std::string(s){}String paddedLeft(char c,int n){return String(std::string(std::max(0,n-int(size())),c)+*this);}
 String toUpperCase(){String s=*this;for(auto& c:s)c=char(std::toupper(c));return s;}
 String& operator<<(const char* x){append(x);return *this;}String& operator<<(const String& x){append(x);return *this;}
};
struct CriticalSection{};struct ScopedLock{ScopedLock(CriticalSection&){assert(depth==0);++depth;}~ScopedLock(){--depth;}};
struct MidiInput{};
struct MidiMessage{bool sysex=true;std::vector<uint8> data;bool isSysEx()const{return sysex;}
 const uint8* getSysExData()const{return data.data();}int getSysExDataSize()const{return int(data.size());}};
}
namespace gp200 {
struct MidiConnection{
 juce::CriticalSection stateLock;juce::String lastMessageText;std::vector<juce::uint8> parsed;int parseCalls=0;
 void handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage&);
 void handleIncomingSysEx(const juce::MidiMessage&);
 void parseGP200SysEx(const juce::uint8* p,int n){assert(depth==1);parsed.assign(p,p+n);++parseCalls;}
};
'''
main=r'''
}
int main(){gp200::MidiConnection c;int count=0;
 for(int n:{0,1,14,15,20,21,100,512,4096}){
  juce::MidiMessage m;m.data.resize(n);for(int i=0;i<n;i++)m.data[i]=(i*71)%128;
  c.handleIncomingMidiMessage(nullptr,m);assert(depth==0);assert(c.parsed.size()==size_t(n+2));
  assert(c.parsed.front()==0xf0&&c.parsed.back()==0xf7);assert(std::equal(m.data.begin(),m.data.end(),c.parsed.begin()+1));
  assert(c.lastMessageText.rfind("Received SysEx: F0 ",0)==0);++count;
 }
 juce::MidiMessage m;m.sysex=false;auto before=c.parseCalls;c.handleIncomingMidiMessage(nullptr,m);
 assert(c.parseCalls==before&&depth==0&&c.lastMessageText=="Received non-SysEx MIDI message");
 std::cout<<count+1<<" production receive checks: identical bytes, formatting outside state lock, parsing inside lock\n";
}
'''
with tempfile.TemporaryDirectory() as d:
    cpp=Path(d)/'receive.cpp';cpp.write_text(h+methods+main)
    exe=Path(d)/'receive'
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
