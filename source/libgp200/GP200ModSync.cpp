#include "GP200ModSync.h"

#include "GP200ModSyncProtocol.h"
#include "GP200EffectDatabase.h"
#include <cmath>

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
std::array<bool,71> inplaceEligible{};
juce::var inplaceDestinations;

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

using EffectKey = std::pair<juce::String, juce::uint32>;
using EffectBank = std::map<EffectKey, std::shared_ptr<const GP200EffectBankEntry>>;
EffectBank effectBank;
bool effectBankLoaded{false};
juce::int64 effectBankModificationMs{-1}, effectBankSize{-1};
juce::String effectBankHash, effectBankFirmware, effectBankError;
double nextMetadataPollMs{0.0};

juce::String canonicalModule (const juce::String& module)
{
    const auto result = module.trim ().toUpperCase ();
    return result == "DIST" ? juce::String ("DST") : result;
}

bool isEffectBankModule (const juce::String& module)
{
    return module == "WAH" || module == "PRE" || module == "DST" || module == "EQ";
}

bool effectBankActiveUnlocked ()
{
    return effectBankLoaded && (manifestFirmware.isEmpty () || manifestFirmware == effectBankFirmware)
        && (manifestFirmwareSha.isEmpty () || manifestFirmwareSha.equalsIgnoreCase (effectBankHash));
}

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
    if (rows == nullptr || rows->size () > (root.getProperty ("features", {}).getProperty ("nam_hot_protocol", {}).toString () == "NH04" ? 62 : 5)) return false;
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

bool parseInplace (const juce::var& root, std::array<bool,71>& eligible, juce::var& destinations)
{
    const auto protocol = root.getProperty ("features", {}).getProperty ("nam_hot_protocol", {}).toString ();
    const auto value = root.getProperty ("nam_inplace_destinations", {});
    if (protocol != "NH04") return value.isVoid ();
    const auto firmware = root.getProperty ("firmware", {}).toString ();
    const auto hash = root.getProperty ("firmware_sha256", {}).toString ();
    const auto* rows = value.getArray ();
    if (root.getProperty ("schema", {}).toString () != "gp200-mod-sync/v1"
        || root.getProperty ("product", {}).toString () != "GP-200"
        || (firmware != "V180" && firmware != "V182")
        || hash.length () != 64 || !hash.containsOnly ("0123456789abcdefABCDEF")
        || rows == nullptr || rows->size () > 62) return false;
    for (const auto& row : *rows)
    {
        const auto index = row.getProperty ("logical_index", {});
        const auto size = row.getProperty ("package_bytes", {});
        const auto offset = row.getProperty ("package_offset", {});
        if (!index.isInt () || !size.isInt () || !offset.isInt ()) return false;
        const int i = static_cast<int> (index), bytes = static_cast<int> (size), at = static_cast<int> (offset);
        if (i < 0 || i >= 71 || eligible[static_cast<std::size_t> (i)] || bytes < 7696
            || at < 68 || at > 0x200000 - bytes
            || parseEffectId (row.getProperty ("effect_id", {})) != modsync::ampIds[static_cast<std::size_t> (i)]) return false;
        eligible[static_cast<std::size_t> (i)] = true;
    }
    destinations = value;
    return true;
}

bool readBankNumber (const juce::var& object, const char* name, float& result)
{
    const auto value = object.getProperty (name, {});
    if (! value.isInt () && ! value.isInt64 () && ! value.isDouble ()) return false;
    result = static_cast<float> (static_cast<double> (value));
    return std::isfinite (result);
}

