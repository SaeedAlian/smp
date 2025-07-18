#include "library.hpp"
#include "common/defines.hpp"
#include "common/utils.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fmt/format.h>
#include <forward_list>
#include <iostream>
#include <sqlite3.h>
#include <string>
#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>

Library::Library(const std::string &db_name) : db(nullptr) {
  if (sqlite3_open(db_name.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << "\n";
    db = nullptr;
  }

  if (db) {
    if (setup_tables() != LibRetCode::SetupTablesRes::Success) {
      std::cerr << "Could not create database tables: " << sqlite3_errmsg(db)
                << "\n";

      sqlite3_close(db);
      db = nullptr;
    }
  }
}

Library::~Library() {
  if (db)
    sqlite3_close(db);
}

bool Library::is_initialized() { return db != nullptr; }

LibRetCode::SetupTablesRes Library::setup_tables() {
  const std::array<std::string, 2> sqls{
      "CREATE TABLE IF NOT EXISTS directories ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "path TEXT UNIQUE"
      ");",

      "CREATE TABLE IF NOT EXISTS files ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "dir_id INTEGER NOT NULL,"
      "filename TEXT NOT NULL,"
      "fulldir_path TEXT NOT NULL,"
      "created_time INTEGER NOT NULL,"
      "modified_time INTEGER NOT NULL,"
      "title TEXT NOT NULL,"
      "album TEXT NOT NULL,"
      "artist TEXT NOT NULL,"
      "albumartist TEXT NOT NULL,"
      "track_number INTEGER NOT NULL,"
      "disc_number INTEGER NOT NULL,"
      "year INTEGER NOT NULL,"
      "genre TEXT NOT NULL,"
      "length INTEGER NOT NULL,"
      "bitrate INTEGER NOT NULL,"
      "filesize INTEGER NOT NULL,"
      "filetype INTEGER NOT NULL,"
      "FOREIGN KEY(dir_id) REFERENCES directories(id)"
      ");"};

  for (const std::string &sql : sqls) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
      PRINT_SQLITE_ERR(db);
      return LibRetCode::SetupTablesRes::SqlError;
    }

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      PRINT_SQLITE_ERR(db);
      sqlite3_finalize(stmt);
      return LibRetCode::SetupTablesRes::SqlError;
    }

    sqlite3_finalize(stmt);
  }

  return LibRetCode::SetupTablesRes::Success;
}

LibRetCode::AddDirRes Library::add_directory(const std::filesystem::path &path,
                                             int &result_id) {
  if (!db)
    return LibRetCode::AddDirRes::SqlError;

  const std::string check_sql =
      "SELECT COUNT(*) FROM directories WHERE path = ?;";
  sqlite3_stmt *check_stmt = nullptr;
  if (sqlite3_prepare_v2(db, check_sql.c_str(), -1, &check_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::AddDirRes::SqlError;
  }

  if (sqlite3_bind_text(check_stmt, 1, path.c_str(), -1, SQLITE_STATIC) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return LibRetCode::AddDirRes::SqlError;
  }

  int rc = sqlite3_step(check_stmt);
  if (rc != SQLITE_ROW) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return LibRetCode::AddDirRes::SqlError;
  }

  int count = sqlite3_column_int(check_stmt, 0);
  sqlite3_finalize(check_stmt);

  if (count > 0) {
    return LibRetCode::AddDirRes::PathAlreadyExists;
  }

  const std::string insert_sql = "INSERT INTO directories (path) VALUES (?);";
  sqlite3_stmt *insert_stmt = nullptr;
  if (sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &insert_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::AddDirRes::SqlError;
  }

  if (sqlite3_bind_text(insert_stmt, 1, path.c_str(), -1, SQLITE_STATIC) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(insert_stmt);
    return LibRetCode::AddDirRes::SqlError;
  }

  rc = sqlite3_step(insert_stmt);

  sqlite3_finalize(insert_stmt);

  if (rc != SQLITE_DONE) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::AddDirRes::SqlError;
  }

  result_id = static_cast<int>(sqlite3_last_insert_rowid(db));

  return LibRetCode::AddDirRes::Success;
}

