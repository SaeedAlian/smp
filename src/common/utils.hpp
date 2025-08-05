#pragma once
#include <filesystem>
#include <optional>
#include <sqlite3.h>
#include <string>

int bind_opt_text(sqlite3_stmt *stmt, int index,
                  const std::optional<std::string> &val);
int bind_opt_int(sqlite3_stmt *stmt, int index, const std::optional<int> &val);

std::string convert_unsigned_char_ptr_to_string(const unsigned char *txt);

std::optional<std::string> read_nullable_string_column(sqlite3_stmt *stmt,
                                                       int index);

std::optional<int> read_nullable_int_column(sqlite3_stmt *stmt, int index);

std::int64_t get_file_mtime_epoch(const std::filesystem::path &path);
