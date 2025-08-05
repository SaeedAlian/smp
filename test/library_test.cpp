#include "../src/library.hpp"
#include <array>
#include <filesystem>
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <map>
#include <memory>

class LibraryTest : public ::testing::Test {
protected:
  void SetUp() override {
    std::filesystem::remove(db_path);
    // lib = std::make_unique<Library>(db_path);
  }
  void TearDown() override { std::filesystem::remove(db_path); }
  std::string db_path = "test_db.db";
  std::unique_ptr<Library> lib;
};

TEST_F(LibraryTest, DatabaseInit) { ASSERT_NE(lib->is_initialized(), false); }

// TEST_F(LibraryTest, AddDir) {
//   std::string test_path = "/test/path";
//   int test_dir_id = 0;
//
//   LibRetCode::AddDirRes rc = lib->add_directory(test_path, test_dir_id);
//   EXPECT_EQ(rc, LibRetCode::AddDirRes::Success);
//   EXPECT_GT(test_dir_id, 0);
// }
//
// TEST_F(LibraryTest, AddDirDuplicate) {
//   std::string test_path = "/duplicate/test/path";
//   int test_dir_id = 0;
//
//   LibRetCode::AddDirRes rc = lib->add_directory(test_path, test_dir_id);
//   EXPECT_EQ(rc, LibRetCode::AddDirRes::Success);
//   EXPECT_GT(test_dir_id, 0);
//
//   rc = lib->add_directory(test_path, test_dir_id);
//   EXPECT_EQ(rc, LibRetCode::AddDirRes::PathAlreadyExists);
// }
//
// TEST_F(LibraryTest, AddDirAndGet) {
//   std::string test_path = "some/test/path";
//   int test_dir_id = 0;
//
//   LibRetCode::AddDirRes rc = lib->add_directory(test_path, test_dir_id);
//   EXPECT_EQ(rc, LibRetCode::AddDirRes::Success);
//   EXPECT_GT(test_dir_id, 0);
//
//   LibEntity::Directory result;
//   LibRetCode::GetDirRes rc2 = lib->get_directory(test_dir_id, result);
//   EXPECT_EQ(rc2, LibRetCode::GetDirRes::Success);
//   EXPECT_EQ(result.id, test_dir_id);
//   EXPECT_EQ(result.path, test_path);
// }
//
// TEST_F(LibraryTest, AddDirsAndGetAll) {
//   std::array<std::string, 5> test_paths;
//   std::array<int, 5> test_dir_ids;
//
//   for (int i = 0; i < 5; i++) {
//     std::string path = fmt::format("/some/path{}", i);
//     test_paths[i] = path;
//     test_dir_ids[i] = 0;
//   }
//
//   for (int i = 0; i < 5; i++) {
//     LibRetCode::AddDirRes rc =
//         lib->add_directory(test_paths[i], test_dir_ids[i]);
//     EXPECT_EQ(rc, LibRetCode::AddDirRes::Success);
//     EXPECT_GT(test_dir_ids[i], 0);
//   }
//
//   std::map<int, LibEntity::Directory> mapres;
//   std::vector<LibEntity::Directory> vecres;
//
//   LibRetCode::GetDirRes rc = lib->get_directories_map(mapres);
//   EXPECT_EQ(rc, LibRetCode::GetDirRes::Success);
//
//   LibRetCode::GetDirRes rc2 = lib->get_directories_list(vecres);
//   EXPECT_EQ(rc2, LibRetCode::GetDirRes::Success);
//
//   EXPECT_GE(mapres.size(), 5);
//   EXPECT_GE(vecres.size(), 5);
// }
//
// TEST_F(LibraryTest, AddDirAndDelete) {
//   std::string test_path = "undefined/path";
//   int test_dir_id = 0;
//
//   LibRetCode::AddDirRes rc = lib->add_directory(test_path, test_dir_id);
//   EXPECT_EQ(rc, LibRetCode::AddDirRes::Success);
//   EXPECT_GT(test_dir_id, 0);
//
//   LibRetCode::RmvDirRes rc2 = lib->remove_directory(test_dir_id);
//   EXPECT_EQ(rc2, LibRetCode::RmvDirRes::Success);
//
//   LibEntity::Directory result;
//   LibRetCode::GetDirRes rc3 = lib->get_directory(test_dir_id, result);
//   EXPECT_EQ(rc3, LibRetCode::GetDirRes::NotFound);
// }
