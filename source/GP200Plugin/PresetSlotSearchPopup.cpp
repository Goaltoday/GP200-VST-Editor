#include "PresetSlotSearchPopup.h"

namespace
{
const juce::Colour backgroundColour { 0xff171d20 };
const juce::Colour panelColour { 0xff222a2e };
const juce::Colour outlineColour { 0xffffa42a };
const juce::Colour textColour { 0xfff0f2f3 };
const juce::Colour mutedTextColour { 0xffaeb5b9 };
}

PresetSlotSearchPopup::PresetSlotSearchPopup()
    : resultsList("GP-200 preset slots", this)
{
    setVisible(false);
    setWantsKeyboardFocus(true);
    setAlwaysOnTop(true);

    addAndMakeVisible(searchEditor);
    searchEditor.setTextToShowWhenEmpty("Search preset name or slot...", mutedTextColour);
    searchEditor.setColour(juce::TextEditor::backgroundColourId, panelColour);
    searchEditor.setColour(juce::TextEditor::textColourId, textColour);
    searchEditor.setColour(juce::TextEditor::outlineColourId, outlineColour.withAlpha(0.65f));
    searchEditor.setColour(juce::TextEditor::focusedOutlineColourId, outlineColour);
    searchEditor.setColour(juce::TextEditor::highlightColourId, outlineColour.withAlpha(0.35f));
    searchEditor.setColour(juce::TextEditor::highlightedTextColourId, textColour);
    searchEditor.onTextChange = [this] { rebuildFilter(); };
    searchEditor.onEscapeKey = [this] { hidePopup(); };

    addAndMakeVisible(closeButton);
    closeButton.setTooltip("Close preset browser");
    closeButton.setColour(juce::TextButton::buttonColourId, panelColour);
    closeButton.setColour(juce::TextButton::buttonOnColourId, panelColour.brighter(0.15f));
    closeButton.setColour(juce::TextButton::textColourOffId, outlineColour);
    closeButton.onClick = [this] { hidePopup(); };

    addAndMakeVisible(statusLabel);
    statusLabel.setColour(juce::Label::textColourId, mutedTextColour);
    statusLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(resultsList);
    resultsList.setColour(juce::ListBox::backgroundColourId, backgroundColour);
    resultsList.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    resultsList.setRowHeight(28);
    resultsList.setMultipleSelectionEnabled(false);
}

void PresetSlotSearchPopup::setPresetNames(const std::array<juce::String, 256>& names,
                                            int newCurrentSlot,
                                            bool newScanRunning)
{
    presetNames = names;
    currentSlot = newCurrentSlot;
    scanRunning = newScanRunning;
    rebuildFilter();
}

void PresetSlotSearchPopup::showFor(juce::Rectangle<int> anchorBounds,
                                     juce::Rectangle<int> availableBounds)
{
    constexpr int desiredWidth = 430;
    constexpr int desiredHeight = 315;

    const int width = juce::jmin(desiredWidth, availableBounds.getWidth() - 16);
    const int height = juce::jmin(desiredHeight, availableBounds.getHeight() - 16);

    int x = anchorBounds.getX();
    x = juce::jlimit(availableBounds.getX() + 8,
                     availableBounds.getRight() - width - 8,
                     x);

    int y = anchorBounds.getBottom() + 4;
    if (y + height > availableBounds.getBottom() - 8)
        y = juce::jmax(availableBounds.getY() + 8, anchorBounds.getY() - height - 4);

    setBounds(x, y, width, height);
    setVisible(true);
    toFront(true);
    searchEditor.grabKeyboardFocus();
    searchEditor.selectAll();

    const auto currentIt = std::find(filteredSlots.begin(), filteredSlots.end(), currentSlot);
    if (currentIt != filteredSlots.end())
        resultsList.selectRow(static_cast<int>(std::distance(filteredSlots.begin(), currentIt)),
                              true,
                              true);
}

void PresetSlotSearchPopup::hidePopup()
{
    setVisible(false);
    searchEditor.clear();
}

