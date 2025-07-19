#include "library.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include "db.hpp"
#include <filesystem>
#include <forward_list>
#include <iostream>
#include <string>
#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>

Library::Library(DB *db__) {
  if (db__->is_initialized()) {
    db = db__;
  } else {
    db = nullptr;
  }
}

Library::~Library() {}

bool Library::is_initialized() { return db != nullptr; }

LibRetCode::ScanRes Library::full_scan() {
  std::vector<Entity::Directory> directories;
  if (db->get_directories_list(directories) != DBRetCode::GetDirRes::Success) {
    return LibRetCode::ScanRes::CannotGetDirs;
  }

  std::forward_list<Entity::UnreadFile> unread_files;
  int unread_file_count = 0;

  std::forward_list<Entity::File> update_needed_files;
  int update_needed_file_count = 0;

  std::map<std::filesystem::path, Entity::FileMainProps> saved_files;

  for (const auto &dir : directories) {
    if (db->get_dir_files_main_props(dir.id, saved_files) !=
        DBRetCode::GetFileRes::Success) {
      return LibRetCode::ScanRes::SqlError;
    }

    if (scan_dir_changed_files(dir, saved_files, unread_files,
                               unread_file_count, update_needed_files,
                               update_needed_file_count) !=
        LibRetCode::ScanRes::Success) {
      return LibRetCode::ScanRes::GettingUnreadFilesError;
    }

    for (const auto &[k, f] : saved_files) {
      std::filesystem::path fullpath =
          db->get_file_fullpath(f.fulldir_path, f.filename);
      if (!std::filesystem::exists(fullpath)) {
        if (db->remove_file(f.id) != DBRetCode::RmvFileRes::Success) {
          return LibRetCode::ScanRes::SqlError;
        }
      }
    }
  }

  std::cout << unread_file_count << " unread file found" << '\n';
  std::cout << update_needed_file_count << " update needed file found" << '\n';

  if (populate_files_into_db(unread_files, unread_file_count,
                             update_needed_files, update_needed_file_count) !=
      LibRetCode::ScanRes::Success) {
    return LibRetCode::ScanRes::AddingUnreadFilesError;
  }

  return LibRetCode::ScanRes::Success;
}

LibRetCode::ScanRes Library::partial_scan(int dir_id) {
  Entity::Directory dir;
  if (db->get_directory(dir_id, dir) != DBRetCode::GetDirRes::Success) {
    return LibRetCode::ScanRes::CannotGetDir;
  }

  std::forward_list<Entity::UnreadFile> unread_files;
  int unread_file_count = 0;

  std::forward_list<Entity::File> update_needed_files;
  int update_needed_file_count = 0;

  std::map<std::filesystem::path, Entity::FileMainProps> saved_files;

  if (db->get_dir_files_main_props(dir.id, saved_files) !=
      DBRetCode::GetFileRes::Success) {
    return LibRetCode::ScanRes::SqlError;
  }

  if (scan_dir_changed_files(dir, saved_files, unread_files, unread_file_count,
                             update_needed_files, update_needed_file_count) !=
      LibRetCode::ScanRes::Success) {
    return LibRetCode::ScanRes::GettingUnreadFilesError;
  }

  for (const auto &[k, f] : saved_files) {
    std::filesystem::path fullpath =
        db->get_file_fullpath(f.fulldir_path, f.filename);
    if (!std::filesystem::exists(fullpath)) {
      if (db->remove_file(f.id) != DBRetCode::RmvFileRes::Success) {
        return LibRetCode::ScanRes::SqlError;
      }
    }
  }

  std::cout << unread_file_count << " unread file found" << '\n';
  std::cout << update_needed_file_count << " update needed file found" << '\n';

  if (populate_files_into_db(unread_files, unread_file_count,
                             update_needed_files, update_needed_file_count) !=
      LibRetCode::ScanRes::Success) {
    return LibRetCode::ScanRes::AddingUnreadFilesError;
  }

  return LibRetCode::ScanRes::Success;
}