LibRetCode::GetDirRes
Library::get_directories_map(std::map<int, LibEntity::Directory> &result) {
  if (!db)
    return LibRetCode::GetDirRes::SqlError;

  result.clear();

  const std::string q = "SELECT * FROM directories;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::GetDirRes::SqlError;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *path_text = sqlite3_column_text(stmt, 1);
    std::string path =
        path_text ? reinterpret_cast<const char *>(path_text) : "";

    result[id] = LibEntity::Directory{id, path};
  }

  sqlite3_finalize(stmt);

  return LibRetCode::GetDirRes::Success;
}

LibRetCode::GetDirRes
Library::get_directories_list(std::vector<LibEntity::Directory> &result) {
  if (!db)
    return LibRetCode::GetDirRes::SqlError;

  result.clear();

  const std::string count_q = "SELECT COUNT(*) FROM directories;";
  sqlite3_stmt *count_stmt = nullptr;
  if (sqlite3_prepare_v2(db, count_q.c_str(), -1, &count_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::GetDirRes::SqlError;
  }

  int rc = sqlite3_step(count_stmt);
  if (rc != SQLITE_ROW) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(count_stmt);
    return LibRetCode::GetDirRes::SqlError;
  }

  int count = sqlite3_column_int(count_stmt, 0);

  sqlite3_finalize(count_stmt);

  result.reserve(count);

  const std::string q = "SELECT * FROM directories;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::GetDirRes::SqlError;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *path_text = sqlite3_column_text(stmt, 1);
    std::string path =
        path_text ? reinterpret_cast<const char *>(path_text) : "";

    result.emplace_back(id, std::move(path));
  }

  sqlite3_finalize(stmt);

  return LibRetCode::GetDirRes::Success;
}

LibRetCode::GetDirRes Library::get_directory(int id,
                                             LibEntity::Directory &result) {
  if (!db)
    return LibRetCode::GetDirRes::SqlError;

  const std::string q = "SELECT * FROM directories WHERE id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::GetDirRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return LibRetCode::GetDirRes::SqlError;
  }

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);

    return LibRetCode::GetDirRes::NotFound;
  }

  result.id = sqlite3_column_int(stmt, 0);
  const unsigned char *path_text = sqlite3_column_text(stmt, 1);
  result.path = path_text ? reinterpret_cast<const char *>(path_text) : "";

  sqlite3_finalize(stmt);

  return LibRetCode::GetDirRes::Success;
}

LibRetCode::RmvDirRes Library::remove_directory(int id) {
  if (!db)
    return LibRetCode::RmvDirRes::SqlError;

  const std::string sql = "DELETE FROM directories WHERE id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::RmvDirRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return LibRetCode::RmvDirRes::SqlError;
  }

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::RmvDirRes::SqlError;
  }

  return LibRetCode::RmvDirRes::Success;
}

LibRetCode::ScanRes Library::scan_dir_changed_files(
    LibEntity::Directory dir,
    const std::map<std::filesystem::path, LibEntity::FileMainProps>
        &saved_files,
    std::forward_list<LibEntity::UnreadFile> &unread_files,
    int &unread_file_count,
    std::forward_list<LibEntity::File> &update_needed_files,
    int &update_needed_file_count) {

  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(dir.path)) {
    LibEntity::FileType filetype = get_filetype(entry);
    if (filetype == LibEntity::FileType::UNKNOWN) {
      continue;
    }

    std::filesystem::path fullpath = entry.path();
    std::filesystem::path basename = fullpath.filename();
    std::filesystem::path fulldir_path = fullpath.parent_path();
    std::filesystem::path subdir_path =
        std::filesystem::relative(fulldir_path, dir.path);

    uintmax_t filesize = std::filesystem::file_size(fullpath);

    std::int64_t cftime = get_file_mtime_epoch(fullpath);

    if (saved_files.find(fullpath) != saved_files.end()) {
      LibEntity::FileMainProps existed_file = saved_files.at(fullpath);
      if (existed_file.modified_time == cftime &&
          existed_file.filesize == filesize) {
        continue;
      }

      LibEntity::File update_needed_file;
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
      continue;
    }

    LibEntity::UnreadFile unread_file;

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

  return LibRetCode::ScanRes::Success;
}

