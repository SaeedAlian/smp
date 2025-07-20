#include "db.hpp"
#include "common/defines.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <sqlite3.h>
#include <string>

DB::DB(const std::string &db_name) : db(nullptr) {
  if (sqlite3_open(db_name.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << "\n";
    db = nullptr;
  }

  if (db) {
    if (setup_tables() != DBRetCode::SetupTablesRes::Success) {
      std::cerr << "Could not create database tables: " << sqlite3_errmsg(db)
                << "\n";

      sqlite3_close(db);
      db = nullptr;
    }
  }
}

DB::~DB() {
  if (db)
    sqlite3_close(db);
}

bool DB::is_initialized() { return db != nullptr; }

DBRetCode::SetupTablesRes DB::setup_tables() {
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
      return DBRetCode::SetupTablesRes::SqlError;
    }

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      PRINT_SQLITE_ERR(db);
      sqlite3_finalize(stmt);
      return DBRetCode::SetupTablesRes::SqlError;
    }

    sqlite3_finalize(stmt);
  }

  return DBRetCode::SetupTablesRes::Success;
}

DBRetCode::AddDirRes DB::add_directory(const std::filesystem::path &path,
                                       int &result_id) {
  if (!db)
    return DBRetCode::AddDirRes::SqlError;

  const std::string check_sql =
      "SELECT COUNT(*) FROM directories WHERE path = ?;";
  sqlite3_stmt *check_stmt = nullptr;
  if (sqlite3_prepare_v2(db, check_sql.c_str(), -1, &check_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::AddDirRes::SqlError;
  }

  if (sqlite3_bind_text(check_stmt, 1, path.c_str(), -1, SQLITE_STATIC) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return DBRetCode::AddDirRes::SqlError;
  }

  int rc = sqlite3_step(check_stmt);
  if (rc != SQLITE_ROW) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return DBRetCode::AddDirRes::SqlError;
  }

  int count = sqlite3_column_int(check_stmt, 0);
  sqlite3_finalize(check_stmt);

  if (count > 0) {
    return DBRetCode::AddDirRes::PathAlreadyExists;
  }

  const std::string insert_sql = "INSERT INTO directories (path) VALUES (?);";
  sqlite3_stmt *insert_stmt = nullptr;
  if (sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &insert_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::AddDirRes::SqlError;
  }

  if (sqlite3_bind_text(insert_stmt, 1, path.c_str(), -1, SQLITE_STATIC) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(insert_stmt);
    return DBRetCode::AddDirRes::SqlError;
  }

  rc = sqlite3_step(insert_stmt);

  sqlite3_finalize(insert_stmt);

  if (rc != SQLITE_DONE) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::AddDirRes::SqlError;
  }

  result_id = static_cast<int>(sqlite3_last_insert_rowid(db));

  return DBRetCode::AddDirRes::Success;
}

DBRetCode::GetDirRes
DB::get_directories_map(std::map<int, Entity::Directory> &result) {
  if (!db)
    return DBRetCode::GetDirRes::SqlError;

  result.clear();

  const std::string q = "SELECT * FROM directories;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetDirRes::SqlError;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *path_text = sqlite3_column_text(stmt, 1);
    std::string path =
        path_text ? reinterpret_cast<const char *>(path_text) : "";

    result[id] = Entity::Directory{id, path};
  }

  sqlite3_finalize(stmt);

  return DBRetCode::GetDirRes::Success;
}

