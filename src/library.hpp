#pragma once
#include <filesystem>
#include <fmt/format.h>
#include <forward_list>
#include <map>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace LibRetCode {

enum class AddDirRes { Success = 0, PathAlreadyExists, SqlError };
enum class GetDirRes { Success = 0, SqlError, NotFound };
enum class RmvDirRes { Success = 0, SqlError };
enum class ScanRes {
  Success = 0,
  CannotGetDirs,
  SqlError,
  GettingUnreadFilesError,
  AddingUnreadFilesError,
  UpdatingFilesError
};
enum class UpdateFromDBRes { Success = 0 };
enum class AddFileRes {
  Success = 0,
  FileAlreadyExists,
  SqlError,
};
enum class GetFileRes { Success = 0, SqlError, NotFound, CannotGetDir };
enum class UpdateFileRes { Success = 0, SqlError, NotFound };
enum class RmvFileRes { Success = 0, SqlError };
enum class SetupTablesRes { Success = 0, SqlError };
enum class ReadFileTagsRes { Success = 0, CannotReadTags };

}; // namespace LibRetCode

namespace LibOption {

enum class SortArtistsBy {
  NameAsc = 0,
  NameDesc,
  NumberOfAlbumsAsc,
  NumberOfAlbumsDesc,
};

enum class SortAlbumsBy {
  NameAsc = 0,
  NameDesc,
  NumberOfTracksAsc,
  NumberOfTracksDesc,
  YearAscAndNameAsc,
  YearDescAndNameAsc,
  YearAscAndNameDesc,
  YearDescAndNameDesc,
};

}; // namespace LibOption

namespace LibEntity {

enum class FileType {
  MP3 = 1,
  FLAC,
  OGG,
  UNKNOWN,
};

struct Directory {
  int id;
  std::filesystem::path path;

  Directory() = default;
  Directory(int id__, std::filesystem::path path__) : id(id__), path(path__) {}
};

struct UnreadFile {
  std::filesystem::path fullpath;
  std::filesystem::path fulldir_path;
  std::filesystem::path filename;
  int dir_id;
  std::int64_t created_time;
  std::int64_t modified_time;
  unsigned int filesize;
  FileType filetype;
};

struct File {
  int id;
  int dir_id;
  std::filesystem::path filename;
  std::filesystem::path fulldir_path;
  std::int64_t created_time;
  std::int64_t modified_time;
  std::string title;
  std::string album;
  std::string artist;
  std::string albumartist;
  int track_number;
  int disc_number;
  int year;
  std::string genre;
  int length;
  int bitrate;
  unsigned int filesize;
  FileType filetype;

  File() = default;
  File(int id__, int dir_id__, std::filesystem::path filename__,
       std::filesystem::path fulldir_path__, std::int64_t created_time__,
       std::int64_t modified_time__, std::string title__, std::string album__,
       std::string artist__, std::string albumartist__, int track_number__,
       int disc_number__, int year__, std::string genre__, int length__,
       int bitrate__, unsigned int filesize__, FileType filetype__)
      : id(id__), dir_id(dir_id__), filename(filename__),
        fulldir_path(fulldir_path__), created_time(created_time__),
        modified_time(modified_time__), title(title__), album(album__),
        artist(artist__), albumartist(albumartist__),
        track_number(track_number__), disc_number(disc_number__), year(year__),
        genre(genre__), length(length__), bitrate(bitrate__),
        filesize(filesize__), filetype(filetype__) {}
};

struct FileMainProps {
  int id;
  int dir_id;
  std::filesystem::path filename;
  std::filesystem::path fulldir_path;
  std::int64_t created_time;
  std::int64_t modified_time;
  unsigned int filesize;
  FileType filetype;

