// Prototypes for wide character equivalents of various standard unix functions.
#ifndef FISH_WUTIL_H
#define FISH_WUTIL_H

#include "config.h"  // IWYU pragma: keep

#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <ctime>
#include <functional>
#include <limits>
#include <locale>
#include <memory>
#include <string>

#include "common.h"
#include "maybe.h"

class autoclose_fd_t;

/// Wide character version of opendir(). Note that opendir() is guaranteed to set close-on-exec by
/// POSIX (hooray).
DIR *wopendir(const wcstring &name);

/// Wide character version of stat().
int wstat(const wcstring &file_name, struct stat *buf);

/// Wide character version of lstat().
int lwstat(const wcstring &file_name, struct stat *buf);

/// Wide character version of access().
int waccess(const wcstring &file_name, int mode);

/// Wide character version of unlink().
int wunlink(const wcstring &file_name);

/// Wide character version of perror().
void wperror(const wchar_t *s);

/// Wide character version of getcwd().
wcstring wgetcwd();

/// Wide character version of readlink().
maybe_t<wcstring> wreadlink(const wcstring &file_name);

/// Wide character version of realpath function.
/// \returns the canonicalized path, or none if the path is invalid.
maybe_t<wcstring> wrealpath(const wcstring &pathname);

/// Given an input path, "normalize" it:
/// 1. Collapse multiple /s into a single /, except maybe at the beginning.
/// 2. .. goes up a level.
/// 3. Remove /./ in the middle.
wcstring normalize_path(const wcstring &path, bool allow_leading_double_slashes = true);

/// Given an input path \p path and a working directory \p wd, do a "normalizing join" in a way
/// appropriate for cd. That is, return effectively wd + path while resolving leading ../s from
/// path. The intent here is to allow 'cd' out of a directory which may no longer exist, without
/// allowing 'cd' into a directory that may not exist; see #5341.
wcstring path_normalize_for_cd(const wcstring &wd, const wcstring &path);

/// Wide character version of readdir().
bool wreaddir(DIR *dir, wcstring &out_name);
bool wreaddir_resolving(DIR *dir, const std::wstring &dir_path, wcstring &out_name,
                        bool *out_is_dir);

/// Like wreaddir, but skip items that are known to not be directories. If this requires a stat
/// (i.e. the file is a symlink), then return it. Note that this does not guarantee that everything
/// returned is a directory, it's just an optimization for cases where we would check for
/// directories anyways.
bool readdir_for_dirs(DIR *dir, std::string *out_name);

/// Wide character version of dirname().
std::wstring wdirname(std::wstring path);

/// Wide character version of basename().
std::wstring wbasename(std::wstring path);

/// Wide character wrapper around the gettext function. For historic reasons, unlike the real
/// gettext function, wgettext takes care of setting the correct domain, etc. using the textdomain
/// and bindtextdomain functions. This should probably be moved out of wgettext, so that wgettext
/// will be nothing more than a wrapper around gettext, like all other functions in this file.
const wcstring &wgettext(const wchar_t *in);

/// Wide character version of mkdir.
int wmkdir(const wcstring &name, int mode);

/// Wide character version of rename.
int wrename(const wcstring &oldName, const wcstring &newv);

/// Write a wide string to a file descriptor. This avoids doing any additional allocation.
/// This does NOT retry on EINTR or EAGAIN, it simply returns.
/// \return -1 on error in which case errno will have been set. In this event, the number of bytes
/// actually written cannot be obtained.
ssize_t wwrite_to_fd(const wchar_t *input, size_t len, int fd);

/// Variant of above that accepts a wcstring.
inline ssize_t wwrite_to_fd(const wcstring &s, int fd) {
    return wwrite_to_fd(s.c_str(), s.size(), fd);
}

#define PUA1_START 0xE000
#define PUA1_END 0xF900
#define PUA2_START 0xF0000
#define PUA2_END 0xFFFFE
#define PUA3_START 0x100000
#define PUA3_END 0x10FFFE

// We need this because there are too many implementations that don't return the proper answer for
// some code points. See issue #3050.
#ifndef FISH_NO_ISW_WRAPPERS
#define iswalnum fish_iswalnum
#define iswgraph fish_iswgraph
#endif
int fish_iswalnum(wint_t wc);
int fish_iswgraph(wint_t wc);

int fish_wcswidth(const wchar_t *str);
int fish_wcswidth(const wcstring &str);

// returns an immortal locale_t corresponding to the C locale.
locale_t fish_c_locale();

void fish_invalidate_numeric_locale();
locale_t fish_numeric_locale();

int fish_wcstoi(const wchar_t *str, const wchar_t **endptr = nullptr, int base = 10);
long fish_wcstol(const wchar_t *str, const wchar_t **endptr = nullptr, int base = 10);
long long fish_wcstoll(const wchar_t *str, const wchar_t **endptr = nullptr, int base = 10);
unsigned long long fish_wcstoull(const wchar_t *str, const wchar_t **endptr = nullptr,
                                 int base = 10);
double fish_wcstod(const wchar_t *str, wchar_t **endptr, size_t len);
double fish_wcstod(const wchar_t *str, wchar_t **endptr);
double fish_wcstod(const wcstring &str, wchar_t **endptr);
double fish_wcstod_underscores(const wchar_t *str, wchar_t **endptr);

/// Class for representing a file's inode. We use this to detect and avoid symlink loops, among
/// other things. While an inode / dev pair is sufficient to distinguish co-existing files, Linux
/// seems to aggressively re-use inodes, so it cannot determine if a file has been deleted (ABA
/// problem). Therefore we include richer information.
struct file_id_t {
    dev_t device{static_cast<dev_t>(-1LL)};
    ino_t inode{static_cast<ino_t>(-1LL)};
    uint64_t size{static_cast<uint64_t>(-1LL)};
    time_t change_seconds{std::numeric_limits<time_t>::min()};
    long change_nanoseconds{-1};
    time_t mod_seconds{std::numeric_limits<time_t>::min()};
    long mod_nanoseconds{-1};

    constexpr file_id_t() = default;

    bool operator==(const file_id_t &rhs) const;
    bool operator!=(const file_id_t &rhs) const;

    // Used to permit these as keys in std::map.
    bool operator<(const file_id_t &rhs) const;

    static file_id_t from_stat(const struct stat &buf);
    bool older_than(const file_id_t &rhs) const;
    wcstring dump() const;

   private:
    int compare_file_id(const file_id_t &rhs) const;
};

/// RAII wrapper for DIR*
struct dir_t {
    DIR *dir;
    bool valid() const;
    bool read(wcstring &name) const;
    dir_t(const wcstring &path);
    ~dir_t();
};

#ifndef HASH_FILE_ID
#define HASH_FILE_ID 1
namespace std {
template <>
struct hash<file_id_t> {
    size_t operator()(const file_id_t &f) const {
        std::hash<decltype(f.device)> hasher1;
        std::hash<decltype(f.inode)> hasher2;

        return hasher1(f.device) ^ hasher2(f.inode);
    }
};
}  // namespace std
#endif

file_id_t file_id_for_fd(int fd);
file_id_t file_id_for_fd(const autoclose_fd_t &fd);
file_id_t file_id_for_path(const wcstring &path);
file_id_t file_id_for_path(const std::string &path);

extern const file_id_t kInvalidFileID;

#endif