bool parseEffectBank (const juce::var& root, EffectBank& next)
{
    if (root.getProperty ("schema", {}).toString () != "gp200-effect-bank/v1"
        || root.getProperty ("product", {}).toString () != "GP-200") return false;
    const auto firmware = root.getProperty ("firmware", {}).toString ();
    const auto hash = root.getProperty ("firmware_sha256", {}).toString ();
    if ((firmware != "V180" && firmware != "V182") || hash.length () != 64
        || ! hash.containsOnly ("0123456789abcdefABCDEF")) return false;
    const auto slots = root.getProperty ("slots", {});
    const auto* rows = slots.getArray ();
    if (rows == nullptr || rows->size () > 80) return false;
    for (const auto& row : *rows)
    {
        if (row.getDynamicObject () == nullptr) return false;
        const auto module = canonicalModule (row.getProperty ("destination_module", {}).toString ());
        const auto idValue = row.getProperty ("destination_effect_id", {});
        const auto sourceValue = row.getProperty ("source_effect_id", {});
        if (idValue.isVoid () || sourceValue.isVoid () || ! isEffectBankModule (module)) return false;
        const auto id = parseEffectId (idValue), sourceId = parseEffectId (sourceValue);
        const auto sourceModule = canonicalModule (row.getProperty ("source_module", {}).toString ());
        if (! isEffectBankModule (sourceModule) && sourceModule != "MOD"
            && sourceModule != "DLY" && sourceModule != "RVB") return false;
        const auto destinations = GP200EffectDatabase::getEffectsForModule (module);
        if (std::none_of (destinations.begin (), destinations.end (), [id](const auto& e){return e.effectId == id;})) return false;
        const auto sources = GP200EffectDatabase::getEffectsForModule (sourceModule);
        if (std::none_of (sources.begin (), sources.end (), [sourceId](const auto& e){return e.effectId == sourceId;})) return false;
        const EffectKey key{module, id};
        if (next.count (key) != 0) return false;
        auto entry = std::make_shared<GP200EffectBankEntry> ();
        entry->destinationModule = module; entry->destinationEffectId = id;
        entry->sourceModule = sourceModule; entry->sourceEffectId = sourceId;
        entry->displayName = row.getProperty ("display_name", {}).toString ().trim ();
        entry->profile = row.getProperty ("profile", {}).toString ();
        if (entry->displayName.isEmpty () || entry->displayName.length () > 64) return false;
        const auto params = row.getProperty ("parameters", {});
        const auto* values = params.getArray ();
        if (values == nullptr || values->isEmpty () || values->size () > static_cast<int> (effectParamCount)) return false;
        std::set<int> seen;
        for (int i = 0; i < values->size (); ++i)
        {
            const auto& value = (*values)[i];
            const auto index = value.getProperty ("index", {});
            if (! index.isInt ()) return false;
            const int idx = static_cast<int> (index);
            if (idx < 0 || idx >= static_cast<int> (effectParamCount) || ! seen.insert (idx).second) return false;
            auto& param = entry->parameters[static_cast<std::size_t> (i)];
            auto& name = entry->parameterNames[static_cast<std::size_t> (i)];
            name = value.getProperty ("name", {}).toString ().trim ();
            if (name.isEmpty () || name.length () > 64) return false;
            for (auto c : name) if (c < 32 || c > 126) return false;
            param.idx = idx; param.name = name.toRawUTF8 ();
            if (! readBankNumber (value, "minimum", param.minValue)
                || ! readBankNumber (value, "maximum", param.maxValue)
                || ! readBankNumber (value, "default", param.defaultValue)
                || ! readBankNumber (value, "step", param.step)
                || param.minValue > param.maxValue || param.step <= 0
                || param.defaultValue < param.minValue || param.defaultValue > param.maxValue) return false;
            param.kind = param.minValue == 0.0f && param.maxValue == 1.0f && param.step == 1.0f
                ? GP200ParamKind::toggle : GP200ParamKind::continuous;
            // Relocated ABI indices may differ from the old VST catalog.
            // Match optional display choices by name once, at import time.
            auto& sourceIndex = entry->sourceParameterIndices[static_cast<std::size_t> (i)];
            sourceIndex = -1;
            if (const auto* source = GP200EffectParamDatabase::findParamsForEffect (sourceId))
                for (int j = 0; j < source->count; ++j)
                {
                    const auto sourceName = juce::String (source->params[j].name);
                    if (sourceName.equalsIgnoreCase (name)
                        || (name == "A Sync" && sourceName == "Sync A")
                        || (name == "B Sync" && sourceName == "Sync B"))
                    {
                        sourceIndex = source->params[j].idx;
                        param.kind = source->params[j].kind;
                        break;
                    }
                }
        }
        entry->parameterSet = {id, entry->parameters.data (), values->size ()};
        next.emplace (key, std::move (entry));
    }
    return true;
}

