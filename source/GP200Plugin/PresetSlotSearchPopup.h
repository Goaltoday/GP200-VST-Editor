#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <functional>
#include <vector>

class PresetSlotSearchPopup final : public juce::Component,
                                    private juce::ListBoxModel
{
public:
    PresetSlotSearchPopup();

    void setPresetNames(const std::array<juce::String, 256>& names,
                        int currentSlot,
                        bool scanRunning);

    void showFor(juce::Rectangle<int> anchorBounds,
                 juce::Rectangle<int> availableBounds);
    void hidePopup();
    bool isPopupVisible() const noexcept { return isVisible(); }

    std::function<void(int slot)> onSlotSelected;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber,
                          juce::Graphics& g,
                          int width,
                          int height,
                          bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void returnKeyPressed(int row) override;

    void rebuildFilter();
    void chooseFilteredRow(int row);
    static juce::String formatSlot(int slot);

    juce::TextEditor searchEditor;
    juce::TextButton closeButton{"X"};
    juce::Label statusLabel;
    juce::ListBox resultsList;

    std::array<juce::String, 256> presetNames{};
    std::vector<int> filteredSlots;
    int currentSlot{-1};
    bool scanRunning{false};
};
