from pathlib import Path
r=Path(__file__).resolve().parent
s=(r.parent/'source/GP200Plugin/PluginEditor.cpp').read_text();h=(r.parent/'source/GP200Plugin/PluginEditor.h').read_text()
a=h.index('    class EffectChainRibbonComponent final');b=h.index('    class DropIndicatorComponent',a)
cls=h[a:b].replace('      private:','      public:')
a=s.index('gp200::RoutingOrder AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getLocalOrder');b=s.index('//==============================================================================',s.index('::EffectChainRibbonComponent::mouseDown',a))
methods=s[a:b]
stubs=r'''#include <algorithm>
#include <array>
#include <vector>
#include <string>
#include <functional>
#include <limits>
#include <cmath>
#include <cassert>
#include <iostream>
namespace juce {
template<class T>T jmin(T a,T b){return std::min(a,b);}template<class T>T jmax(T a,T b){return std::max(a,b);}template<class T>T jlimit(T a,T b,T v){return std::clamp(v,a,b);}template<class T>bool isPositiveAndBelow(T a,T b){return a>=0&&a<b;}
struct String:std::string {using std::string::string;String()=default;String(int x):std::string(std::to_string(x)){}String(std::string x):std::string(x){}};
template<class T>struct Point{T x{},y{};Point()=default;Point(T a,T b):x(a),y(b){} Point operator-(Point p)const{return {x-p.x,y-p.y};}float getDistanceFrom(Point p)const{return std::hypot(float(x-p.x),float(y-p.y));}};
template<class T>struct Rectangle {T x{},y{},w{},h{};Rectangle()=default;Rectangle(T a,T b,T c,T d):x(a),y(b),w(c),h(d){}T getX()const{return x;}T getY()const{return y;}T getWidth()const{return w;}T getHeight()const{return h;}T getRight()const{return x+w;}T getBottom()const{return y+h;}T getCentreX()const{return x+w/2;}T getCentreY()const{return y+h/2;}Point<T>getPosition()const{return {x,y};}bool contains(Point<T>p)const{return p.x>=x&&p.x<x+w&&p.y>=y&&p.y<y+h;}bool intersects(Rectangle p)const{return x<p.x+p.w&&x+w>p.x&&y<p.y+p.h&&y+h>p.y;}Rectangle reduced(T a)const{return reduced(a,a);}Rectangle reduced(T a,T b)const{return {x+a,y+b,w-2*a,h-2*b};}Rectangle expanded(T a)const{return {x-a,y-a,w+2*a,h+2*a};}Rectangle withTrimmedBottom(T a)const{return {x,y,w,h-a};}Rectangle withTrimmedTop(T a)const{return {x,y+a,w,h-a};}Rectangle withHeight(T a)const{return {x,y,w,a};}Rectangle withPosition(Point<T>p)const{return {p.x,p.y,w,h};}Rectangle<float>toFloat()const{return {float(x),float(y),float(w),float(h)};}};
struct Colour{unsigned v{};Colour()=default;Colour(unsigned a):v(a){}Colour withAlpha(float)const{return *this;}};namespace Colours{const Colour white{0xffffffff};}
struct Path{void startNewSubPath(float,float){}void lineTo(float,float){}void closeSubPath(){}};
struct Justification{static constexpr int centred=0,centredLeft=1;};
struct Graphics {
 Colour colour;float opacity=1;std::vector<float>stack;std::vector<Rectangle<float>>tiles,marks;std::vector<float>ghostOpacity;
 void setColour(Colour c){colour=c;}void setFont(float){}void saveState(){stack.push_back(opacity);}void restoreState(){opacity=stack.back();stack.pop_back();}void setOpacity(float a){opacity=a;ghostOpacity.push_back(a);}
 void fillRoundedRectangle(Rectangle<float> r,float){if(opacity==1&&colour.v==0xff202427)tiles.push_back(r);if(colour.v==0xffffa42a)marks.push_back(r);}void fillRoundedRectangle(float x,float y,float w,float h,float k){fillRoundedRectangle({x,y,w,h},k);}
 void drawRoundedRectangle(Rectangle<float>,float,float){}void drawLine(float,float,float,float,float){}void fillEllipse(float,float,float,float){}void fillPath(Path&){}
 template<class T>void drawText(const String&,Rectangle<T>,int){}void drawText(const String&,int,int,int,int,int){}
};
struct MouseEvent{int x{},y{};Point<int>getPosition()const{return {x,y};}};
struct KeyPress{static constexpr int escapeKey=27;int key{};int getKeyCode()const{return key;}};
struct Component{enum FocusChangeType{focusChangedDirectly};int width=960,height=160;bool focus=false;virtual ~Component()=default;virtual void paint(Graphics&){}virtual void mouseDown(const MouseEvent&){}virtual void mouseDrag(const MouseEvent&){}virtual void mouseUp(const MouseEvent&){}virtual bool keyPressed(const KeyPress&){return false;}virtual void focusLost(FocusChangeType){}void repaint(){}int getWidth()const{return width;}int getHeight()const{return height;}Rectangle<int>getLocalBounds()const{return {0,0,width,height};}void setWantsKeyboardFocus(bool){}void grabKeyboardFocus(){focus=true;}};
}
namespace gp200{using RoutingOrder=std::array<int,11>;}
namespace gp200ui{float regular(float a){return a;}float semibold(float a){return a;}}
void drawRibbonBlockIcon(juce::Graphics&,const juce::String&,juce::Rectangle<float>,juce::Colour){}
'''
tests=r'''
using Ribbon=AudioPluginAudioProcessorEditor::EffectChainRibbonComponent;
int cases=0;
Ribbon make(bool parallel){Ribbon x;std::vector<Ribbon::Item>v;for(int i=0;i<11;i++)v.push_back({i,juce::String(i),true,juce::Colour(0xffaaaaaa)});x.setItems(v);x.setDeviceRouting(2,5,8,parallel);return x;}
juce::MouseEvent at(juce::Point<int>p){return {p.x,p.y};}
juce::Point<int> centre(juce::Rectangle<int>r){return {r.getCentreX(),r.getCentreY()};}
void start(Ribbon&x,int index){x.mouseDown(at(centre(x.getTileBounds(index))));}
void verifyPaint(Ribbon&x){auto order=x.getLocalOrder();auto s=x.getSend(),p=x.getBoundary(),r=x.getReturn();juce::Graphics g;x.paint(g);assert(!x.paintingDragPreview);assert(order==x.getLocalOrder()&&s==x.getSend()&&p==x.getBoundary()&&r==x.getReturn());for(auto m:g.marks)for(auto t:g.tiles)assert(!m.intersects(t));assert(g.ghostOpacity.size()>=1&&g.ghostOpacity.back()==0.65f);cases++;}
int main(){
for(bool parallel:{false,true}){
 for(int source=0;source<11;source++){
  auto x=make(parallel);int sends=0;x.onRoutingChanged=[&]{sends++;};auto initial=x.getLocalOrder();start(x,source);auto c=centre(x.getTileBounds(source));x.mouseDrag({c.x,c.y+7});verifyPaint(x);x.mouseUp({c.x,c.y+7});assert(sends==0&&x.getLocalOrder()==initial);cases++;
  for(auto outside:{juce::Point<int>{-1,80},{960,80},{300,-1},{300,160}}){start(x,source);x.mouseDrag(at(outside));assert(x.dragTargetPosition==-1);x.mouseUp(at(outside));assert(sends==0&&x.getLocalOrder()==initial);cases++;}
  start(x,source);x.mouseDrag({850,80});assert(x.keyPressed({27}));x.mouseUp({850,80});assert(sends==0&&x.getLocalOrder()==initial);cases++;
  start(x,source);x.mouseDrag({850,80});x.focusLost(juce::Component::focusChangedDirectly);x.mouseUp({850,80});assert(sends==0&&x.getLocalOrder()==initial);cases++;
 }
 for(int source=0;source<11;source++)for(int y:{45,80,115})for(int px=25;px<950;px+=25){
  auto x=make(parallel);int sends=0;x.onRoutingChanged=[&]{sends++;};start(x,source);auto c=centre(x.getTileBounds(source));x.mouseDrag({c.x,c.y+8});x.mouseDrag({px,y});const bool changed=x.dragChangesRouting();auto expected=x.items;int send=x.getSend(),p=x.getBoundary(),ret=x.getReturn();const int group=x.dragTargetGroup,pos=x.dragTargetPosition;
  verifyPaint(x);
  if(changed){if(parallel)x.moveLocalItem(source,group,pos);else{auto item=x.items[source];x.items.erase(x.items.begin()+source);x.items.insert(x.items.begin()+pos,item);}expected=x.items;send=x.getSend();p=x.getBoundary();ret=x.getReturn();x=make(parallel);x.onRoutingChanged=[&]{sends++;};start(x,source);auto c=centre(x.getTileBounds(source));x.mouseDrag({c.x,c.y+8});x.mouseDrag({px,y});}
  x.mouseUp({px,y});assert(sends==int(changed));assert(x.getSend()==send&&x.getBoundary()==p&&x.getReturn()==ret);for(int i=0;i<11;i++)assert(x.items[i].blockIndex==expected[i].blockIndex);cases++;
 }
 // Poll of unchanged device state must not cancel an active drag.
 auto x=make(parallel);start(x,3);x.mouseDrag({800,100});x.setItems(x.items);x.setLoopPositions(2,8);x.setDeviceRouting(2,5,8,parallel);assert(x.dragging);cases++;
 // A changed device layout invalidates the drag.
 auto fresh=x.items;std::swap(fresh[0],fresh[1]);x.setItems(fresh);assert(!x.dragging&&x.pressedItemIndex==-1);cases++;
 // Re-entry restores a valid target; release position is recalculated without a final drag event.
 x=make(parallel);int sends=0;x.onRoutingChanged=[&]{sends++;};start(x,3);x.mouseDrag({-4,80});x.mouseDrag({900,80});assert(x.dragTargetPosition>=0);x.mouseUp({30,80});assert(sends==1&&x.items[0].blockIndex==3);cases++;
 // Ordinary click remains selection only.
 x=make(parallel);int clicks=0;x.onBlockSelected=[&](int i){assert(i==3);clicks++;};start(x,3);x.mouseUp(at(centre(x.getTileBounds(3))));assert(clicks==1);cases++;
}
// Empty branches, all blocks in a branch, and coincident S/P/R boundaries.
for(auto geometry:{std::array<int,3>{0,0,0},{0,0,11},{0,11,11},{11,11,11},{2,2,8},{2,8,8}})
 for(int source=0;source<11;source++)for(int group=0;group<4;group++)for(bool last:{false,true}){
  auto x=make(true);x.setDeviceRouting(geometry[0],geometry[1],geometry[2],true);
  auto area=x.getGroupArea(group);auto target=juce::Point<int>{last ? area.getRight()-1 : area.getX()+1,area.getCentreY()};
  start(x,source);auto c=centre(x.getTileBounds(source));x.mouseDrag({c.x,c.y+8});x.mouseDrag(at(target));verifyPaint(x);
  const bool changed=x.dragChangesRouting();int sends=0;x.onRoutingChanged=[&]{sends++;};x.mouseUp(at(target));assert(sends==int(changed));
  assert(x.getSend()<=x.getBoundary()&&x.getBoundary()<=x.getReturn()&&x.getReturn()<=11);auto order=x.getLocalOrder();std::sort(order.begin(),order.end());for(int i=0;i<11;i++)assert(order[i]==i);cases++;
 }
std::cout<<cases<<" drag preview / drop / cancellation checks passed (real ribbon methods; simulated JUCE drawing/events)\n";
}
'''
(r/'drag_regressions.cpp').write_text(stubs+'\nstruct AudioPluginAudioProcessorEditor {\n'+cls+'\n};\n'+methods+tests)
print('Real ribbon methods extracted')