LibRetCode::ScanRes Library::populate_files_into_db(
    const std::forward_list<LibEntity::UnreadFile> &unread_files,
    int unread_file_count,
    const std::forward_list<LibEntity::File> &update_needed_files,
    int update_needed_file_count) {
  int added_count = 0;
  int updated_count = 0;

  for (const LibEntity::UnreadFile &file : unread_files) {
    LibEntity::File newfile;
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
    LibRetCode::AddFileRes rc = add_file(newfile, result_id);
    if (rc == LibRetCode::AddFileRes::FileAlreadyExists) {
      continue;
    }

    if (rc != LibRetCode::AddFileRes::Success) {
      return LibRetCode::ScanRes::AddingUnreadFilesError;
    }

    added_count++;

    std::cout << "Added (" << added_count << " / " << unread_file_count
              << ") files..." << '\n';
  }

  for (const LibEntity::File &file : update_needed_files) {
    LibEntity::File newfile;

    std::filesystem::path fullpath = get_file_fullpath(file);

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

    LibRetCode::UpdateFileRes rc = update_file(file.id, newfile);
    if (rc == LibRetCode::UpdateFileRes::NotFound) {
      continue;
    }

    if (rc != LibRetCode::UpdateFileRes::Success) {
      return LibRetCode::ScanRes::UpdatingFilesError;
    }

    updated_count++;

    std::cout << "Updated (" << updated_count << " / "
              << update_needed_file_count << ") files..." << '\n';
  }

  return LibRetCode::ScanRes::Success;
}

