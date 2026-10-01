#include <vector>
#include <algorithm>
#include <cassert>
#include <iostream>
namespace juce { int jlimit(int a,int b,int x){return std::max(a,std::min(b,x));} }
struct Item {int blockIndex;};
struct Model { std::vector<Item> items; int fxLoopSendPosition,localBoundary,fxLoopReturnPosition;
int getParallelGroupForItem(const Item&) const;
void moveLocalItem(int,int,int);
};
int Model::getParallelGroupForItem (const Item& item) const
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
void Model::moveLocalItem (int source, int group, int position)
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
int main(){long checks=0;
for(int S=0;S<=11;++S)for(int P=S;P<=11;++P)for(int R=P;R<=11;++R)
for(int source=0;source<11;++source)for(int group=0;group<4;++group)for(int rank=0;rank<=11;++rank){
 Model m; m.fxLoopSendPosition=S;m.localBoundary=P;m.fxLoopReturnPosition=R;
 for(int i=0;i<11;++i)m.items.push_back({i});
 m.moveLocalItem(source,group,rank);
 assert(0<=m.fxLoopSendPosition && m.fxLoopSendPosition<=m.localBoundary && m.localBoundary<=m.fxLoopReturnPosition && m.fxLoopReturnPosition<=11);
 std::vector<int> ids;for(auto item:m.items)ids.push_back(item.blockIndex);std::sort(ids.begin(),ids.end());for(int i=0;i<11;++i)assert(ids[i]==i);
 for(auto item:m.items)if(item.blockIndex==source)assert(m.getParallelGroupForItem(item)==group);
 ++checks;
}std::cout<<checks<<" routing moves passed\n";
}
