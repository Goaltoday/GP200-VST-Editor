/*
    GP200 VST

    Portions adapted from phash/gp200editor and its contributors.
    Those portions are licensed under GPL-3.0-or-later.

  
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "PluginEditor.h"
#include "GP200Typography.h"
#include "BinaryData.h"
#include "../libgp200/GP200Preset.h"
#include "../libgp200/GP200EffectParamDatabase.h"
#include "../libgp200/GP200EffectDatabase.h"
#include "../libgp200/GP200Constants.h"

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace
{
const juce::Colour backgroundColour{0xff4f4f4f};
const juce::Colour panelColour{0xff222b2f};
const juce::Colour panelOutlineColour{0xffffa42a};
const juce::Colour textColour{0xffffffff};
const juce::Colour mutedTextColour{0xffd8d8d8};
const juce::Colour statusOnColour{0xff57f05f};
const juce::Colour statusOffColour{0xffd84545};

void drawRibbonBlockIcon (juce::Graphics& g,
                          const juce::String& blockName,
                          juce::Rectangle<float> area,
                          juce::Colour colour)
{
    struct SvgResource
    {
        const char* data{};
        int size{};
    };

    const auto getResource = [&]() -> SvgResource
    {
        if (blockName == "VOL") return { BinaryData::vol_svg, BinaryData::vol_svgSize };
        if (blockName == "PRE") return { BinaryData::pre_svg, BinaryData::pre_svgSize };
        if (blockName == "WAH") return { BinaryData::wah_svg, BinaryData::wah_svgSize };
        if (blockName == "DST") return { BinaryData::dst_svg, BinaryData::dst_svgSize };
        if (blockName == "AMP") return { BinaryData::amp_svg, BinaryData::amp_svgSize };
        if (blockName == "NR")  return { BinaryData::nr_svg,  BinaryData::nr_svgSize  };
        if (blockName == "CAB") return { BinaryData::cab_svg, BinaryData::cab_svgSize };
        if (blockName == "EQ")  return { BinaryData::eq_svg,  BinaryData::eq_svgSize  };
        if (blockName == "MOD") return { BinaryData::mod_svg, BinaryData::mod_svgSize };
        if (blockName == "DLY") return { BinaryData::dly_svg, BinaryData::dly_svgSize };
        if (blockName == "RVB") return { BinaryData::rvb_svg, BinaryData::rvb_svgSize };
        return {};
    };

    const auto resource = getResource();
    if (resource.data == nullptr || resource.size <= 0)
        return;

    // The cache avoids parsing SVG XML on every repaint/drag operation.
    static std::map<juce::String, std::unique_ptr<juce::Drawable>> iconCache;
    auto& icon = iconCache[blockName];

    if (icon == nullptr)
        icon = juce::Drawable::createFromImageData (resource.data,
                                                    static_cast<std::size_t> (resource.size));

    if (icon == nullptr)
        return;

    const auto sourceColour = juce::Colours::white;
    icon->replaceColour (sourceColour, colour);
    icon->drawWithin (g,
                      area.reduced (1.0f),
                      juce::RectanglePlacement::centred,
                      1.0f);
    icon->replaceColour (colour, sourceColour);
}

gp200::GP200Preset makeDefaultOfflinePreset ()
{
    gp200::GP200Preset preset;
    preset.isValid = true;
    preset.patchName = "Offline preset";
    preset.fxLoopSend = 4;
    preset.fxLoopReturn = 4;

    static constexpr const char* modules[] = {
        "PRE", "WAH", "DST", "AMP", "NR", "CAB",
        "EQ", "MOD", "DLY", "RVB", "VOL"
    };

    static constexpr int defaultRoutingOrder[] = { 10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    for (int blockIndex = 0; blockIndex < static_cast<int> (gp200::effectBlockCount); ++blockIndex)
    {
        preset.routingOrder[static_cast<std::size_t> (blockIndex)] = defaultRoutingOrder[blockIndex];

        auto& slot = preset.effects[static_cast<std::size_t> (blockIndex)];
        slot.blockIndex = blockIndex;
        slot.slotIndex = blockIndex;
        slot.enabled = false;
        slot.params.fill (0.0f);

        const auto effects = gp200::GP200EffectDatabase::getEffectsForModule (modules[blockIndex]);
        if (!effects.empty ())
            slot.effectId = effects.front ().effectId;

        if (const auto* paramSet = gp200::GP200EffectParamDatabase::findParamsForEffect (slot.effectId))
        {
            for (int i = 0; i < paramSet->count; ++i)
            {
                const auto& param = paramSet->params[i];
                if (param.idx >= 0 && param.idx < static_cast<int> (slot.params.size ()))
                    slot.params[static_cast<std::size_t> (param.idx)] = param.defaultValue;
            }
        }
    }

    return preset;
}


constexpr juce::uint32 offlineSnapshotMagic = 0x4f503247u; // "GP2O"
constexpr int offlineSnapshotVersion = 2;

juce::MemoryBlock serialiseOfflineSnapshot (const gp200::GP200Preset& preset,
                                            int patchVolume,
                                            int patchPan,
                                            int patchTempo)
{
    juce::MemoryBlock result;
    juce::MemoryOutputStream out (result, false);

    out.writeInt (static_cast<int> (offlineSnapshotMagic));
    out.writeInt (offlineSnapshotVersion);
    out.writeString (preset.patchName);
    out.writeString (preset.author);
    out.writeInt (static_cast<int> (preset.prstRawSource.getSize ()));
    if (preset.prstRawSource.getSize () > 0)
        out.write (preset.prstRawSource.getData (), preset.prstRawSource.getSize ());
    out.writeInt (preset.fxLoopSend);
    out.writeInt (preset.fxLoopReturn);
    out.writeInt (patchVolume);
    out.writeInt (patchPan);
    out.writeInt (patchTempo);

    for (const auto value : preset.routingOrder)
        out.writeInt (value);

    for (const auto& effect : preset.effects)
    {
        out.writeInt (effect.blockIndex);
        out.writeInt (effect.slotIndex);
        out.writeBool (effect.enabled);
        out.writeInt (static_cast<int> (effect.effectId));

        for (const auto value : effect.params)
            out.writeFloat (value);
    }

    return result;
}

bool deserialiseOfflineSnapshot (const juce::MemoryBlock& data,
                                 gp200::GP200Preset& preset,
                                 int& patchVolume,
                                 int& patchPan,
                                 int& patchTempo)
{
    if (data.getSize () < 8)
        return false;

    juce::MemoryInputStream in (data, false);

    if (static_cast<juce::uint32> (in.readInt ()) != offlineSnapshotMagic)
        return false;

    const auto snapshotVersion = in.readInt ();
    if (snapshotVersion != 1 && snapshotVersion != offlineSnapshotVersion)
        return false;

    gp200::GP200Preset decoded;
    decoded.isValid = true;
    decoded.patchName = in.readString ();
    decoded.author = in.readString ();

    if (snapshotVersion >= 2)
    {
        const auto rawSize = in.readInt ();
        if (rawSize < 0 || static_cast<juce::int64> (rawSize) > in.getNumBytesRemaining ())
            return false;
        if (rawSize > 0)
        {
            decoded.prstRawSource.setSize (static_cast<std::size_t> (rawSize), false);
            if (in.read (decoded.prstRawSource.getData (), rawSize) != rawSize)
                return false;
        }
    }

    decoded.fxLoopSend = in.readInt ();
    decoded.fxLoopReturn = in.readInt ();
    patchVolume = juce::jlimit (0, 100, in.readInt ());
    patchPan = juce::jlimit (-50, 50, in.readInt ());
    patchTempo = juce::jlimit (40, 250, in.readInt ());

    for (auto& value : decoded.routingOrder)
        value = in.readInt ();

    for (auto& effect : decoded.effects)
    {
        effect.blockIndex = in.readInt ();
        effect.slotIndex = in.readInt ();
        effect.enabled = in.readBool ();
        effect.effectId = static_cast<juce::uint32> (in.readInt ());

        for (auto& value : effect.params)
            value = in.readFloat ();
    }

    if (in.isExhausted () && decoded.patchName.isEmpty ())
        decoded.patchName = "Offline preset";

    preset = std::move (decoded);
    return true;
}


class SoundCloneImportComponent final : public juce::Component,
                                              private juce::ListBoxModel,
                                              private juce::Timer
{
public:
    using ImportCallback = std::function<void (const juce::File&, int)>;
    using StatusCallback = std::function<juce::String ()>;
    using BusyCallback = std::function<bool ()>;

    SoundCloneImportComponent (ImportCallback callback,
                               StatusCallback statusCallback,
                               BusyCallback busyCallback)
        : onImport (std::move (callback)),
          getUploadStatus (std::move (statusCallback)),
          isUploadBusy (std::move (busyCallback))
    {
        setLookAndFeel (&spaceGroteskLookAndFeel);

        addAndMakeVisible (pathLabel);
        addAndMakeVisible (pathEditor);
        addAndMakeVisible (browseButton);
        addAndMakeVisible (fileList);
        addAndMakeVisible (destinationBox);
        addAndMakeVisible (importButton);
        addAndMakeVisible (statusLabel);

        juce::PropertiesFile::Options options;
        options.applicationName = "GP200 VST";
        options.filenameSuffix = "settings";
        options.folderName = "GP200Studio";
        options.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (options);

        pathLabel.setText ("Clone folder", juce::dontSendNotification);
        pathLabel.setColour (juce::Label::textColourId, mutedTextColour);

        pathEditor.setTextToShowWhenEmpty (
            "Choose or type a folder containing .clo or .tone files",
            mutedTextColour.withAlpha (0.65f));
        pathEditor.setFont (gp200ui::regular (14.75f));
        pathEditor.setColour (juce::TextEditor::backgroundColourId, panelColour);
        pathEditor.setColour (juce::TextEditor::textColourId, textColour);
        pathEditor.setColour (juce::TextEditor::outlineColourId, panelOutlineColour);
        pathEditor.setColour (juce::TextEditor::focusedOutlineColourId,
                              panelOutlineColour.brighter (0.2f));
        pathEditor.onReturnKey = [this] { setLibraryRootFromEditor (); };
        pathEditor.onFocusLost = [this]
        {
            const auto typedFolder = juce::File (pathEditor.getText ().trim ());
            if (typedFolder.isDirectory() && typedFolder != currentFolder)
                setLibraryRoot (typedFolder);
        };

        browseButton.setButtonText ("Browse...");
        setupButtonColours (browseButton);
        browseButton.onClick = [this]
        {
            const auto startFolder = currentFolder.isDirectory()
                                         ? currentFolder
                                         : juce::File::getSpecialLocation (
                                               juce::File::userDocumentsDirectory);

            folderChooser = std::make_unique<juce::FileChooser> (
                "Choose a Sound Clone folder", startFolder);

            folderChooser->launchAsync (
                juce::FileBrowserComponent::openMode |
                    juce::FileBrowserComponent::canSelectDirectories,
                [safeThis = juce::Component::SafePointer<SoundCloneImportComponent> (this)]
                (const juce::FileChooser& chooser)
                {
                    if (safeThis == nullptr)
                        return;

                    const auto folder = chooser.getResult ();
                    if (folder.isDirectory ())
                        safeThis->setLibraryRoot (folder);
                });
        };

        fileList.setModel (this);
        fileList.setRowHeight (28);
        fileList.setMultipleSelectionEnabled (false);
        fileList.setColour (juce::ListBox::backgroundColourId, panelColour.darker (0.2f));
        fileList.setColour (juce::ListBox::outlineColourId, panelOutlineColour);
        fileList.setOutlineThickness (1);

        for (int i = 1; i <= 5; ++i)
            destinationBox.addItem ("SnapTone " + juce::String (i) + " (AMP)", i);

        for (int i = 6; i <= 10; ++i)
            destinationBox.addItem ("SnapTone " + juce::String (i) + " (DIST)", i);

        destinationBox.setSelectedId (1, juce::dontSendNotification);
        destinationBox.setColour (juce::ComboBox::backgroundColourId, panelColour);
        destinationBox.setColour (juce::ComboBox::textColourId, panelOutlineColour);
        destinationBox.setColour (juce::ComboBox::outlineColourId, panelOutlineColour);

        importButton.setButtonText ("Import selected clone");
        setupButtonColours (importButton);
        importButton.setEnabled (false);
        importButton.onClick = [this] { importSelectedFile (); };

        statusLabel.setText ("Choose a folder to list its .clo and .tone files.",
                             juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, mutedTextColour);
        statusLabel.setJustificationType (juce::Justification::centredLeft);

        const auto savedPath = settings != nullptr
                                   ? settings->getValue ("soundCloneFolder")
                                   : juce::String();

        if (savedPath.isNotEmpty())
            setLibraryRoot (juce::File (savedPath));

        startTimerHz (10);
        setSize (620, 420);
    }

    ~SoundCloneImportComponent () override
    {
        setLookAndFeel (nullptr);
        stopTimer();
        fileList.setModel (nullptr);
    }

    void resized () override
    {
        auto area = getLocalBounds ().reduced (20);
        auto pathRow = area.removeFromTop (32);
        pathLabel.setBounds (pathRow.removeFromLeft (78));
        pathRow.removeFromLeft (8);
        browseButton.setBounds (pathRow.removeFromRight (100));
        pathRow.removeFromRight (8);
        pathEditor.setBounds (pathRow);
        area.removeFromTop (12);
        statusLabel.setBounds (area.removeFromTop (24));
        area.removeFromTop (8);
        auto bottomRow = area.removeFromBottom (34);
        destinationBox.setBounds (bottomRow.removeFromLeft (190));
        bottomRow.removeFromLeft (12);
        importButton.setBounds (bottomRow.removeFromLeft (190));
        area.removeFromBottom (12);
        fileList.setBounds (area);
    }

private:
    struct BrowserEntry
    {
        juce::File file;
        bool isParent{false};
        bool isDirectory{false};
    };

    static void setupButtonColours (juce::TextButton& button)
    {
        button.setColour (juce::TextButton::buttonColourId, panelColour);
        button.setColour (juce::TextButton::buttonOnColourId, panelColour.brighter (0.2f));
        button.setColour (juce::TextButton::textColourOffId, panelOutlineColour);
        button.setColour (juce::TextButton::textColourOnId, panelOutlineColour.brighter (0.15f));
    }

    int getNumRows () override { return static_cast<int> (entries.size ()); }

    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (!juce::isPositiveAndBelow (rowNumber, static_cast<int> (entries.size ())))
            return;

        const auto& entry = entries[static_cast<std::size_t> (rowNumber)];
        if (rowIsSelected)
            g.fillAll (panelOutlineColour.withAlpha (0.18f));

        g.setColour (entry.isDirectory ? panelOutlineColour
                                       : (rowIsSelected ? panelOutlineColour : textColour));
        g.setFont (gp200ui::regular (15.25f));

        juce::String displayText;
        if (entry.isParent)
            displayText = "[..] Parent folder";
        else if (entry.isDirectory)
            displayText = "[Folder] " + entry.file.getFileName ();
        else
            displayText = entry.file.getFileName ();

        g.drawText (displayText, 10, 0, width - 20, height,
                    juce::Justification::centredLeft, true);
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        const bool selectedFile =
            juce::isPositiveAndBelow (lastRowSelected, static_cast<int> (entries.size ()))
            && !entries[static_cast<std::size_t> (lastRowSelected)].isDirectory;
        importButton.setEnabled (selectedFile && !isCurrentlyBusy ());
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        if (!juce::isPositiveAndBelow (row, static_cast<int> (entries.size ())))
            return;

        fileList.selectRow (row);
        const auto entry = entries[static_cast<std::size_t> (row)];
        if (entry.isDirectory)
            scanFolder (entry.file);
        else
            importSelectedFile ();
    }

    void setLibraryRootFromEditor ()
    {
        setLibraryRoot (juce::File (pathEditor.getText ().trim ()));
    }

    void setLibraryRoot (const juce::File& folder)
    {
        if (!folder.isDirectory ())
        {
            statusLabel.setColour (juce::Label::textColourId, statusOffColour);
            statusLabel.setText ("The selected path is not a valid folder.", juce::dontSendNotification);
            return;
        }

        libraryRootFolder = folder;
        if (settings != nullptr)
        {
            settings->setValue ("soundCloneFolder", libraryRootFolder.getFullPathName ());
            settings->saveIfNeeded();
        }
        scanFolder (libraryRootFolder);
    }

    bool canNavigateToParent () const
    {
        if (!libraryRootFolder.isDirectory() || !currentFolder.isDirectory()
            || currentFolder == libraryRootFolder)
            return false;
        return currentFolder.isAChildOf (libraryRootFolder);
    }

    void scanFolder (const juce::File& folder)
    {
        entries.clear ();
        fileList.deselectAllRows ();
        importButton.setEnabled (false);

        if (!folder.isDirectory ())
        {
            statusLabel.setColour (juce::Label::textColourId, statusOffColour);
            statusLabel.setText ("The selected path is not a valid folder.", juce::dontSendNotification);
            fileList.updateContent ();
            fileList.repaint ();
            return;
        }

        currentFolder = folder;
        pathEditor.setText (currentFolder.getFullPathName (), juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, mutedTextColour);

        int folderCount = 0;
        int cloneCount = 0;

        if (canNavigateToParent ())
            entries.push_back ({ currentFolder.getParentDirectory (), true, true });

        juce::Array<juce::File> childFolders;
        currentFolder.findChildFiles (childFolders, juce::File::findDirectories, false);
        std::sort (childFolders.begin (), childFolders.end (),
                   [] (const juce::File& a, const juce::File& b)
                   { return a.getFileName ().compareNatural (b.getFileName (), true) < 0; });

        for (const auto& childFolder : childFolders)
        {
            entries.push_back ({ childFolder, false, true });
            ++folderCount;
        }

        juce::Array<juce::File> allFiles;
        currentFolder.findChildFiles (allFiles, juce::File::findFiles, false);
        std::vector<juce::File> cloneFiles;

        for (const auto& file : allFiles)
        {
            const auto extension = file.getFileExtension ();
            if (extension.equalsIgnoreCase (".clo") || extension.equalsIgnoreCase (".tone"))
                cloneFiles.push_back (file);
        }

        std::sort (cloneFiles.begin (), cloneFiles.end (),
                   [] (const juce::File& a, const juce::File& b)
                   { return a.getFileName ().compareNatural (b.getFileName (), true) < 0; });

        for (const auto& cloneFile : cloneFiles)
        {
            entries.push_back ({ cloneFile, false, false });
            ++cloneCount;
        }

        fileList.updateContent ();
        fileList.repaint ();
        statusLabel.setText (juce::String (folderCount)
                                 + (folderCount == 1 ? " folder, " : " folders, ")
                                 + juce::String (cloneCount)
                                 + (cloneCount == 1 ? " Sound Clone file." : " Sound Clone files."),
                             juce::dontSendNotification);

        if (!entries.empty ())
            fileList.selectRow (0);
    }

    bool isCurrentlyBusy () const
    {
        return isUploadBusy != nullptr && isUploadBusy();
    }

    void importSelectedFile ()
    {
        const auto selectedRow = fileList.getSelectedRow ();
        if (!juce::isPositiveAndBelow (selectedRow, static_cast<int> (entries.size ())))
            return;

        const auto& entry = entries[static_cast<std::size_t> (selectedRow)];
        if (entry.isDirectory)
            return;

        const auto selectedId = destinationBox.getSelectedId ();
        if (selectedId <= 0)
            return;

        activeImportFile = entry.file;
        uploadWasBusy = true;
        statusLabel.setColour (juce::Label::textColourId, panelOutlineColour);
        statusLabel.setText ("Starting import: " + activeImportFile.getFileName (), juce::dontSendNotification);
        importButton.setEnabled (false);
        onImport (activeImportFile, selectedId - 1);
    }

    void timerCallback () override
    {
        const bool busy = isCurrentlyBusy ();
        if (busy)
        {
            uploadWasBusy = true;
            importButton.setEnabled (false);
            statusLabel.setColour (juce::Label::textColourId, panelOutlineColour);
            const auto status = getUploadStatus != nullptr ? getUploadStatus() : juce::String();
            statusLabel.setText (status.isNotEmpty() ? status : juce::String ("Importing Sound Clone..."),
                                 juce::dontSendNotification);
            return;
        }

        if (uploadWasBusy)
        {
            uploadWasBusy = false;
            statusLabel.setColour (juce::Label::textColourId, statusOnColour);
            const auto status = getUploadStatus != nullptr ? getUploadStatus() : juce::String();
            statusLabel.setText (status.isNotEmpty() ? status : juce::String ("Sound Clone import completed."),
                                 juce::dontSendNotification);
        }

        const auto selectedRow = fileList.getSelectedRow ();
        const bool selectedFile =
            juce::isPositiveAndBelow (selectedRow, static_cast<int> (entries.size ()))
            && !entries[static_cast<std::size_t> (selectedRow)].isDirectory;
        importButton.setEnabled (selectedFile);
    }

    gp200ui::SpaceGroteskLookAndFeel spaceGroteskLookAndFeel;
    ImportCallback onImport;
    StatusCallback getUploadStatus;
    BusyCallback isUploadBusy;
    juce::Label pathLabel;
    juce::TextEditor pathEditor;
    juce::TextButton browseButton;
    juce::ListBox fileList;
    juce::ComboBox destinationBox;
    juce::TextButton importButton;
    juce::Label statusLabel;
    std::vector<BrowserEntry> entries;
    juce::File libraryRootFolder;
    juce::File currentFolder;
    juce::File activeImportFile;
    bool uploadWasBusy{false};
    std::unique_ptr<juce::FileChooser> folderChooser;
    std::unique_ptr<juce::PropertiesFile> settings;
};} // namespace


//==============================================================================
void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::setItems (std::vector<Item> newItems)
{
    items = std::move (newItems);
    const auto selectedStillExists = std::any_of (items.begin (), items.end (), [this] (const Item& item)
    { return item.blockIndex == selectedBlockIndex; });
    if (!selectedStillExists)
        selectedBlockIndex = items.empty () ? -1 : items.front ().blockIndex;
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
    auto area = getLocalBounds ().reduced (54, 14);
    constexpr int gap = 9;
    const auto count = static_cast<int> (items.size ());
    const auto tileWidth = juce::jmax (54, (area.getWidth () - gap * (count - 1)) / count);
    const auto tileHeight = juce::jmin (84, area.getHeight ());
    const auto totalWidth = tileWidth * count + gap * (count - 1);
    const auto startX = area.getCentreX () - totalWidth / 2;
    return {startX + itemIndex * (tileWidth + gap), area.getCentreY () - tileHeight / 2, tileWidth, tileHeight};
}

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

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds ().toFloat ().reduced (0.5f);
    g.setColour (juce::Colour (0xff161a1d));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (juce::Colour (0xff4b4f52));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
    if (items.empty ()) return;

    const auto first = getTileBounds (0);
    const auto chainY = first.getCentreY ();
    g.setColour (juce::Colour (0xff7b8083));
    g.drawLine (36.0f, static_cast<float> (chainY), static_cast<float> (getWidth () - 36), static_cast<float> (chainY), 2.0f);
    g.setFont (gp200ui::regular (12.75f));
    g.setColour (juce::Colour (0xffb9bdc0));
    g.drawText ("IN", 12, chainY - 12, 34, 24, juce::Justification::centred);
    g.drawText ("OUT", getWidth () - 46, chainY - 12, 34, 24, juce::Justification::centred);

    for (int i = 0; i < static_cast<int> (items.size ()); ++i)
    {
        const auto& item = items[static_cast<std::size_t> (i)];
        const auto tile = getTileBounds (i);
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
        const auto iconArea = tile.toFloat().reduced (10.0f, 8.0f).withTrimmedBottom (24.0f);
        drawRibbonBlockIcon (g, item.blockName, iconArea, displayColour);

        g.setColour (displayColour);
        g.setFont (gp200ui::semibold (14.25f));
        g.drawText (item.blockName,
                    tile.withTrimmedTop (tile.getHeight() - 24).reduced (3, 2),
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

    if (dragging && dragTargetPosition >= 0)
    {
        const auto count = static_cast<int> (items.size ());
        const auto lineX = dragTargetPosition >= count ? getTileBounds (count - 1).getRight () + 5
                                                       : getTileBounds (dragTargetPosition).getX () - 5;
        g.setColour (juce::Colour (0xffffa42a));
        g.fillRoundedRectangle (static_cast<float> (lineX - 2), static_cast<float> (first.getY () - 4),
                                4.0f, static_cast<float> (first.getHeight () + 8), 2.0f);
    }
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::mouseDown (const juce::MouseEvent& event)
{
    pressedItemIndex = getItemIndexAt (event.getPosition ());
    mouseDownPosition = event.getPosition ();
    dragTargetPosition = -1;
    dragging = false;
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (pressedItemIndex < 0) return;
    if (!dragging && event.getPosition ().getDistanceFrom (mouseDownPosition) >= 6.0f)
        dragging = true;
    if (dragging)
    {
        dragTargetPosition = getTargetPositionAtX (event.x);
        repaint ();
    }
}

void AudioPluginAudioProcessorEditor::EffectChainRibbonComponent::mouseUp (const juce::MouseEvent&)
{
    if (pressedItemIndex < 0 || pressedItemIndex >= static_cast<int> (items.size ()))
    {
        dragging = false; dragTargetPosition = -1; return;
    }
    const auto blockIndex = items[static_cast<std::size_t> (pressedItemIndex)].blockIndex;
    if (dragging && dragTargetPosition >= 0)
    {
        if (onBlockReordered) onBlockReordered (blockIndex, dragTargetPosition);
    }
    else if (onBlockSelected)
        onBlockSelected (blockIndex);
    dragging = false;
    dragTargetPosition = -1;
    pressedItemIndex = -1;
    repaint ();
}

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (
    AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      midiConnection (p.getMidiConnection())
{
    setSize (960, 390);

    addAndMakeVisible (previousBankButton);
    addAndMakeVisible (previousPresetButton);
    addAndMakeVisible (presetSlotButton);
    addAndMakeVisible (nextPresetButton);
    addAndMakeVisible (nextBankButton);

    addAndMakeVisible (compareAButton);
addAndMakeVisible (compareBButton);
addAndMakeVisible (savePresetButton);
addAndMakeVisible (recallPresetButton);
addAndMakeVisible (storePresetButton);
addAndMakeVisible (importPrstButton);
addAndMakeVisible (exportPrstButton);
addAndMakeVisible (importIRButton);
addAndMakeVisible (soundCloneButton);
addAndMakeVisible (userIRSlotBox);
    addAndMakeVisible (patchVolumeSlider);
    addAndMakeVisible (panSlider);
    addAndMakeVisible (tempoSlider);
	addAndMakeVisible (tapTempoButton);
    addAndMakeVisible (presetNameEditor);
    addAndMakeVisible (tunerButton);
    addAndMakeVisible (allBlocksOffButton);
	addAndMakeVisible (toneMatchButton);
	
	addChildComponent(tunerDisplay);
tunerDisplay.setVisible(false);

    addAndMakeVisible (effectChainRibbon);
    addAndMakeVisible (effectsViewport);

    effectChainRibbon.onBlockSelected = [this] (int blockIndex)
    {
        // Clicking the selected block again closes the parameter editor.
        selectEffectBlock (selectedEffectBlockIndex == blockIndex ? -1 : blockIndex);
    };
    effectChainRibbon.onBlockReordered = [this] (int blockIndex, int targetPosition)
    {
        moveEffectBlockToPosition (blockIndex, targetPosition);
    };

    effectsViewport.setViewedComponent (&effectsContent, false);
    effectsViewport.setScrollBarsShown (true, false);

    dropIndicator.setInterceptsMouseClicks (false, false);
    dropIndicator.setAlwaysOnTop (true);
    dropIndicator.setVisible (false);
    effectsContent.addChildComponent (dropIndicator);

    auto setupButton = [] (juce::TextButton& button)
    {
        button.setColour (juce::TextButton::buttonColourId, panelColour);
        button.setColour (juce::TextButton::buttonOnColourId, panelColour.brighter (0.2f));
        button.setColour (juce::TextButton::textColourOffId, panelOutlineColour);
        button.setColour (juce::TextButton::textColourOnId, panelOutlineColour);
    };

    setupButton (previousBankButton);
    setupButton (previousPresetButton);
    setupButton (presetSlotButton);
    setupButton (nextPresetButton);
    setupButton (nextBankButton);
	
	setupButton (compareAButton);
setupButton (compareBButton);

   setupButton (savePresetButton);
setupButton (recallPresetButton);
setupButton (storePresetButton);
setupButton (importPrstButton);
setupButton (exportPrstButton);
setupButton (importIRButton);
setupButton (soundCloneButton);
setupButton (tunerButton);
setupButton (tapTempoButton);

tapTempoButton.setColour (
    juce::TextButton::textColourOffId,
    panelOutlineColour
);

tapTempoButton.setColour (
    juce::TextButton::textColourOnId,
    panelOutlineColour.brighter (0.15f)
);

setupButton (allBlocksOffButton);
setupButton (toneMatchButton);

    presetSlotButton.setTooltip ("Click the slot number to show all GP-200 presets");
    presetSlotButton.setMouseClickGrabsKeyboardFocus (false);
    presetSlotButton.setTriggeredOnMouseDown (false);
    presetSlotButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    presetSlotButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    presetSlotButton.setColour (juce::TextButton::textColourOffId, textColour);
    presetSlotButton.setColour (juce::TextButton::textColourOnId, panelOutlineColour);
    presetSlotButton.onClick = [this] { openPresetSlotMenu (); };

    applyInterfaceTypography ();

// Store to GP-200 is the main hardware action.
storePresetButton.setColour (
    juce::TextButton::buttonColourId,
    panelOutlineColour.withAlpha (0.12f)
);

storePresetButton.setColour (
    juce::TextButton::buttonOnColourId,
    panelOutlineColour.withAlpha (0.22f)
);

storePresetButton.setColour (
    juce::TextButton::textColourOffId,
    panelOutlineColour
);

storePresetButton.setColour (
    juce::TextButton::textColourOnId,
    panelOutlineColour.brighter (0.15f)
);
    updateTunerButtonText ();
    updateAllBlocksOffButtonText ();

    patchVolumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    patchVolumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
    patchVolumeSlider.setRange (0.0, 100.0, 1.0);
    patchVolumeSlider.setValue (50.0, juce::dontSendNotification);
    patchVolumeSlider.setNumDecimalPlacesToDisplay (0);
    patchVolumeSlider.setDoubleClickReturnValue (true, 50.0);

    patchVolumeSlider.setColour (juce::Slider::backgroundColourId, panelColour.brighter (0.25f));
    patchVolumeSlider.setColour (juce::Slider::trackColourId, panelOutlineColour);
    patchVolumeSlider.setColour (juce::Slider::thumbColourId, panelOutlineColour);
    patchVolumeSlider.setColour (juce::Slider::textBoxTextColourId, textColour);
    patchVolumeSlider.setColour (juce::Slider::textBoxBackgroundColourId, panelColour);
    patchVolumeSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    panSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
    panSlider.setRange (-100.0, 100.0, 1.0);
    panSlider.setValue (0.0, juce::dontSendNotification);
    panSlider.setNumDecimalPlacesToDisplay (0);
    panSlider.setDoubleClickReturnValue (true, 0.0);

    panSlider.setColour (juce::Slider::backgroundColourId, panelColour.brighter (0.25f));
    panSlider.setColour (juce::Slider::trackColourId, panelOutlineColour);
    panSlider.setColour (juce::Slider::thumbColourId, panelOutlineColour);
    panSlider.setColour (juce::Slider::textBoxTextColourId, textColour);
    panSlider.setColour (juce::Slider::textBoxBackgroundColourId, panelColour);
    panSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    panSlider.textFromValueFunction = [] (double value)

    {
        const auto pan = static_cast<int> (value);

        if (pan == 0)
            return juce::String ("C");

        if (pan < 0)
            return juce::String ("L") + juce::String (-pan);

        return juce::String ("R") + juce::String (pan);
    };

    tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 18);
    tempoSlider.setRange (40.0, 250.0, 1.0);
    tempoSlider.setValue (120.0, juce::dontSendNotification);
    tempoSlider.setNumDecimalPlacesToDisplay (0);
    tempoSlider.setDoubleClickReturnValue (true, 120.0);

    tempoSlider.setColour (juce::Slider::backgroundColourId, panelColour.brighter (0.25f));
    tempoSlider.setColour (juce::Slider::trackColourId, panelOutlineColour);
    tempoSlider.setColour (juce::Slider::thumbColourId, panelOutlineColour);
    tempoSlider.setColour (juce::Slider::textBoxTextColourId, textColour);
    tempoSlider.setColour (juce::Slider::textBoxBackgroundColourId, panelColour);
    tempoSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    tempoSlider.textFromValueFunction = [] (double value)
    { return juce::String (static_cast<int> (value)) + " BPM"; };
	
	

    presetNameEditor.setMultiLine (false);
    presetNameEditor.setReturnKeyStartsNewLine (false);
    presetNameEditor.setInputRestrictions (gp200::presetNameMaxLength);
    presetNameEditor.setSelectAllWhenFocused (true);
    presetNameEditor.setFont (gp200ui::semibold (17.0f));
    presetNameEditor.setJustification (juce::Justification::centredLeft);
    presetNameEditor.setTextToShowWhenEmpty ("Preset name", mutedTextColour);

    presetNameEditor.setColour (
    juce::TextEditor::backgroundColourId,
    juce::Colours::transparentBlack
);
    presetNameEditor.setColour (juce::TextEditor::textColourId, textColour);
    presetNameEditor.setColour (juce::TextEditor::highlightColourId, panelOutlineColour.withAlpha (0.35f));
    presetNameEditor.setColour (juce::TextEditor::highlightedTextColourId, textColour);

    // Sin recuadro interior.
    presetNameEditor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    presetNameEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);

    presetNameEditor.setIndents (4, 0);

    patchVolumeSlider.onValueChange = [this] { sendPatchVolumeFromSlider (); };

    panSlider.onValueChange = [this] { sendPatchPanFromSlider (); };

    tempoSlider.onValueChange = [this] { sendPatchTempoFromSlider (); };
	
	tapTempoButton.onClick = [this] { handleTapTempo (); };

    presetNameEditor.onReturnKey = [this]
    {
        if (!midiConnection.isConnected ())
        {
            offlinePreset.patchName = presetNameEditor.getText ().trim ().substring (0, gp200::presetNameMaxLength);
            offlinePresetDirty = true;
            ++offlinePresetRevision;
            effectsStatusText = "Offline preset name updated";
            repaint ();
            return;
        }

        storeCurrentPresetToGP200 ();
    };

    tunerButton.onClick = [this] { toggleTuner (); };

    allBlocksOffButton.onClick = [this] { toggleAllBlocksOff (); };
	
	toneMatchButton.onClick =
    [this]
    {
        if (toneMatchPanel == nullptr)
        {
            toneMatchPanel =
                std::make_unique<ToneMatchPanel> (
                    processorRef);

            toneMatchPanel->setCloseCallback (
                [this]
                {
                    if (toneMatchPanel != nullptr)
                        toneMatchPanel->setVisible (false);

                    scheduleEditorHeightUpdate ();
                });

            addAndMakeVisible (*toneMatchPanel);

            toneMatchPanel->setBounds (
                getLocalBounds().reduced (50, 60));

            toneMatchPanel->toFront (true);
            scheduleEditorHeightUpdate ();
        }
        else
        {
            toneMatchPanel->setVisible (true);

            toneMatchPanel->setBounds (
                getLocalBounds().reduced (50, 60));

            toneMatchPanel->toFront (true);
            scheduleEditorHeightUpdate ();
        }
    };

    previousBankButton.onClick = [this] { loadPreviousBank (); };
    previousPresetButton.onClick = [this] { loadPreviousPreset (); };

    nextPresetButton.onClick = [this]
    {
        openPresetMenuWhenScanFinishes = false;
        midiConnection.cancelPresetNameScan ();
        loadNextPreset ();
    };
    nextBankButton.onClick = [this] { loadNextBank (); };
	
	compareAButton.onClick = [this]
{
    selectCompareSnapshot (CompareSnapshot::A);
};

compareBButton.onClick = [this]
{
    selectCompareSnapshot (CompareSnapshot::B);
};

    savePresetButton.onClick = [this] { saveCurrentPresetToProject (); };

    recallPresetButton.onClick = [this] { recallSavedPresetToGP200 (); };
    storePresetButton.onClick = [this] { storeCurrentPresetToGP200 (); };
	importPrstButton.onClick =
    [this]
    {
        openPrstFileChooser ();
    };

    exportPrstButton.onClick =
    [this]
    {
        openExportPrstFileChooser ();
    };

    for (int i = 0; i < gp200::userIRCount; ++i)
        userIRSlotBox.addItem ("User IR " + juce::String (i + 1), i + 1);
    userIRSlotBox.setSelectedId (1, juce::dontSendNotification);
    userIRSlotBox.setColour (juce::ComboBox::backgroundColourId, panelColour);
    userIRSlotBox.setColour (juce::ComboBox::textColourId, panelOutlineColour);
    userIRSlotBox.setColour (juce::ComboBox::outlineColourId, panelOutlineColour);

    importIRButton.onClick = [this] { openIRFileChooser (); };
    soundCloneButton.onClick = [this] { openSoundCloneWindow (); };
	updateCompareSnapshotButtons ();

  offlinePreset = makeDefaultOfflinePreset ();
  offlinePresetRevision = 1;
  processorRef.ensureGP200Connection();

lastInitialPresetRequestMs =
    juce::Time::getMillisecondCounterHiRes();

startTimerHz (idleTimerHz);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor ()
{
    clearInterfaceTypography ();
}

void AudioPluginAudioProcessorEditor::applyInterfaceTypography ()
{
    interfaceLookAndFeel = std::make_unique<gp200ui::SpaceGroteskLookAndFeel> ();
    patchSettingsLookAndFeel = std::make_unique<juce::LookAndFeel_V4> ();

    // Space Grotesk is the default for the complete editor and all child
    // components. PATCH SETTINGS deliberately keeps JUCE's original font.
    setLookAndFeel (interfaceLookAndFeel.get ());

    patchVolumeSlider.setLookAndFeel (patchSettingsLookAndFeel.get ());
    panSlider.setLookAndFeel (patchSettingsLookAndFeel.get ());
    tempoSlider.setLookAndFeel (patchSettingsLookAndFeel.get ());
    tapTempoButton.setLookAndFeel (patchSettingsLookAndFeel.get ());

    presetNameEditor.setFont (gp200ui::semibold (17.0f));
}

void AudioPluginAudioProcessorEditor::clearInterfaceTypography ()
{
    patchVolumeSlider.setLookAndFeel (nullptr);
    panSlider.setLookAndFeel (nullptr);
    tempoSlider.setLookAndFeel (nullptr);
    tapTempoButton.setLookAndFeel (nullptr);

    setLookAndFeel (nullptr);
    patchSettingsLookAndFeel.reset ();
    interfaceLookAndFeel.reset ();
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    // ============================================================
    // Plugin title
    // ============================================================

    g.setFont (gp200ui::semibold (25.0f));
    g.setColour (textColour);
    g.drawText ("GP200",
                20, 8, 92, 30,
                juce::Justification::centredLeft);

    g.setFont (gp200ui::regular (22.0f));
    g.setColour (mutedTextColour);
    g.drawText ("VST",
                112, 9, 74, 29,
                juce::Justification::centredLeft);

    g.setFont (gp200ui::semibold (14.75f));
    g.setColour (panelOutlineColour);
    g.drawText ("0.1",
                184, 11, 42, 26,
                juce::Justification::centredLeft);


    const bool connected = midiConnection.isConnected ();
    g.setColour (connected ? statusOnColour : statusOffColour);
    g.fillEllipse (244.0f, 17.0f, 12.0f, 12.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (244.0f, 17.0f, 12.0f, 12.0f, 1.0f);

    // ============================================================
    // Main top container
    // ============================================================

    const juce::Rectangle<int> topPanel
    {
        8,
        42,
        getWidth() - 16,
        194
    };

    g.setColour (juce::Colour (0xff303030));
    g.fillRoundedRectangle (topPanel.toFloat(), 8.0f);

    g.setColour (juce::Colour (0xff1d1d1d));
    g.drawRoundedRectangle (
        topPanel.toFloat().reduced (0.5f),
        8.0f,
        1.0f
    );

    // ============================================================
    // Current preset card
    // ============================================================

    const juce::Rectangle<int> currentPresetBox
    {
        18,
        52,
        400,
        124
    };

    g.setColour (juce::Colour (0xff202528));
    g.fillRoundedRectangle (currentPresetBox.toFloat(), 7.0f);

    g.setColour (panelOutlineColour);
    g.drawRoundedRectangle (
        currentPresetBox.toFloat().reduced (0.5f),
        7.0f,
        1.25f
    );

    // The current slot is rendered by presetSlotButton.
    // Do not draw it here as well, otherwise the text appears duplicated.

    // Divider between current and saved preset information.
    g.drawHorizontalLine (
        currentPresetBox.getY() + 76,
        static_cast<float> (currentPresetBox.getX() + 16),
        static_cast<float> (currentPresetBox.getRight() - 16)
    );

    g.setFont (gp200ui::regular (14.25f));
    g.setColour (mutedTextColour.withAlpha (0.72f));

    g.drawText (
    "Snapshot:",
    currentPresetBox.getX() + 16,
    currentPresetBox.getY() + 88,
    68,
    24,
    juce::Justification::centredLeft
);

    g.setFont (gp200ui::semibold (15.25f));
    g.setColour (textColour);

    auto savedPresetText = getSavedPresetCompactText();

    if (savedPresetText.isEmpty())
        savedPresetText = "unknown";

    g.drawText (
    savedPresetText,
    currentPresetBox.getX() + 154,
    currentPresetBox.getY() + 88,
    currentPresetBox.getWidth() - 170,
    24,
    juce::Justification::centredLeft
);

    // ============================================================
    // DAW / GP-200 action buttons
    // ============================================================
    // The four buttons draw their own backgrounds and borders.
    // No additional panel or Store outline is painted here.

    // ============================================================
    // Patch settings
    // ============================================================

    const juce::Rectangle<int> patchPanel
    {
        704,
        52,
        238,
        124
    };

    g.setColour (juce::Colour (0xff202528));
    g.fillRoundedRectangle (patchPanel.toFloat(), 7.0f);

    g.setColour (juce::Colour (0xff56595b));
    g.drawRoundedRectangle (
        patchPanel.toFloat().reduced (0.5f),
        7.0f,
        1.0f
    );

    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.setColour (panelOutlineColour);

    g.drawText (
        "PATCH SETTINGS",
        patchPanel.getX() + 14,
        patchPanel.getY() + 6,
        patchPanel.getWidth() - 28,
        22,
        juce::Justification::centredLeft
    );

    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.setColour (textColour);

    g.drawText (
        "VOL",
        patchPanel.getX() + 14,
        patchPanel.getY() + 34,
        38,
        18,
        juce::Justification::centredLeft
    );

    g.drawText (
        "PAN",
        patchPanel.getX() + 14,
        patchPanel.getY() + 68,
        38,
        18,
        juce::Justification::centredLeft
    );

    g.drawText (
        "BPM",
        patchPanel.getX() + 14,
        patchPanel.getY() + 102,
        38,
        18,
        juce::Justification::centredLeft
    );

    // ============================================================
    // Utility bar
    // ============================================================

    const juce::Rectangle<int> utilityBar
    {
        18,
        184,
        getWidth() - 36,
        42
    };

    g.setColour (juce::Colour (0xff252a2d));
    g.fillRoundedRectangle (utilityBar.toFloat(), 6.0f);

    g.setColour (juce::Colour (0xff44484a));
    g.drawRoundedRectangle (
        utilityBar.toFloat().reduced (0.5f),
        6.0f,
        1.0f
    );
}

void AudioPluginAudioProcessorEditor::resized ()
{
    // ============================================================
    // Current preset
    // ============================================================

    previousBankButton.setBounds (30, 70, 50, 42);
    previousPresetButton.setBounds (86, 70, 30, 42);
    presetSlotButton.setBounds (122, 70, 64, 42);
    nextPresetButton.setBounds (314, 70, 30, 42);
    nextBankButton.setBounds (350, 70, 50, 42);

    compareAButton.setBounds (96, 142, 30, 24);
    compareBButton.setBounds (132, 142, 30, 24);

    presetNameEditor.setBounds (192, 70, 116, 42);

    // ============================================================
    // DAW / GP-200 actions
    // ============================================================

const juce::Rectangle<int> actionsArea {
    428, 54, 270, 114
};

constexpr int horizontalGap = 10;
constexpr int verticalGap = 10;

const int buttonWidth =
    (actionsArea.getWidth () - horizontalGap) / 2;

const int buttonHeight =
    (actionsArea.getHeight () - verticalGap) / 2;

savePresetButton.setBounds (
    actionsArea.getX (),
    actionsArea.getY (),
    buttonWidth,
    buttonHeight);

recallPresetButton.setBounds (
    actionsArea.getX () + buttonWidth + horizontalGap,
    actionsArea.getY (),
    buttonWidth,
    buttonHeight);

storePresetButton.setBounds (
    actionsArea.getX (),
    actionsArea.getY () + buttonHeight + verticalGap,
    buttonWidth,
    buttonHeight);

const auto prstX = actionsArea.getX () + buttonWidth + horizontalGap;
const auto prstY = actionsArea.getY () + buttonHeight + verticalGap;
constexpr int prstButtonGap = 4;
const auto prstButtonHeight = (buttonHeight - prstButtonGap) / 2;

importPrstButton.setBounds (
    prstX,
    prstY,
    buttonWidth,
    prstButtonHeight);

exportPrstButton.setBounds (
    prstX,
    prstY + prstButtonHeight + prstButtonGap,
    buttonWidth,
    buttonHeight - prstButtonHeight - prstButtonGap);

    userIRSlotBox.setBounds (418, 191, 130, 28);
    importIRButton.setBounds (558, 191, 140, 28);

    // ============================================================
    // Patch settings
    // ============================================================

    patchVolumeSlider.setBounds (758, 84, 170, 20);
    panSlider.setBounds (758, 118, 170, 20);
    tempoSlider.setBounds (758, 152, 118, 20);
tapTempoButton.setBounds (882, 150, 46, 24);

    // ============================================================
    // Utility bar
    // ============================================================

    tunerButton.setBounds (30, 191, 130, 28);
allBlocksOffButton.setBounds (170, 191, 120, 28);
toneMatchButton.setBounds (708, 191, 110, 28);
soundCloneButton.setBounds (828, 191, 110, 28);

tunerDisplay.setBounds (
    418,
    189,
    getWidth() - 448,
    32);

    // ============================================================
    // Effects list
    // ============================================================

    effectChainRibbon.setBounds (20, 246, getWidth () - 40, 116);

    const bool hasSelectedBlock = selectedEffectBlockIndex >= 0;
    effectsViewport.setVisible (hasSelectedBlock);

    if (hasSelectedBlock)
        effectsViewport.setBounds (20, 372, getWidth () - 40, juce::jmax (0, getHeight () - 392));
    else
        effectsViewport.setBounds (20, 372, getWidth () - 40, 0);


if (toneMatchPanel != nullptr)
{
    toneMatchPanel->setBounds (
        getLocalBounds().reduced (50, 60));
}
    layoutEffectBlocks();
}

//==============================================================================
void AudioPluginAudioProcessorEditor::timerCallback ()
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const bool connectedNow = midiConnection.isConnected ();
    if (connectedNow != lastConnectionIndicatorState)
    {
        lastConnectionIndicatorState = connectedNow;
        effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
        repaint ();
    }
	
	  // La conexión MIDI puede estar abierta antes de que el GP-200
    // haya respondido a la primera petición. Reintentamos solamente
    // mientras todavía no exista ningún preset vivo recibido.
    if (midiConnection.isConnected()
        && midiConnection.getCurrentPresetDumpSize() == 0
        && nowMs - lastInitialPresetRequestMs >= 200.0)
    {
        lastInitialPresetRequestMs = nowMs;
        midiConnection.requestCurrentPresetFromGP200();
    }

    midiConnection.processIRUpload ();
    midiConnection.processSoundCloneUpload ();
    midiConnection.processPendingLivePresetRefresh ();

    const bool transferInProgress =
        midiConnection.isIRUploadInProgress () ||
        midiConnection.isSoundCloneUploadInProgress ();

    importIRButton.setEnabled (!transferInProgress);
    soundCloneButton.setEnabled (!transferInProgress);

    if (midiConnection.isSoundCloneUploadInProgress ())
        effectsStatusText = midiConnection.getSoundCloneUploadStatusText ();

    if (tapFlashUntilMs > 0.0 && nowMs >= tapFlashUntilMs)
    {
        tapFlashUntilMs = 0.0;

        tapTempoButton.setColour (juce::TextButton::buttonColourId, panelColour);
        tapTempoButton.setColour (juce::TextButton::buttonOnColourId,
                                  panelColour.brighter (0.2f));
        tapTempoButton.setColour (juce::TextButton::textColourOffId,
                                  panelOutlineColour);
        tapTempoButton.setColour (juce::TextButton::textColourOnId,
                                  panelOutlineColour.brighter (0.15f));
        tapTempoButton.repaint();
    }

    midiConnection.processPresetNameScan ();

    const auto scanRevision = midiConnection.getPresetNameScanRevision ();
    if (scanRevision != lastPresetNameScanRevision)
    {
        lastPresetNameScanRevision = scanRevision;

        if (openPresetMenuWhenScanFinishes && !midiConnection.isPresetNameScanRunning ())
        {
            openPresetMenuWhenScanFinishes = false;
            showPresetSlotMenu ();
        }
    }

    if (openPresetMenuWhenScanFinishes && midiConnection.isPresetNameScanRunning ())
    {
        const auto completed = juce::roundToInt (midiConnection.getPresetNameScanProgress () * 256.0f);
        effectsStatusText = "Reading GP-200 preset names: "
                            + juce::String (juce::jlimit (0, 256, completed)) + "/256";
    }

    if (presetRestoreInProgress)
    {
        constexpr int restoreStepsPerTimerTick = 4;

        for (int i = 0; i < restoreStepsPerTimerTick && presetRestoreInProgress; ++i)
            processFullPresetRestoreStep ();

        if (presetRestoreInProgress)
        {
            repaint ();
            return;
        }
    }
    else
    {
        midiConnection.requestPresetNameForCurrentSlotIfNeeded ();
    }

    syncPresetNameEditorFromCurrentPreset ();

    {
        const auto displayedSlot = midiConnection.getCurrentSlot ();
        const auto displayedSlotText = displayedSlot >= 0 ? formatSlotCompact (displayedSlot) : juce::String ("--");
        if (presetSlotButton.getButtonText () != displayedSlotText)
            presetSlotButton.setButtonText (displayedSlotText);
    }

    updateEffectBlocksUI ();
	
	if (tunerIsOn)
{
    const auto result = processorRef.getTunerResult ();

    if (result.valid)
    {
        tunerDisplay.setTunerReading (
            midiNoteToName (result.midiNote),
            result.frequencyHz,
            result.cents,
            true);
    }
    else
    {
        tunerDisplay.clearReading ();
    }
}

    repaint ();
}

//==============================================================================

void AudioPluginAudioProcessorEditor::selectCompareSnapshot (
    CompareSnapshot snapshot)
{
    if (presetRestoreInProgress)
    {
        effectsStatusText =
            "A/B selection disabled while Recall is restoring";

        repaint ();
        return;
    }

    selectedCompareSnapshot = snapshot;

    updateCompareSnapshotButtons ();

    effectsStatusText =
        "Selected DAW snapshot " +
        getSelectedCompareSnapshotLabel ();

    effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();

    updateEffectBlocksUI ();
    repaint ();
}

void AudioPluginAudioProcessorEditor::updateCompareSnapshotButtons ()
{
    const auto selectedA =
        selectedCompareSnapshot == CompareSnapshot::A;

    const auto selectedB =
        selectedCompareSnapshot == CompareSnapshot::B;

    auto updateButton =
        [] (juce::TextButton& button, bool selected)
    {
        button.setColour (
            juce::TextButton::buttonColourId,
            selected
                ? panelOutlineColour.withAlpha (0.28f)
                : panelColour);

        button.setColour (
            juce::TextButton::buttonOnColourId,
            selected
                ? panelOutlineColour.withAlpha (0.38f)
                : panelColour.brighter (0.2f));

        button.setColour (
            juce::TextButton::textColourOffId,
            selected ? textColour : panelOutlineColour);

        button.setColour (
            juce::TextButton::textColourOnId,
            selected ? textColour : panelOutlineColour);

        button.repaint ();
    };

    updateButton (compareAButton, selectedA);
    updateButton (compareBButton, selectedB);
}

int AudioPluginAudioProcessorEditor::
    getSelectedCompareSnapshotIndex () const
{
    return selectedCompareSnapshot == CompareSnapshot::A
               ? 0
               : 1;
}

juce::String AudioPluginAudioProcessorEditor::
    getSelectedCompareSnapshotLabel () const
{
    return selectedCompareSnapshot == CompareSnapshot::A
               ? "A"
               : "B";
}

void AudioPluginAudioProcessorEditor::saveCurrentPresetToProject ()
{
    const auto snapshotIndex = getSelectedCompareSnapshotIndex ();

    if (!midiConnection.isConnected ())
    {
        const auto visiblePresetName =
            presetNameEditor.getText ().trim ().substring (0, gp200::presetNameMaxLength);

        if (visiblePresetName.isNotEmpty ())
            offlinePreset.patchName = visiblePresetName;

        const auto snapshotData = serialiseOfflineSnapshot (
            offlinePreset,
            static_cast<int> (patchVolumeSlider.getValue ()),
            static_cast<int> (panSlider.getValue ()),
            static_cast<int> (tempoSlider.getValue ()));

        processorRef.setGP200PresetSnapshotState (
            snapshotIndex,
            -1,
            offlinePreset.patchName,
            snapshotData);

        offlinePresetDirty = false;
        effectsStatusText = "Saved offline preset snapshot " +
                            getSelectedCompareSnapshotLabel () +
                            " to DAW";
        updateEffectBlocksUI ();
        repaint ();
        return;
    }

    const auto currentSlot = midiConnection.getCurrentSlot ();

    if (currentSlot < 0)
        return;

    const auto presetData = midiConnection.getCurrentPresetDumpDataCopy ();

    if (presetData.getSize () == 0)
    {
        effectsStatusText = "Save Preset failed: no full preset data captured yet";
        repaint ();
        return;
    }

    processorRef.setGP200PresetSnapshotState (
        snapshotIndex,
        currentSlot,
        midiConnection.getCurrentPresetName (),
        presetData);

    effectsStatusText = "Saved full preset snapshot " +
                        getSelectedCompareSnapshotLabel () +
                        " to DAW";
    updateEffectBlocksUI ();
    repaint ();
}

void AudioPluginAudioProcessorEditor::openExportPrstFileChooser ()
{
    gp200::GP200Preset preset;

    if (!midiConnection.isConnected ())
    {
        preset = offlinePreset;
    }
    else
    {
        const auto liveData = midiConnection.getCurrentPresetDumpDataCopy ();
        preset = gp200::GP200PresetCodec::decodeLivePresetDump (liveData);
    }

    if (!preset.isValid)
    {
        effectsStatusText = "Export PRST failed: no valid current preset";
        repaint ();
        return;
    }

    const auto visibleName = presetNameEditor.getText ().trim ().substring (
        0, gp200::presetNameMaxLength);
    if (visibleName.isNotEmpty ())
        preset.patchName = visibleName;

    auto suggestedName = preset.patchName.trim ();
    if (suggestedName.isEmpty () || suggestedName == "unknown")
        suggestedName = "GP200 preset";

    suggestedName = suggestedName.replaceCharacters ("\\/:*?\"<>|", "_________");

    exportPrstFileChooser = std::make_unique<juce::FileChooser> (
        "Export GP-200 preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile (suggestedName + ".prst"),
        "*.prst");

    exportPrstFileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safeThis = juce::Component::SafePointer<AudioPluginAudioProcessorEditor> (this)]
        (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            auto file = chooser.getResult ();
            if (file == juce::File ())
                return;

            if (!file.hasFileExtension ("prst"))
                file = file.withFileExtension ("prst");

            safeThis->exportCurrentPresetToPrst (file);
        });
}

void AudioPluginAudioProcessorEditor::exportCurrentPresetToPrst (
    const juce::File& file)
{
    gp200::GP200Preset preset;

    if (!midiConnection.isConnected ())
    {
        preset = offlinePreset;
    }
    else
    {
        preset = gp200::GP200PresetCodec::decodeLivePresetDump (
            midiConnection.getCurrentPresetDumpDataCopy ());
    }

    if (!preset.isValid)
    {
        effectsStatusText = "Export PRST failed: no valid current preset";
        repaint ();
        return;
    }

    const auto visibleName = presetNameEditor.getText ().trim ().substring (
        0, gp200::presetNameMaxLength);
    if (visibleName.isNotEmpty ())
    {
        preset.patchName = visibleName;
        if (!midiConnection.isConnected ())
            offlinePreset.patchName = visibleName;
    }

    const auto encoded = gp200::GP200PresetCodec::encodePrstFile (preset);
    if (encoded.getSize () != 1224)
    {
        effectsStatusText = "Export PRST failed: encoder returned invalid data";
        repaint ();
        return;
    }

    if (!file.replaceWithData (encoded.getData (), encoded.getSize ()))
    {
        effectsStatusText = "Export PRST failed: could not write " + file.getFileName ();
        repaint ();
        return;
    }

    if (!midiConnection.isConnected ())
        offlinePreset.prstRawSource = encoded;

    effectsStatusText = "Exported PRST: " + file.getFileName ();
    repaint ();
}

void AudioPluginAudioProcessorEditor::openIRFileChooser ()
{
    if (presetRestoreInProgress || midiConnection.isIRUploadInProgress ())
    {
        effectsStatusText = "Import IR unavailable: another transfer is in progress";
        repaint ();
        return;
    }

    irFileChooser = std::make_unique<juce::FileChooser> (
        "Import GP-200 User IR", juce::File{}, "*.wav");

    irFileChooser->launchAsync (
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser)
{
    const auto file = chooser.getResult();

    if (!file.existsAsFile())
        return;

    if (!file.hasFileExtension("wav"))
    {
        effectsStatusText =
            "Import IR failed: the selected file is not a WAV";

        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Invalid IR",
            "Select a WAV file.");

        repaint();
        return;
    }

    importIRFile(file);
});
}

void AudioPluginAudioProcessorEditor::importIRFile(
    const juce::File& file)
{
    const auto selectedId =
        userIRSlotBox.getSelectedId();

    if (selectedId <= 0)
    {
        effectsStatusText =
            "Import IR failed: choose a User IR slot";

        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Invalid IR destination",
            "Choose a User IR slot before importing the file.");

        repaint();
        return;
    }

    if (!midiConnection.startIRUpload(
            file,
            selectedId - 1))
    {
        effectsStatusText =
            midiConnection.getIRUploadStatusText();

        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Invalid IR",
            effectsStatusText);

        repaint();
        return;
    }

    effectsStatusText =
        juce::String("Import IR started: ")
        + file.getFileNameWithoutExtension()
        + " -> User IR "
        + juce::String(selectedId);

    repaint();
}


void AudioPluginAudioProcessorEditor::openSoundCloneWindow ()
{
    if (presetRestoreInProgress ||
        midiConnection.isIRUploadInProgress () ||
        midiConnection.isSoundCloneUploadInProgress ())
    {
        effectsStatusText = "Sound Clone unavailable: another transfer is in progress";
        repaint ();
        return;
    }

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Sound Clone";
    options.dialogBackgroundColour = backgroundColour;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.content.setOwned (new SoundCloneImportComponent (
        [safeThis = juce::Component::SafePointer<AudioPluginAudioProcessorEditor> (this)]
        (const juce::File& file, int globalSlot)
        {
            if (safeThis != nullptr)
                safeThis->importSoundCloneFile (file, globalSlot);
        },
        [safeThis = juce::Component::SafePointer<AudioPluginAudioProcessorEditor> (this)]
        {
            return safeThis != nullptr
                       ? safeThis->midiConnection.getSoundCloneUploadStatusText()
                       : juce::String();
        },
        [safeThis = juce::Component::SafePointer<AudioPluginAudioProcessorEditor> (this)]
        {
            return safeThis != nullptr
                       && safeThis->midiConnection.isSoundCloneUploadInProgress();
        }));

    options.launchAsync ();
}

void AudioPluginAudioProcessorEditor::importSoundCloneFile (
    const juce::File& file,
    int globalSlot)
{
    if (!midiConnection.startSoundCloneUpload (file, globalSlot))
    {
        effectsStatusText = midiConnection.getSoundCloneUploadStatusText ();

        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Sound Clone import failed",
            effectsStatusText);

        repaint ();
        return;
    }

    const juce::String destination =
    juce::String ("SnapTone ")
    + juce::String (globalSlot + 1)
    + (globalSlot < 5 ? " (AMP)" : " (DIST)");

    effectsStatusText =
        juce::String ("Sound Clone import started: ") +
        file.getFileNameWithoutExtension () +
        " -> " + destination;

    repaint ();
}

void AudioPluginAudioProcessorEditor::syncUserIRSlotBoxFromCabEffectId(
    juce::uint32 effectId)
{
    // User IR 1-20:
    // 0x0A100000 ... 0x0A100013
    if ((effectId & 0xFFFFFF00u) != 0x0A100000u)
        return;

    const auto zeroBasedSlot =
        static_cast<int>(effectId & 0xFFu);

    if (!juce::isPositiveAndBelow(zeroBasedSlot, 20))
        return;

    const auto comboBoxId = zeroBasedSlot + 1;

    userIRSlotBox.setSelectedId(
        comboBoxId,
        juce::dontSendNotification);
}


void AudioPluginAudioProcessorEditor::
    openPrstFileChooser ()
{
    if (presetRestoreInProgress)
    {
        effectsStatusText =
            "Import PRST unavailable: "
            "preset restore already in progress";

        repaint ();
        return;
    }

    prstFileChooser =
        std::make_unique<juce::FileChooser> (
            "Import GP-200 preset",
            juce::File{},
            "*.prst");

    prstFileChooser->launchAsync (
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult ();

            if (file.existsAsFile ())
                importPrstFile (file);
        });
}





void AudioPluginAudioProcessorEditor::importPrstFile (
    const juce::File& file)
{
    if (presetRestoreInProgress)
    {
        effectsStatusText =
            "Import PRST unavailable: "
            "preset restore already in progress";

        repaint ();
        return;
    }

    juce::MemoryBlock prstData;

    if (!file.loadFileAsData (prstData))
    {
        effectsStatusText =
            "Import PRST failed: "
            "could not read the selected file";

        repaint ();
        return;
    }

    const auto prstPreset =
        gp200::GP200PresetCodec::decodePrstFile (
            prstData);

    if (!prstPreset.isValid)
    {
        effectsStatusText =
            "Import PRST failed: invalid, damaged "
            "or unsupported GP-200 preset";

        repaint ();
        return;
    }

    if (!midiConnection.isConnected ())
    {
        offlinePreset = prstPreset;
        offlinePresetDirty = false;
        ++offlinePresetRevision;
        effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
        presetNameEditorSignature.clear ();
        presetNameEditor.setText (offlinePreset.patchName, juce::dontSendNotification);
        effectsStatusText = "Offline preset loaded: " + offlinePreset.patchName;
        updateEffectBlocksUI ();
        repaint ();
        return;
    }

    const auto targetSlot =
        midiConnection.getCurrentSlot ();

    if (targetSlot < 0 || targetSlot > 255)
    {
        effectsStatusText =
            "Import PRST failed: "
            "current GP-200 slot is unknown";

        repaint ();
        return;
    }

    const auto currentLivePresetData =
        midiConnection.getCurrentPresetDumpDataCopy ();

    if (currentLivePresetData.getSize () == 0)
    {
        effectsStatusText =
            "Import PRST failed: current GP-200 "
            "preset data is unavailable";

        repaint ();
        return;
    }

    const auto importSnapshotData =
        gp200::GP200PresetCodec::
            makeLivePresetDumpFromPrst (
                prstPreset,
                currentLivePresetData);

    if (importSnapshotData.getSize () == 0)
    {
        effectsStatusText =
            "Import PRST failed: could not build "
            "a safe restore snapshot";

        repaint ();
        return;
    }

    // Se vuelve a decodificar con el decoder consolidado.
    // Así el restaurador recibe exactamente su formato habitual.
    const auto importPreset =
        gp200::GP200PresetCodec::
            decodeLivePresetDump (
                importSnapshotData);

    if (!importPreset.isValid)
    {
        effectsStatusText =
            "Import PRST failed: generated restore "
            "snapshot is invalid";

        repaint ();
        return;
    }

    // Mismos campos utilizados por Recall.
    presetRestoreSnapshotData =
        importSnapshotData;

    presetRestoreSlot =
        targetSlot;

    presetRestoreName =
        prstPreset.patchName;

    // Función consolidada, sin cambios.
    buildFullPresetRestoreSteps (
        importPreset,
        importSnapshotData);

    if (presetRestoreSteps.empty ())
    {
        effectsStatusText =
            "Import PRST failed: "
            "no restore steps were generated";

        repaint ();
        return;
    }

    presetRestoreStepIndex = 0;
    presetRestoreInProgress = true;

    startTimerHz (restoreTimerHz);

    effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
    patchVolumeSourceSignature.clear ();
    presetNameEditorSignature.clear ();

    effectsStatusText =
        "Import PRST: restoring \"" +
        prstPreset.patchName +
        "\" into the current slot";

    updateEffectBlocksUI ();
    repaint ();
}

void AudioPluginAudioProcessorEditor::recallSavedPresetToGP200 ()
{
    startFullPresetRestoreFromSnapshot ();
}

void AudioPluginAudioProcessorEditor::startFullPresetRestoreFromSnapshot ()
{
    if (presetRestoreInProgress)
    {
        effectsStatusText = "Recall Preset: restore already in progress";
        repaint ();
        return;
    }
	const auto snapshotIndex =
    getSelectedCompareSnapshotIndex ();

const auto snapshotLabel =
    getSelectedCompareSnapshotLabel ();

    if (!processorRef.hasSavedGP200PresetData (
        snapshotIndex))
    {
        effectsStatusText =
    "Recall snapshot " + snapshotLabel +
    " failed: no preset saved";
        repaint ();
        return;
    }

    const auto presetData =
    processorRef.getSavedGP200PresetDataCopy (
        snapshotIndex);

    if (presetData.getSize () == 0)
    {
        effectsStatusText = "Recall Preset failed: saved preset data is empty";
        repaint ();
        return;
    }

    if (!midiConnection.isConnected ())
    {
        int restoredVolume = 50;
        int restoredPan = 0;
        int restoredTempo = 120;
        gp200::GP200Preset restoredPreset;

        if (!deserialiseOfflineSnapshot (presetData,
                                         restoredPreset,
                                         restoredVolume,
                                         restoredPan,
                                         restoredTempo))
        {
            effectsStatusText =
                "Recall snapshot " + snapshotLabel +
                " failed: it was saved from a connected GP-200";
            repaint ();
            return;
        }

        offlinePreset = std::move (restoredPreset);
        offlinePresetDirty = false;
        ++offlinePresetRevision;

        patchVolumeSlider.setValue (restoredVolume, juce::dontSendNotification);
        panSlider.setValue (restoredPan, juce::dontSendNotification);
        tempoSlider.setValue (restoredTempo, juce::dontSendNotification);
        presetNameEditor.setText (offlinePreset.patchName, juce::dontSendNotification);

        effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
        patchVolumeSourceSignature.clear ();
        presetNameEditorSignature.clear ();

        effectsStatusText = "Recalled offline preset snapshot " +
                            snapshotLabel +
                            " from DAW";
        updateEffectBlocksUI ();
        repaint ();
        return;
    }

    const auto preset = gp200::GP200PresetCodec::decodeLivePresetDump (presetData);

    if (!preset.isValid)
    {
        effectsStatusText = "Recall Preset failed: saved preset data could not be decoded";
        repaint ();
        return;
    }

    const auto targetSlot = midiConnection.getCurrentSlot ();

    if (targetSlot < 0 || targetSlot > 255)
    {
        effectsStatusText = "Recall Preset failed: current GP-200 slot is unknown";
        repaint ();
        return;
    }

    auto savedName =
    processorRef.getSavedGP200PresetSnapshotName (
        snapshotIndex);

    if (!isUsefulPresetName (savedName))
        savedName = preset.patchName;

    // Important: do not change slot here.
    // Recall Preset restores the saved sound into the currently selected GP-200 slot,
    // so the user can then press Store preset and save it wherever they are.
    presetRestoreSnapshotData = presetData;
    presetRestoreSlot = targetSlot;
    presetRestoreName = savedName;

    buildFullPresetRestoreSteps (preset, presetData);

    if (presetRestoreSteps.empty ())
    {
        effectsStatusText = "Recall Preset failed: no restore steps were generated";
        repaint ();
        return;
    }

    presetRestoreStepIndex = 0;
    presetRestoreInProgress = true;
    startTimerHz (restoreTimerHz);

    effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
    patchVolumeSourceSignature.clear ();
    presetNameEditorSignature.clear ();

    effectsStatusText = "Recall Preset: restoring snapshot into current slot 0/" +
                        juce::String (static_cast<int> (presetRestoreSteps.size ()));

    updateEffectBlocksUI ();
    repaint ();
}

void AudioPluginAudioProcessorEditor::buildFullPresetRestoreSteps (const gp200::GP200Preset& preset,
                                                                   const juce::MemoryBlock& presetData)
{
    presetRestoreSteps.clear ();

    auto addParameterStep = [this] (const gp200::GP200EffectSlot& effect, int paramIndex)
    {
        if (effect.blockIndex < 0 || effect.blockIndex >= static_cast<int> (gp200::effectBlockCount))
            return;

        if (paramIndex < 0 || paramIndex >= static_cast<int> (effect.params.size ()))
            return;

        const auto value = effect.params[static_cast<std::size_t> (paramIndex)];

        if (!std::isfinite (value))
            return;

        PresetRestoreStep step;
        step.type = PresetRestoreStepType::ParamChange;
        step.blockIndex = effect.blockIndex;
        step.paramIndex = paramIndex;
        step.effectId = effect.effectId;
        step.value = value;

        presetRestoreSteps.push_back (step);
    };

    if (presetData.getSize () > gp200::patchVolumeOffset)
    {
        const auto* data = static_cast<const juce::uint8*> (presetData.getData ());

        if (data != nullptr)
        {
            PresetRestoreStep step;
            step.type = PresetRestoreStepType::PatchVolume;
            step.value =
                static_cast<float> (juce::jlimit (0, 100, static_cast<int> (data[gp200::patchVolumeOffset])));

            presetRestoreSteps.push_back (step);
        }
    }

    if (presetData.getSize () > gp200::patchTempoOffset)
    {
        const auto* data = static_cast<const juce::uint8*> (presetData.getData ());

        if (data != nullptr)
        {
            PresetRestoreStep step;
            step.type = PresetRestoreStepType::PatchTempo;
            step.value =
                static_cast<float> (juce::jlimit (40, 250, static_cast<int> (data[gp200::patchTempoOffset])));

            presetRestoreSteps.push_back (step);
        }
    }

    for (const auto& effect : preset.effects)
    {
        if (effect.blockIndex < 0 || effect.blockIndex >= static_cast<int> (gp200::effectBlockCount))
            continue;

        PresetRestoreStep step;
        step.type = PresetRestoreStepType::EffectChange;
        step.blockIndex = effect.blockIndex;
        step.effectId = effect.effectId;

        presetRestoreSteps.push_back (step);
    }

    auto addKnownParameterPass = [this, &preset, &addParameterStep]
    {
        for (const auto& effect : preset.effects)
        {
            if (effect.blockIndex < 0 || effect.blockIndex >= static_cast<int> (gp200::effectBlockCount))
                continue;

            const auto* paramSet = gp200::GP200EffectParamDatabase::findParamsForEffect (effect.effectId);

            // Only restore parameters that are known for this effect.
            // Do not send reserved/unused parameter slots from the raw 15-float dump.
            if (paramSet == nullptr || paramSet->count <= 0 || paramSet->params == nullptr)
                continue;

            for (int i = 0; i < paramSet->count; ++i)
                addParameterStep (effect, paramSet->params[i].idx);
        }
    };

    auto resendFirstKnownParameter = [this, &preset, &addParameterStep]
    {
        for (const auto& effect : preset.effects)
        {
            if (effect.blockIndex < 0 || effect.blockIndex >= static_cast<int> (gp200::effectBlockCount))
                continue;

            const auto* paramSet = gp200::GP200EffectParamDatabase::findParamsForEffect (effect.effectId);

            if (paramSet == nullptr || paramSet->count <= 0 || paramSet->params == nullptr)
                continue;

            for (int i = 0; i < paramSet->count; ++i)
            {
                const auto paramIndex = paramSet->params[i].idx;

                if (paramIndex >= 0 && paramIndex < static_cast<int> (effect.params.size ()))
                {
                    addParameterStep (effect, paramIndex);
                    break;
                }
            }
        }
    };

    addKnownParameterPass ();

    // The GP-200 can ignore the first value after an effect type change.
    // Re-send the first known parameter for each effect. For CAB/User IR this is P1,
    // not P0, so we avoid writing reserved IR internals.
    resendFirstKnownParameter ();

    for (const auto& effect : preset.effects)
    {
        if (effect.blockIndex < 0 || effect.blockIndex >= static_cast<int> (gp200::effectBlockCount))
            continue;

        PresetRestoreStep step;
        step.type = PresetRestoreStepType::ToggleEffect;
        step.blockIndex = effect.blockIndex;
        step.shouldBeOn = effect.enabled;

        presetRestoreSteps.push_back (step);
    }

    addKnownParameterPass ();

    PresetRestoreStep reorderStep;
    reorderStep.type = PresetRestoreStepType::ReorderEffects;
    reorderStep.routingOrder = preset.routingOrder;
    reorderStep.fxLoopSend = preset.fxLoopSend;
    reorderStep.fxLoopReturn = preset.fxLoopReturn;

    presetRestoreSteps.push_back (reorderStep);
}

void AudioPluginAudioProcessorEditor::processFullPresetRestoreStep ()
{
    if (!presetRestoreInProgress)
        return;

    if (presetRestoreStepIndex >= static_cast<int> (presetRestoreSteps.size ()))
    {
        finishFullPresetRestore ();
        return;
    }

    const auto& step = presetRestoreSteps[static_cast<std::size_t> (presetRestoreStepIndex)];

    bool sent = false;

    switch (step.type)
    {
    case PresetRestoreStepType::PatchVolume:
        sent = midiConnection.sendPatchVolume (static_cast<int> (std::round (step.value)));
        break;

    case PresetRestoreStepType::PatchTempo:
        sent = midiConnection.sendPatchTempoBpm (static_cast<int> (std::round (step.value)));
        break;

    case PresetRestoreStepType::EffectChange:
        sent = midiConnection.sendEffectChange (step.blockIndex, step.effectId);
        break;

    case PresetRestoreStepType::ParamChange:
        sent = midiConnection.sendParamChange (step.blockIndex, step.paramIndex, step.effectId, step.value);
        break;

    case PresetRestoreStepType::ToggleEffect:
        sent = midiConnection.sendEffectOnOff (step.blockIndex, step.shouldBeOn);
        break;

    case PresetRestoreStepType::ReorderEffects:
        sent = midiConnection.sendReorderEffects (step.routingOrder, step.fxLoopSend, step.fxLoopReturn);
        break;
    }

    if (!sent)
    {
        presetRestoreInProgress = false;
        startTimerHz (idleTimerHz);
        effectsStatusText = "Recall Preset failed: " + midiConnection.getLastMessageText ();
        return;
    }

    ++presetRestoreStepIndex;

    effectsStatusText = "Recall Preset: restoring snapshot into current slot " +
                        juce::String (presetRestoreStepIndex) + "/" +
                        juce::String (static_cast<int> (presetRestoreSteps.size ()));

    if (presetRestoreStepIndex >= static_cast<int> (presetRestoreSteps.size ()))
        finishFullPresetRestore ();
}

void AudioPluginAudioProcessorEditor::finishFullPresetRestore ()
{
    presetRestoreInProgress = false;
    startTimerHz (idleTimerHz);

    midiConnection.adoptCurrentPresetSnapshot (
        presetRestoreSlot, presetRestoreName, presetRestoreSnapshotData);

    presetRestoreSteps.clear ();
    presetRestoreStepIndex = 0;

    effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
    patchVolumeSourceSignature.clear ();
    presetNameEditorSignature.clear ();

    hasSavedBlockEnabledStates = false;
    allBlocksAreTemporarilyOff = false;
    savedBlockEnabledSlot = -1;
    updateAllBlocksOffButtonText ();

    effectsStatusText =
        "Recall Preset: snapshot restored into current slot. Press Store preset to save it here.";

    updateEffectBlocksUI ();
}

void AudioPluginAudioProcessorEditor::storeCurrentPresetToGP200 ()
{
    auto newName = presetNameEditor.getText ().trim ();

    if (newName.isEmpty ())
    {
        effectsStatusText = "Store Preset failed: empty preset name";
        repaint ();
        return;
    }

    if (newName.length () > gp200::presetNameMaxLength)
        newName = newName.substring (0, gp200::presetNameMaxLength);

    const auto currentName = midiConnection.getCurrentPresetName ().trim ();

    if (newName != currentName)
    {
        if (!midiConnection.renameCurrentPresetOnGP200 (newName))
        {
            effectsStatusText = midiConnection.getLastMessageText ();
            repaint ();
            return;
        }

        presetNameEditor.setText (midiConnection.getCurrentPresetName (),
                                  juce::dontSendNotification);

        presetNameEditorSignature.clear ();
        effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
    }

    if (!midiConnection.storeCurrentPresetToGP200 ())
    {
        effectsStatusText = midiConnection.getLastMessageText ();
        repaint ();
        return;
    }

    effectsStatusText = "Stored preset: " + midiConnection.getCurrentPresetName ();

    repaint ();
}

void AudioPluginAudioProcessorEditor::sendPatchVolumeFromSlider ()
{
    const auto value = static_cast<int> (patchVolumeSlider.getValue ());

    if (midiConnection.isConnected ())
    {
        pendingPatchVolumeValue = value;
        patchVolumeLocalEditUntilMs =
            juce::Time::getMillisecondCounterHiRes () + 450.0;
        midiConnection.sendPatchVolume (value);
    }
    else
    {
        offlinePresetDirty = true;
    }

    repaint ();
}

void AudioPluginAudioProcessorEditor::sendPatchPanFromSlider ()
{
    const auto value = static_cast<int> (panSlider.getValue ());

    if (midiConnection.isConnected ())
    {
        pendingPatchPanValue = value;
        patchPanLocalEditUntilMs =
            juce::Time::getMillisecondCounterHiRes () + 450.0;
        midiConnection.sendPatchPan (value);
    }
    else
    {
        offlinePresetDirty = true;
    }

    repaint ();
}

void AudioPluginAudioProcessorEditor::sendPatchTempoFromSlider ()
{
    const auto bpm = static_cast<int> (tempoSlider.getValue ());

    if (midiConnection.isConnected ())
    {
        pendingPatchTempoValue = bpm;
        patchTempoLocalEditUntilMs =
            juce::Time::getMillisecondCounterHiRes () + 450.0;
        midiConnection.sendPatchTempoBpm (bpm);
    }
    else
    {
        offlinePresetDirty = true;
    }

    repaint ();
}

void AudioPluginAudioProcessorEditor::handleTapTempo ()
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();

    midiConnection.processIRUpload ();
    midiConnection.processSoundCloneUpload ();

    const bool transferInProgress =
        midiConnection.isIRUploadInProgress () ||
        midiConnection.isSoundCloneUploadInProgress ();

    importIRButton.setEnabled (!transferInProgress);
    soundCloneButton.setEnabled (!transferInProgress);

    // Immediate visual feedback, including the first tap.
    tapFlashUntilMs = nowMs + 150.0;

    tapTempoButton.setColour (juce::TextButton::buttonColourId,
                              panelOutlineColour);
    tapTempoButton.setColour (juce::TextButton::buttonOnColourId,
                              panelOutlineColour.brighter (0.1f));
    tapTempoButton.setColour (juce::TextButton::textColourOffId,
                              juce::Colours::black);
    tapTempoButton.setColour (juce::TextButton::textColourOnId,
                              juce::Colours::black);
    tapTempoButton.repaint();

    if (lastTapTimeMs <= 0.0)
    {
        lastTapTimeMs = nowMs;
        tapTempoIntervals.clear();

        effectsStatusText = "Tap Tempo: waiting for next tap";
        repaint();
        return;
    }

    const auto intervalMs = nowMs - lastTapTimeMs;
    lastTapTimeMs = nowMs;

    // A pause longer than two seconds starts a new measurement.
    if (intervalMs > 2000.0)
    {
        tapTempoIntervals.clear();

        effectsStatusText = "Tap Tempo: measurement restarted";
        repaint();
        return;
    }

    // Ignore accidental extremely fast double-clicks.
    if (intervalMs < 240.0)
    {
        effectsStatusText = "Tap Tempo: tap too fast";
        repaint();
        return;
    }

    tapTempoIntervals.push_back (intervalMs);

    while (tapTempoIntervals.size() > 4)
        tapTempoIntervals.pop_front();

    // Use the median so an imprecise tap has less influence.
    std::vector<double> sortedIntervals (tapTempoIntervals.begin(),
                                         tapTempoIntervals.end());

    std::sort (sortedIntervals.begin(), sortedIntervals.end());

    const auto intervalCount = sortedIntervals.size();
    double medianIntervalMs = 0.0;

    if ((intervalCount % 2) == 1)
    {
        medianIntervalMs = sortedIntervals[intervalCount / 2];
    }
    else
    {
        medianIntervalMs =
            (sortedIntervals[intervalCount / 2 - 1] +
             sortedIntervals[intervalCount / 2]) * 0.5;
    }

    const auto calculatedBpm = juce::jlimit (
        40,
        250,
        static_cast<int> (std::round (60000.0 / medianIntervalMs))
    );

    // The first valid interval exists on the second tap, so MIDI is sent from here.
    tempoSlider.setValue (
        static_cast<double> (calculatedBpm),
        juce::sendNotificationSync
    );

    effectsStatusText =
        "Tap Tempo: " + juce::String (calculatedBpm) + " BPM";

    repaint();
}

void AudioPluginAudioProcessorEditor::syncPresetNameEditorFromCurrentPreset ()
{
    if (!midiConnection.isConnected ())
    {
        const auto signature = "offline:" + juce::String (static_cast<juce::int64> (offlinePresetRevision));
        if (signature != presetNameEditorSignature && !presetNameEditor.hasKeyboardFocus (true))
        {
            presetNameEditorSignature = signature;
            presetNameEditor.setText (offlinePreset.patchName, juce::dontSendNotification);
        }
        return;
    }

    const auto slot = midiConnection.getCurrentSlot ();
    const auto name = midiConnection.getCurrentPresetName ();

    if (slot < 0 || !isUsefulPresetName (name))
        return;

    const auto signature = juce::String (slot) + ":" + name;

    if (signature == presetNameEditorSignature)
        return;

    if (presetNameEditor.hasKeyboardFocus (true))
        return;

    presetNameEditorSignature = signature;

    presetNameEditor.setText (name.trim ().substring (0, gp200::presetNameMaxLength),
                              juce::dontSendNotification);
}

void AudioPluginAudioProcessorEditor::toggleTuner()
{
    const bool newState = !tunerIsOn;

    const bool hardwareTunerSent =
        midiConnection.sendTunerOnOff(newState);

    tunerIsOn = newState;

    processorRef.setTunerEnabled(tunerIsOn);

    tunerDisplay.setVisible(tunerIsOn);

    // El afinador ocupa toda la zona de utilidades de la derecha.
    // Ocultamos completamente los controles de IR mientras está activo.
    const bool showIRControls = !tunerIsOn;
    userIRSlotBox.setVisible(showIRControls);
    importIRButton.setVisible(showIRControls);
    toneMatchButton.setVisible(showIRControls);

    if (tunerIsOn)
        tunerDisplay.toFront(false);

    tunerDisplay.clearReading();

    if (hardwareTunerSent)
    {
        effectsStatusText =
            tunerIsOn
                ? "Plugin tuner ON | GP-200 tuner ON"
                : "Plugin tuner OFF | GP-200 tuner OFF";
    }
    else
    {
        effectsStatusText =
            tunerIsOn
                ? "Plugin tuner ON | GP-200 tuner unavailable"
                : "Plugin tuner OFF | GP-200 tuner unavailable";
    }

    updateTunerButtonText();
    repaint();
}

void AudioPluginAudioProcessorEditor::updateTunerButtonText ()
{
    tunerButton.setButtonText (tunerIsOn ? "Tuner ON" : "Tuner OFF");

    tunerButton.setColour (juce::TextButton::buttonColourId, tunerIsOn ? statusOnColour : panelColour);

    tunerButton.setColour (juce::TextButton::buttonOnColourId,
                           tunerIsOn ? statusOnColour.brighter (0.1f) : panelColour.brighter (0.2f));

    tunerButton.setColour (juce::TextButton::textColourOffId,
                           tunerIsOn ? juce::Colours::black : panelOutlineColour);

    tunerButton.setColour (juce::TextButton::textColourOnId,
                           tunerIsOn ? juce::Colours::black : panelOutlineColour);
}

void AudioPluginAudioProcessorEditor::toggleAllBlocksOff ()
{
    if (presetRestoreInProgress)
    {
        effectsStatusText = "FX OFF disabled while Recall Preset is restoring";
        repaint ();
        return;
    }

    const auto currentSlot = midiConnection.getCurrentSlot ();

    if (!allBlocksAreTemporarilyOff)
    {
        BlockEnabledStates currentStates{};

        if (!captureCurrentBlockEnabledStates (currentStates))
        {
            effectsStatusText = "FX  OFF failed: no current preset data";
            repaint ();
            return;
        }

        BlockEnabledStates offStates{};
        offStates.fill (false);

        if (!applyBlockEnabledStates (offStates))
            return;

        savedBlockEnabledStates = currentStates;
        hasSavedBlockEnabledStates = true;
        allBlocksAreTemporarilyOff = true;
        savedBlockEnabledSlot = currentSlot;

        effectsStatusText = "All FX OFF. Press Restore FX to recover previous block states.";
        updateAllBlocksOffButtonText ();
        repaint ();
        return;
    }

    if (!hasSavedBlockEnabledStates)
    {
        allBlocksAreTemporarilyOff = false;
        savedBlockEnabledSlot = -1;
        updateAllBlocksOffButtonText ();

        effectsStatusText = "Restore FX failed: no previous FX state saved";
        repaint ();
        return;
    }

    if (currentSlot != savedBlockEnabledSlot)
    {
        hasSavedBlockEnabledStates = false;
        allBlocksAreTemporarilyOff = false;
        savedBlockEnabledSlot = -1;
        updateAllBlocksOffButtonText ();

        effectsStatusText = "Restore FX cancelled: preset changed";
        repaint ();
        return;
    }

    if (!applyBlockEnabledStates (savedBlockEnabledStates))
        return;

    hasSavedBlockEnabledStates = false;
    allBlocksAreTemporarilyOff = false;
    savedBlockEnabledSlot = -1;

    effectsStatusText = "Restored previous FX ON/OFF states.";
    updateAllBlocksOffButtonText ();
    repaint ();
}

bool AudioPluginAudioProcessorEditor::captureCurrentBlockEnabledStates (BlockEnabledStates& states)
{
    states.fill (false);

    const auto currentDump = midiConnection.getCurrentPresetDumpDataCopy ();

    if (currentDump.getSize () == 0)
        return false;

    const auto preset = gp200::GP200PresetCodec::decodeLivePresetDump (currentDump);

    if (!preset.isValid)
        return false;

    for (const auto& effect : preset.effects)
    {
        const auto blockIndex = effect.blockIndex >= 0 ? effect.blockIndex : effect.slotIndex;

        if (blockIndex >= 0 && blockIndex < static_cast<int> (states.size ()))
            states[static_cast<std::size_t> (blockIndex)] = effect.enabled;
    }

    return true;
}

bool AudioPluginAudioProcessorEditor::applyBlockEnabledStates (const BlockEnabledStates& states)
{
    for (int blockIndex = 0; blockIndex < static_cast<int> (states.size ()); ++blockIndex)
    {
        const auto shouldBeOn = states[static_cast<std::size_t> (blockIndex)];

        if (!midiConnection.sendEffectOnOff (blockIndex, shouldBeOn))
        {
            effectsStatusText = "FX toggle failed: " + midiConnection.getLastMessageText ();
            repaint ();
            return false;
        }

        for (auto& blockToUpdate : effectBlocks)
        {
            if (blockToUpdate != nullptr && blockToUpdate->getBlockIndex () == blockIndex)
            {
                blockToUpdate->setEnabledForDisplay (shouldBeOn);
                break;
            }
        }
    }

    effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();

    return true;
}

void AudioPluginAudioProcessorEditor::updateAllBlocksOffButtonText ()
{
    allBlocksOffButton.setButtonText (allBlocksAreTemporarilyOff ? "Restore FX" : "FX OFF");

    allBlocksOffButton.setColour (juce::TextButton::buttonColourId,
                                  allBlocksAreTemporarilyOff ? statusOffColour.withAlpha (0.30f)
                                                             : panelColour);

    allBlocksOffButton.setColour (juce::TextButton::buttonOnColourId,
                                  allBlocksAreTemporarilyOff ? statusOffColour.withAlpha (0.42f)
                                                             : panelColour.brighter (0.2f));

    allBlocksOffButton.setColour (juce::TextButton::textColourOffId,
                                  allBlocksAreTemporarilyOff ? textColour : panelOutlineColour);

    allBlocksOffButton.setColour (juce::TextButton::textColourOnId,
                                  allBlocksAreTemporarilyOff ? textColour : panelOutlineColour);
}

void AudioPluginAudioProcessorEditor::syncPatchVolumeSliderFromPresetData (
    const juce::MemoryBlock& presetData, const juce::String& presetDataSignature)
{
    if (presetDataSignature == patchVolumeSourceSignature)
        return;

    patchVolumeSourceSignature = presetDataSignature;

    if (presetData.getSize () <= gp200::patchVolumeOffset)
        return;

    const auto* data = static_cast<const juce::uint8*> (presetData.getData ());

    if (data == nullptr)
        return;

    const auto patchVolume = juce::jlimit (
        0, 100, static_cast<int> (data[gp200::patchVolumeOffset]));

    const auto nowMs = juce::Time::getMillisecondCounterHiRes ();

    // MIDI echo/live-dump updates can lag behind the user's slider movement.
    // Do not paint an older value during that round trip. Release the guard as
    // soon as the dump confirms the locally sent value, or after the timeout.
    if (patchVolumeLocalEditUntilMs > nowMs)
    {
        if (patchVolume == pendingPatchVolumeValue)
            patchVolumeLocalEditUntilMs = 0.0;
        else
            return;
    }

    patchVolumeSlider.setValue (static_cast<double> (patchVolume),
                                juce::dontSendNotification);
}

//==============================================================================
void AudioPluginAudioProcessorEditor::openPresetSlotMenu ()
{
    if (!midiConnection.isConnected ())
    {
        effectsStatusText = "Preset browser requires a connected GP-200";
        repaint ();
        return;
    }

    if (midiConnection.hasCompletePresetNameCache ())
    {
        openPresetMenuWhenScanFinishes = false;
        showPresetSlotMenu ();
        return;
    }

    if (midiConnection.isPresetNameScanRunning ())
        return;

    openPresetMenuWhenScanFinishes = true;
    lastPresetNameScanRevision = midiConnection.getPresetNameScanRevision ();
    midiConnection.startPresetNameScan (midiConnection.getCurrentSlot ());
}

void AudioPluginAudioProcessorEditor::showPresetSlotMenu ()
{
    juce::PopupMenu menu;
    const int currentSlot = midiConnection.getCurrentSlot ();

    for (int bank = 0; bank < 64; ++bank)
    {
        juce::PopupMenu bankMenu;

        for (int subSlot = 0; subSlot < 4; ++subSlot)
        {
            const int slot = bank * 4 + subSlot;
            auto name = midiConnection.getPresetSlotName (slot);
            if (name.isEmpty ())
                name = "(name unavailable)";

            const auto itemText = formatSlotCompact (slot) + "   " + name;
            bankMenu.addItem (slot + 1, itemText, true, slot == currentSlot);
        }

        const auto bankText = juce::String (bank + 1).paddedLeft ('0', 2);
        menu.addSubMenu ("BANK " + bankText, bankMenu);
    }

    auto safeThis = juce::Component::SafePointer<AudioPluginAudioProcessorEditor> (this);
    menu.showMenuAsync (juce::PopupMenu::Options ().withTargetComponent (&presetSlotButton),
                        [safeThis] (int result)
                        {
                            if (safeThis == nullptr || result <= 0)
                                return;

                            const int slot = result - 1;
                            if (slot >= 0 && slot < 256)
                                safeThis->midiConnection.sendPresetChange (slot);
                        });
}

void AudioPluginAudioProcessorEditor::loadPreviousBank ()
{
    // Cada banco del GP-200 contiene cuatro presets: A, B, C y D.
    // Restar cuatro conserva la letra actual al cambiar de banco.
    loadPresetRelative (-4);
}

void AudioPluginAudioProcessorEditor::loadNextBank ()
{
    // Sumar cuatro conserva la letra actual al cambiar de banco.
    loadPresetRelative (4);
}

void AudioPluginAudioProcessorEditor::loadPreviousPreset ()
{
    loadPresetRelative (-1);
}

void AudioPluginAudioProcessorEditor::loadNextPreset ()
{
    loadPresetRelative (1);
}

void AudioPluginAudioProcessorEditor::loadPresetRelative (int delta)
{
    auto currentSlot = midiConnection.getCurrentSlot ();

    if (currentSlot < 0)
        currentSlot = processorRef.getSavedGP200Slot ();

    if (currentSlot < 0)
        return;

    const auto targetSlot = wrapPresetSlot (currentSlot + delta);

    midiConnection.sendPresetChange (targetSlot);

    // Keep the current ribbon and selected editor visible until the new preset
    // dump is complete. Clearing the components here produced a visible flash
    // between the preset-change command and the incoming preset data.
    effectBlocksDataSignature.clear ();

    hasSavedBlockEnabledStates = false;
    allBlocksAreTemporarilyOff = false;
    savedBlockEnabledSlot = -1;
    updateAllBlocksOffButtonText ();

    effectsStatusText = "Effects: changing preset...";

    repaint ();
}

//==============================================================================
void AudioPluginAudioProcessorEditor::updateEffectBlocksUI ()
{
    juce::MemoryBlock presetDataForDisplay;
    juce::String sourceText;
    std::uint64_t presetRevision = 0;

    if (!midiConnection.isConnected ())
    {
        sourceText = offlinePresetDirty ? "Offline preset (edited)" : "Offline preset";
        presetRevision = offlinePresetRevision;
    }
    else if (midiConnection.getCurrentPresetDumpSize () > 0)
    {
        presetDataForDisplay = midiConnection.getCurrentPresetDumpDataCopy ();
        sourceText = "Current GP-200 preset";
        presetRevision = midiConnection.getPresetRevision ();
    }
    else
    {
        // Never substitute a DAW snapshot while the GP-200 is connected.
        // During a preset change the live dump is briefly unavailable; showing
        // the saved DAW snapshot here created the visible intermediate preset.
        // Keep the currently painted chain until the new live dump arrives.
        effectsStatusText = effectBlocks.empty ()
                                ? "Effects: waiting for GP-200 preset data"
                                : "Effects: changing preset...";
        return;
    }

    const auto assignmentRevision =
        midiConnection.getAssignmentNamesRevision ();

    const auto revisionText =
        juce::String (static_cast<juce::int64> (presetRevision));

    const auto patchVolumeSignature =
        sourceText + ":" + revisionText;

    const auto dataSignature =
        sourceText + ":" + revisionText + ":"
        + juce::String (
            static_cast<juce::int64> (assignmentRevision));

    // A connected GP-200 may publish several intermediate revisions while a
    // preset/effect model is being changed. If no newer revision has arrived
    // for the debounce interval, apply the last complete staged structure.
    constexpr double structuralDebounceMs = 140.0;
    const auto nowMs = juce::Time::getMillisecondCounterHiRes ();

    if (dataSignature == effectBlocksDataSignature)
    {
        if (pendingStructuralRefresh
            && nowMs - pendingStructuralLastChangeMs >= structuralDebounceMs)
        {
            const auto presetToApply = pendingStructuralPreset;
            const auto signatureToApply = pendingStructuralSignature;
            const auto dataSignatureToApply = pendingStructuralDataSignature;

            pendingStructuralRefresh = false;
            pendingStructuralSignature.clear ();
            pendingStructuralDataSignature.clear ();

            rebuildEffectBlocks (presetToApply, signatureToApply);
            effectBlocksDataSignature = dataSignatureToApply;
            repaint ();
        }

        return;
    }

    const auto preset =
        !midiConnection.isConnected ()
            ? offlinePreset
            : gp200::GP200PresetCodec::decodeLivePresetDump (
                  presetDataForDisplay);

    if (!preset.isValid)
    {
        effectBlocksDataSignature = dataSignature;
        effectsStatusText = "Effects: could not decode preset data";
        return;
    }

    if (midiConnection.isConnected ())
        syncPatchVolumeSliderFromPresetData (
            presetDataForDisplay,
            patchVolumeSignature);

    effectsStatusText =
        "Effects: " + sourceText + " | "
        + preset.getSignalChainText ();

    // Rebuild only when the chain structure or selected effect models change.
    // ON/OFF and parameter changes are applied to the existing components.
    // This prevents the grey/colour flash caused by destroying and recreating
    // the complete ribbon and editor after every preset revision.
    juce::String structureSignature =
        sourceText + ":assign="
        + juce::String (
            static_cast<juce::int64> (assignmentRevision));

    structureSignature << ":order=";

    for (const auto blockIndex : preset.routingOrder)
        structureSignature << juce::String (blockIndex) << ",";

    structureSignature << ":effects=";

    for (const auto& effect : preset.effects)
    {
        structureSignature
            << juce::String (effect.blockIndex) << "/"
            << juce::String (effect.slotIndex) << "/"
            << juce::String::toHexString (
                   static_cast<juce::int64> (effect.effectId))
            << ";";
    }

    const bool canRefreshInPlace =
        !effectBlocks.empty ()
        && structureSignature == effectBlocksSignature;

    if (canRefreshInPlace)
    {
        // Any staged structural refresh is obsolete: the current component
        // tree already matches the newest preset structure.
        pendingStructuralRefresh = false;
        pendingStructuralSignature.clear ();
        pendingStructuralDataSignature.clear ();

        for (const auto& effect : preset.effects)
        {
            const auto blockIndex =
                effect.blockIndex >= 0
                    ? effect.blockIndex
                    : effect.slotIndex;

            for (auto& block : effectBlocks)
            {
                if (block == nullptr
                    || block->getBlockIndex () != blockIndex)
                    continue;

                block->setEnabledForDisplay (effect.enabled);

                for (int paramIndex = 0;
                     paramIndex < static_cast<int> (effect.params.size ());
                     ++paramIndex)
                {
                    block->setParameterValueForDisplay (
                        paramIndex,
                        effect.params[
                            static_cast<std::size_t> (paramIndex)]);
                }

                break;
            }
        }

        updateEffectChainRibbon (preset);
        effectBlocksDataSignature = dataSignature;
        repaint ();
        return;
    }

    // Offline edits are deterministic and the first population has nothing
    // visible to preserve, so those can be rebuilt immediately. Connected
    // structural changes are staged until the device stops publishing partial
    // revisions. A single synchronous rebuild then replaces the old UI.
    if (!midiConnection.isConnected () || effectBlocks.empty ())
    {
        pendingStructuralRefresh = false;
        pendingStructuralSignature.clear ();
        pendingStructuralDataSignature.clear ();
        rebuildEffectBlocks (preset, structureSignature);
        effectBlocksDataSignature = dataSignature;
        return;
    }

    pendingStructuralPreset = preset;
    pendingStructuralSignature = structureSignature;
    pendingStructuralDataSignature = dataSignature;
    pendingStructuralLastChangeMs = nowMs;
    pendingStructuralRefresh = true;

    // Mark this revision as consumed while leaving the currently painted
    // component tree intact. The timer will commit the staged preset after the
    // quiet period, or replace it if a newer revision arrives first.
    effectBlocksDataSignature = dataSignature;
}

void AudioPluginAudioProcessorEditor::rebuildEffectBlocks (const gp200::GP200Preset& preset,
                                                           const juce::String& newSignature)
{
    BlockEnabledStates wasExpanded{};
    wasExpanded.fill (false);

    for (const auto& block : effectBlocks)
    {
        if (block != nullptr)
        {
            const auto blockIndex = block->getSlotIndex ();

            if (blockIndex >= 0 && blockIndex < static_cast<int> (wasExpanded.size ()))
                wasExpanded[static_cast<std::size_t> (blockIndex)] = block->isExpanded ();
        }
    }

    effectBlocks.clear ();
    effectsContent.removeAllChildren ();

    effectsContent.addChildComponent (dropIndicator);
    dropIndicator.setVisible (false);

    BlockEnabledStates alreadyAdded{};
    alreadyAdded.fill (false);

    for (const auto effectBlockIndex : preset.routingOrder)
    {
        if (effectBlockIndex < 0 || effectBlockIndex >= static_cast<int> (preset.effects.size ()))
            continue;

        const auto& effect = preset.effects[static_cast<std::size_t> (effectBlockIndex)];
		
		if ((effect.effectId & 0xFFFFFF00u)
    == 0x0A100000u)
{
    syncUserIRSlotBoxFromCabEffectId(
        effect.effectId);
}

        const auto blockIndex = effect.blockIndex;

        if (blockIndex < 0 || blockIndex >= static_cast<int> (alreadyAdded.size ()))
            continue;

        if (alreadyAdded[static_cast<std::size_t> (blockIndex)])
            continue;

        alreadyAdded[static_cast<std::size_t> (blockIndex)] = true;

        auto block = std::make_unique<EffectBlockComponent> (
            effect,
            [this] (juce::uint32 effectId) -> juce::String
            {
                // CAB User IR 1-20: 0x0A100000 - 0x0A100013
                if ((effectId & 0xFFFFFF00u) == 0x0A100000u)
                {
                    const auto index = static_cast<int> (effectId & 0xFFu);
                    return midiConnection.getUserIRDisplayName (index);
                }

                // SnapTone 1-10: 0x0F000000 - 0x0F000009
                if ((effectId & 0xFFFFFF00u) == 0x0F000000u)
                {
                    const auto index = static_cast<int> (effectId & 0xFFu);
                    return midiConnection.getSnapToneDisplayName (index);
                }

                return {};
            });

        const auto currentPosition = static_cast<int> (effectBlocks.size ());
        block->setMoveButtonsEnabled (currentPosition > 0,
                                      currentPosition < static_cast<int> (preset.routingOrder.size ()) - 1);

        block->setExpanded (blockIndex == selectedEffectBlockIndex);

        block->onHeightChanged = [this]
        {
            layoutEffectBlocks ();
            scheduleEditorHeightUpdate ();
        };

        block->onToggleRequested = [this] (int blockIndex, bool shouldBeOn)
        {
            const bool offline = !midiConnection.isConnected ();
            const bool accepted = offline || midiConnection.sendEffectOnOff (blockIndex, shouldBeOn);

            if (accepted)
            {
                if (offline && juce::isPositiveAndBelow (blockIndex, static_cast<int> (offlinePreset.effects.size ())))
                {
                    offlinePreset.effects[static_cast<std::size_t> (blockIndex)].enabled = shouldBeOn;
                    offlinePresetDirty = true;
                    ++offlinePresetRevision;
                }

                for (auto& blockToUpdate : effectBlocks)
                {
                    if (blockToUpdate != nullptr && blockToUpdate->getBlockIndex () == blockIndex)
                    {
                        blockToUpdate->setEnabledForDisplay (shouldBeOn);
                        break;
                    }
                }

                effectChainRibbon.setBlockEnabled (blockIndex, shouldBeOn);
            }

            repaint ();
        };

        block->onEffectChangeRequested = [this] (int blockIndex, juce::uint32 effectId)
        {
            const bool offline = !midiConnection.isConnected ();
            if (!offline && !midiConnection.sendEffectChange (blockIndex, effectId))
            {
                repaint ();
                return;
            }

            if (offline && juce::isPositiveAndBelow (blockIndex, static_cast<int> (offlinePreset.effects.size ())))
            {
                auto& effect = offlinePreset.effects[static_cast<std::size_t> (blockIndex)];
                effect.effectId = effectId;
                effect.params.fill (0.0f);
                if (const auto* paramSet = gp200::GP200EffectParamDatabase::findParamsForEffect (effectId))
                {
                    for (int i = 0; i < paramSet->count; ++i)
                    {
                        const auto& param = paramSet->params[i];
                        if (param.idx >= 0 && param.idx < static_cast<int> (effect.params.size ()))
                            effect.params[static_cast<std::size_t> (param.idx)] = param.defaultValue;
                    }
                }
                offlinePresetDirty = true;
                ++offlinePresetRevision;
            }

            syncUserIRSlotBoxFromCabEffectId(effectId);

            juce::MessageManager::callAsync (
                [this]
                {
                    // Keep the existing component tree visible. The incoming
                    // live revisions will be staged and committed once, rather
                    // than rebuilding through every intermediate effect state.
                    effectBlocksDataSignature.clear ();
                    updateEffectBlocksUI ();
                    repaint ();
                });
        };

        block->onParameterChangeRequested =
            [this] (int blockIndex, int paramIndex, juce::uint32 effectId, float value)
        {
            const bool offline = !midiConnection.isConnected ();
            const bool accepted = offline || midiConnection.sendParamChange (blockIndex, paramIndex, effectId, value);

            if (accepted)
            {
                if (offline
                    && juce::isPositiveAndBelow (blockIndex, static_cast<int> (offlinePreset.effects.size ()))
                    && juce::isPositiveAndBelow (paramIndex, static_cast<int> (gp200::effectParamCount)))
                {
                    offlinePreset.effects[static_cast<std::size_t> (blockIndex)]
                        .params[static_cast<std::size_t> (paramIndex)] = value;
                    offlinePresetDirty = true;
                    ++offlinePresetRevision;
                }

                for (auto& blockToUpdate : effectBlocks)
                {
                    if (blockToUpdate != nullptr && blockToUpdate->getBlockIndex () == blockIndex)
                    {
                        blockToUpdate->setParameterValueForDisplay (paramIndex, value);
                        break;
                    }
                }
            }

            repaint ();
        };

        block->onMoveRequested = [this] (int blockIndex, int direction)
        {
            int currentPosition = -1;

            for (int i = 0; i < static_cast<int> (effectBlocks.size ()); ++i)
            {
                const auto* blockToCheck = effectBlocks[static_cast<std::size_t> (i)].get ();

                if (blockToCheck != nullptr && blockToCheck->getBlockIndex () == blockIndex)
                {
                    currentPosition = i;
                    break;
                }
            }

            if (currentPosition < 0)
            {
                repaint ();
                return;
            }

            const auto targetPosition = direction < 0 ? currentPosition - 1 : currentPosition + 2;

            moveEffectBlockToPosition (blockIndex, targetPosition);
        };

        effectsContent.addAndMakeVisible (*block);
        effectBlocks.push_back (std::move (block));
    }

    effectBlocksSignature = newSignature;
    updateEffectChainRibbon (preset);
    selectEffectBlock (selectedEffectBlockIndex);
}

void AudioPluginAudioProcessorEditor::layoutEffectBlocks ()
{
    const auto contentWidth = juce::jmax (100, effectsViewport.getWidth () - 16);
    int contentHeight = effectsViewport.getHeight ();
    for (auto& block : effectBlocks)
    {
        if (block == nullptr) continue;
        const auto selected = block->getBlockIndex () == selectedEffectBlockIndex;
        block->setVisible (selected);
        if (!selected) continue;
        block->setExpanded (true);
        const auto height = block->getPreferredHeight (contentWidth);
        block->setBounds (0, 0, contentWidth, height);
        contentHeight = juce::jmax (height + 8, effectsViewport.getHeight ());
    }
    effectsContent.setSize (contentWidth, contentHeight);
}

void AudioPluginAudioProcessorEditor::selectEffectBlock (int blockIndex)
{
    if (blockIndex >= 0)
    {
        bool found = false;

        for (const auto& block : effectBlocks)
        {
            if (block != nullptr && block->getBlockIndex () == blockIndex)
            {
                found = true;
                break;
            }
        }

        if (!found)
            blockIndex = -1;
    }

    selectedEffectBlockIndex = blockIndex;
    effectChainRibbon.setSelectedBlockIndex (blockIndex);
    effectsViewport.setViewPosition (0, 0);
    effectsViewport.setVisible (blockIndex >= 0);
    layoutEffectBlocks ();
    scheduleEditorHeightUpdate ();
    repaint ();
}

void AudioPluginAudioProcessorEditor::scheduleEditorHeightUpdate ()
{
    if (editorHeightUpdatePending)
        return;

    editorHeightUpdatePending = true;

    juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safeThis (this);

    juce::MessageManager::callAsync (
        [safeThis]
        {
            if (safeThis == nullptr)
                return;

            safeThis->editorHeightUpdatePending = false;
            safeThis->updateEditorHeight ();
        });
}

void AudioPluginAudioProcessorEditor::updateEditorHeight ()
{
    constexpr int compactHeight = 390;
    constexpr int editorTop = 372;
    constexpr int editorBottomMargin = 20;
    constexpr int maximumHeight = 900;

    // Tone Match needs enough vertical room for the two capture panels,
    // the status row, the spectrum graph and the bottom button row.
    // The normal compact/expanded editor height is restored when it closes.
    if (toneMatchPanel != nullptr && toneMatchPanel->isVisible ())
    {
        constexpr int toneMatchEditorHeight = 690;

        if (getHeight () != toneMatchEditorHeight)
            setSize (getWidth (), toneMatchEditorHeight);
        else
            resized ();

        return;
    }

    int targetHeight = compactHeight;

    if (selectedEffectBlockIndex >= 0)
    {
        const auto contentWidth = juce::jmax (100, getWidth () - 56);

        for (const auto& block : effectBlocks)
        {
            if (block != nullptr && block->getBlockIndex () == selectedEffectBlockIndex)
            {
                targetHeight = editorTop
                             + block->getPreferredHeight (contentWidth)
                             + editorBottomMargin;
                break;
            }
        }
    }

    targetHeight = juce::jlimit (compactHeight, maximumHeight, targetHeight);

    if (getHeight () != targetHeight)
        setSize (getWidth (), targetHeight);
    else
        resized ();
}

void AudioPluginAudioProcessorEditor::updateEffectChainRibbon (const gp200::GP200Preset& preset)
{
    auto colourForBlock = [] (const juce::String& name)
    {
        // Keep the ribbon palette identical to EffectBlockComponent::colourForSlotIndex().
        if (name == "PRE") return juce::Colour (0xffffb12b);
        if (name == "WAH") return juce::Colour (0xffd761ff);
        if (name == "DST") return juce::Colour (0xffff5050);
        if (name == "AMP") return juce::Colour (0xffff8a2a);
        if (name == "NR")  return juce::Colour (0xff9aa6a6);
        if (name == "CAB") return juce::Colour (0xff3be07d);
        if (name == "EQ")  return juce::Colour (0xff35c8ff);
        if (name == "MOD") return juce::Colour (0xff4f8dff);
        if (name == "DLY") return juce::Colour (0xff7068ff);
        if (name == "RVB") return juce::Colour (0xffb96cff);
        if (name == "VOL") return juce::Colour (0xffb7b7b7);
        return juce::Colour (0xffffa42a);
    };
    std::vector<EffectChainRibbonComponent::Item> items;
    for (const auto blockIndex : preset.routingOrder)
    {
        if (!juce::isPositiveAndBelow (blockIndex, static_cast<int> (preset.effects.size ()))) continue;
        const auto& effect = preset.effects[static_cast<std::size_t> (blockIndex)];
        EffectChainRibbonComponent::Item item;
        item.blockIndex = blockIndex;
        item.blockName = gp200::GP200PresetCodec::blockNameForSlotIndex (effect.slotIndex);
        item.enabled = effect.enabled;
        item.colour = colourForBlock (item.blockName);
        items.push_back (std::move (item));
    }
    effectChainRibbon.setItems (std::move (items));
    effectChainRibbon.setSelectedBlockIndex (selectedEffectBlockIndex);
}

int AudioPluginAudioProcessorEditor::getDropPositionForContentY (int contentY) const
{
    if (effectBlocks.empty ())
        return 0;

    for (int i = 0; i < static_cast<int> (effectBlocks.size ()); ++i)
    {
        const auto* block = effectBlocks[static_cast<std::size_t> (i)].get ();

        if (block == nullptr)
            continue;

        const auto midpoint = block->getY () + block->getHeight () / 2;

        if (contentY < midpoint)
            return i;
    }

    return static_cast<int> (effectBlocks.size ());
}

int AudioPluginAudioProcessorEditor::getDropLineYForPosition (int dropPosition) const
{
    if (effectBlocks.empty ())
        return 0;

    const auto count = static_cast<int> (effectBlocks.size ());
    const auto safePosition = juce::jlimit (0, count, dropPosition);

    if (safePosition >= count)
    {
        const auto* lastBlock = effectBlocks.back ().get ();

        if (lastBlock == nullptr)
            return 0;

        return lastBlock->getY () + lastBlock->getHeight () + 4;
    }

    const auto* block = effectBlocks[static_cast<std::size_t> (safePosition)].get ();

    if (block == nullptr)
        return 0;

    return juce::jmax (0, block->getY () - 4);
}

void AudioPluginAudioProcessorEditor::updateDragDropIndicator (int contentY)
{
    const auto dropPosition = getDropPositionForContentY (contentY);
    const auto lineY = getDropLineYForPosition (dropPosition);

    dropIndicator.setBounds (0, juce::jmax (0, lineY - 3), effectsContent.getWidth (), 6);

    dropIndicator.toFront (false);
    dropIndicator.setVisible (true);
    dropIndicator.repaint ();
}

void AudioPluginAudioProcessorEditor::hideDragDropIndicator ()
{
    dropIndicator.setVisible (false);
}

void AudioPluginAudioProcessorEditor::moveEffectBlockToPosition (int blockIndex, int targetPosition)
{
    if (!midiConnection.isConnected ())
    {
        std::vector<int> order (offlinePreset.routingOrder.begin (), offlinePreset.routingOrder.end ());
        const auto currentIterator = std::find (order.begin (), order.end (), blockIndex);
        if (currentIterator == order.end ())
            return;

        const auto currentPosition = static_cast<int> (std::distance (order.begin (), currentIterator));
        auto adjustedTargetPosition = juce::jlimit (0, static_cast<int> (order.size ()), targetPosition);
        if (currentPosition < adjustedTargetPosition)
            --adjustedTargetPosition;
        if (adjustedTargetPosition == currentPosition)
            return;

        const auto movedBlock = *currentIterator;
        order.erase (currentIterator);
        adjustedTargetPosition = juce::jlimit (0, static_cast<int> (order.size ()), adjustedTargetPosition);
        order.insert (order.begin () + adjustedTargetPosition, movedBlock);
        for (std::size_t i = 0; i < offlinePreset.routingOrder.size (); ++i)
            offlinePreset.routingOrder[i] = order[i];

        offlinePresetDirty = true;
        ++offlinePresetRevision;
        effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
        updateEffectBlocksUI ();
        repaint ();
        return;
    }

    const auto currentDump = midiConnection.getCurrentPresetDumpDataCopy ();

    if (currentDump.getSize () == 0)
    {
        repaint ();
        return;
    }

    const auto currentPreset = gp200::GP200PresetCodec::decodeLivePresetDump (currentDump);

    if (!currentPreset.isValid)
    {
        repaint ();
        return;
    }

    std::vector<int> order;
    order.reserve (currentPreset.routingOrder.size ());

    for (const auto index : currentPreset.routingOrder)
    {
        if (index >= 0 && index < static_cast<int> (gp200::effectBlockCount))
            order.push_back (index);
    }

    auto currentIterator = std::find (order.begin (), order.end (), blockIndex);

    if (currentIterator == order.end ())
    {
        repaint ();
        return;
    }

    const auto currentPosition = static_cast<int> (std::distance (order.begin (), currentIterator));

    const auto safeTargetPosition = juce::jlimit (0, static_cast<int> (order.size ()), targetPosition);

    auto adjustedTargetPosition = safeTargetPosition;

    if (currentPosition < adjustedTargetPosition)
        --adjustedTargetPosition;

    if (adjustedTargetPosition == currentPosition)
        return;

    const auto movedBlock = *currentIterator;

    order.erase (currentIterator);

    adjustedTargetPosition = juce::jlimit (0, static_cast<int> (order.size ()), adjustedTargetPosition);

    order.insert (order.begin () + adjustedTargetPosition, movedBlock);

    auto newOrder = currentPreset.routingOrder;

    for (int i = 0; i < static_cast<int> (newOrder.size ()) && i < static_cast<int> (order.size ()); ++i)
        newOrder[static_cast<std::size_t> (i)] = order[static_cast<std::size_t> (i)];

    if (!midiConnection.sendReorderEffects (newOrder, currentPreset.fxLoopSend, currentPreset.fxLoopReturn))
    {
        repaint ();
        return;
    }

    juce::MessageManager::callAsync (
        [this]
        {
            effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
            updateEffectBlocksUI ();
            repaint ();
        });
}

//==============================================================================
void AudioPluginAudioProcessorEditor::drawInfoBox (juce::Graphics& g,
                                                   juce::Rectangle<int> bounds,
                                                   const juce::String& title,
                                                   const juce::String& value) const
{
    g.setColour (panelColour);
    g.fillRoundedRectangle (bounds.toFloat (), 5.0f);

    g.setColour (panelOutlineColour);
    g.drawRoundedRectangle (bounds.toFloat ().reduced (0.5f), 5.0f, 1.2f);

    if (value.isEmpty ())
    {
        g.setFont (gp200ui::medium (15.75f));
        g.setColour (panelOutlineColour);
        g.drawText (title, bounds.reduced (8, 0), juce::Justification::centred);
        return;
    }

    g.setFont (gp200ui::regular (14.25f));
    g.setColour (mutedTextColour);
    g.drawText (title, bounds.withHeight (18).reduced (8, 0), juce::Justification::centred);

    g.setFont (gp200ui::medium (15.75f));
    g.setColour (textColour);
    g.drawText (value, bounds.withTrimmedTop (18).reduced (8, 0), juce::Justification::centred);
}

void AudioPluginAudioProcessorEditor::drawStatusPill (juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    const auto connected = midiConnection.isConnected ();

    g.setColour (connected ? statusOnColour : statusOffColour);
    g.fillRoundedRectangle (bounds.toFloat (), 5.0f);

    g.setColour (juce::Colours::black);
    g.setFont (gp200ui::medium (15.75f));
    g.drawText (connected ? "ON" : "OFF", bounds, juce::Justification::centred);
}

//==============================================================================
juce::String AudioPluginAudioProcessorEditor::getCurrentPresetCompactText () const
{
    return formatPresetCompact (midiConnection.getCurrentSlot (), midiConnection.getCurrentPresetName ());
}

juce::String AudioPluginAudioProcessorEditor::
    getSavedPresetCompactText () const
{
    const auto snapshotIndex =
        getSelectedCompareSnapshotIndex ();

    const auto slot =
        processorRef.getSavedGP200PresetSnapshotSlot (
            snapshotIndex);

    if (!processorRef.hasSavedGP200PresetData (
            snapshotIndex))
    {
        return "empty";
    }

    const auto savedName =
        processorRef.getSavedGP200PresetSnapshotName (
            snapshotIndex).trim ();

    // Offline snapshots intentionally use slot -1. They are valid DAW
    // snapshots and must display their saved preset name instead of "empty".
    if (slot < 0)
        return savedName.isNotEmpty () ? savedName : "Offline preset";

    return formatPresetCompact (slot, savedName);
}

int AudioPluginAudioProcessorEditor::wrapPresetSlot (int slot)
{
    while (slot < 0)
        slot += 256;

    while (slot > 255)
        slot -= 256;

    return slot;
}

juce::String AudioPluginAudioProcessorEditor::formatSlotCompact (int slot)
{
    if (slot < 0)
        return "unknown";

    const int bank = slot / 4 + 1;
    const int slotInBank = slot % 4;

    const juce::String slotLetter = juce::String ("ABCD").substring (slotInBank, slotInBank + 1);

    return juce::String (bank).paddedLeft ('0', 2) + "-" + slotLetter;
}

juce::String AudioPluginAudioProcessorEditor::midiNoteToName (
    int midiNote)
{
    if (midiNote < 0)
        return "--";

    static constexpr const char* noteNames[]
    {
        "C",
        "C#",
        "D",
        "D#",
        "E",
        "F",
        "F#",
        "G",
        "G#",
        "A",
        "A#",
        "B"
    };

    const int noteIndex = midiNote % 12;
    const int octave = midiNote / 12 - 1;

    return juce::String (noteNames[noteIndex]) +
           juce::String (octave);
}

juce::String AudioPluginAudioProcessorEditor::formatPresetCompact (int slot, const juce::String& presetName)
{
    if (slot < 0)
        return "unknown";

    auto text = formatSlotCompact (slot);

    if (isUsefulPresetName (presetName))
        text << "  " << presetName.trim ();

    return text;
}

bool AudioPluginAudioProcessorEditor::isUsefulPresetName (const juce::String& presetName)
{
    const auto name = presetName.trim ();

    return name.isNotEmpty () && name != "unknown" && name != "requesting...";
}