LibRetCode::ScanRes Library::full_scan() {
  std::vector<LibEntity::Directory> directories;
  if (get_directories_list(directories) != LibRetCode::GetDirRes::Success) {
    return LibRetCode::ScanRes::CannotGetDirs;
  }

  std::forward_list<LibEntity::UnreadFile> unread_files;
  int unread_file_count = 0;

  std::forward_list<LibEntity::File> update_needed_files;
  int update_needed_file_count = 0;

  std::map<std::filesystem::path, LibEntity::FileMainProps> saved_files;

  for (const auto &dir : directories) {
    if (get_dir_files_main_props(dir.id, saved_files) !=
        LibRetCode::GetFileRes::Success) {
      return LibRetCode::ScanRes::SqlError;
    }

    if (scan_dir_changed_files(dir, saved_files, unread_files,
                               unread_file_count, update_needed_files,
                               update_needed_file_count) !=
        LibRetCode::ScanRes::Success) {
      return LibRetCode::ScanRes::GettingUnreadFilesError;
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
  return LibRetCode::ScanRes::Success;
}

LibRetCode::UpdateFromDBRes Library::update_from_db() {
  return LibRetCode::UpdateFromDBRes::Success;
}

LibRetCode::ReadFileTagsRes
Library::read_file_tags(std::filesystem::path fullpath,
                        LibEntity::File &result) {
  TagLib::FileRef f(fullpath.c_str());
  if (f.isNull() || !f.tag()) {
    return LibRetCode::ReadFileTagsRes::CannotReadTags;
  }

  TagLib::Tag *tag = f.tag();
  TagLib::PropertyMap props = f.properties();

  TagLib::StringList albumartist_list = props["ALBUMARTIST"];

  std::string albumartist = "";
  int disc_number = 0;

  if (albumartist_list.size() > 0) {
    albumartist = albumartist_list[0].to8Bit(true);
  }

  TagLib::StringList disc_number_list = props["DISCNUMBER"];
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

LibRetCode::AddFileRes Library::add_file(const LibEntity::File &file,
                                         int &result_id) {
  if (!db)
    return LibRetCode::AddFileRes::SqlError;

  const std::string check_sql = "SELECT COUNT(*) FROM files WHERE dir_id = ? "
                                "AND fulldir_path = ? AND filename = ?;";
  sqlite3_stmt *check_stmt = nullptr;
  if (sqlite3_prepare_v2(db, check_sql.c_str(), -1, &check_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::AddFileRes::SqlError;
  }

  if (sqlite3_bind_int(check_stmt, 1, file.dir_id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return LibRetCode::AddFileRes::SqlError;
  }

  if (sqlite3_bind_text(check_stmt, 2, file.fulldir_path.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return LibRetCode::AddFileRes::SqlError;
  }

  if (sqlite3_bind_text(check_stmt, 3, file.filename.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return LibRetCode::AddFileRes::SqlError;
  }

  int rc = sqlite3_step(check_stmt);
  if (rc != SQLITE_ROW) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return LibRetCode::AddFileRes::SqlError;
  }

  int count = sqlite3_column_int(check_stmt, 0);
  sqlite3_finalize(check_stmt);

  if (count > 0) {
    return LibRetCode::AddFileRes::FileAlreadyExists;
  }

  const std::string insert_sql =
      "INSERT INTO files ("
      "dir_id, fulldir_path, filename, title, album,"
      "artist, albumartist, track_number,"
      "disc_number, year, genre, length, bitrate,"
      "filesize, filetype, created_time, modified_time"
      ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
  sqlite3_stmt *insert_stmt = nullptr;
  if (sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &insert_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::AddFileRes::SqlError;
  }

  int idx = 1;

  if (sqlite3_bind_int(insert_stmt, idx++, file.dir_id) != SQLITE_OK ||
      sqlite3_bind_text(insert_stmt, idx++, file.fulldir_path.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(insert_stmt, idx++, file.filename.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(insert_stmt, idx++, file.title.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(insert_stmt, idx++, file.album.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(insert_stmt, idx++, file.artist.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(insert_stmt, idx++, file.albumartist.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, file.track_number) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, file.disc_number) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, file.year) != SQLITE_OK ||
      sqlite3_bind_text(insert_stmt, idx++, file.genre.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, file.length) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, file.bitrate) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, file.filesize) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, (int)file.filetype) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, file.created_time) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, file.modified_time) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(insert_stmt);
    return LibRetCode::AddFileRes::SqlError;
  }

  rc = sqlite3_step(insert_stmt);

  sqlite3_finalize(insert_stmt);

  if (rc != SQLITE_DONE) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::AddFileRes::SqlError;
  }

  result_id = static_cast<int>(sqlite3_last_insert_rowid(db));

  return LibRetCode::AddFileRes::Success;
}

LibRetCode::GetFileRes
Library::get_dir_files_list(int dir_id, std::vector<LibEntity::File> &result) {
  if (!db)
    return LibRetCode::GetFileRes::SqlError;

  result.clear();

  const std::string count_q = "SELECT COUNT(*) FROM files WHERE dir_id = ?;";
  sqlite3_stmt *count_stmt = nullptr;
  if (sqlite3_prepare_v2(db, count_q.c_str(), -1, &count_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_int(count_stmt, 1, dir_id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(count_stmt);
    return LibRetCode::GetFileRes::SqlError;
  }

  int rc = sqlite3_step(count_stmt);
  if (rc != SQLITE_ROW) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(count_stmt);
    return LibRetCode::GetFileRes::SqlError;
  }

  int count = sqlite3_column_int(count_stmt, 0);

  sqlite3_finalize(count_stmt);

  result.reserve(count);

  const std::string q = "SELECT * FROM files WHERE dir_id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, dir_id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return LibRetCode::GetFileRes::SqlError;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int idx = 0;

    int id = sqlite3_column_int(stmt, idx++);
    int dir_id = sqlite3_column_int(stmt, idx++);
    std::filesystem::path filename =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::filesystem::path fulldir_path =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::int64_t created_time = sqlite3_column_int(stmt, idx++);
    std::int64_t modified_time = sqlite3_column_int(stmt, idx++);
    std::string title =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::string album =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::string artist =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::string albumartist =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    int track_number = sqlite3_column_int(stmt, idx++);
    int disc_number = sqlite3_column_int(stmt, idx++);
    int year = sqlite3_column_int(stmt, idx++);
    std::string genre =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    int length = sqlite3_column_int(stmt, idx++);
    int bitrate = sqlite3_column_int(stmt, idx++);
    unsigned int filesize = sqlite3_column_int(stmt, idx++);
    LibEntity::FileType filetype =
        (LibEntity::FileType)sqlite3_column_int(stmt, idx++);

    result.emplace_back(id, dir_id, filename, fulldir_path, created_time,
                        modified_time, title, album, artist, albumartist,
                        track_number, disc_number, year, genre, length, bitrate,
                        filesize, filetype);
  }

  sqlite3_finalize(stmt);

  return LibRetCode::GetFileRes::Success;
}

LibRetCode::GetFileRes
Library::get_dir_files_map(int dir_id, std::map<int, LibEntity::File> &result) {
  if (!db)
    return LibRetCode::GetFileRes::SqlError;

  result.clear();

  const std::string q = "SELECT * FROM files WHERE dir_id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, dir_id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return LibRetCode::GetFileRes::SqlError;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int idx = 0;

    int id = sqlite3_column_int(stmt, idx++);
    int dir_id = sqlite3_column_int(stmt, idx++);
    std::filesystem::path filename =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::filesystem::path fulldir_path =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::int64_t created_time = sqlite3_column_int(stmt, idx++);
    std::int64_t modified_time = sqlite3_column_int(stmt, idx++);
    std::string title =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::string album =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::string artist =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::string albumartist =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    int track_number = sqlite3_column_int(stmt, idx++);
    int disc_number = sqlite3_column_int(stmt, idx++);
    int year = sqlite3_column_int(stmt, idx++);
    std::string genre =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    int length = sqlite3_column_int(stmt, idx++);
    int bitrate = sqlite3_column_int(stmt, idx++);
    unsigned int filesize = sqlite3_column_int(stmt, idx++);
    LibEntity::FileType filetype =
        (LibEntity::FileType)sqlite3_column_int(stmt, idx++);

    result[id] = LibEntity::File{
        id,    dir_id, filename, fulldir_path, created_time, modified_time,
        title, album,  artist,   albumartist,  track_number, disc_number,
        year,  genre,  length,   bitrate,      filesize,     filetype};
  }

  sqlite3_finalize(stmt);

  return LibRetCode::GetFileRes::Success;
}

LibRetCode::GetFileRes Library::get_dir_files_main_props(
    int dir_id,
    std::map<std::filesystem::path, LibEntity::FileMainProps> &result) {
  if (!db)
    return LibRetCode::GetFileRes::SqlError;

  result.clear();

  const std::string q = "SELECT "
                        "id, dir_id, filename, fulldir_path, created_time,"
                        "modified_time, filesize, filetype"
                        " FROM files WHERE dir_id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, dir_id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return LibRetCode::GetFileRes::SqlError;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int idx = 0;

    int id = sqlite3_column_int(stmt, idx++);
    int dir_id = sqlite3_column_int(stmt, idx++);
    std::filesystem::path filename =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::filesystem::path fulldir_path =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::int64_t created_time = sqlite3_column_int(stmt, idx++);
    std::int64_t modified_time = sqlite3_column_int(stmt, idx++);
    unsigned int filesize = sqlite3_column_int(stmt, idx++);
    LibEntity::FileType filetype =
        (LibEntity::FileType)sqlite3_column_int(stmt, idx++);

    std::filesystem::path fullpath = get_file_fullpath(fulldir_path, filename);

    result[fullpath] = LibEntity::FileMainProps{
        id,           dir_id,        filename, fulldir_path,
        created_time, modified_time, filesize, filetype};
  }

  sqlite3_finalize(stmt);

  return LibRetCode::GetFileRes::Success;
}

LibRetCode::GetFileRes Library::get_file(int id, LibEntity::File &result) {
  if (!db)
    return LibRetCode::GetFileRes::SqlError;

  const std::string q = "SELECT * FROM files WHERE id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return LibRetCode::GetFileRes::SqlError;
  }

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);

    return LibRetCode::GetFileRes::NotFound;
  }

  int idx = 0;

  result.id = sqlite3_column_int(stmt, idx++);
  result.dir_id = sqlite3_column_int(stmt, idx++);
  result.filename =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.fulldir_path =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.created_time = sqlite3_column_int(stmt, idx++);
  result.modified_time = sqlite3_column_int(stmt, idx++);
  result.title =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.album =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.artist =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.albumartist =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.track_number = sqlite3_column_int(stmt, idx++);
  result.disc_number = sqlite3_column_int(stmt, idx++);
  result.year = sqlite3_column_int(stmt, idx++);
  result.genre =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.length = sqlite3_column_int(stmt, idx++);
  result.bitrate = sqlite3_column_int(stmt, idx++);
  result.filesize = sqlite3_column_int(stmt, idx++);
  result.filetype = (LibEntity::FileType)sqlite3_column_int(stmt, idx++);

  sqlite3_finalize(stmt);

  return LibRetCode::GetFileRes::Success;
}

