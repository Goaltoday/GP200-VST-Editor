#include "GP200ModSync.h"

#include "GP200ModSyncProtocol.h"

#include <algorithm>
#include <map>
#include <set>

namespace gp200
{
namespace
{
struct OverrideEntry
{
    juce::String displayName;
    juce::String sourceFile;
    bool customCloAmp{false};
    bool customNamAmp{false};
    bool isCab{false};
};

juce::CriticalSection syncLock;
std::map<juce::uint32, OverrideEntry> overrides;
juce::int64 lastModificationMs{-1};
bool loadedOnce{false};
std::uint64_t revision{0};
int knownUserIRSamples{0};
bool deviceSnapshot{false};
bool unsavedDeviceSnapshot{false};
// MS11 cannot discover NAM2 or verify the generated firmware SHA. Keep the
// imported profile and its provenance separate from live CLO/CAB refreshes.
juce::String manifestFirmware;
juce::String manifestFirmwareSha;
juce::String metadataError;

struct PreBankEntry
{
    juce::uint32 sourceEffectId{0};
    juce::String displayName;
    juce::String sourceModule;
    juce::String description;
};
std::map<juce::uint32, PreBankEntry> preBankOverrides;
juce::int64 preBankLastModificationMs{-1};
bool preBankLoadedOnce{false};
bool livePreSnapshot{false};
std::map<juce::uint32, PreBankEntry> wahConfigOverrides;
juce::int64 wahConfigLastModificationMs{-1};
bool wahConfigLoadedOnce{false};

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
                         bool isCab,
                         std::map<juce::uint32, OverrideEntry>& destination)
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
        destination[effectId] = std::move (entry);
    }
}

bool validCloOverrides (const juce::var& root)
{
    const auto value = root.getProperty ("factory_amp_overrides", {});
    if (value.isVoid ()) return true;
    const auto* rows = value.getArray ();
    if (rows == nullptr) return false;
    std::set<juce::uint32> seen;
    for (const auto& row : *rows)
    {
        const auto id = parseEffectId (row.getProperty ("effect_id", {}));
        if (std::find (modsync::ampIds.begin (), modsync::ampIds.end (), id) == modsync::ampIds.end ()
            || ! seen.insert (id).second) return false;
        // Older cache entries omit these fields. Explicitly typed profiles
        // must be CLO5: NAM2 must never enter this writable destination map.
        const auto profile = row.getProperty ("parameter_profile", {});
        const auto count = row.getProperty ("parameter_count", {});
        if ((! profile.isVoid () && profile.toString () != "CLO5")
            || (! count.isVoid () && static_cast<int> (count) != 5)) return false;
    }
    return true;
}

// NAM2 is accepted only from the generated manifest, never inferred from
// the name of an AMP or from a two-parameter stock amplifier.
bool parseNamOverrides (const juce::var& root,
                        std::map<juce::uint32, OverrideEntry>& destination)
{
    const auto value = root.getProperty ("nam_amp_overrides", {});
    if (value.isVoid ()) return true; // Legacy manifest.
    const auto* rows = value.getArray ();
    if (rows == nullptr || rows->size () > 5) return false;
    if (root.getProperty ("schema", {}).toString () != "gp200-mod-sync/v1"
        || root.getProperty ("product", {}).toString () != "GP-200") return false;
    const auto firmware = root.getProperty ("firmware", {}).toString ();
    if (firmware != "V180" && firmware != "V182") return false;
    if (! rows->isEmpty ())
    {
        const auto hash = root.getProperty ("firmware_sha256", {}).toString ();
        if (hash.length () != 64 || ! hash.containsOnly ("0123456789abcdefABCDEF")) return false;
    }
    std::set<juce::uint32> seen;
    for (const auto& row : *rows)
    {
        if (row.getDynamicObject () == nullptr) return false;
        const auto id = parseEffectId (row.getProperty ("effect_id", {}));
        if (std::find (modsync::ampIds.begin (), modsync::ampIds.end (), id) == modsync::ampIds.end ()
            || ! seen.insert (id).second || destination.count (id) != 0) return false;
        const auto name = row.getProperty ("display_name", {}).toString ().trim ();
        if (name.isEmpty () || name.length () > 16) return false;
        for (auto c : name) if (c < 32 || c > 126) return false;
        if (row.getProperty ("parameter_profile", {}).toString () != "NAM2"
            || ! row.getProperty ("parameter_count", {}).isInt ()
            || static_cast<int> (row.getProperty ("parameter_count", {})) != 2
            || ! row.getProperty ("neutral", {}).isInt ()
            || static_cast<int> (row.getProperty ("neutral", {})) != 50) return false;
        const auto controls = row.getProperty ("controls", {});
        const auto* names = controls.getArray ();
        if (names == nullptr || names->size () != 2
            || (*names)[0].toString () != "Input" || (*names)[1].toString () != "Output") return false;
        OverrideEntry entry;
        entry.displayName = name;
        entry.sourceFile = row.getProperty ("source_file", {}).toString ().trim ();
        entry.customNamAmp = true;
        destination[id] = std::move (entry);
    }
    return true;
}

