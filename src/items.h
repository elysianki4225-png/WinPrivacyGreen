// Catalogue of Windows advertising / privacy switches.
#pragma once
#include <string>
#include <vector>

#include "sys.h"

namespace wpg {

enum class ItemGroup { Ads = 0, Search, Privacy, Diagnostics, Count };

enum class IconKind {
    Megaphone,
    IdCard,
    Sparkle,
    Grid,
    Search,
    Lock,
    History,
    Doc,
    Keyboard,
    Location,
    Chart,
    Alert,
    Shield,
    Speech,
    Cloud,
    Gear,
};

struct ItemOp {
    RegAddress addr;
    bool isString = false;
    DWORD blockDword = 0;
    DWORD openDword = 1;
    const wchar_t* blockString = L"Deny";
    const wchar_t* openString = L"Allow";
};

struct ItemDef {
    std::string id;
    ItemGroup group;
    IconKind icon;
    const wchar_t* zhTitle;
    const wchar_t* enTitle;
    const wchar_t* zhDesc;
    const wchar_t* enDesc;
    bool needsAdmin;
    int minBuild;
    std::vector<ItemOp> ops;
};

const std::vector<ItemDef>& itemCatalog();
const wchar_t* groupTitle(ItemGroup g, bool chinese);

struct ItemState {
    bool blocked = false;
    bool available = true;
};

// Reads the current state from the registry.
ItemState readItemState(const ItemDef& def);

// Writes the registry values. "block" == privacy friendly / ads off.
// Returns false when Windows refused at least one write (usually rights).
bool applyItem(const ItemDef& def, bool block, bool* accessDenied);

std::vector<RegAddress> itemAddresses(const ItemDef& def);

}  // namespace wpg
