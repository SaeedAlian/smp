#pragma once
#include "db.hpp"
#include <forward_list>
#include <vector>

namespace LibRetCode {

enum class ScanRes {
  Success = 0,
  CannotGetDirs,
  CannotGetDir,
  SqlError,
  GettingUnreadFilesError,
  AddingUnreadFilesError,
  UpdatingFilesError
};
enum class ReadFileTagsRes { Success = 0, CannotReadTags };
enum class InitArtistsRes { Success = 0, SqlError };
enum class SetArtistAlbumsRes { Success = 0, SqlError };

}; // namespace LibRetCode

class Library {
public:
  Library(DB *db__);
  ~Library();

  bool is_initialized();

  LibRetCode::ScanRes full_scan();
  LibRetCode::ScanRes partial_scan(int dir_id);

  LibRetCode::InitArtistsRes init_artists();
  LibRetCode::SetArtistAlbumsRes set_artist_albums(Entity::Artist &artist);
  LibRetCode::SetArtistAlbumsRes set_artist_albums(int index);

  std::vector<Entity::Artist> *get_artists();

  DBGetOpt::SortArtists get_artists_sortby_opt();
  DBGetOpt::SortAlbums get_albums_sortby_opt();
  bool is_using_albumartist();

private:
  DB *db = nullptr;
  std::vector<Entity::Artist> artists;
  DBGetOpt::SortArtists artists_sortby = DBGetOpt::SortArtists::NameAsc;
  DBGetOpt::SortAlbums albums_sortby = DBGetOpt::SortAlbums::YearAscAndNameAsc;
  bool use_albumartist = true;

  LibRetCode::ReadFileTagsRes read_file_tags(std::filesystem::path fullpath,
                                             Entity::File &result);

  LibRetCode::ScanRes scan_dir_changed_files(
      Entity::Directory dir,
      const std::map<std::filesystem::path, Entity::FileMainProps> &saved_files,
      std::forward_list<Entity::UnreadFile> &unread_files,
      int &unread_file_count,
      std::forward_list<Entity::File> &update_needed_files,
      int &update_needed_file_count);
  LibRetCode::ScanRes populate_files_into_db(
      const std::forward_list<Entity::UnreadFile> &unread_files,
      int unread_file_count,
      const std::forward_list<Entity::File> &update_needed_files,
      int update_needed_file_count);
};