void PresetSlotSearchPopup::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(backgroundColour);
    g.fillRoundedRectangle(bounds, 7.0f);
    g.setColour(outlineColour);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 7.0f, 1.2f);
}

void PresetSlotSearchPopup::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto top = area.removeFromTop(34);
    closeButton.setBounds(top.removeFromRight(34));
    top.removeFromRight(8);
    searchEditor.setBounds(top);

    area.removeFromTop(7);
    statusLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(3);
    resultsList.setBounds(area);
}

bool PresetSlotSearchPopup::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        hidePopup();
        return true;
    }

    return false;
}

int PresetSlotSearchPopup::getNumRows()
{
    return static_cast<int>(filteredSlots.size());
}

void PresetSlotSearchPopup::paintListBoxItem(int rowNumber,
                                              juce::Graphics& g,
                                              int width,
                                              int height,
                                              bool rowIsSelected)
{
    if (!juce::isPositiveAndBelow(rowNumber, static_cast<int>(filteredSlots.size())))
        return;

    const int slot = filteredSlots[static_cast<std::size_t>(rowNumber)];
    const auto name = presetNames[static_cast<std::size_t>(slot)].isNotEmpty()
                        ? presetNames[static_cast<std::size_t>(slot)]
                        : (scanRunning ? "Loading..." : "(name unavailable)");

    if (rowIsSelected)
    {
        g.setColour(outlineColour.withAlpha(0.18f));
        g.fillRoundedRectangle(juce::Rectangle<float>(2.0f, 1.0f,
                                                       static_cast<float>(width - 4),
                                                       static_cast<float>(height - 2)),
                               4.0f);
    }

    g.setColour(slot == currentSlot ? outlineColour : textColour);
    g.setFont(14.0f);
    g.drawText(formatSlot(slot), 10, 0, 54, height, juce::Justification::centredLeft, false);

    g.setColour(name == "Loading..." ? mutedTextColour : textColour);
    g.drawText(name, 68, 0, width - 78, height, juce::Justification::centredLeft, true);
}

void PresetSlotSearchPopup::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    chooseFilteredRow(row);
}

void PresetSlotSearchPopup::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    chooseFilteredRow(row);
}

void PresetSlotSearchPopup::returnKeyPressed(int row)
{
    chooseFilteredRow(row);
}

void PresetSlotSearchPopup::rebuildFilter()
{
    filteredSlots.clear();
    const auto query = searchEditor.getText().trim().toLowerCase();
    int loadedCount = 0;

    for (int slot = 0; slot < 256; ++slot)
    {
        const auto& name = presetNames[static_cast<std::size_t>(slot)];
        if (name.isNotEmpty())
            ++loadedCount;

        const auto searchable = (formatSlot(slot) + " " + name).toLowerCase();
        if (query.isEmpty() || searchable.contains(query))
            filteredSlots.push_back(slot);
    }

    statusLabel.setText(juce::String(filteredSlots.size()) + " results  |  "
                            + juce::String(loadedCount) + "/256 names"
                            + (scanRunning ? "  |  scanning..." : ""),
                        juce::dontSendNotification);

    resultsList.updateContent();
    resultsList.repaint();
}

void PresetSlotSearchPopup::chooseFilteredRow(int row)
{
    if (!juce::isPositiveAndBelow(row, static_cast<int>(filteredSlots.size())))
        return;

    const int slot = filteredSlots[static_cast<std::size_t>(row)];
    hidePopup();

    if (onSlotSelected)
        onSlotSelected(slot);
}

juce::String PresetSlotSearchPopup::formatSlot(int slot)
{
    const int safeSlot = juce::jlimit(0, 255, slot);
    const int bank = safeSlot / 4 + 1;
    const juce::juce_wchar letter = static_cast<juce::juce_wchar>('A' + (safeSlot % 4));
    return juce::String(bank).paddedLeft('0', 2) + "-" + juce::String::charToString(letter);
}