DBRetCode::GetDirRes
DB::get_directories_list(std::vector<Entity::Directory> &result) {
  if (!db)
    return DBRetCode::GetDirRes::SqlError;

  result.clear();

  const std::string count_q = "SELECT COUNT(*) FROM directories;";
  sqlite3_stmt *count_stmt = nullptr;
  if (sqlite3_prepare_v2(db, count_q.c_str(), -1, &count_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetDirRes::SqlError;
  }

  int rc = sqlite3_step(count_stmt);
  if (rc != SQLITE_ROW) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(count_stmt);
    return DBRetCode::GetDirRes::SqlError;
  }

  int count = sqlite3_column_int(count_stmt, 0);

  sqlite3_finalize(count_stmt);

  result.reserve(count);

  const std::string q = "SELECT * FROM directories;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetDirRes::SqlError;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const unsigned char *path_text = sqlite3_column_text(stmt, 1);
    std::string path =
        path_text ? reinterpret_cast<const char *>(path_text) : "";

    result.emplace_back(id, std::move(path));
  }

  sqlite3_finalize(stmt);

  return DBRetCode::GetDirRes::Success;
}

DBRetCode::GetDirRes DB::get_directory(int id, Entity::Directory &result) {
  if (!db)
    return DBRetCode::GetDirRes::SqlError;

  const std::string q = "SELECT * FROM directories WHERE id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetDirRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::GetDirRes::SqlError;
  }

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);

    return DBRetCode::GetDirRes::NotFound;
  }

  result.id = sqlite3_column_int(stmt, 0);
  const unsigned char *path_text = sqlite3_column_text(stmt, 1);
  result.path = path_text ? reinterpret_cast<const char *>(path_text) : "";

  sqlite3_finalize(stmt);

  return DBRetCode::GetDirRes::Success;
}

DBRetCode::RmvDirRes DB::remove_directory(int id) {
  if (!db)
    return DBRetCode::RmvDirRes::SqlError;

  const std::string sql = "DELETE FROM directories WHERE id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::RmvDirRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::RmvDirRes::SqlError;
  }

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::RmvDirRes::SqlError;
  }

  return DBRetCode::RmvDirRes::Success;
}