void loadPreBankUnlocked (const juce::File& file)
{
    preBankOverrides.clear ();
    if (! file.existsAsFile ())
    {
        preBankLastModificationMs = -1;
        preBankLoadedOnce = true;
        ++revision;
        return;
    }

    const auto parsed = juce::JSON::parse (file.loadFileAsString ());
    if (! parsed.isVoid () && parsed.getDynamicObject () != nullptr)
    {
        if (const auto* array = parsed.getProperty ("slots", {}).getArray ())
        {
            for (const auto& item : *array)
            {
                const auto* object = item.getDynamicObject ();
                if (object == nullptr) continue;
                const auto preId = parseEffectId (object->getProperty ("pre_effect_id"));
                const auto sourceId = parseEffectId (object->getProperty ("source_effect_id"));
                if (preId == 0 || sourceId == 0) continue;
                PreBankEntry entry;
                entry.sourceEffectId = sourceId;
                entry.displayName = object->getProperty ("display_name").toString ().trim ();
                entry.sourceModule = object->getProperty ("source_module").toString ().trim ().toUpperCase ();
                entry.description = object->getProperty ("relocation_profile").toString ().trim ();
                preBankOverrides[preId] = std::move (entry);
            }
        }
    }
    preBankLastModificationMs = file.getLastModificationTime ().toMilliseconds ();
    preBankLoadedOnce = true;
    ++revision;
}

void reloadPreBankUnlocked ()
{
    const auto file = GP200ModSync::getPreBankFile ();
    if (! preBankLoadedOnce)
    {
        loadPreBankUnlocked (file);
        return;
    }
    if (! file.existsAsFile ())
    {
        if (preBankLastModificationMs >= 0 || ! preBankOverrides.empty ())
            loadPreBankUnlocked (file);
        return;
    }
    const auto ms = file.getLastModificationTime ().toMilliseconds ();
    if (ms != preBankLastModificationMs)
        loadPreBankUnlocked (file);
}

void loadWahConfigUnlocked (const juce::File& file)
{
    wahConfigOverrides.clear ();
    if (! file.existsAsFile ())
    {
        wahConfigLastModificationMs = -1;
        wahConfigLoadedOnce = true;
        ++revision;
        return;
    }

    const auto parsed = juce::JSON::parse (file.loadFileAsString ());
    if (! parsed.isVoid () && parsed.getDynamicObject () != nullptr)
    {
        if (const auto* array = parsed.getProperty ("slots", {}).getArray ())
        {
            for (const auto& item : *array)
            {
                const auto* object = item.getDynamicObject ();
                if (object == nullptr) continue;
                const auto wahId = parseEffectId (object->getProperty ("wah_effect_id"));
                const auto sourceId = parseEffectId (object->getProperty ("source_effect_id"));
                if (wahId == 0 || sourceId == 0) continue;
                PreBankEntry entry;
                entry.sourceEffectId = sourceId;
                entry.displayName = object->getProperty ("display_name").toString ().trim ();
                entry.sourceModule = object->getProperty ("source_module").toString ().trim ().toUpperCase ();
                entry.description = object->getProperty ("relocation_profile").toString ().trim ();
                wahConfigOverrides[wahId] = std::move (entry);
            }
        }
    }
    wahConfigLastModificationMs = file.getLastModificationTime ().toMilliseconds ();
    wahConfigLoadedOnce = true;
    ++revision;
}

