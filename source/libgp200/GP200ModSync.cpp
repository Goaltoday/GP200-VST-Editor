#include "GP200ModSync.h"

#include <map>

namespace gp200
{
namespace
{
struct OverrideEntry
{
    juce::String displayName;
    juce::String sourceFile;
    bool customCloAmp{false};
    bool isCab{false};
};

juce::CriticalSection syncLock;
std::map<juce::uint32, OverrideEntry> overrides;
juce::int64 lastModificationMs{-1};
bool loadedOnce{false};
std::uint64_t revision{0};

juce::uint32 parseEffectId (const juce::var& value)
{
    if (value.isInt () || value.isInt64 ())
        return static_cast<juce::uint32> (static_cast<juce::int64> (value));

    auto text = value.toString ().trim ();
    if (text.startsWithIgnoreCase ("0x"))
        text = text.substring (2);

    return static_cast<juce::uint32> (text.getHexValue32 ());
}

void parseOverrideArray (const juce::var& root,
                         const juce::Identifier& property,
                         bool customCloAmp,
                         bool isCab)
{
    const auto arrayVar = root.getProperty (property, {});
    const auto* array = arrayVar.getArray ();
    if (array == nullptr)
        return;

    for (const auto& item : *array)
    {
        const auto* object = item.getDynamicObject ();
        if (object == nullptr)
            continue;

        const auto effectId = parseEffectId (object->getProperty ("effect_id"));
        if (effectId == 0)
            continue;

        OverrideEntry entry;
        entry.displayName = object->getProperty ("display_name").toString ().trim ();
        entry.sourceFile = object->getProperty ("source_file").toString ().trim ();
        entry.customCloAmp = customCloAmp;
        entry.isCab = isCab;
        overrides[effectId] = std::move (entry);
    }
}

void loadUnlocked (const juce::File& file)
{
    overrides.clear ();

    if (! file.existsAsFile ())
    {
        lastModificationMs = -1;
        loadedOnce = true;
        ++revision;
        return;
    }

    const auto parsed = juce::JSON::parse (file.loadFileAsString ());
    if (parsed.isVoid () || parsed.getDynamicObject () == nullptr)
    {
        lastModificationMs = file.getLastModificationTime ().toMilliseconds ();
        loadedOnce = true;
        ++revision;
        return;
    }

    parseOverrideArray (parsed, "factory_amp_overrides", true, false);
    parseOverrideArray (parsed, "factory_cab_overrides", false, true);

    lastModificationMs = file.getLastModificationTime ().toMilliseconds ();
    loadedOnce = true;
    ++revision;
}

void saveUnlocked ()
{
    auto* rootObject = new juce::DynamicObject ();
    rootObject->setProperty ("schema", "gp200-mod-sync/v1");
    rootObject->setProperty ("product", "GP-200");
    rootObject->setProperty ("firmware", "V180");

    auto* features = new juce::DynamicObject ();
    features->setProperty ("user_ir_samples", 2048);
    features->setProperty ("factory_cab_samples", 1024);
    rootObject->setProperty ("features", juce::var (features));

    juce::Array<juce::var> ampArray;
    juce::Array<juce::var> cabArray;

    for (const auto& [effectId, entry] : overrides)
    {
        auto* object = new juce::DynamicObject ();
        object->setProperty ("effect_id", static_cast<juce::int64> (effectId));
        object->setProperty ("display_name", entry.displayName);
        object->setProperty ("source_file", entry.sourceFile);
        if (entry.customCloAmp)
            ampArray.add (juce::var (object));
        else if (entry.isCab)
            cabArray.add (juce::var (object));
        else
            delete object;
    }

    rootObject->setProperty ("factory_amp_overrides", juce::var (ampArray));
    rootObject->setProperty ("factory_cab_overrides", juce::var (cabArray));

    const auto file = GP200ModSync::getManifestFile ();
    file.getParentDirectory ().createDirectory ();
    file.replaceWithText (juce::JSON::toString (juce::var (rootObject), true) + "\n");
    lastModificationMs = file.getLastModificationTime ().toMilliseconds ();
    loadedOnce = true;
    ++revision;
}

void recordOverrideUnlocked (juce::uint32 effectId,
                             const juce::String& displayName,
                             const juce::String& sourceFile,
                             bool customCloAmp,
                             bool isCab)
{
    OverrideEntry entry;
    entry.displayName = displayName.trim ();
    entry.sourceFile = sourceFile.trim ();
    entry.customCloAmp = customCloAmp;
    entry.isCab = isCab;
    overrides[effectId] = std::move (entry);
    saveUnlocked ();
}

} // namespace

juce::File GP200ModSync::getManifestFile ()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("GP200")
        .getChildFile ("GP200_MOD_SYNC.json");
}

void GP200ModSync::reloadIfChanged ()
{
    const juce::ScopedLock lock (syncLock);
    const auto file = getManifestFile ();

    if (! loadedOnce)
    {
        loadUnlocked (file);
        return;
    }

    if (! file.existsAsFile ())
    {
        if (lastModificationMs >= 0 || ! overrides.empty ())
            loadUnlocked (file);
        return;
    }

    const auto modificationMs = file.getLastModificationTime ().toMilliseconds ();
    if (modificationMs != lastModificationMs)
        loadUnlocked (file);
}

std::uint64_t GP200ModSync::getRevision ()
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    return revision;
}

bool GP200ModSync::isCustomCloAmp (juce::uint32 effectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = overrides.find (effectId);
    return it != overrides.end () && it->second.customCloAmp;
}

juce::String GP200ModSync::getDisplayName (juce::uint32 effectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = overrides.find (effectId);
    return it != overrides.end () ? it->second.displayName : juce::String{};
}

juce::String GP200ModSync::getSourceFile (juce::uint32 effectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = overrides.find (effectId);
    return it != overrides.end () ? it->second.sourceFile : juce::String{};
}

juce::String GP200ModSync::getDescription (juce::uint32 effectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = overrides.find (effectId);
    if (it == overrides.end ())
        return {};

    const auto& entry = it->second;
    if (entry.sourceFile.isNotEmpty ())
        return (entry.customCloAmp ? "CLO: " : (entry.isCab ? "IR: " : "Source: ")) + entry.sourceFile;

    if (entry.customCloAmp)
        return "Factory AMP converted to embedded CLO";
    if (entry.isCab)
        return "Modified Factory CAB";
    return {};
}

void GP200ModSync::recordFactoryAmpOverride (juce::uint32 effectId,
                                             const juce::String& displayName,
                                             const juce::String& sourceFile)
{
    const juce::ScopedLock lock (syncLock);
    if (!loadedOnce)
        loadUnlocked (getManifestFile ());
    recordOverrideUnlocked (effectId, displayName, sourceFile, true, false);
}

void GP200ModSync::recordFactoryCabOverride (juce::uint32 effectId,
                                             const juce::String& displayName,
                                             const juce::String& sourceFile)
{
    const juce::ScopedLock lock (syncLock);
    if (!loadedOnce)
        loadUnlocked (getManifestFile ());
    recordOverrideUnlocked (effectId, displayName, sourceFile, false, true);
}

} // namespace gp200