void reloadEffectBankUnlocked ()
{
    const auto file = GP200ModSync::getEffectBankFile ();
    if (! file.existsAsFile ())
    {
        if (effectBankLoaded || effectBankModificationMs >= 0)
        {
            effectBank.clear (); effectBankLoaded = false;
            effectBankModificationMs = -1; effectBankSize = -1; effectBankError.clear ();
            ++revision;
        }
        return;
    }
    const auto stamp = file.getLastModificationTime ().toMilliseconds (), size = file.getSize ();
    if (stamp == effectBankModificationMs && size == effectBankSize) return;
    effectBankModificationMs = stamp; effectBankSize = size;
    EffectBank next;
    const auto root = size <= 1024 * 1024 ? juce::JSON::parse (file.loadFileAsString ()) : juce::var ();
    if (! parseEffectBank (root, next))
    {
        effectBankError = "Invalid GP200_EFFECT_BANK.json; previous valid bank retained";
        ++revision;
        return;
    }
    effectBank = std::move (next); effectBankLoaded = true;
    effectBankHash = root.getProperty ("firmware_sha256", {}).toString ();
    effectBankFirmware = root.getProperty ("firmware", {}).toString ();
    effectBankError.clear (); ++revision;
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
        inplaceEligible = {}; inplaceDestinations = juce::var{};
        metadataError.clear ();
        deviceSnapshot = false;
        lastModificationMs = -1;
        loadedOnce = true;
        ++revision;
        return;
    }

    const auto parsed = juce::JSON::parse (file.loadFileAsString ());
    std::map<juce::uint32, OverrideEntry> next;
    std::array<bool,71> nextEligible{}; juce::var nextDestinations;
    if (! parsed.isVoid () && parsed.getDynamicObject () != nullptr)
    {
        parseOverrideArray (parsed, "factory_amp_overrides", true, false, next);
        parseOverrideArray (parsed, "factory_cab_overrides", false, true, next);
    }
    if (parsed.isVoid () || parsed.getDynamicObject () == nullptr
        || ! validCloOverrides (parsed) || ! parseNamOverrides (parsed, next) || !parseInplace (parsed, nextEligible, nextDestinations))
    {
        // A partially copied/invalid manifest must not remove NAM protections.
        metadataError = "Invalid firmware metadata; previous profiles retained";
        lastModificationMs = file.getLastModificationTime ().toMilliseconds ();
        loadedOnce = true;
        ++revision;
        return;
    }
    overrides = std::move (next);
    inplaceEligible = nextEligible; inplaceDestinations = nextDestinations;
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
    if (!inplaceDestinations.isVoid ())
    {
        features->setProperty ("nam_hot_protocol", "NH04");
        features->setProperty ("nam_hot_storage", "own-amp-package");
        rootObject->setProperty ("nam_inplace_destinations", inplaceDestinations);
    }
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
    reloadEffectBankUnlocked ();
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

void GP200ModSync::pollForChanges ()
{
    const juce::ScopedLock lock (syncLock);
    const auto now = juce::Time::getMillisecondCounterHiRes ();
    if (now < nextMetadataPollMs) return;
    nextMetadataPollMs = now + 1000.0;
    reloadIfChanged ();
}

juce::File GP200ModSync::getEffectBankFile ()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("GP200").getChildFile ("GP200_EFFECT_BANK.json");
}

bool GP200ModSync::hasEffectBank ()
{
    const juce::ScopedLock lock (syncLock);
    return effectBankActiveUnlocked ();
}

std::shared_ptr<const GP200EffectBankEntry> GP200ModSync::findEffectBankEntry (
    juce::uint32 effectId, const juce::String& moduleName)
{
    const juce::ScopedLock lock (syncLock);
    if (! effectBankActiveUnlocked ()) return {};
    const auto it = effectBank.find ({canonicalModule (moduleName), effectId});
    return it != effectBank.end () ? it->second : nullptr;
}