void reloadWahConfigUnlocked ()
{
    const auto file = GP200ModSync::getWahConfigFile ();
    if (! wahConfigLoadedOnce)
    {
        loadWahConfigUnlocked (file);
        return;
    }
    if (! file.existsAsFile ())
    {
        if (wahConfigLastModificationMs >= 0 || ! wahConfigOverrides.empty ())
            loadWahConfigUnlocked (file);
        return;
    }
    const auto ms = file.getLastModificationTime ().toMilliseconds ();
    if (ms != wahConfigLastModificationMs)
        loadWahConfigUnlocked (file);
}

void loadUnlocked (const juce::File& file)
{
    if (! file.existsAsFile ())
    {
        overrides.clear ();
        knownUserIRSamples = 0;
        manifestFirmware.clear ();
        manifestFirmwareSha.clear ();
        metadataError.clear ();
        deviceSnapshot = false;
        lastModificationMs = -1;
        loadedOnce = true;
        ++revision;
        return;
    }

    const auto parsed = juce::JSON::parse (file.loadFileAsString ());
    std::map<juce::uint32, OverrideEntry> next;
    if (! parsed.isVoid () && parsed.getDynamicObject () != nullptr)
    {
        parseOverrideArray (parsed, "factory_amp_overrides", true, false, next);
        parseOverrideArray (parsed, "factory_cab_overrides", false, true, next);
    }
    if (parsed.isVoid () || parsed.getDynamicObject () == nullptr
        || ! validCloOverrides (parsed) || ! parseNamOverrides (parsed, next))
    {
        // A partially copied/invalid manifest must not remove NAM protections.
        metadataError = "Invalid firmware metadata; previous profiles retained";
        lastModificationMs = file.getLastModificationTime ().toMilliseconds ();
        loadedOnce = true;
        ++revision;
        return;
    }
    overrides = std::move (next);
    const auto features = parsed.getProperty ("features", {});
    const auto samples = static_cast<int> (features.getProperty ("user_ir_samples", 0));
    knownUserIRSamples = samples == 1024 || samples == 2048 ? samples : 0;
    manifestFirmware = parsed.getProperty ("firmware", {}).toString ();
    manifestFirmwareSha = parsed.getProperty ("firmware_sha256", {}).toString ();
    deviceSnapshot = false;
    metadataError.clear ();
    lastModificationMs = file.getLastModificationTime ().toMilliseconds ();
    loadedOnce = true;
    ++revision;
}

