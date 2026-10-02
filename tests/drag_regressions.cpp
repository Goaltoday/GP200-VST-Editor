#include <algorithm>
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

struct AudioPluginAudioProcessorEditor {
    class EffectChainRibbonComponent final : public juce::Component
    {
      public:
        struct Item
        {
            int blockIndex{-1};
            juce::String blockName;
            bool enabled{false};
            juce::Colour colour;
        };

        void setItems (std::vector<Item> newItems);
        void setLoopPositions (int sendPosition, int returnPosition);
        void setParallelMode (bool shouldBeParallel);
        void setSelectedBlockIndex (int blockIndex);
        void setBlockEnabled (int blockIndex, bool enabled);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseUp (const juce::MouseEvent& event) override;
        bool keyPressed (const juce::KeyPress& key) override;
        void focusLost (FocusChangeType cause) override;

        gp200::RoutingOrder getLocalOrder () const;
        void keepRoutingDraft () { routingDraftEdited = true; }
        void releaseRoutingDraft () { routingDraftEdited = false; }
        void setDeviceRouting (int send, int boundary, int ret, bool parallel);

        int getSend () const { return fxLoopSendPosition; }
        int getBoundary () const { return localBoundary; }
        int getReturn () const { return fxLoopReturnPosition; }
        std::function<void ()> onRoutingChanged;
        std::function<void (int blockIndex)> onBlockSelected;
        std::function<void (int blockIndex, int targetPosition)> onBlockReordered;
        std::function<void (int sendPosition, int returnPosition)> onLoopPositionsChanged;

      public:
        juce::Rectangle<int> getTileBounds (int itemIndex) const;
        int getItemIndexAt (juce::Point<int> position) const;
        int getTargetPositionAtX (int x) const;
        int getLoopMarkerAt (juce::Point<int> position) const;
        int getLoopPositionAtX (int x) const;
        int getLoopMarkerX (int position) const;
        int getDropGroup (juce::Point<int> position) const;
        juce::Rectangle<int> getGroupArea (int group) const;
        void moveLocalItem (int source, int group, int position);
        void cancelDrag ();
        void updateBlockDragTarget (juce::Point<int> position);
        bool dragChangesRouting () const;
        juce::Point<int> dragCursorPosition;
        juce::Point<int> dragGrabOffset;
        bool paintingDragPreview{false};
        std::array<juce::Rectangle<int>, 4> previewGroupAreas;
        int localBoundary{5};
        bool localInitialised{false};
        bool routingDraftEdited{false};
        int dragTargetGroup{0};
        int getEffectiveSendPosition () const;
        int getEffectiveReturnPosition () const;
        int getParallelGroupForItem (const Item& item) const;