LibRetCode::GetFileRes
Library::get_file_by_path(std::filesystem::path fulldir_path,
                          std::filesystem::path filename,
                          LibEntity::File &result) {
  if (!db)
    return LibRetCode::GetFileRes::SqlError;

  const std::string q =
      "SELECT * FROM files WHERE fulldir_path = ? AND filename = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_text(stmt, 1, fulldir_path.c_str(), -1, SQLITE_STATIC) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return LibRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_text(stmt, 2, filename.c_str(), -1, SQLITE_STATIC) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return LibRetCode::GetFileRes::SqlError;
  }

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);

    return LibRetCode::GetFileRes::NotFound;
  }

  int idx = 0;

  result.id = sqlite3_column_int(stmt, idx++);
  result.dir_id = sqlite3_column_int(stmt, idx++);
  result.filename =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.fulldir_path =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.created_time = sqlite3_column_int(stmt, idx++);
  result.modified_time = sqlite3_column_int(stmt, idx++);
  result.title =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.album =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.artist =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.albumartist =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.track_number = sqlite3_column_int(stmt, idx++);
  result.disc_number = sqlite3_column_int(stmt, idx++);
  result.year = sqlite3_column_int(stmt, idx++);
  result.genre =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.length = sqlite3_column_int(stmt, idx++);
  result.bitrate = sqlite3_column_int(stmt, idx++);
  result.filesize = sqlite3_column_int(stmt, idx++);
  result.filetype = (LibEntity::FileType)sqlite3_column_int(stmt, idx++);

  sqlite3_finalize(stmt);

  return LibRetCode::GetFileRes::Success;
}

