#pragma once
// Test doubles only. No JUCE GUI/USB/DAW implementation is provided here.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
namespace juce {
inline constexpr int dontSendNotification=0;
using uint8=std::uint8_t; using uint32=std::uint32_t; using int64=long long;
class String {
public:
 std::string s;
 String()=default;String(const char* p):s(p?p:""){}String(std::string p):s(std::move(p)){}
 String(int v):s(std::to_string(v)){}String(int64 v):s(std::to_string(v)){}
 String(double v,int digits){std::ostringstream o;o<<std::fixed<<std::setprecision(digits)<<v;s=o.str();}
 bool isEmpty()const{return s.empty();}bool isNotEmpty()const{return !s.empty();}int length()const{return int(s.size());}void clear(){s.clear();}
 String trim()const{auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");return a==std::string::npos?String():String(s.substr(a,b-a+1));}
 String substring(int a)const{return s.substr(std::min(s.size(),size_t(a)));}String substring(int a,int b)const{return s.substr(size_t(a),size_t(b-a));}
 String toUpperCase()const{auto t=s;for(auto& c:t)c=char(std::toupper(static_cast<unsigned char>(c)));return t;}
 bool equalsIgnoreCase(String x)const{return toUpperCase()==x.toUpperCase();}
 bool startsWithIgnoreCase(String x)const{return toUpperCase().s.rfind(x.toUpperCase().s,0)==0;}
 bool containsOnly(String x)const{return s.find_first_not_of(x.s)==std::string::npos;}
 bool contains(String x)const{return s.find(x.s)!=std::string::npos;}
 uint32 getHexValue32()const{try{return uint32(std::stoull(s,nullptr,16));}catch(...){return 0;}}
 const char* toRawUTF8()const{return s.c_str();}
 static String fromUTF8(const char* p,int n){return std::string(p,strnlen(p,size_t(n)));}
 static String toHexString(int64 n){std::ostringstream o;o<<std::hex<<n;return o.str();}
 String paddedLeft(char c,int n)const{return n>length()?String(std::string(size_t(n-length()),c)+s):*this;}
 auto begin()const{return s.begin();}auto end()const{return s.end();}
 String& operator+=(String x){s+=x.s;return *this;}String& operator<<(String x){return *this+=x;}
 friend String operator+(String a,String b){a+=b;return a;}
 friend bool operator==(String a,String b){return a.s==b.s;}friend bool operator!=(String a,String b){return !(a==b);}
 friend bool operator<(String a,String b){return a.s<b.s;}
};
using Identifier=String;
template<class T> struct Array:std::vector<T>{using std::vector<T>::vector;void add(T v){this->push_back(std::move(v));}bool isEmpty()const{return this->empty();}int size()const{return int(std::vector<T>::size());}};
struct DynamicObject;
class var {
 using Object=std::shared_ptr<DynamicObject>;using List=std::shared_ptr<Array<var>>;
 std::variant<std::monostate,bool,int,int64,double,String,Object,List> data;
public:
 var()=default;var(bool v):data(v){}var(int v):data(v){}var(int64 v):data(v){}var(double v):data(v){}var(String v):data(std::move(v)){}var(const char* v):data(String(v)){}
 var(DynamicObject* v):data(Object(v)){}var(const Array<var>& v):data(std::make_shared<Array<var>>(v)){}
 bool isVoid()const{return std::holds_alternative<std::monostate>(data);}bool isInt()const{return std::holds_alternative<int>(data);}bool isInt64()const{return std::holds_alternative<int64>(data);}
 bool isBool()const{return std::holds_alternative<bool>(data);}bool isDouble()const{return std::holds_alternative<double>(data);}bool isString()const{return std::holds_alternative<String>(data);}
 DynamicObject* getDynamicObject()const{auto p=std::get_if<Object>(&data);return p?p->get():nullptr;}
 Array<var>* getArray()const{auto p=std::get_if<List>(&data);return p?p->get():nullptr;}
 explicit operator int64()const{if(auto p=std::get_if<int>(&data))return *p;if(auto p=std::get_if<int64>(&data))return *p;if(auto p=std::get_if<double>(&data))return int64(*p);return 0;}
 explicit operator int()const{return int(static_cast<int64>(*this));}
 explicit operator double()const{if(auto p=std::get_if<double>(&data))return *p;return double(static_cast<int64>(*this));}
 String toString()const{if(auto p=std::get_if<String>(&data))return *p;if(auto p=std::get_if<bool>(&data))return *p?"1":"0";if(isDouble()){std::ostringstream o;o<<std::get<double>(data);return o.str();}if(isInt()||isInt64())return String(static_cast<int64>(*this));return {};}
 var getProperty(Identifier key,var fallback)const;
};
struct DynamicObject {std::map<String,var> fields;void setProperty(Identifier key,var v){fields[key]=std::move(v);}var getProperty(Identifier key)const{auto i=fields.find(key);return i==fields.end()?var():i->second;}};
inline var var::getProperty(Identifier key,var fallback)const{auto p=getDynamicObject();if(!p)return fallback;auto i=p->fields.find(key);return i==p->fields.end()?fallback:i->second;}
struct JSON {
 struct Parser {std::string s;size_t at=0;void ws(){while(at<s.size()&&std::isspace(static_cast<unsigned char>(s[at])))++at;}char take(){if(at==s.size())throw std::runtime_error("json eof");return s[at++];}void expect(char c){ws();if(take()!=c)throw std::runtime_error("json token");}
 String str(){expect('"');std::string out;for(;;){char c=take();if(c=='"')return out;if(c=='\\'){c=take();switch(c){case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;case 'u':{unsigned code=0;for(int i=0;i<4;++i){char h=take();code=code*16+unsigned(std::isdigit(h)?h-'0':std::tolower(h)-'a'+10);}c=code<128?char(code):char(0xff);break;}default:break;}}out+=c;}}
 var val(){ws();if(at==s.size())throw std::runtime_error("json empty");char c=s[at];if(c=='"')return str();if(c=='{'){++at;auto o=std::make_unique<DynamicObject>();ws();if(at<s.size()&&s[at]=='}'){++at;return o.release();}for(;;){auto k=str();expect(':');o->setProperty(k,val());ws();if(take()=='}')return o.release();--at;expect(',');}}
 if(c=='['){++at;Array<var> a;ws();if(at<s.size()&&s[at]==']'){++at;return a;}for(;;){a.add(val());ws();if(take()==']')return a;--at;expect(',');}}
 if(s.compare(at,4,"null")==0){at+=4;return {};}if(s.compare(at,4,"true")==0){at+=4;return true;}if(s.compare(at,5,"false")==0){at+=5;return false;}
 size_t start=at;while(at<s.size()&&(std::isdigit(static_cast<unsigned char>(s[at]))||s[at]=='-'||s[at]=='+'||s[at]=='.'||s[at]=='e'||s[at]=='E'))++at;if(at==start)throw std::runtime_error("json value");auto n=s.substr(start,at-start);if(n.find_first_of(".eE")!=std::string::npos)return std::stod(n);auto x=std::stoll(n);return x>=INT32_MIN&&x<=INT32_MAX?var(int(x)):var(int64(x));}
 };
 static inline unsigned parseCalls=0;
 static var parse(String s){++parseCalls;try{Parser p{s.s};auto v=p.val();p.ws();return p.at==p.s.size()?v:var();}catch(...){return {};}}
 static std::string quote(String s){std::ostringstream o;o<<'"';for(unsigned char c:s.s){switch(c){case '"':o<<"\\\"";break;case '\\':o<<"\\\\";break;case '\n':o<<"\\n";break;case '\r':o<<"\\r";break;case '\t':o<<"\\t";break;default:if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c);else o<<char(c);}}o<<'"';return o.str();}
 static String toString(const var& v,bool=false){if(v.isVoid())return "null";if(auto p=v.getDynamicObject()){std::string s="{";for(auto& [k,x]:p->fields){if(s.size()>1)s+=',';s+=quote(k)+":"+toString(x).s;}return s+"}";}if(auto a=v.getArray()){std::string s="[";for(auto& x:*a){if(s.size()>1)s+=',';s+=toString(x).s;}return s+"]";}if(v.isString())return quote(v.toString());if(v.isBool())return v.toString()=="1"?"true":"false";return v.toString();}
};
struct Result {bool success;String error;static Result ok(){return {true,{}};}static Result fail(String s){return {false,s};}bool failed()const{return !success;}bool wasOk()const{return success;}String getErrorMessage()const{return error;}};
struct Time {int64 stamp=0;static inline double now=0;int64 toMilliseconds()const{return stamp;}static double getMillisecondCounterHiRes(){return now;}};
struct File {
 std::filesystem::path p;static inline unsigned statCalls=0,readCalls=0;static inline bool failWrites=false;static inline std::map<std::string,int64> stamps;static inline int64 nextStamp=1;
 enum SpecialLocationType{userDocumentsDirectory};File()=default;File(String s):p(s.s){}explicit File(std::filesystem::path s):p(std::move(s)){}
 static File getSpecialLocation(SpecialLocationType){return File(String(std::getenv("TEST_NAM2_DIR")));}
 File getChildFile(String x)const{return File(p/x.s);}File getParentDirectory()const{return File(p.parent_path());}
 bool existsAsFile()const{++statCalls;return std::filesystem::is_regular_file(p);}String loadFileAsString()const{++readCalls;std::ifstream f(p);return std::string(std::istreambuf_iterator<char>(f),{});}
 int64 getSize()const{++statCalls;return existsAsFile()?int64(std::filesystem::file_size(p)):0;}
 Time getLastModificationTime()const{++statCalls;auto key=p.string();auto i=stamps.find(key);return {i==stamps.end()?0:i->second};}
 Result createDirectory()const{std::error_code ec;std::filesystem::create_directories(p,ec);return ec?Result::fail(ec.message()):Result::ok();}
 bool replaceWithText(String x)const{if(failWrites)return false;std::ofstream f(p);f<<x.s;bool ok=bool(f);if(ok)stamps[p.string()]=nextStamp++;return ok;}
 String getFileName()const{return p.filename().string();}String getFileNameWithoutExtension()const{return p.stem().string();}
};
struct FileInputStream {std::ifstream f;explicit FileInputStream(const File& x):f(x.p,std::ios::binary){}bool openedOk()const{return bool(f);}int64 getTotalLength(){auto pos=f.tellg();f.seekg(0,std::ios::end);auto n=f.tellg();f.seekg(pos);return int64(n);}int read(void* p,int n){f.read(static_cast<char*>(p),n);return int(f.gcount());}};
struct CriticalSection{mutable std::recursive_mutex m;};struct ScopedLock{const CriticalSection& x;explicit ScopedLock(const CriticalSection& c):x(c){x.m.lock();}~ScopedLock(){x.m.unlock();}};
template<class T>T jlimit(T a,T b,T x){return std::clamp(x,a,b);}
template<class T>T jmin(T a,T b){return std::min(a,b);}template<class T>T jmax(T a,T b){return std::max(a,b);}inline int roundToInt(float v){return int(std::round(v));}template<class T>bool isPositiveAndBelow(T x,T n){return x>=0&&x<n;}
struct MemoryBlock {std::vector<uint8> v;size_t getSize()const{return v.size();}void setSize(size_t n){v.resize(n);}const void* getData()const{return v.data();}void* getData(){return v.data();}};
struct ByteOrder{static uint32 littleEndianInt(const void* p){auto b=static_cast<const uint8*>(p);return uint32(b[0])|(uint32(b[1])<<8)|(uint32(b[2])<<16)|(uint32(b[3])<<24);}};
struct MidiMessage {std::vector<uint8> bytes;static MidiMessage createSysExMessage(const void* p,int n){auto b=static_cast<const uint8*>(p);MidiMessage m;m.bytes.push_back(0xf0);m.bytes.insert(m.bytes.end(),b,b+n);m.bytes.push_back(0xf7);return m;}const uint8* getRawData()const{return bytes.data();}int getRawDataSize()const{return int(bytes.size());}};
}