DBRetCode::AddFileRes DB::add_file(const Entity::File &file, int &result_id) {
  if (!db)
    return DBRetCode::AddFileRes::SqlError;

  const std::string check_sql = "SELECT COUNT(*) FROM files WHERE dir_id = ? "
                                "AND fulldir_path = ? AND filename = ?;";
  sqlite3_stmt *check_stmt = nullptr;
  if (sqlite3_prepare_v2(db, check_sql.c_str(), -1, &check_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::AddFileRes::SqlError;
  }

  if (sqlite3_bind_int(check_stmt, 1, file.dir_id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return DBRetCode::AddFileRes::SqlError;
  }

  if (sqlite3_bind_text(check_stmt, 2, file.fulldir_path.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return DBRetCode::AddFileRes::SqlError;
  }

  if (sqlite3_bind_text(check_stmt, 3, file.filename.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return DBRetCode::AddFileRes::SqlError;
  }

  int rc = sqlite3_step(check_stmt);
  if (rc != SQLITE_ROW) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(check_stmt);
    return DBRetCode::AddFileRes::SqlError;
  }

  int count = sqlite3_column_int(check_stmt, 0);
  sqlite3_finalize(check_stmt);

  if (count > 0) {
    return DBRetCode::AddFileRes::FileAlreadyExists;
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
    return DBRetCode::AddFileRes::SqlError;
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
    return DBRetCode::AddFileRes::SqlError;
  }

  rc = sqlite3_step(insert_stmt);

  sqlite3_finalize(insert_stmt);

  if (rc != SQLITE_DONE) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::AddFileRes::SqlError;
  }

  result_id = static_cast<int>(sqlite3_last_insert_rowid(db));

  return DBRetCode::AddFileRes::Success;
}

DBRetCode::GetFileRes
DB::get_dir_files_list(int dir_id, std::vector<Entity::File> &result) {
  if (!db)
    return DBRetCode::GetFileRes::SqlError;

  result.clear();

  const std::string count_q = "SELECT COUNT(*) FROM files WHERE dir_id = ?;";
  sqlite3_stmt *count_stmt = nullptr;
  if (sqlite3_prepare_v2(db, count_q.c_str(), -1, &count_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_int(count_stmt, 1, dir_id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(count_stmt);
    return DBRetCode::GetFileRes::SqlError;
  }

  int rc = sqlite3_step(count_stmt);
  if (rc != SQLITE_ROW) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(count_stmt);
    return DBRetCode::GetFileRes::SqlError;
  }

  int count = sqlite3_column_int(count_stmt, 0);

  sqlite3_finalize(count_stmt);

  result.reserve(count);

  const std::string q = "SELECT * FROM files WHERE dir_id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, dir_id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::GetFileRes::SqlError;
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
    Enum::FileType filetype = (Enum::FileType)sqlite3_column_int(stmt, idx++);

    result.emplace_back(id, dir_id, filename, fulldir_path, created_time,
                        modified_time, title, album, artist, albumartist,
                        track_number, disc_number, year, genre, length, bitrate,
                        filesize, filetype);
  }

  sqlite3_finalize(stmt);

  return DBRetCode::GetFileRes::Success;
}

DBRetCode::GetFileRes
DB::get_dir_files_map(int dir_id, std::map<int, Entity::File> &result) {
  if (!db)
    return DBRetCode::GetFileRes::SqlError;

  result.clear();

  const std::string q = "SELECT * FROM files WHERE dir_id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, dir_id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::GetFileRes::SqlError;
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
    Enum::FileType filetype = (Enum::FileType)sqlite3_column_int(stmt, idx++);

    result[id] = Entity::File{
        id,    dir_id, filename, fulldir_path, created_time, modified_time,
        title, album,  artist,   albumartist,  track_number, disc_number,
        year,  genre,  length,   bitrate,      filesize,     filetype};
  }

  sqlite3_finalize(stmt);

  return DBRetCode::GetFileRes::Success;
}

DBRetCode::GetFileRes DB::get_dir_files_main_props(
    int dir_id,
    std::map<std::filesystem::path, Entity::FileMainProps> &result) {
  if (!db)
    return DBRetCode::GetFileRes::SqlError;

  result.clear();

  const std::string q = "SELECT "
                        "id, dir_id, filename, fulldir_path, created_time,"
                        "modified_time, filesize, filetype"
                        " FROM files WHERE dir_id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, dir_id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::GetFileRes::SqlError;
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
    Enum::FileType filetype = (Enum::FileType)sqlite3_column_int(stmt, idx++);

    std::filesystem::path fullpath = get_file_fullpath(fulldir_path, filename);

    result[fullpath] = Entity::FileMainProps{
        id,           dir_id,        filename, fulldir_path,
        created_time, modified_time, filesize, filetype};
  }

  sqlite3_finalize(stmt);

  return DBRetCode::GetFileRes::Success;
}

DBRetCode::GetFileRes DB::get_file(int id, Entity::File &result) {
  if (!db)
    return DBRetCode::GetFileRes::SqlError;

  const std::string q = "SELECT * FROM files WHERE id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::GetFileRes::SqlError;
  }

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);

    return DBRetCode::GetFileRes::NotFound;
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
  result.filetype = (Enum::FileType)sqlite3_column_int(stmt, idx++);

  sqlite3_finalize(stmt);

  return DBRetCode::GetFileRes::Success;
}

DBRetCode::GetFileRes DB::get_file_by_path(std::filesystem::path fulldir_path,
                                           std::filesystem::path filename,
                                           Entity::File &result) {
  if (!db)
    return DBRetCode::GetFileRes::SqlError;

  const std::string q =
      "SELECT * FROM files WHERE fulldir_path = ? AND filename = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_text(stmt, 1, fulldir_path.c_str(), -1, SQLITE_STATIC) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::GetFileRes::SqlError;
  }

  if (sqlite3_bind_text(stmt, 2, filename.c_str(), -1, SQLITE_STATIC) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::GetFileRes::SqlError;
  }

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);

    return DBRetCode::GetFileRes::NotFound;
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
  result.filetype = (Enum::FileType)sqlite3_column_int(stmt, idx++);

  sqlite3_finalize(stmt);

  return DBRetCode::GetFileRes::Success;
}