juce::String GP200ModSync::getEffectBankStatus ()
{
    const juce::ScopedLock lock (syncLock);
    if (effectBankError.isNotEmpty ()) return effectBankError;
    if (effectBankLoaded && ! effectBankActiveUnlocked ()) return "Effect bank / NAM manifest firmware metadata differ";
    return effectBankLoaded ? juce::String (static_cast<int> (effectBank.size ())) + " effect assignments from JSON" : juce::String ();
}

std::uint64_t GP200ModSync::getRevision ()
{
    const juce::ScopedLock lock (syncLock);
    return revision;
}

bool GP200ModSync::isCustomCloAmp (juce::uint32 effectId)
{
    const juce::ScopedLock lock (syncLock);
    const auto it = overrides.find (effectId);
    return it != overrides.end () && it->second.customCloAmp;
}

bool GP200ModSync::isNamAmp (juce::uint32 effectId)
{
    const juce::ScopedLock lock (syncLock);
    const auto it = overrides.find (effectId);
    return it != overrides.end () && it->second.customNamAmp;
}

bool GP200ModSync::canWriteFactoryClo (int zeroBasedFactoryAmpIndex)
{
    const juce::ScopedLock lock (syncLock);
    if (metadataError.isNotEmpty () || zeroBasedFactoryAmpIndex < 0
        || zeroBasedFactoryAmpIndex >= static_cast<int> (modsync::ampIds.size ())) return false;
    const auto it = overrides.find (modsync::ampIds[static_cast<std::size_t> (zeroBasedFactoryAmpIndex)]);
    return it != overrides.end () && it->second.customCloAmp && ! it->second.customNamAmp;
}

juce::String GP200ModSync::getNamMetadataStatus ()
{
    const juce::ScopedLock lock (syncLock);
    if (metadataError.isNotEmpty ()) return metadataError;
    int count = 0;
    for (const auto& pair : overrides) if (pair.second.customNamAmp) ++count;
    return count == 0 ? juce::String ()
                     : juce::String (count) + " NAM2 from firmware JSON (not verified by MIDI)";
}

juce::String GP200ModSync::getDisplayName (juce::uint32 effectId)
{
    const juce::ScopedLock lock (syncLock);
    const auto it = overrides.find (effectId);
    return it != overrides.end () ? it->second.displayName : juce::String{};
}

juce::String GP200ModSync::getSourceFile (juce::uint32 effectId)
{
    const juce::ScopedLock lock (syncLock);
    const auto it = overrides.find (effectId);
    return it != overrides.end () ? it->second.sourceFile : juce::String{};
}

juce::String GP200ModSync::getDescription (juce::uint32 effectId)
{
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
    if (const auto entry = findEffectBankEntry (preEffectId, "PRE")) return entry->sourceEffectId;
    if (hasEffectBank ()) return 0u;
    const juce::ScopedLock lock (syncLock);
    const auto it = preBankOverrides.find (preEffectId);
    return it != preBankOverrides.end () ? it->second.sourceEffectId : 0u;
}

juce::String GP200ModSync::getPreBankDisplayName (juce::uint32 preEffectId)
{
    if (const auto entry = findEffectBankEntry (preEffectId, "PRE")) return entry->displayName;
    if (hasEffectBank ()) return {};
    const juce::ScopedLock lock (syncLock);
    const auto it = preBankOverrides.find (preEffectId);
    return it != preBankOverrides.end () ? it->second.displayName : juce::String{};
}

juce::String GP200ModSync::getPreBankSourceModule (juce::uint32 preEffectId)
{
    if (const auto entry = findEffectBankEntry (preEffectId, "PRE")) return entry->sourceModule;
    if (hasEffectBank ()) return {};
    const juce::ScopedLock lock (syncLock);
    const auto it = preBankOverrides.find (preEffectId);
    return it != preBankOverrides.end () ? it->second.sourceModule : juce::String{};
}