        std::vector<Item> items;
        int selectedBlockIndex{-1};
        int pressedItemIndex{-1};
        int dragTargetPosition{-1};
        bool dragging{false};
        bool parallelMode{false};
        int fxLoopSendPosition{4};
        int fxLoopReturnPosition{4};
        int draggedLoopMarker{-1};
        int draggedLoopPosition{-1};
        juce::Point<int> mouseDownPosition;
    };


};
gp200::RoutingOrder AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getLocalOrder () const
{
    gp200::RoutingOrder order{};
    for (std::size_t i = 0; i < order.size () && i < items.size (); ++i)
        order[i] = items[i].blockIndex;
    return order;
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::setItems (std::vector<Item> newItems)
{
    if (!localInitialised || !routingDraftEdited)
    {
        bool sameOrder = items.size () == newItems.size ();
        for (std::size_t i = 0; sameOrder && i < items.size (); ++i)
            sameOrder = items[i].blockIndex == newItems[i].blockIndex;
        if (!sameOrder) cancelDrag ();
        items = std::move (newItems);
        if (!localInitialised)
        {
            fxLoopSendPosition = juce::jmin (2, static_cast<int> (items.size ()));
            localBoundary = juce::jmin (5, static_cast<int> (items.size ()));
            fxLoopReturnPosition = juce::jmin (8, static_cast<int> (items.size ()));
        }
        localInitialised = true;
    }
    else
    {
        // Keep an edited routing draft; before editing, import the actual device order.
        for (auto& item : items)
            for (const auto& fresh : newItems)
                if (item.blockIndex == fresh.blockIndex) item = fresh;
    }

    const auto selectedStillExists = std::any_of (items.begin (), items.end (), [this] (const Item& item)
    { return item.blockIndex == selectedBlockIndex; });
    if (!selectedStillExists)
        selectedBlockIndex = items.empty () ? -1 : items.front ().blockIndex;
    repaint ();
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::setLoopPositions (int sendPosition, int returnPosition)
{
    if (routingDraftEdited) return;
    const int count = static_cast<int> (items.size ());
    if (sendPosition < 0 || returnPosition < sendPosition || returnPosition > count) return;
    if (fxLoopSendPosition != sendPosition || fxLoopReturnPosition != returnPosition) cancelDrag ();
    fxLoopSendPosition = sendPosition;
    fxLoopReturnPosition = returnPosition;
    localBoundary = juce::jlimit (sendPosition, returnPosition, localBoundary);
    repaint ();

}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::setDeviceRouting (int send, int boundary, int ret, bool parallel)
{
    if (send < 0 || send > boundary || boundary > ret || ret > static_cast<int> (items.size ())) return;
    if (fxLoopSendPosition != send || localBoundary != boundary || fxLoopReturnPosition != ret || parallelMode != parallel) cancelDrag ();
    fxLoopSendPosition = send; localBoundary = boundary; fxLoopReturnPosition = ret;
    parallelMode = parallel;
    repaint ();
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::setParallelMode (bool shouldBeParallel)
{
    if (parallelMode == shouldBeParallel) return;
    cancelDrag ();
    parallelMode = shouldBeParallel;
    repaint ();
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::setSelectedBlockIndex (int blockIndex)
{
    if (selectedBlockIndex != blockIndex)
    {
        selectedBlockIndex = blockIndex;
        repaint ();
    }
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::setBlockEnabled (int blockIndex, bool enabled)
{
    for (auto& item : items)
    {
        if (item.blockIndex == blockIndex)
        {
            item.enabled = enabled;
            repaint ();
            break;
        }
    }
}

juce::Rectangle<int> AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getTileBounds (int itemIndex) const
{
    if (items.empty () || itemIndex < 0 || itemIndex >= static_cast<int> (items.size ()))
        return {};
    if (parallelMode)
    {
        const auto group = getParallelGroupForItem (items[static_cast<std::size_t> (itemIndex)]);
        const auto area = getGroupArea (group);
        int rank = 0, count = 0;
        for (int i = 0; i < static_cast<int> (items.size ()); ++i)
            if (getParallelGroupForItem (items[static_cast<std::size_t> (i)]) == group)
            { if (i < itemIndex) ++rank; ++count; }
        const int step = area.getWidth () / juce::jmax (1, count);
        const int width = juce::jmin (60, juce::jmax (18, step - 6));
        return {area.getX () + rank * step + (step - width) / 2,
                area.getCentreY () - 26, width, 52};
    }
    auto area = getLocalBounds ().reduced (54, 14);
    constexpr int gap = 9;
    const auto count = static_cast<int> (items.size ());
    const auto tileWidth = juce::jmax (54, (area.getWidth () - gap * (count - 1)) / count);
    const auto tileHeight = juce::jmin (84, area.getHeight ());
    const auto totalWidth = tileWidth * count + gap * (count - 1);
    const auto startX = area.getCentreX () - totalWidth / 2;
    return {startX + itemIndex * (tileWidth + gap), area.getCentreY () - tileHeight / 2, tileWidth, tileHeight};
}

int AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getParallelGroupForItem (const Item& item) const
{
    const auto itemIterator = std::find_if (items.begin (), items.end (), [&item] (const Item& candidate)
    { return candidate.blockIndex == item.blockIndex; });
    if (itemIterator == items.end ()) return 0;
    const auto itemPosition = static_cast<int> (std::distance (items.begin (), itemIterator));
    if (itemPosition < fxLoopSendPosition) return 0;
    if (itemPosition < localBoundary) return 1;
    if (itemPosition < fxLoopReturnPosition) return 2;
    return 3;
}

juce::Rectangle<int> AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getGroupArea (int group) const
{
    if (paintingDragPreview) return previewGroupAreas[static_cast<std::size_t> (group)];
    const int available = juce::jmax (1, getWidth () - 100);
    const int middleCount = juce::jmax (1, juce::jmax (localBoundary - fxLoopSendPosition,
                                                     fxLoopReturnPosition - localBoundary));
    const int total = juce::jmax (1, fxLoopSendPosition) + middleCount
                    + juce::jmax (1, static_cast<int> (items.size ()) - fxLoopReturnPosition);
    const int unit = available / total;
    const int split = 50 + unit * juce::jmax (1, fxLoopSendPosition);
    const int merge = split + unit * middleCount;
    const int y = getHeight () / 2;
    if (group == 0) return {50, y - 26, split - 54, 52};
    if (group == 3) return {merge + 4, y - 26, getWidth () - 50 - merge - 4, 52};
    return {split + 4, y + (group == 1 ? -53 : 1), merge - split - 8, 52};
}

int AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getDropGroup (juce::Point<int> position) const
{
    const auto middle = getGroupArea (1);
    if (position.x < middle.getX ()) return 0;
    if (position.x > middle.getRight ()) return 3;
    return position.y < getHeight () / 2 ? 1 : 2;
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::moveLocalItem (int source, int group, int position)
{
    std::vector<Item> groups[4];
    const auto moved = items[static_cast<std::size_t> (source)];
    for (int i = 0; i < static_cast<int> (items.size ()); ++i)
        if (i != source) groups[getParallelGroupForItem (items[static_cast<std::size_t> (i)])].push_back (items[static_cast<std::size_t> (i)]);
    auto& destination = groups[group];
    destination.insert (destination.begin () + juce::jlimit (0, static_cast<int> (destination.size ()), position), moved);
    items.clear ();
    for (const auto& list : groups) items.insert (items.end (), list.begin (), list.end ());
    fxLoopSendPosition = static_cast<int> (groups[0].size ());
    localBoundary = fxLoopSendPosition + static_cast<int> (groups[1].size ());
    fxLoopReturnPosition = localBoundary + static_cast<int> (groups[2].size ());
}

int AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getEffectiveSendPosition () const
{ return fxLoopSendPosition; }

int AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getEffectiveReturnPosition () const
{ return fxLoopReturnPosition; }

int AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getItemIndexAt (juce::Point<int> position) const
{
    for (int i = 0; i < static_cast<int> (items.size ()); ++i)
        if (getTileBounds (i).contains (position))
            return i;
    return -1;
}

int AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getTargetPositionAtX (int x) const
{
    for (int i = 0; i < static_cast<int> (items.size ()); ++i)
        if (x < getTileBounds (i).getCentreX ())
            return i;
    return static_cast<int> (items.size ());
}

int AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getLoopMarkerX (int position) const
{
    if (items.empty ()) return getWidth () / 2;
    if (parallelMode)
        return position == fxLoopSendPosition ? getGroupArea (1).getX () - 4
                                             : getGroupArea (1).getRight () + 4;
    if (position == 0) return getTileBounds (0).getX () - 5;
    return getTileBounds (juce::jlimit (0, static_cast<int> (items.size ()) - 1, position - 1)).getRight () + 5;
}

int AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getLoopPositionAtX (int x) const
{
    const int minimumPosition = draggedLoopMarker == 0 ? 0 : localBoundary;
    const int maximumPosition = draggedLoopMarker == 0 ? localBoundary : static_cast<int> (items.size ());
    auto bestPosition = minimumPosition;
    auto bestDistance = std::numeric_limits<int>::max ();
    for (int position = minimumPosition; position <= maximumPosition; ++position)
    {
        const auto boundaryX = parallelMode ? 50 + position * (getWidth () - 100) / juce::jmax (1, static_cast<int> (items.size ())) : getLoopMarkerX (position);
        const auto distance = std::abs (x - boundaryX);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestPosition = position;
        }
    }
    return bestPosition;
}

int AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::getLoopMarkerAt (juce::Point<int> position) const
{
    const auto sendX = getLoopMarkerX (parallelMode ? getEffectiveSendPosition () : fxLoopSendPosition);
    const auto returnX = getLoopMarkerX (parallelMode ? getEffectiveReturnPosition () : fxLoopReturnPosition);
    if (std::abs (position.x - sendX) <= 10 && position.y <= 18) return 0;
    if (std::abs (position.x - returnX) <= 10 && position.y >= getHeight () - 18) return 1;
    return -1;
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds ().toFloat ().reduced (0.5f);
    g.setColour (juce::Colour (0xff161a1d));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (juce::Colour (0xff4b4f52));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
    if (items.empty ()) return;

    const bool blockDrag = dragging && juce::isPositiveAndBelow (pressedItemIndex, static_cast<int> (items.size ()));
    const bool preview = blockDrag && dragTargetPosition >= 0 && dragChangesRouting ();
    const auto originalTile = blockDrag ? getTileBounds (pressedItemIndex) : juce::Rectangle<int> {};
    const Item draggedItem = blockDrag ? items[static_cast<std::size_t> (pressedItemIndex)] : Item {};
    struct LayoutRestore
    {
        EffectChainRibbonComponent& ribbon;
        std::vector<Item> originalItems;
        int send, boundary, ret;
        ~LayoutRestore ()
        {
            ribbon.items = std::move (originalItems);
            ribbon.fxLoopSendPosition = send; ribbon.localBoundary = boundary; ribbon.fxLoopReturnPosition = ret;
            ribbon.paintingDragPreview = false;
        }
    } restore {*this, items, fxLoopSendPosition, localBoundary, fxLoopReturnPosition};
    if (preview)
    {
        // Keep branch boundaries stable while previewing; redistribute tiles within each branch.
        for (int group = 0; group < 4; ++group) previewGroupAreas[static_cast<std::size_t> (group)] = getGroupArea (group);
        paintingDragPreview = parallelMode;
        if (parallelMode) moveLocalItem (pressedItemIndex, dragTargetGroup, dragTargetPosition);
        else
        {
            items.erase (items.begin () + pressedItemIndex);
            items.insert (items.begin () + dragTargetPosition, draggedItem);
        }
    }

    g.setColour (juce::Colour (0xffb9bdc0));
    g.setFont (gp200ui::regular (10.0f));
    g.drawText ("SPR S=" + juce::String (fxLoopSendPosition) + " P=" + juce::String (localBoundary)
                + " R=" + juce::String (fxLoopReturnPosition), 8, 0, 220, 13, juce::Justification::centredLeft);
    const auto vol = std::find_if (items.begin (), items.end (), [] (const Item& item) { return item.blockIndex == 10; });
    const int volPosition = static_cast<int> (std::distance (items.begin (), vol));
    const auto volText = vol == items.end () || !vol->enabled ? "VOL: OFF"
        : parallelMode && volPosition >= fxLoopReturnPosition ? "VOL: BLEND" : "VOL: LEVEL";
    g.drawText (volText, 225, 0, 150, 13, juce::Justification::centredLeft);
    if (parallelMode)
    {
        const char* labels[] = {"IN COMMON", "A", "B", "OUT COMMON"};
        for (int group = 0; group < 4; ++group)
        {
            const auto area = getGroupArea (group);
            g.setColour (juce::Colour (0xff444a50));
            g.drawRoundedRectangle (area.toFloat (), 4.0f, 1.0f);
            g.drawText (labels[group], area.withHeight (12), juce::Justification::centred);
        }
    }
    const auto first = getTileBounds (0);
    const auto chainY = parallelMode ? getHeight () / 2 : first.getCentreY ();
    g.setColour (juce::Colour (0xff7b8083));
    if (parallelMode)
    {
        const auto splitX = static_cast<float> (getLoopMarkerX (getEffectiveSendPosition ()));
        const auto mergeX = static_cast<float> (getLoopMarkerX (getEffectiveReturnPosition ()));
        const auto upperY = static_cast<float> (getHeight () / 2 - 27);
        const auto lowerY = static_cast<float> (getHeight () / 2 + 27);
        g.drawLine (36.0f, static_cast<float> (chainY), splitX, static_cast<float> (chainY), 2.0f);
        g.drawLine (splitX, upperY, splitX, lowerY, 2.0f);
        g.drawLine (splitX, upperY, mergeX, upperY, 2.0f);
        g.drawLine (splitX, lowerY, mergeX, lowerY, 2.0f);
        g.drawLine (mergeX, upperY, mergeX, lowerY, 2.0f);
        g.drawLine (mergeX, static_cast<float> (chainY), static_cast<float> (getWidth () - 36),
                    static_cast<float> (chainY), 2.0f);
        g.fillEllipse (splitX - 3.0f, static_cast<float> (chainY) - 3.0f, 6.0f, 6.0f);
        g.fillEllipse (mergeX - 3.0f, static_cast<float> (chainY) - 3.0f, 6.0f, 6.0f);
    }
    else
        g.drawLine (36.0f, static_cast<float> (chainY), static_cast<float> (getWidth () - 36), static_cast<float> (chainY), 2.0f);
    g.setFont (gp200ui::regular (12.75f));
    g.setColour (juce::Colour (0xffb9bdc0));
    g.drawText ("IN", 12, chainY - 12, 34, 24, juce::Justification::centred);
    g.drawText ("OUT", getWidth () - 46, chainY - 12, 34, 24, juce::Justification::centred);

    for (int i = 0; i < static_cast<int> (items.size ()); ++i)
    {
        const auto& item = items[static_cast<std::size_t> (i)];
        const auto tile = getTileBounds (i);
        // The moved tile reserves a real slot; paint its gap instead of a destination block.
        if (blockDrag && item.blockIndex == draggedItem.blockIndex)
        {
            if (preview)
            {
                const float x = static_cast<float> (tile.getCentreX ());
                g.setColour (juce::Colour (0xffffa42a));
                g.fillRoundedRectangle (x - 1.5f, static_cast<float> (tile.getY ()), 3.0f,
                                        static_cast<float> (tile.getHeight ()), 1.5f);
                g.drawLine (x - 5, static_cast<float> (tile.getY ()), x + 5, static_cast<float> (tile.getY ()), 2.0f);
                g.drawLine (x - 5, static_cast<float> (tile.getBottom ()), x + 5, static_cast<float> (tile.getBottom ()), 2.0f);
            }
            continue;
        }
        const auto selected = item.blockIndex == selectedBlockIndex;
        const auto displayColour = item.enabled ? item.colour : juce::Colour (0xff74787b);
        g.setColour (juce::Colour (0xff202427));
        g.fillRoundedRectangle (tile.toFloat (), 6.0f);
        if (item.enabled)
        {
            g.setColour (displayColour.withAlpha (0.13f));
            g.fillRoundedRectangle (tile.toFloat ().reduced (2.0f), 5.0f);
        }
        g.setColour (displayColour.withAlpha (selected ? 1.0f : 0.78f));
        g.drawRoundedRectangle (tile.toFloat ().reduced (0.5f), 6.0f, selected ? 2.4f : 1.4f);
        if (selected)
        {
            g.setColour (displayColour.withAlpha (0.18f));
            g.drawRoundedRectangle (tile.toFloat ().expanded (3.0f), 8.0f, 2.0f);
        }
        const auto iconArea = parallelMode
            ? tile.toFloat ().reduced (7.0f, 4.0f).withTrimmedBottom (18.0f)
            : tile.toFloat ().reduced (10.0f, 8.0f).withTrimmedBottom (24.0f);
        drawRibbonBlockIcon (g, item.blockName, iconArea, displayColour);

        g.setColour (displayColour);
        g.setFont (gp200ui::semibold (parallelMode ? 10.5f : 14.25f));
        g.drawText (item.blockName,
                    tile.withTrimmedTop (tile.getHeight() - (parallelMode ? 18 : 24)).reduced (3, 1),
                    juce::Justification::centred);

        if (selected)
        {
            juce::Path selectionArrow;
            const auto centreX = static_cast<float> (tile.getCentreX ());
            const auto arrowTop = static_cast<float> (tile.getBottom () + 5);
            selectionArrow.startNewSubPath (centreX - 6.0f, arrowTop + 8.0f);
            selectionArrow.lineTo (centreX, arrowTop);
            selectionArrow.lineTo (centreX + 6.0f, arrowTop + 8.0f);
            selectionArrow.closeSubPath ();
            g.setColour (displayColour);
            g.fillPath (selectionArrow);
        }
    }

    const auto sendMarkerPosition = draggedLoopMarker == 0 ? draggedLoopPosition
        : parallelMode ? getEffectiveSendPosition () : fxLoopSendPosition;
    const auto returnMarkerPosition = draggedLoopMarker == 1 ? draggedLoopPosition
        : parallelMode ? getEffectiveReturnPosition () : fxLoopReturnPosition;
    auto drawLoopMarker = [&] (int loopPosition, int y, juce::Colour colour, const juce::String& label, bool pointsUp)
    {
        const auto x = static_cast<float> (parallelMode && draggedLoopMarker >= 0
            ? 50 + loopPosition * (getWidth () - 100) / juce::jmax (1, static_cast<int> (items.size ()))
            : getLoopMarkerX (loopPosition));
        juce::Path arrow;
        if (pointsUp)
        {
            arrow.startNewSubPath (x, static_cast<float> (y));
            arrow.lineTo (x - 6.0f, static_cast<float> (y + 7));
            arrow.lineTo (x + 6.0f, static_cast<float> (y + 7));
        }
        else
        {
            arrow.startNewSubPath (x, static_cast<float> (y));
            arrow.lineTo (x - 6.0f, static_cast<float> (y - 7));
            arrow.lineTo (x + 6.0f, static_cast<float> (y - 7));
        }
        arrow.closeSubPath ();
        g.setColour (colour);
        g.fillPath (arrow);
        g.setFont (gp200ui::semibold (9.5f));
        g.drawText (label, juce::Rectangle<int> (static_cast<int> (x) - 16, pointsUp ? 1 : getHeight () - 13, 32, 12),
                    juce::Justification::centred);
    };
    drawLoopMarker (sendMarkerPosition, 15, juce::Colour (0xff32a8ff), "SEND", true);
    drawLoopMarker (returnMarkerPosition, getHeight () - 14, juce::Colour (0xffbd5cff), "RETURN", false);

    if (draggedLoopMarker >= 0 && draggedLoopPosition >= 0)
    {
        const auto x = getLoopMarkerX (draggedLoopPosition);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine (static_cast<float> (x), 19.0f, static_cast<float> (x), static_cast<float> (getHeight () - 19), 1.0f);
    }

    if (blockDrag)
    {
        auto drawDraggedTile = [&] (juce::Rectangle<int> tile, float opacity)
        {
            g.saveState ();
            g.setOpacity (opacity);
            const auto colour = draggedItem.enabled ? draggedItem.colour : juce::Colour (0xff74787b);
            g.setColour (juce::Colour (0xff202427));
            g.fillRoundedRectangle (tile.toFloat (), 6.0f);
            g.setColour (colour);
            g.drawRoundedRectangle (tile.toFloat ().reduced (0.5f), 6.0f, 1.4f);
            const auto iconArea = tile.toFloat ().reduced (7.0f, 4.0f).withTrimmedBottom (18.0f);
            drawRibbonBlockIcon (g, draggedItem.blockName, iconArea, colour);
            g.setFont (gp200ui::semibold (parallelMode ? 10.5f : 14.25f));
            g.drawText (draggedItem.blockName, tile.withTrimmedTop (tile.getHeight () - 18).reduced (3, 1),
                        juce::Justification::centred);
            g.restoreState ();
        };
        // Only retain the attenuated original when it cannot obscure another preview tile.
        bool originalClear = true;
        for (int i = 0; i < static_cast<int> (items.size ()); ++i)
            if ((preview || items[static_cast<std::size_t> (i)].blockIndex != draggedItem.blockIndex)
                && getTileBounds (i).intersects (originalTile)) originalClear = false;
        if (originalClear) drawDraggedTile (originalTile, 0.22f);
        drawDraggedTile (originalTile.withPosition (dragCursorPosition - dragGrabOffset), 0.65f);
    }
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::cancelDrag ()
{
    pressedItemIndex = -1; dragTargetPosition = -1; dragging = false;
    draggedLoopMarker = -1; draggedLoopPosition = -1;
    repaint ();
}

bool AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode () != juce::KeyPress::escapeKey) return false;
    if (pressedItemIndex < 0 && draggedLoopMarker < 0) return false;
    cancelDrag (); return true;
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::focusLost (FocusChangeType)
{
    cancelDrag ();
}

bool AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::dragChangesRouting () const
{
    if (pressedItemIndex < 0 || dragTargetPosition < 0) return false;
    if (!parallelMode) return dragTargetPosition != pressedItemIndex;
    const int sourceGroup = getParallelGroupForItem (items[static_cast<std::size_t> (pressedItemIndex)]);
    if (sourceGroup != dragTargetGroup) return true;
    int sourceRank = 0;
    for (int i = 0; i < pressedItemIndex; ++i)
        if (getParallelGroupForItem (items[static_cast<std::size_t> (i)]) == sourceGroup) ++sourceRank;
    return sourceRank != dragTargetPosition;
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::updateBlockDragTarget (juce::Point<int> position)
{
    dragCursorPosition = position;
    dragTargetPosition = -1;
    if (!getLocalBounds ().contains (position)) return;
    dragTargetGroup = parallelMode ? getDropGroup (position) : 0;
    dragTargetPosition = 0;
    for (int i = 0; i < static_cast<int> (items.size ()); ++i)
        if (i != pressedItemIndex && (!parallelMode || getParallelGroupForItem (items[static_cast<std::size_t> (i)]) == dragTargetGroup)
            && position.x >= getTileBounds (i).getCentreX ()) ++dragTargetPosition;
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::mouseDown (const juce::MouseEvent& event)
{
    cancelDrag ();
    setWantsKeyboardFocus (true);
    grabKeyboardFocus ();
    mouseDownPosition = dragCursorPosition = event.getPosition ();
    draggedLoopMarker = getLoopMarkerAt (event.getPosition ());
    if (draggedLoopMarker >= 0)
    {
        draggedLoopPosition = draggedLoopMarker == 0 ? fxLoopSendPosition : fxLoopReturnPosition;
        return;
    }
    pressedItemIndex = getItemIndexAt (event.getPosition ());
    if (pressedItemIndex >= 0) dragGrabOffset = event.getPosition () - getTileBounds (pressedItemIndex).getPosition ();
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (draggedLoopMarker >= 0)
    {
        draggedLoopPosition = getLocalBounds ().contains (event.getPosition ()) ? getLoopPositionAtX (event.x) : -1;
        repaint (); return;
    }
    if (pressedItemIndex < 0) return;
    if (!dragging && event.getPosition ().getDistanceFrom (mouseDownPosition) >= 6.0f) dragging = true;
    if (dragging) { updateBlockDragTarget (event.getPosition ()); repaint (); }
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::mouseUp (const juce::MouseEvent& event)
{
    if (!getLocalBounds ().contains (event.getPosition ())) { cancelDrag (); return; }
    if (draggedLoopMarker >= 0)
    {
        const int position = getLoopPositionAtX (event.x);
        int& marker = draggedLoopMarker == 0 ? fxLoopSendPosition : fxLoopReturnPosition;
        const bool changed = marker != position;
        marker = position;
        cancelDrag ();
        if (changed && onRoutingChanged) onRoutingChanged ();
        return;
    }
    if (!juce::isPositiveAndBelow (pressedItemIndex, static_cast<int> (items.size ()))) { cancelDrag (); return; }
    const auto blockIndex = items[static_cast<std::size_t> (pressedItemIndex)].blockIndex;
    bool changed = false;
    if (dragging)
    {
        updateBlockDragTarget (event.getPosition ());
        changed = dragChangesRouting ();
        if (changed)
        {
            if (parallelMode) moveLocalItem (pressedItemIndex, dragTargetGroup, dragTargetPosition);
            else
            {
                const auto moved = items[static_cast<std::size_t> (pressedItemIndex)];
                items.erase (items.begin () + pressedItemIndex);
                items.insert (items.begin () + dragTargetPosition, moved);
            }
        }
    }
    else if (onBlockSelected) onBlockSelected (blockIndex);
    cancelDrag ();
    if (changed && onRoutingChanged) onRoutingChanged ();
}


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