LibRetCode::InitArtistsRes Library::init_artists() {
  DBGetOpt::ArtistsOptions opts;
  opts.sortby = artists_sortby;
  opts.use_albumartist = use_albumartist;
  if (db->get_distinct_artists(artists, opts) !=
      DBRetCode::GetDistinctArtistsRes::Success) {
    return LibRetCode::InitArtistsRes::SqlError;
  }

  return LibRetCode::InitArtistsRes::Success;
}

LibRetCode::SetArtistAlbumsRes Library::set_artist_albums(int index) {
  Entity::Artist *artist = &artists[index];
  return set_artist_albums(*artist);
}

LibRetCode::SetArtistAlbumsRes
Library::set_artist_albums(Entity::Artist &artist) {
  DBGetOpt::AlbumsOptions opts;
  opts.sortby = albums_sortby;
  opts.use_albumartist = use_albumartist;

  if (db->get_artist_albums(artist, opts) !=
      DBRetCode::GetArtistAlbumsRes::Success) {
    return LibRetCode::SetArtistAlbumsRes::SqlError;
  }

  DBGetOpt::TrackOptions track_opts;
  track_opts.use_albumartist = use_albumartist;

  for (Entity::Album &a : artist.albums) {
    if (db->get_album_tracks(artist, a, track_opts) !=
        DBRetCode::GetAlbumTracksRes::Success) {
      return LibRetCode::SetArtistAlbumsRes::SqlError;
    }
  }

  return LibRetCode::SetArtistAlbumsRes::Success;
}

std::vector<Entity::Artist> *Library::get_artists() { return &artists; }

DBGetOpt::SortArtists Library::get_artists_sortby_opt() {
  return artists_sortby;
}

DBGetOpt::SortAlbums Library::get_albums_sortby_opt() { return albums_sortby; }

bool Library::is_using_albumartist() { return use_albumartist; }

LibRetCode::ScanRes Library::scan_dir_changed_files(
    Entity::Directory dir,
    const std::map<std::filesystem::path, Entity::FileMainProps> &saved_files,
    std::forward_list<Entity::UnreadFile> &unread_files, int &unread_file_count,
    std::forward_list<Entity::File> &update_needed_files,
    int &update_needed_file_count) {

  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(dir.path)) {
    Enum::FileType filetype = db->get_filetype(entry);
    if (filetype == Enum::FileType::UNKNOWN) {
      continue;
    }

    std::filesystem::path fullpath = entry.path();
    std::filesystem::path basename = fullpath.filename();
    std::filesystem::path fulldir_path = fullpath.parent_path();

    uintmax_t filesize = std::filesystem::file_size(fullpath);

    std::int64_t cftime = get_file_mtime_epoch(fullpath);

    try {
      Entity::FileMainProps existed_file = saved_files.at(fullpath);
      if (existed_file.modified_time == cftime &&
          existed_file.filesize == filesize) {
        continue;
      }

      Entity::File update_needed_file;
      update_needed_file.id = existed_file.id;
      update_needed_file.dir_id = existed_file.dir_id;
      update_needed_file.filename = existed_file.filename;
      update_needed_file.fulldir_path = existed_file.fulldir_path;
      update_needed_file.created_time = existed_file.created_time;
      update_needed_file.filetype = existed_file.filetype;
      update_needed_file.filesize = filesize;
      update_needed_file.modified_time = cftime;

      update_needed_file_count++;
      update_needed_files.push_front(update_needed_file);
    } catch (std::out_of_range) {
      Entity::UnreadFile unread_file;

      unread_file.fullpath = fullpath;
      unread_file.filename = basename;
      unread_file.dir_id = dir.id;
      unread_file.fulldir_path = fulldir_path;
      unread_file.created_time = cftime;
      unread_file.modified_time = cftime;
      unread_file.filesize = filesize;
      unread_file.filetype = filetype;

      unread_file_count++;
      unread_files.push_front(unread_file);
    }
  }

  return LibRetCode::ScanRes::Success;
}

