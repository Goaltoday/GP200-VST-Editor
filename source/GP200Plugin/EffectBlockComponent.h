#pragma once

#include "../libgp200/GP200EffectDatabase.h"
#include "../libgp200/GP200EffectParamDatabase.h"
#include "../libgp200/GP200Preset.h"

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

class EffectBlockComponent final : public juce::Component
{
  public:
    using EffectDisplayNameResolver = std::function<juce::String (juce::uint32 effectId)>;

    explicit EffectBlockComponent (const gp200::GP200EffectSlot& effectToShow,
                                   EffectDisplayNameResolver displayNameResolver = {});

    void paint (juce::Graphics& g) override;
    void resized () override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

    void updateEffect (const gp200::GP200EffectSlot& newEffect);

    int getPreferredHeight (int width) const;

    bool isExpanded () const;
    void setExpanded (bool shouldBeExpanded);

    int getSlotIndex () const;
    int getBlockIndex () const;

    void setEnabledForDisplay (bool shouldBeEnabled);
    void setParameterValueForDisplay (int paramIndex, float value);
    void setMoveButtonsEnabled (bool canMoveUp, bool canMoveDown);

    std::function<void ()> onHeightChanged;
    std::function<void (int blockIndex, bool shouldBeOn)> onToggleRequested;
    std::function<void (int blockIndex, juce::uint32 effectId)> onEffectChangeRequested;
    std::function<void (int blockIndex, int paramIndex, juce::uint32 effectId, float value)>
        onParameterChangeRequested;
    std::function<void (int blockIndex, int direction)> onMoveRequested;
    std::function<void (int blockIndex, int contentY)> onDragReorderPreview;
    std::function<void (int blockIndex, int contentY)> onDragReorderRequested;

  private:
    static constexpr int headerHeight = 68;
    static constexpr int paramControlHeight = 34;
    static constexpr int verticalPadding = 12;

    struct ParameterControl
    {
        int paramIndex{-1};
        juce::String name;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
    };

    void rebuildParameterControls ();
    void rebuildEffectChoices ();
    void updateParameterControlsVisibility ();
    void updateEffectDescriptionLabel ();
    void changeEffectByOffset (int offset);

    juce::Rectangle<int> getOnOffBounds () const;
    juce::Rectangle<int> getEffectSelectorBounds () const;

    juce::Colour getBlockColour () const;
    juce::String getBlockName () const;
    juce::String getEffectName () const;
    juce::String getEffectDisplayName (juce::uint32 effectId, const juce::String& fallbackName) const;

    gp200::GP200EffectSlot effect;
    EffectDisplayNameResolver effectDisplayNameResolver;

    juce::TextButton moveUpButton{"^"};
    juce::TextButton moveDownButton{"v"};
    juce::TextButton previousEffectButton{"<"};
    juce::TextButton nextEffectButton{">"};

    juce::ComboBox effectSelector;
    juce::Label effectNameLabel;
    juce::Label effectDescriptionLabel;
    std::vector<juce::uint32> effectChoiceIds;
    bool updatingEffectSelector{false};

    bool expanded{false};
    std::vector<ParameterControl> parameterControls;

    bool dragCandidate{false};
    bool draggingForReorder{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectBlockComponent)
};