DBRetCode::GetFileRes DB::get_file_by_path(int dir_id,
                                           std::filesystem::path subdir_path,
                                           std::filesystem::path filename,
                                           Entity::File &result) {
  if (!db)
    return DBRetCode::GetFileRes::SqlError;

  Entity::Directory dir;
  if (get_directory(dir_id, dir) != DBRetCode::GetDirRes::Success) {
    return DBRetCode::GetFileRes::CannotGetDir;
  }

  std::filesystem::path fulldir_path =
      fmt::format("{}/{}", dir.path.c_str(), subdir_path.c_str());

  return get_file_by_path(fulldir_path, filename, result);
}

DBRetCode::GetFileRes DB::get_batch_files(const std::vector<int> &ids,
                                          std::vector<Entity::File> &result) {
  if (!db)
    return DBRetCode::GetFileRes::SqlError;

  const unsigned int ids_size = ids.size();
  result.clear();
  result.reserve(ids_size);

  std::string qparams = "";
  for (int i = 0; i < ids_size; i++) {
    if (i == ids_size - 1) {
      qparams += "?";
    } else {
      qparams += "?,";
    }
  }

  const std::string q =
      fmt::format("SELECT * FROM files WHERE id IN ({});", qparams);
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetFileRes::SqlError;
  }

  for (int i = 0; i < ids_size; i++) {
    if (sqlite3_bind_int(stmt, i + 1, ids[i]) != SQLITE_OK) {
      PRINT_SQLITE_ERR(db);
      sqlite3_finalize(stmt);
      return DBRetCode::GetFileRes::SqlError;
    }
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int idx = 0;

    int id = sqlite3_column_int(stmt, idx++);
    int dir_id = sqlite3_column_int(stmt, idx++);
    std::filesystem::path filename =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::filesystem::path fulldir_path =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    int created_time = sqlite3_column_int(stmt, idx++);
    int modified_time = sqlite3_column_int(stmt, idx++);
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
    int filesize = sqlite3_column_int(stmt, idx++);
    Enum::FileType filetype = (Enum::FileType)sqlite3_column_int(stmt, idx++);

    result.emplace_back(id, dir_id, filename, fulldir_path, created_time,
                        modified_time, title, album, artist, albumartist,
                        track_number, disc_number, year, genre, length, bitrate,
                        filesize, filetype);
  }

  sqlite3_finalize(stmt);

  return DBRetCode::GetFileRes::Success;
}

DBRetCode::UpdateFileRes DB::update_file(int id,
                                         const Entity::File &updated_file) {
  const std::string sql =
      "UPDATE files SET "
      "modified_time = ?, title = ?, album = ?, "
      "artist = ?, albumartist = ?, track_number = ?, disc_number = ?, "
      "year = ?, genre = ?, length = ?, bitrate = ?, filesize = ? "
      "WHERE id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::UpdateFileRes::SqlError;
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
    return DBRetCode::UpdateFileRes::SqlError;
  }

  int rc = sqlite3_step(stmt);

  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::UpdateFileRes::SqlError;
  }

  return DBRetCode::UpdateFileRes::Success;
}

DBRetCode::RmvFileRes DB::remove_file(int id) {
  if (!db)
    return DBRetCode::RmvFileRes::SqlError;

  const std::string sql = "DELETE FROM files WHERE id = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::RmvFileRes::SqlError;
  }

  if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::RmvFileRes::SqlError;
  }

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::RmvFileRes::SqlError;
  }

  return DBRetCode::RmvFileRes::Success;
}

