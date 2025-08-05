#include "db.hpp"
#include "library.hpp"
#include "player.hpp"
#include <filesystem>
#include <fmt/format.h>
#include <iostream>
#include <ncurses.h>
#include <optional>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <thread>
#include <vector>

#define FORMAT_ART(id) ("artist " + std::to_string(id))
#define FORMAT_ALB(id) ("album " + std::to_string(id))
#define FORMAT_TRK(id) ("track " + std::to_string(id))

int main() {
  DB db = DB("database.db");
  Library lib = Library(&db);
  MusicQueue q = MusicQueue(&db, 5);
  int r;
  // db.add_directory("/home/entropy/music", r);
  db.add_directory("/home/entropy/projects/smp/test_dir", r);
  lib.full_scan();

  q.enqueue(1);

  Player p = Player();

  std::vector<Entity::File> queue = q.get_queue();
  p.load(queue[0]);
  std::thread t(&Player::play, &p);

  t.join();

  // q.move(0, 3);

  // lib.init_artists();
  // std::vector<Entity::Artist> *artists = lib.get_artists();
  //
  // for (auto const &a : *artists) {
  //   std::cout << a.name << " " << a.album_count << '\n';
  // }
  //
  // int selected = 713;
  //
  // lib.set_artist_albums((*artists)[selected]);
  //
  // std::cout << '\n';
  // std::cout << (*artists)[selected].name << '\n';
  //
  // for (auto const &a : (*artists)[selected].albums) {
  //   std::cout << '\n';
  //   std::cout << a.title << " " << a.track_count << " " << a.year << '\n';
  //
  //   for (auto const &t : a.tracks) {
  //     std::cout << t.track_number << " " << t.title << '\n';
  //   }
  // }

  // std::string path = "/home/entropy/music";
  // for (const auto &entry :
  //      std::filesystem::recursive_directory_iterator(path)) {
  //   TagLib::FileRef f(entry.path().c_str());
  //   if (!f.isNull() && f.tag()) {
  //
  //     TagLib::Tag *tag = f.tag();
  //     std::cout << "Title: " << tag->title() << std::endl;
  //     std::cout << "Artist: " << tag->artist() << std::endl;
  //     std::cout << "Year: " << tag->year() << std::endl;
  //   }
  // }

  // std::optional<std::string> result;
  //
  // std::cout << result.has_value() << '\n';
  // result.emplace("HELLO");
  // std::cout << result.has_value() << '\n';
  // std::cout << result.value() << '\n';

  // const std::string dbname = "database.db";
  // Library lib = Library(dbname);

  // const std::string new_dir_path = "/home/entropy/music";
  // int new_dir_id;
  //
  // if (lib.add_directory(new_dir_path, new_dir_id) !=
  //     LibRetCode::AddDirRes::Success) {
  //   std::cout << "Error" << "\n";
  //   return 1;
  // }

  // std::cout << "id " << new_dir_id << "\n";
  //

  // std::vector<LibEntity::Directory> result;
  // lib.get_directories_list(result);
  //
  // for (LibEntity::Directory d : result) {
  //   std::cout << d.path << '\n';
  // }

  // setlocale(LC_ALL, "");
  //
  // initscr();
  // noecho();
  //
  // curs_set(0);
  // keypad(stdscr, true);
  //
  // std::vector<Artist> artists;
  //
  // for (int i = 0; i < 50; i++) {
  //   const std::string name = FORMAT_ART(i + 1);
  //   Artist *art = new Artist(name);
  //
  //   for (int j = 0; j < 3; j++) {
  //     const std::string albtitle = FORMAT_ALB(j + 1);
  //     Album *alb = new Album(albtitle, 2020);
  //
  //     for (int k = 0; k < 10; k++) {
  //       const std::string trtitle = FORMAT_TRK(j + 1);
  //       Track *trk = new Track(trtitle, 323);
  //       alb->append_track(*trk);
  //     }
  //     art->append_album(*alb);
  //   }
  //
  //   artists.push_back(*art);
  // }
  //
  // LibraryUI *lib_ui = new LibraryUI(artists);
  // lib_ui->draw();
  //
  // int c;
  // while ((c = getch()) != 'q') {
  //   if (c == KEY_RESIZE) {
  //     flushinp();
  //     lib_ui->draw();
  //   }
  //
  //   if (c == 'j') {
  //     lib_ui->artistswin_scroll_down();
  //   }
  // }
  //
  // lib_ui->cleanup();
  // endwin();
  return 0;
}