LibRetCode::GetFileRes
Library::get_file_by_path(int dir_id, std::filesystem::path subdir_path,
                          std::filesystem::path filename,
                          LibEntity::File &result) {
  if (!db)
    return LibRetCode::GetFileRes::SqlError;

  LibEntity::Directory dir;
  if (get_directory(dir_id, dir) != LibRetCode::GetDirRes::Success) {
    return LibRetCode::GetFileRes::CannotGetDir;
  }

  std::filesystem::path fulldir_path =
      fmt::format("{}/{}", dir.path.c_str(), subdir_path.c_str());

  return get_file_by_path(fulldir_path, filename, result);
}

LibRetCode::UpdateFileRes
Library::update_file(int id, const LibEntity::File &updated_file) {
  const std::string sql =
      "UPDATE files SET "
      "modified_time = ?, title = ?, album = ?, "
      "artist = ?, albumartist = ?, track_number = ?, disc_number = ?, "
      "year = ?, genre = ?, length = ?, bitrate = ?, filesize = ? "
      "WHERE id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::UpdateFileRes::SqlError;
  }

  int idx = 1;

  if (sqlite3_bind_int(stmt, idx++, updated_file.modified_time) != SQLITE_OK ||
      sqlite3_bind_text(stmt, idx++, updated_file.title.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(stmt, idx++, updated_file.album.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(stmt, idx++, updated_file.artist.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_text(stmt, idx++, updated_file.albumartist.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_int(stmt, idx++, updated_file.track_number) != SQLITE_OK ||
      sqlite3_bind_int(stmt, idx++, updated_file.disc_number) != SQLITE_OK ||
      sqlite3_bind_int(stmt, idx++, updated_file.year) != SQLITE_OK ||
      sqlite3_bind_text(stmt, idx++, updated_file.genre.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      sqlite3_bind_int(stmt, idx++, updated_file.length) != SQLITE_OK ||
      sqlite3_bind_int(stmt, idx++, updated_file.bitrate) != SQLITE_OK ||
      sqlite3_bind_int(stmt, idx++, updated_file.filesize) != SQLITE_OK ||
      sqlite3_bind_int(stmt, idx++, id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return LibRetCode::UpdateFileRes::SqlError;
  }

  int rc = sqlite3_step(stmt);

  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::UpdateFileRes::SqlError;
  }

  return LibRetCode::UpdateFileRes::Success;
}

LibRetCode::RmvFileRes Library::remove_file(int id) {
  if (!db)
    return LibRetCode::RmvFileRes::SqlError;

  const std::string sql = "DELETE FROM files WHERE id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::RmvFileRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return LibRetCode::RmvFileRes::SqlError;
  }

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::RmvFileRes::SqlError;
  }

  return LibRetCode::RmvFileRes::Success;
}

LibEntity::FileType Library::get_filetype(const std::filesystem::path &path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if (ext == ".mp3")
    return LibEntity::FileType::MP3;
  else if (ext == ".flac")
    return LibEntity::FileType::FLAC;
  else if (ext == ".ogg")
    return LibEntity::FileType::OGG;
  else
    return LibEntity::FileType::UNKNOWN;
}

std::ostream &operator<<(std::ostream &os, const LibEntity::File &f) {
  os << "ID: " << f.id << '\n';
  os << "Dir ID: " << f.dir_id << '\n';
  os << "Filename: " << f.filename << '\n';
  os << "FullDir Path: " << f.fulldir_path << '\n';
  os << "Created Time: " << f.created_time << '\n';
  os << "Modified Time: " << f.modified_time << '\n';
  os << "Title: " << f.title << '\n';
  os << "Artist: " << f.artist << '\n';
  os << "Album: " << f.album << '\n';
  os << "Track: " << f.track_number << '\n';
  os << "Disc: " << f.disc_number << '\n';
  os << "Year: " << f.year << '\n';
  os << "Genre: " << f.genre << '\n';
  os << "Length: " << f.length << '\n';
  os << "Bitrate: " << f.bitrate << '\n';
  os << "Size: " << f.filesize << '\n';
  os << "FileType: " << (int)f.filetype << '\n';
  return os;
}

std::filesystem::path get_file_fullpath(const LibEntity::File &file) {
  std::filesystem::path fullpath =
      fmt::format("{}/{}", file.fulldir_path.c_str(), file.filename.c_str());
  return fullpath;
}

std::filesystem::path
get_file_fullpath(const std::filesystem::path fulldir_path,
                  const std::filesystem::path filename) {
  std::filesystem::path fullpath =
      fmt::format("{}/{}", fulldir_path.c_str(), filename.c_str());
  return fullpath;
}
