#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace Enum {

enum class FileType {
  MP3 = 1,
  FLAC,
  OGG,
  UNKNOWN,
};

enum class OutputType {
  ALSA = 0,
};

enum class DecoderType {
  MPG123 = 0,
  UNKNOWN,
};

enum class OutputDeviceType {
  DEFAULT = 0,
  PULSE,
  UNKNOWN,
};

}; // namespace Enum

namespace Entity {

struct Directory {
  int id;
  std::filesystem::path path;

  Directory() = default;
  Directory(int id__, std::filesystem::path path__) : id(id__), path(path__) {}
};

struct File {
  int id;
  int dir_id;
  std::filesystem::path filename;
  std::filesystem::path fulldir_path;
  std::int64_t created_time;
  std::int64_t modified_time;
  std::string title;
  std::string album;
  std::string artist;
  std::string albumartist;
  int track_number;
  int disc_number;
  int year;
  std::string genre;
  int length;
  int bitrate;
  unsigned int filesize;
  Enum::FileType filetype;

  File() = default;
  File(int id__, int dir_id__, std::filesystem::path filename__,
       std::filesystem::path fulldir_path__, std::int64_t created_time__,
       std::int64_t modified_time__, std::string title__, std::string album__,
       std::string artist__, std::string albumartist__, int track_number__,
       int disc_number__, int year__, std::string genre__, int length__,
       int bitrate__, unsigned int filesize__, Enum::FileType filetype__)
      : id(id__), dir_id(dir_id__), filename(filename__),
        fulldir_path(fulldir_path__), created_time(created_time__),
        modified_time(modified_time__), title(title__), album(album__),
        artist(artist__), albumartist(albumartist__),
        track_number(track_number__), disc_number(disc_number__), year(year__),
        genre(genre__), length(length__), bitrate(bitrate__),
        filesize(filesize__), filetype(filetype__) {}
};

struct FileMainProps {
  int id;
  int dir_id;
  std::filesystem::path filename;
  std::filesystem::path fulldir_path;
  std::int64_t created_time;
  std::int64_t modified_time;
  unsigned int filesize;
  Enum::FileType filetype;

  FileMainProps() = default;
  FileMainProps(int id__, int dir_id__, std::filesystem::path filename__,
                std::filesystem::path fulldir_path__,
                std::int64_t created_time__, std::int64_t modified_time__,
                unsigned int filesize__, Enum::FileType filetype__)
      : id(id__), dir_id(dir_id__), filename(filename__),
        fulldir_path(fulldir_path__), created_time(created_time__),
        modified_time(modified_time__), filesize(filesize__),
        filetype(filetype__) {}
};

struct UnreadFile {
  std::filesystem::path fullpath;
  std::filesystem::path fulldir_path;
  std::filesystem::path filename;
  int dir_id;
  std::int64_t created_time;
  std::int64_t modified_time;
  unsigned int filesize;
  Enum::FileType filetype;
};

struct Track {
  int file_id;
  int dir_id;
  std::filesystem::path filename;
  std::filesystem::path fulldir_path;
  std::string title;
  int track_number;
  int disc_number;
  int length;
  int bitrate;
  unsigned int filesize;
  Enum::FileType filetype;

  Track() = default;
  Track(int file_id__, int dir_id__, std::filesystem::path filename__,
        std::filesystem::path fulldir_path__, std::string title__,
        int track_number__, int disc_number__, int length__, int bitrate__,
        unsigned int filesize__, Enum::FileType filetype__)
      : file_id(file_id__), dir_id(dir_id__), filename(filename__),
        fulldir_path(fulldir_path__), title(title__),
        track_number(track_number__), disc_number(disc_number__),
        length(length__), bitrate(bitrate__), filesize(filesize__),
        filetype(filetype__) {}
};

struct Album {
  std::string title;
  std::string genre;
  int year;
  int track_count;
  std::vector<Track> tracks;

  Album() = default;
  Album(std::string title__, std::string genre__, int year__, int track_count__)
      : title(title__), genre(genre__), year(year__),
        track_count(track_count__) {
    tracks.reserve(track_count__);
  }
};

struct Artist {
  std::string name;
  int album_count;
  std::vector<Album> albums;

  Artist() = default;
  Artist(std::string name__, int album_count__)
      : name(name__), album_count(album_count__) {}
};

}; // namespace Entity

namespace Audio {

struct FormatInfo {
  int frame_size;
  unsigned int rate;
  int channels;
  int encoding;
  int bitrate;
  int bits;
  int is_signed;
  int is_bigendian;
};

}; // namespace Audio

inline std::ostream &operator<<(std::ostream &os, const Entity::File &f) {
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

inline std::string output_device_str(Enum::OutputDeviceType type) {
  switch (type) {
  case Enum::OutputDeviceType::DEFAULT:
    return "default";
  case Enum::OutputDeviceType::PULSE:
    return "pulse";
  default:
    return "default";
  }
}

inline Enum::OutputDeviceType output_device_enum(std::string type) {
  if (type == "default") {
    return Enum::OutputDeviceType::DEFAULT;
  } else if (type == "pulse") {
    return Enum::OutputDeviceType::PULSE;
  } else {
    return Enum::OutputDeviceType::UNKNOWN;
  }
}
