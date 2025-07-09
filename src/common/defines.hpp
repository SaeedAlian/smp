#pragma once
#include <fmt/format.h>
#include <sqlite3.h>

#define PRINT_SQLITE_ERR(db)                                                   \
  std::cerr << "SQLite Error: " << sqlite3_errmsg(db) << "\n"
