#pragma once
#include <map>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace LibRetCode {

enum class AddDirRes { Success = 0, PathAlreadyExists, SqlError };
enum class GetDirRes { Success = 0, SqlError, NotFound };
enum class RmvDirRes { Success = 0, SqlError };
enum class ScanRes { Success = 0 };
enum class UpdateFromDBRes { Success = 0 };
enum class AddFileRes {
  Success = 0,
  FileAlreadyExists,
  SqlError,
};
enum class GetFileRes { Success = 0, SqlError, NotFound };
enum class RmvFileRes { Success = 0, SqlError };
enum class SetupTablesRes { Success = 0, SqlError };

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
};

struct Directory {
  int id;
  std::string path;

  Directory() = default;
  Directory(int id__, std::string path__) : id(id__), path(path__) {}
};

struct File {
  int id;
  int dir_id;
  std::optional<std::string> subdir_path;
  std::string filename;
  std::optional<std::string> title;
  std::optional<std::string> album;
  std::optional<std::string> artist;
  std::optional<std::string> albumartist;
  std::optional<int> track_number;
  std::optional<int> disc_number;
  std::optional<int> year;
  std::optional<std::string> genre;
  int length;
  int bitrate;
  int filesize;
  FileType filetype;
};

struct Track {
  int file_id;
  std::string fullpath;
  std::string filename;
  std::string title;
  int track_number;
  int disc_number;
  int length;
  int bitrate;
  int filesize;
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

}; // namespace LibEntity

class Library {
public:
  Library(const std::string &db_name);
  ~Library();

  bool is_initialized();

  LibRetCode::AddDirRes add_directory(const std::string &path, int &result_id);
  LibRetCode::GetDirRes
  get_directories_map(std::map<int, LibEntity::Directory> &result);
  LibRetCode::GetDirRes
  get_directories_list(std::vector<LibEntity::Directory> &result);
  LibRetCode::GetDirRes get_directory(int id, LibEntity::Directory &result);
  LibRetCode::RmvDirRes remove_directory(int id);

  LibRetCode::ScanRes full_scan();
  LibRetCode::ScanRes partial_scan(int dir_id);

  LibRetCode::UpdateFromDBRes populate_from_db();

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
  LibRetCode::RmvFileRes remove_file(int id);
};