bool saveUnlocked ()
{
    auto* rootObject = new juce::DynamicObject ();
    rootObject->setProperty ("schema", "gp200-mod-sync/v1");
    rootObject->setProperty ("product", "GP-200");
    if (manifestFirmware.isNotEmpty ()) rootObject->setProperty ("firmware", manifestFirmware);
    if (manifestFirmwareSha.isNotEmpty ()) rootObject->setProperty ("firmware_sha256", manifestFirmwareSha);
    rootObject->setProperty ("nam_metadata_source", "generated-manifest-not-verified-by-ms11");

    auto* features = new juce::DynamicObject ();
    if (knownUserIRSamples != 0) features->setProperty ("user_ir_samples", knownUserIRSamples);
    rootObject->setProperty ("source", deviceSnapshot ? "device-midi-ms11" : "local-cache");
    features->setProperty ("factory_cab_samples", 1024);
    rootObject->setProperty ("features", juce::var (features));

    juce::Array<juce::var> ampArray;
    juce::Array<juce::var> namArray;
    juce::Array<juce::var> cabArray;

    for (const auto& [effectId, entry] : overrides)
    {
        auto* object = new juce::DynamicObject ();
        object->setProperty ("effect_id", static_cast<juce::int64> (effectId));
        object->setProperty ("display_name", entry.displayName);
        object->setProperty ("source_file", entry.sourceFile);
        if (entry.customNamAmp)
        {
            object->setProperty ("parameter_profile", "NAM2");
            object->setProperty ("parameter_count", 2);
            object->setProperty ("neutral", 50);
            juce::Array<juce::var> controls;
            controls.add ("Input"); controls.add ("Output");
            object->setProperty ("controls", juce::var (controls));
            namArray.add (juce::var (object));
        }
        else if (entry.customCloAmp)
        {
            object->setProperty ("parameter_profile", "CLO5");
            object->setProperty ("parameter_count", 5);
            ampArray.add (juce::var (object));
        }
        else if (entry.isCab)
            cabArray.add (juce::var (object));
        else
            delete object;
    }

    features->setProperty ("nam_m7opt_count", namArray.size ());
    if (namArray.size () != 0 || manifestFirmware.isNotEmpty ())
        rootObject->setProperty ("nam_amp_overrides", juce::var (namArray));
    rootObject->setProperty ("factory_amp_overrides", juce::var (ampArray));
    rootObject->setProperty ("factory_cab_overrides", juce::var (cabArray));

    const auto file = GP200ModSync::getManifestFile ();
    file.getParentDirectory ().createDirectory ();
    const bool saved = file.replaceWithText (juce::JSON::toString (juce::var (rootObject), true) + "\n");
    lastModificationMs = file.getLastModificationTime ().toMilliseconds ();
    unsavedDeviceSnapshot = deviceSnapshot && !saved;
    loadedOnce = true;
    ++revision;
    return saved;
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

bool GP200ModSync::replacePreBankFromDevice (const juce::var& snapshot)
{
    const juce::ScopedLock lock (syncLock);
    preBankOverrides.clear ();
    if (const auto* rows = snapshot.getProperty ("slots", {}).getArray ())
        for (const auto& row : *rows)
        {
            PreBankEntry entry;
            entry.sourceEffectId = parseEffectId (row.getProperty ("source_effect_id", {}));
            entry.displayName = row.getProperty ("display_name", {}).toString ();
            entry.sourceModule = row.getProperty ("source_module", {}).toString ();
            entry.description = row.getProperty ("relocation_profile", {}).toString ();
            preBankOverrides[parseEffectId (row.getProperty ("pre_effect_id", {}))] = std::move (entry);
        }
    livePreSnapshot = true; preBankLoadedOnce = true; ++revision;
    const auto file = getPreBankFile ();
    const bool saved = file.getParentDirectory ().createDirectory ().wasOk ()
        && file.replaceWithText (juce::JSON::toString (snapshot));
    preBankLastModificationMs = saved ? file.getLastModificationTime ().toMilliseconds () : -1;
    return saved;
}

bool GP200ModSync::replaceFromDevice (const juce::var& snapshot, int userIRSamples)
{
    reloadIfChanged (); // Import local profiles even when MIDI wins the startup race.
    const juce::ScopedLock lock (syncLock);
    if (metadataError.isNotEmpty () || ! validCloOverrides (snapshot)) return false;
    std::map<juce::uint32, OverrideEntry> next;
    parseOverrideArray (snapshot, "factory_amp_overrides", true, false, next);
    parseOverrideArray (snapshot, "factory_cab_overrides", false, true, next);
    // Stock records in MS11 mean "not CLO", not "not NAM". Do not erase
    // NAM2 on reconnect. An explicit CLO conflict rejects the whole snapshot.
    for (const auto& [id, entry] : overrides)
        if (entry.customNamAmp)
        {
            if (next.count (id) != 0)
            {
                metadataError = "NAM manifest conflicts with live CLO map; load matching firmware metadata";
                ++revision;
                return false;
            }
            next[id] = entry;
        }
    overrides = std::move (next);
    knownUserIRSamples = userIRSamples;
    deviceSnapshot = true;
    return saveUnlocked ();
}

juce::File GP200ModSync::getManifestFile ()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("GP200")
        .getChildFile ("GP200_MOD_SYNC.json");
}

juce::File GP200ModSync::getPreBankFile ()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("GP200")
        .getChildFile ("GP200_PRE_BANK.json");
}

juce::File GP200ModSync::getWahConfigFile ()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("GP200")
        .getChildFile ("GP200_WAH_CONFIG.json");
}

void GP200ModSync::reloadIfChanged ()
{
    const juce::ScopedLock lock (syncLock);
    if (!livePreSnapshot) reloadPreBankUnlocked ();
    reloadWahConfigUnlocked ();
    // Retain a live snapshot after a failed cache write, but allow the user
    // to recover by replacing the file with a new generated manifest.
    const auto file = getManifestFile ();
    if (unsavedDeviceSnapshot && (! file.existsAsFile ()
        || file.getLastModificationTime ().toMilliseconds () == lastModificationMs)) return;
    unsavedDeviceSnapshot = false;

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

bool GP200ModSync::isNamAmp (juce::uint32 effectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = overrides.find (effectId);
    return it != overrides.end () && it->second.customNamAmp;
}

bool GP200ModSync::canWriteFactoryClo (int zeroBasedFactoryAmpIndex)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    if (metadataError.isNotEmpty () || zeroBasedFactoryAmpIndex < 0
        || zeroBasedFactoryAmpIndex >= static_cast<int> (modsync::ampIds.size ())) return false;
    const auto it = overrides.find (modsync::ampIds[static_cast<std::size_t> (zeroBasedFactoryAmpIndex)]);
    return it != overrides.end () && it->second.customCloAmp && ! it->second.customNamAmp;
}