juce::String GP200ModSync::getPreBankDescription (juce::uint32 preEffectId)
{
    if (const auto entry = findEffectBankEntry (preEffectId, "PRE")) return entry->profile;
    if (hasEffectBank ()) return {};
    const juce::ScopedLock lock (syncLock);
    const auto it = preBankOverrides.find (preEffectId);
    if (it == preBankOverrides.end ()) return {};
    auto text = it->second.sourceModule + " effect relocated into PRE";
    if (it->second.description.isNotEmpty ()) text += " (" + it->second.description + ")";
    return text;
}

juce::uint32 GP200ModSync::getWahConfigSourceEffectId (juce::uint32 wahEffectId)
{
    if (const auto entry = findEffectBankEntry (wahEffectId, "WAH")) return entry->sourceEffectId;
    if (hasEffectBank ()) return 0u;
    const juce::ScopedLock lock (syncLock);
    const auto it = wahConfigOverrides.find (wahEffectId);
    return it != wahConfigOverrides.end () ? it->second.sourceEffectId : 0u;
}

juce::String GP200ModSync::getWahConfigDisplayName (juce::uint32 wahEffectId)
{
    if (const auto entry = findEffectBankEntry (wahEffectId, "WAH")) return entry->displayName;
    if (hasEffectBank ()) return {};
    const juce::ScopedLock lock (syncLock);
    const auto it = wahConfigOverrides.find (wahEffectId);
    return it != wahConfigOverrides.end () ? it->second.displayName : juce::String{};
}

juce::String GP200ModSync::getWahConfigSourceModule (juce::uint32 wahEffectId)
{
    if (const auto entry = findEffectBankEntry (wahEffectId, "WAH")) return entry->sourceModule;
    if (hasEffectBank ()) return {};
    const juce::ScopedLock lock (syncLock);
    const auto it = wahConfigOverrides.find (wahEffectId);
    return it != wahConfigOverrides.end () ? it->second.sourceModule : juce::String{};
}

juce::String GP200ModSync::getWahConfigDescription (juce::uint32 wahEffectId)
{
    if (const auto entry = findEffectBankEntry (wahEffectId, "WAH")) return entry->profile;
    if (hasEffectBank ()) return {};
    const juce::ScopedLock lock (syncLock);
    const auto it = wahConfigOverrides.find (wahEffectId);
    if (it == wahConfigOverrides.end ()) return {};
    auto text = it->second.sourceModule + " effect relocated into WAH";
    if (it->second.description.isNotEmpty ()) text += " (" + it->second.description + ")";
    return text;
}

bool GP200ModSync::canUseNamInPlace (int index)
{
    const juce::ScopedLock lock (syncLock);
    if (index < 0 || index >= 71 || metadataError.isNotEmpty () || !inplaceEligible[static_cast<std::size_t> (index)]) return false;
    const auto it = overrides.find (modsync::ampIds[static_cast<std::size_t> (index)]);
    return it == overrides.end () || !it->second.customCloAmp;
}

bool GP200ModSync::recordNamAmpOverride (juce::uint32 effectId, const juce::String& displayName, const juce::String& sourceFile)
{
    reloadIfChanged ();
    const juce::ScopedLock lock (syncLock);
    if (!loadedOnce) loadUnlocked (getManifestFile ());
    const auto existing = overrides.find (effectId);
    const auto pos = std::find (modsync::ampIds.begin (), modsync::ampIds.end (), effectId);
    if (metadataError.isNotEmpty () || pos == modsync::ampIds.end ()
        || (existing != overrides.end () && existing->second.customCloAmp)
        || ((existing == overrides.end () || !existing->second.customNamAmp)
            && !inplaceEligible[static_cast<std::size_t> (pos-modsync::ampIds.begin ())])) return false;
    if (displayName.isEmpty () || displayName.length () > 12) return false;
    for (auto c : displayName) if (c < 32 || c > 126) return false;
    auto& entry = overrides[effectId]; entry.customNamAmp = true;
    entry.displayName = displayName;
    entry.sourceFile = sourceFile;
    return saveUnlocked ();
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