DBRetCode::GetDistinctArtistsRes
DB::get_distinct_artists(std::vector<Entity::Artist> &artists,
                         const DBGetOpt::ArtistsOptions &opts) {
  artists.clear();

  std::string colname =
      opts.use_albumartist
          ? "DISTINCT(COALESCE(NULLIF(albumartist, ''), "
            "NULLIF(artist, ''), 'Unknown Artist'))"
          : "DISTINCT(COALESCE(NULLIF(artist, ''), 'Unknown Artist'))";

  std::string orderby = "a ASC";

  switch (opts.sortby) {
  case DBGetOpt::SortArtists::NameDesc: {
    orderby = "a DESC";
    break;
  }

  default: {
    break;
  }
  }

  std::string count_q = fmt::format("SELECT COUNT({}) FROM files;", colname);
  sqlite3_stmt *count_stmt = nullptr;
  if (sqlite3_prepare_v2(db, count_q.c_str(), -1, &count_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetDistinctArtistsRes::SqlError;
  }

  int rc = sqlite3_step(count_stmt);
  if (rc != SQLITE_ROW) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(count_stmt);
    return DBRetCode::GetDistinctArtistsRes::SqlError;
  }

  int count = sqlite3_column_int(count_stmt, 0);

  sqlite3_finalize(count_stmt);

  artists.reserve(count);

  std::string q = fmt::format("SELECT {} AS a, COUNT(DISTINCT(album)) AS c "
                              "FROM files GROUP BY a ORDER BY {};",
                              colname, orderby);
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetDistinctArtistsRes::SqlError;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int idx = 0;

    std::string name =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    int album_count = sqlite3_column_int(stmt, idx++);

    artists.emplace_back(name, album_count);
  }

  sqlite3_finalize(stmt);

  return DBRetCode::GetDistinctArtistsRes::Success;
}

DBRetCode::GetArtistAlbumsRes
DB::get_artist_albums(Entity::Artist &artist,
                      const DBGetOpt::AlbumsOptions &opts) {
  artist.albums.clear();

  std::string colname =
      opts.use_albumartist ? "artist = ? OR albumartist = ?" : "artist = ?";

  std::string orderby = "album ASC";

  switch (opts.sortby) {
  case DBGetOpt::SortAlbums::NameDesc: {
    orderby = "album DESC";
    break;
  }

  case DBGetOpt::SortAlbums::YearAscAndNameAsc: {
    orderby = "year ASC, album ASC";
    break;
  }

  case DBGetOpt::SortAlbums::YearAscAndNameDesc: {
    orderby = "year ASC, album DESC";
    break;
  }

  case DBGetOpt::SortAlbums::YearDescAndNameAsc: {
    orderby = "year DESC, album ASC";
    break;
  }

  case DBGetOpt::SortAlbums::YearDescAndNameDesc: {
    orderby = "year DESC, album DESC";
    break;
  }

  default: {
    break;
  }
  }

  std::string count_q = fmt::format(
      "SELECT COUNT(DISTINCT(album)) FROM files WHERE {};", colname);
  sqlite3_stmt *count_stmt = nullptr;
  if (sqlite3_prepare_v2(db, count_q.c_str(), -1, &count_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetArtistAlbumsRes::SqlError;
  }

  if (sqlite3_bind_text(count_stmt, 1, artist.name.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(count_stmt);
    return DBRetCode::GetArtistAlbumsRes::SqlError;
  }

  if (opts.use_albumartist) {
    if (sqlite3_bind_text(count_stmt, 2, artist.name.c_str(), -1,
                          SQLITE_STATIC) != SQLITE_OK) {
      PRINT_SQLITE_ERR(db);
      sqlite3_finalize(count_stmt);
      return DBRetCode::GetArtistAlbumsRes::SqlError;
    }
  }

  int rc = sqlite3_step(count_stmt);
  if (rc != SQLITE_ROW) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(count_stmt);
    return DBRetCode::GetArtistAlbumsRes::SqlError;
  }

  int count = sqlite3_column_int(count_stmt, 0);

  sqlite3_finalize(count_stmt);

  artist.albums.reserve(count);

  std::string q = fmt::format("SELECT album, genre, year, COUNT(title)"
                              "FROM files WHERE {} GROUP BY album ORDER BY {};",
                              colname, orderby);
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetArtistAlbumsRes::SqlError;
  }

  if (sqlite3_bind_text(stmt, 1, artist.name.c_str(), -1, SQLITE_STATIC) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::GetArtistAlbumsRes::SqlError;
  }

  if (opts.use_albumartist) {
    if (sqlite3_bind_text(stmt, 2, artist.name.c_str(), -1, SQLITE_STATIC) !=
        SQLITE_OK) {
      PRINT_SQLITE_ERR(db);
      sqlite3_finalize(stmt);
      return DBRetCode::GetArtistAlbumsRes::SqlError;
    }
  }

  int i = 0;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int idx = 0;

    std::string album_name =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::string genre =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    int year = sqlite3_column_int(stmt, idx++);
    int track_count = sqlite3_column_int(stmt, idx++);

    artist.albums.emplace_back(album_name, genre, year, track_count);
  }

  sqlite3_finalize(stmt);

  return DBRetCode::GetArtistAlbumsRes::Success;
}

