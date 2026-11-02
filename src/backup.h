// Backup & restore of the registry values we touch.
#pragma once
#include <string>
#include <vector>

#include "sys.h"

namespace wpg {

// Two layers of safety:
//  * "originals"  - the value of every setting the first time we change it.
//  * "snapshots"  - full, timestamped captures the user can roll back to.
class BackupStore {
public:
    void init();
    bool remember(const RegAddress& addr);  // record first-seen original, no-op if known
    bool hasOriginals() const { return !originals_.empty(); }
    size_t originalCount() const { return originals_.size(); }
    bool restoreOriginals(std::wstring* error);
    bool createSnapshot(std::wstring* error);
    bool restoreLatestSnapshot(std::wstring* error);
    int snapshotCount() const { return (int)snapshots_.size(); }
    std::wstring latestSnapshotLabel() const;
    std::wstring directory() const { return backupsDir(); }

private:
    std::vector<ValueSnapshot> originals_;
    std::vector<std::wstring> snapshots_;

    static std::vector<ValueSnapshot> readValuesFile(const std::wstring& path);
    static bool writeValuesFile(const std::wstring& path, const std::vector<ValueSnapshot>& values);
    std::wstring originalsPath() const;
    void loadOriginals();
    bool saveOriginals();
    void refreshSnapshots();
};

std::wstring snapshotLabelFromPath(const std::wstring& path);

}  // namespace wpg