LibRetCode::ScanRes Library::populate_files_into_db(
    const std::forward_list<Entity::UnreadFile> &unread_files,
    int unread_file_count,
    const std::forward_list<Entity::File> &update_needed_files,
    int update_needed_file_count) {
  int added_count = 0;
  int updated_count = 0;

  for (const Entity::UnreadFile &file : unread_files) {
    Entity::File newfile;
    if (read_file_tags(file.fullpath, newfile) !=
        LibRetCode::ReadFileTagsRes::Success) {
      std::cerr << "Could not read metadata of " << file.fullpath << '\n';
      continue;
    }

    newfile.dir_id = file.dir_id;
    newfile.filename = file.filename;
    newfile.fulldir_path = file.fulldir_path;
    newfile.created_time = file.created_time;
    newfile.modified_time = file.modified_time;
    newfile.filesize = file.filesize;
    newfile.filetype = file.filetype;

    int result_id;
    DBRetCode::AddFileRes rc = db->add_file(newfile, result_id);
    if (rc == DBRetCode::AddFileRes::FileAlreadyExists) {
      continue;
    }

    if (rc != DBRetCode::AddFileRes::Success) {
      return LibRetCode::ScanRes::AddingUnreadFilesError;
    }

    added_count++;

    std::cout << "Added (" << added_count << " / " << unread_file_count
              << ") files..." << '\n';
  }

  for (const Entity::File &file : update_needed_files) {
    Entity::File newfile;

    std::filesystem::path fullpath = db->get_file_fullpath(file);

    if (read_file_tags(fullpath, newfile) !=
        LibRetCode::ReadFileTagsRes::Success) {
      std::cerr << "Could not read metadata of " << fullpath << '\n';
      continue;
    }

    newfile.id = file.id;
    newfile.dir_id = file.dir_id;
    newfile.filename = file.filename;
    newfile.fulldir_path = file.fulldir_path;
    newfile.created_time = file.created_time;
    newfile.modified_time = file.modified_time;
    newfile.filesize = file.filesize;
    newfile.filetype = file.filetype;

    DBRetCode::UpdateFileRes rc = db->update_file(file.id, newfile);
    if (rc == DBRetCode::UpdateFileRes::NotFound) {
      continue;
    }

    if (rc != DBRetCode::UpdateFileRes::Success) {
      return LibRetCode::ScanRes::UpdatingFilesError;
    }

    updated_count++;

    std::cout << "Updated (" << updated_count << " / "
              << update_needed_file_count << ") files..." << '\n';
  }

  return LibRetCode::ScanRes::Success;
}

LibRetCode::ReadFileTagsRes
Library::read_file_tags(std::filesystem::path fullpath, Entity::File &result) {
  TagLib::FileRef f(fullpath.c_str());
  if (f.isNull() || !f.tag()) {
    return LibRetCode::ReadFileTagsRes::CannotReadTags;
  }

  TagLib::Tag *tag = f.tag();
  TagLib::PropertyMap props = f.properties();

  TagLib::StringList albumartist_list = props["ALBUMARTIST"];
  if (albumartist_list.size() == 0) {
    albumartist_list = props["ALBUM ARTIST"];
  }

  std::string albumartist = "";
  int disc_number = 0;

  if (albumartist_list.size() > 0) {
    albumartist = albumartist_list[0].to8Bit(true);
  }

  TagLib::StringList disc_number_list = props["DISCNUMBER"];
  if (disc_number_list.size() == 0) {
    disc_number_list = props["DISC NUMBER"];
  }

  if (disc_number_list.size() > 0) {
    std::string val = disc_number_list[0].to8Bit(true);
    try {
      disc_number = std::stoi(val);
    } catch (std::invalid_argument) {
      disc_number = 0;
    }
  }

  TagLib::StringList length_list = props["LENGTH"];

  TagLib::AudioProperties *audio_props = f.audioProperties();
  int bitrate = audio_props->bitrate();
  int length = audio_props->lengthInSeconds();

  std::string title = tag->title().to8Bit(true);
  std::string artist = tag->artist().to8Bit(true);
  std::string album = tag->album().to8Bit(true);
  std::string genre = tag->genre().to8Bit(true);
  int track_number = tag->track();
  int year = tag->year();

  result.title = title;
  result.artist = artist;
  result.album = album;
  result.albumartist = albumartist;
  result.track_number = track_number;
  result.disc_number = disc_number;
  result.year = year;
  result.genre = genre;
  result.length = length;
  result.bitrate = bitrate;

  return LibRetCode::ReadFileTagsRes::Success;
}
