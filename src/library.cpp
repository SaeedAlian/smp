#include "library.hpp"
#include "common/defines.hpp"
#include "common/utils.hpp"
#include <iostream>
#include <sqlite3.h>

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
  const std::string sql = R"(
    CREATE TABLE IF NOT EXISTS directories (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      path TEXT UNIQUE
    );

    CREATE TABLE IF NOT EXISTS files (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      dir_id INTEGER NOT NULL,
      subdir_path TEXT,
      filename TEXT NOT NULL,
      title TEXT,
      album TEXT,
      artist TEXT,
      albumartist TEXT,
      track_number INTEGER,
      disc_number INTEGER,
      year INTEGER,
      genre TEXT,
      length INTEGER NOT NULL,
      bitrate INTEGER NOT NULL,
      filesize INTEGER NOT NULL,
      filetype INTEGER NOT NULL,

      FOREIGN KEY(dir_id) REFERENCES directories(id)
    );
  )";

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

  return LibRetCode::SetupTablesRes::Success;
}

LibRetCode::AddDirRes Library::add_directory(const std::string &path,
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

LibRetCode::ScanRes Library::full_scan() {
  return LibRetCode::ScanRes::Success;
}

LibRetCode::ScanRes Library::partial_scan(int dir_id) {
  return LibRetCode::ScanRes::Success;
}

LibRetCode::UpdateFromDBRes Library::populate_from_db() {
  return LibRetCode::UpdateFromDBRes::Success;
}

LibRetCode::AddFileRes Library::add_file(const LibEntity::File &file,
                                         int &result_id) {
  if (!db)
    return LibRetCode::AddFileRes::SqlError;

  const std::string check_sql = "SELECT COUNT(*) FROM files WHERE dir_id = ? "
                                "AND subdir_path IS ? AND filename = ?;";
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

  if (file.subdir_path.has_value()) {
    if (sqlite3_bind_text(check_stmt, 2, file.subdir_path->c_str(), -1,
                          SQLITE_STATIC) != SQLITE_OK) {
      PRINT_SQLITE_ERR(db);
      sqlite3_finalize(check_stmt);
      return LibRetCode::AddFileRes::SqlError;
    }
  } else {
    if (sqlite3_bind_null(check_stmt, 2) != SQLITE_OK) {
      PRINT_SQLITE_ERR(db);
      sqlite3_finalize(check_stmt);
      return LibRetCode::AddFileRes::SqlError;
    }
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

  const std::string insert_sql = "INSERT INTO files ("
                                 "dir_id, subdir_path, filename, title, album,"
                                 "artist, albumartist, track_number,"
                                 "disc_number, year, genre, length, bitrate,"
                                 "filesize, filetype"
                                 ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
  sqlite3_stmt *insert_stmt = nullptr;
  if (sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &insert_stmt, nullptr) !=
      SQLITE_OK) {
    PRINT_SQLITE_ERR(db);
    return LibRetCode::AddFileRes::SqlError;
  }

  int idx = 1;

  if (sqlite3_bind_int(insert_stmt, idx++, file.dir_id) != SQLITE_OK ||
      bind_opt_text(insert_stmt, idx++, file.subdir_path) != SQLITE_OK ||
      sqlite3_bind_text(insert_stmt, idx++, file.filename.c_str(), -1,
                        SQLITE_STATIC) != SQLITE_OK ||
      bind_opt_text(insert_stmt, idx++, file.title) != SQLITE_OK ||
      bind_opt_text(insert_stmt, idx++, file.album) != SQLITE_OK ||
      bind_opt_text(insert_stmt, idx++, file.artist) != SQLITE_OK ||
      bind_opt_text(insert_stmt, idx++, file.albumartist) != SQLITE_OK ||
      bind_opt_int(insert_stmt, idx++, file.track_number) != SQLITE_OK ||
      bind_opt_int(insert_stmt, idx++, file.disc_number) != SQLITE_OK ||
      bind_opt_int(insert_stmt, idx++, file.year) != SQLITE_OK ||
      bind_opt_text(insert_stmt, idx++, file.genre) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, file.length) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, file.bitrate) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, file.filesize) != SQLITE_OK ||
      sqlite3_bind_int(insert_stmt, idx++, (int)file.filetype) != SQLITE_OK) {
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
  result.subdir_path = read_nullable_string_column(stmt, idx++);
  result.filename =
      convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, idx++));
  result.title = read_nullable_string_column(stmt, idx++);
  result.album = read_nullable_string_column(stmt, idx++);
  result.artist = read_nullable_string_column(stmt, idx++);
  result.albumartist = read_nullable_string_column(stmt, idx++);
  result.track_number = read_nullable_int_column(stmt, idx++);
  result.disc_number = read_nullable_int_column(stmt, idx++);
  result.year = read_nullable_int_column(stmt, idx++);
  result.genre = read_nullable_string_column(stmt, idx++);
  result.length = sqlite3_column_int(stmt, idx++);
  result.bitrate = sqlite3_column_int(stmt, idx++);
  result.filesize = sqlite3_column_int(stmt, idx++);
  result.filetype = (LibEntity::FileType)sqlite3_column_int(stmt, idx++);

  sqlite3_finalize(stmt);

  return LibRetCode::GetFileRes::Success;
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
