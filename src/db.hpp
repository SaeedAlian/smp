#pragma once
#include "common/types.hpp"
#include <filesystem>
#include <map>
#include <sqlite3.h>
#include <string>

namespace DBRetCode {

enum class AddDirRes { Success = 0, PathAlreadyExists, SqlError };
enum class GetDirRes { Success = 0, SqlError, NotFound };
enum class RmvDirRes { Success = 0, SqlError };
enum class AddFileRes {
  Success = 0,
  FileAlreadyExists,
  SqlError,
};
enum class GetFileRes { Success = 0, SqlError, NotFound, CannotGetDir };
enum class UpdateFileRes { Success = 0, SqlError, NotFound };
enum class RmvFileRes { Success = 0, SqlError };
enum class SetupTablesRes { Success = 0, SqlError };
enum class GetDistinctArtistsRes { Success = 0, SqlError };
enum class GetArtistAlbumsRes { Success = 0, SqlError };
enum class GetAlbumTracksRes { Success = 0, SqlError };

}; // namespace DBRetCode

namespace DBGetOpt {

enum class SortArtists {
  NameAsc = 0,
  NameDesc,
};

enum class SortAlbums {
  NameAsc = 0,
  NameDesc,
  YearAscAndNameAsc,
  YearDescAndNameAsc,
  YearAscAndNameDesc,
  YearDescAndNameDesc,
};

struct ArtistsOptions {
  SortArtists sortby;
  bool use_albumartist;
};

struct AlbumsOptions {
  SortAlbums sortby;
  bool use_albumartist;
};

struct TrackOptions {
  bool use_albumartist;
};

}; // namespace DBGetOpt

class DB {
public:
  DB(const std::string &db_name);
  ~DB();

  bool is_initialized();

  DBRetCode::AddDirRes add_directory(const std::filesystem::path &path,
                                     int &result_id);
  DBRetCode::GetDirRes
  get_directories_map(std::map<int, Entity::Directory> &result);
  DBRetCode::GetDirRes
  get_directories_list(std::vector<Entity::Directory> &result);
  DBRetCode::GetDirRes get_directory(int id, Entity::Directory &result);
  DBRetCode::RmvDirRes remove_directory(int id);

  Enum::FileType get_filetype(const std::filesystem::path &path);
  std::filesystem::path get_file_fullpath(const Entity::File &file);
  std::filesystem::path
  get_file_fullpath(const std::filesystem::path fulldir_path,
                    const std::filesystem::path filename);

  DBRetCode::AddFileRes add_file(const Entity::File &file, int &result_id);
  DBRetCode::GetFileRes get_file(int id, Entity::File &result);
  DBRetCode::GetFileRes get_batch_files(const std::vector<int> &ids,
                                        std::vector<Entity::File> &result);
  DBRetCode::GetFileRes get_dir_files_list(int dir_id,
                                           std::vector<Entity::File> &result);
  DBRetCode::GetFileRes get_dir_files_map(int dir_id,
                                          std::map<int, Entity::File> &result);
  DBRetCode::GetFileRes get_dir_files_main_props(
      int dir_id,
      std::map<std::filesystem::path, Entity::FileMainProps> &result);
  DBRetCode::GetFileRes get_file_by_path(int dir_id,
                                         std::filesystem::path subdir_path,
                                         std::filesystem::path filename,
                                         Entity::File &result);
  DBRetCode::GetFileRes get_file_by_path(std::filesystem::path fulldir_path,
                                         std::filesystem::path filename,
                                         Entity::File &result);
  DBRetCode::UpdateFileRes update_file(int id,
                                       const Entity::File &updated_file);
  DBRetCode::RmvFileRes remove_file(int id);

  DBRetCode::GetDistinctArtistsRes
  get_distinct_artists(std::vector<Entity::Artist> &artists,
                       const DBGetOpt::ArtistsOptions &opts);
  DBRetCode::GetArtistAlbumsRes
  get_artist_albums(Entity::Artist &artist,
                    const DBGetOpt::AlbumsOptions &opts);
  DBRetCode::GetAlbumTracksRes
  get_album_tracks(const Entity::Artist &artist, Entity::Album &album,
                   const DBGetOpt::TrackOptions &opts);

private:
  sqlite3 *db;
  std::string db_name;

  DBRetCode::SetupTablesRes setup_tables();
};
