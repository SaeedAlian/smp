#include "utils.hpp"

int bind_opt_text(sqlite3_stmt *stmt, int index,
                  const std::optional<std::string> &val) {
  if (val.has_value()) {
    return sqlite3_bind_text(stmt, index, val->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    return sqlite3_bind_null(stmt, index);
  }
}

int bind_opt_int(sqlite3_stmt *stmt, int index, const std::optional<int> &val) {
  if (val.has_value()) {
    return sqlite3_bind_int(stmt, index, val.value());
  } else {
    return sqlite3_bind_null(stmt, index);
  }
}

std::string convert_unsigned_char_ptr_to_string(const unsigned char *txt) {
  return txt ? reinterpret_cast<const char *>(txt) : "";
}

std::optional<std::string> read_nullable_string_column(sqlite3_stmt *stmt,
                                                       int index) {
  int type = sqlite3_column_type(stmt, index);
  std::optional<std::string> result;
  if (type == SQLITE_TEXT) {
    std::string out =
        convert_unsigned_char_ptr_to_string(sqlite3_column_text(stmt, index));
    result.emplace(out);
    return result;
  } else {
    return result;
  }
}

std::optional<int> read_nullable_int_column(sqlite3_stmt *stmt, int index) {
  int type = sqlite3_column_type(stmt, index);
  std::optional<int> result;
  if (type == SQLITE_INTEGER) {
    int out = sqlite3_column_int(stmt, index);
    result.emplace(out);
    return result;
  } else {
    return result;
  }
}