DBRetCode::GetAlbumTracksRes
DB::get_album_tracks(const Entity::Artist &artist, Entity::Album &album,
                     const DBGetOpt::TrackOptions &opts) {
  album.tracks.clear();

  std::string colname =
      opts.use_albumartist ? "artist = ? OR albumartist = ?" : "artist = ?";

  std::string q =
      fmt::format("SELECT "
                  "id, dir_id, filename, fulldir_path, title, track_number, "
                  "disc_number, length, bitrate, filesize, filetype "
                  "FROM files WHERE ({}) AND album = ? ORDER BY disc_number "
                  "ASC, track_number ASC;",
                  colname);
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return DBRetCode::GetAlbumTracksRes::SqlError;
  }

  int bind_idx = 1;

  if (sqlite3_bind_text(stmt, bind_idx++, artist.name.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::GetAlbumTracksRes::SqlError;
  }

  if (opts.use_albumartist) {
    if (sqlite3_bind_text(stmt, bind_idx++, artist.name.c_str(), -1,
                          SQLITE_STATIC) != SQLITE_OK) {
      PRINT_SQLITE_ERR(db);
      sqlite3_finalize(stmt);
      return DBRetCode::GetAlbumTracksRes::SqlError;
    }
  }

  if (sqlite3_bind_text(stmt, bind_idx++, album.title.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    sqlite3_finalize(stmt);
    return DBRetCode::GetAlbumTracksRes::SqlError;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int idx = 0;

    int id = sqlite3_column_int(stmt, idx++);
    int dir_id = sqlite3_column_int(stmt, idx++);
    std::filesystem::path filename =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::filesystem::path fulldir_path =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    std::string title =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
    int track_number = sqlite3_column_int(stmt, idx++);
    int disc_number = sqlite3_column_int(stmt, idx++);
    int length = sqlite3_column_int(stmt, idx++);
    int bitrate = sqlite3_column_int(stmt, idx++);
    int filesize = sqlite3_column_int(stmt, idx++);
    Enum::FileType filetype = (Enum::FileType)sqlite3_column_int(stmt, idx++);

    album.tracks.emplace_back(id, dir_id, filename, fulldir_path, title,
                              track_number, disc_number, length, bitrate,
                              filesize, filetype);
  }

  sqlite3_finalize(stmt);

  return DBRetCode::GetAlbumTracksRes::Success;
}

Enum::FileType DB::get_filetype(const std::filesystem::path &path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if (ext == ".mp3")
    return Enum::FileType::MP3;
  else if (ext == ".flac")
    return Enum::FileType::FLAC;
  else if (ext == ".ogg")
    return Enum::FileType::OGG;
  else
    return Enum::FileType::UNKNOWN;
}

std::filesystem::path DB::get_file_fullpath(const Entity::File &file) {
  std::filesystem::path fullpath =
      fmt::format("{}/{}", file.fulldir_path.c_str(), file.filename.c_str());
  return fullpath;
}

std::filesystem::path
DB::get_file_fullpath(const std::filesystem::path fulldir_path,
                      const std::filesystem::path filename) {
  std::filesystem::path fullpath =
      fmt::format("{}/{}", fulldir_path.c_str(), filename.c_str());
  return fullpath;
}