juce::String GP200ModSync::getNamMetadataStatus ()
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    if (metadataError.isNotEmpty ()) return metadataError;
    int count = 0;
    for (const auto& pair : overrides) if (pair.second.customNamAmp) ++count;
    return count == 0 ? juce::String ()
                     : juce::String (count) + " NAM2 from firmware JSON (not verified by MIDI)";
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
    if (entry.customNamAmp)
        return "NAM A2 Lite 48 kHz M7OPT: Input / Output (50 neutral). Metadata from firmware JSON; not verified by MIDI."
            + (entry.sourceFile.isNotEmpty () ? " Source: " + entry.sourceFile : juce::String ());
    if (entry.sourceFile.isNotEmpty ())
        return (entry.customCloAmp ? "CLO: " : (entry.isCab ? "IR: " : "Source: ")) + entry.sourceFile;

    if (entry.customCloAmp)
        return "Factory AMP converted to embedded CLO";
    if (entry.isCab)
        return "Modified Factory CAB";
    return {};
}

juce::uint32 GP200ModSync::getPreBankSourceEffectId (juce::uint32 preEffectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = preBankOverrides.find (preEffectId);
    return it != preBankOverrides.end () ? it->second.sourceEffectId : 0u;
}

juce::String GP200ModSync::getPreBankDisplayName (juce::uint32 preEffectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = preBankOverrides.find (preEffectId);
    return it != preBankOverrides.end () ? it->second.displayName : juce::String{};
}

juce::String GP200ModSync::getPreBankSourceModule (juce::uint32 preEffectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = preBankOverrides.find (preEffectId);
    return it != preBankOverrides.end () ? it->second.sourceModule : juce::String{};
}

juce::String GP200ModSync::getPreBankDescription (juce::uint32 preEffectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = preBankOverrides.find (preEffectId);
    if (it == preBankOverrides.end ()) return {};
    auto text = it->second.sourceModule + " effect relocated into PRE";
    if (it->second.description.isNotEmpty ()) text += " (" + it->second.description + ")";
    return text;
}

juce::uint32 GP200ModSync::getWahConfigSourceEffectId (juce::uint32 wahEffectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = wahConfigOverrides.find (wahEffectId);
    return it != wahConfigOverrides.end () ? it->second.sourceEffectId : 0u;
}

juce::String GP200ModSync::getWahConfigDisplayName (juce::uint32 wahEffectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = wahConfigOverrides.find (wahEffectId);
    return it != wahConfigOverrides.end () ? it->second.displayName : juce::String{};
}

juce::String GP200ModSync::getWahConfigSourceModule (juce::uint32 wahEffectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = wahConfigOverrides.find (wahEffectId);
    return it != wahConfigOverrides.end () ? it->second.sourceModule : juce::String{};
}

juce::String GP200ModSync::getWahConfigDescription (juce::uint32 wahEffectId)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    const auto it = wahConfigOverrides.find (wahEffectId);
    if (it == wahConfigOverrides.end ()) return {};
    auto text = it->second.sourceModule + " effect relocated into WAH";
    if (it->second.description.isNotEmpty ()) text += " (" + it->second.description + ")";
    return text;
}

void GP200ModSync::recordFactoryAmpOverride (juce::uint32 effectId,
                                             const juce::String& displayName,
                                             const juce::String& sourceFile)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    if (!loadedOnce)
        loadUnlocked (getManifestFile ());
    const auto existing = overrides.find (effectId);
    if (metadataError.isNotEmpty () || (existing != overrides.end () && existing->second.customNamAmp)) return;
    recordOverrideUnlocked (effectId, displayName, sourceFile, true, false);
}

void GP200ModSync::recordFactoryCabOverride (juce::uint32 effectId,
                                             const juce::String& displayName,
                                             const juce::String& sourceFile)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    if (!loadedOnce)
        loadUnlocked (getManifestFile ());
    recordOverrideUnlocked (effectId, displayName, sourceFile, false, true);
}

} // namespace gp200