  FileMainProps() = default;
  FileMainProps(int id__, int dir_id__, std::filesystem::path filename__,
                std::filesystem::path fulldir_path__,
                std::int64_t created_time__, std::int64_t modified_time__,
                unsigned int filesize__, FileType filetype__)
      : id(id__), dir_id(dir_id__), filename(filename__),
        fulldir_path(fulldir_path__), created_time(created_time__),
        modified_time(modified_time__), filesize(filesize__),
        filetype(filetype__) {}
};

struct Track {
  int file_id;
  int dir_id;
  std::filesystem::path filename;
  std::filesystem::path fulldir_path;
  std::string title;
  int track_number;
  int disc_number;
  int length;
  int bitrate;
  unsigned int filesize;
  FileType filetype;
};

struct Album {
  std::string title;
  std::string genre;
  int year;
  std::vector<Track> tracks;
};

struct Artist {
  std::string name;
  std::vector<Album> albums;
};

std::ostream &operator<<(std::ostream &os, const LibEntity::File &f);

}; // namespace LibEntity

class Library {
public:
  Library(const std::string &db_name);
  ~Library();

  bool is_initialized();

  LibRetCode::AddDirRes add_directory(const std::filesystem::path &path,
                                      int &result_id);
  LibRetCode::GetDirRes
  get_directories_map(std::map<int, LibEntity::Directory> &result);
  LibRetCode::GetDirRes
  get_directories_list(std::vector<LibEntity::Directory> &result);
  LibRetCode::GetDirRes get_directory(int id, LibEntity::Directory &result);
  LibRetCode::RmvDirRes remove_directory(int id);

  LibEntity::FileType get_filetype(const std::filesystem::path &path);

  LibRetCode::ScanRes full_scan();
  LibRetCode::ScanRes partial_scan(int dir_id);

  LibRetCode::UpdateFromDBRes update_from_db();

  LibRetCode::ReadFileTagsRes read_file_tags(std::filesystem::path fullpath,
                                             LibEntity::File &result);

private:
  sqlite3 *db;
  std::string db_name;
  std::vector<LibEntity::Artist> artists;
  bool use_albumartist = false;
  LibOption::SortArtistsBy artists_sort = LibOption::SortArtistsBy::NameAsc;
  LibOption::SortAlbumsBy albums_sort =
      LibOption::SortAlbumsBy::YearAscAndNameAsc;

  LibRetCode::SetupTablesRes setup_tables();

  LibRetCode::AddFileRes add_file(const LibEntity::File &file, int &result_id);
  LibRetCode::GetFileRes get_file(int id, LibEntity::File &result);
  LibRetCode::GetFileRes
  get_dir_files_list(int dir_id, std::vector<LibEntity::File> &result);
  LibRetCode::GetFileRes
  get_dir_files_map(int dir_id, std::map<int, LibEntity::File> &result);
  LibRetCode::GetFileRes get_dir_files_main_props(
      int dir_id,
      std::map<std::filesystem::path, LibEntity::FileMainProps> &result);
  LibRetCode::GetFileRes get_file_by_path(int dir_id,
                                          std::filesystem::path subdir_path,
                                          std::filesystem::path filename,
                                          LibEntity::File &result);
  LibRetCode::GetFileRes get_file_by_path(std::filesystem::path fulldir_path,
                                          std::filesystem::path filename,
                                          LibEntity::File &result);
  LibRetCode::UpdateFileRes update_file(int id,
                                        const LibEntity::File &updated_file);
  LibRetCode::RmvFileRes remove_file(int id);

  LibRetCode::ScanRes scan_dir_changed_files(
      LibEntity::Directory dir,
      const std::map<std::filesystem::path, LibEntity::FileMainProps>
          &saved_files,
      std::forward_list<LibEntity::UnreadFile> &unread_files,
      int &unread_file_count,
      std::forward_list<LibEntity::File> &update_needed_files,
      int &update_needed_file_count);
  LibRetCode::ScanRes populate_files_into_db(
      const std::forward_list<LibEntity::UnreadFile> &unread_files,
      int unread_file_count,
      const std::forward_list<LibEntity::File> &update_needed_files,
      int update_needed_file_count);
};
