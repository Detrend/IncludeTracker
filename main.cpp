// Include tracker
// Author: Matus Rozek
#include <string>
#include <string_view>
#include <format>
#include <print>
#include <filesystem>
#include <fstream>
#include <vector>
#include <set>
#include <regex>
#include <map>

// Args: include_tracker.exe <file> <directory> [--depth=X] [--list] [--help]

using namespace std;
namespace fs = filesystem;

int main(int argc, char* argv[])
{
  int  rc_depth      = 5;
  bool list_included = false;

  for (int i = 0; i < argc; ++i)
  {
    string_view view{argv[i]};

    if (view == "--help" || argc == 1)
    {
      println("Simple include tracker. Usage:");
      println("IncludeTracker <file> <directory to check for headers> [--list] [--depth=N] [--help]");
      println("--list:    Lists all files that get transitively included.");
      println("--depth=N: Recursion depth of scanning. 0 for infinite. Default = 5.");
      println("--help:    Prints this guide.");
    }
    else if (view.starts_with("--depth="))
    {
      rc_depth = stoi(string{view.substr(strlen("--depth="))});
      if (rc_depth <= 0)
      {
        rc_depth = 66666;
      }
    }
    else if (view == "--list")
    {
      list_included = true;
    }
  }

  if (argc < 3)
  {
    return 1;
  }

  auto file_path = filesystem::path(argv[1]);

  if (file_path.extension() != ".h" || !filesystem::is_regular_file(file_path))
  {
    println("\"{}\" is either not a C++ header or it does not exist.", argv[1]);
    return 1;
  }

  auto directory = filesystem::path(argv[2]);

  if (!filesystem::is_directory(directory))
  {
    println("\"{}\" is not a directory.", argv[2]);
    return 1;
  }

  map<string, fs::path> header_path_mapping;
  for (const fs::directory_entry& dir_entry : fs::recursive_directory_iterator(directory))
  {
    if (dir_entry.is_regular_file())
    {
      fs::path path = dir_entry.path();
      if (path.extension() == ".h" || path.extension() == ".inl")
      {
        string filename = path.filename().string();
        header_path_mapping.insert({move(filename), move(path)});
      }
    }
  }

  set<string> checked;
  vector<string>        to_check_this_round;
  vector<string>        to_check_next_round;

  regex pattern(R"abc(#include\s+"([^"]+)")abc");

  to_check_next_round.push_back(argv[1]);

  int depth_remaining = rc_depth;
  while (depth_remaining-->0 && to_check_next_round.size())
  {
    to_check_this_round = move(to_check_next_round);
    to_check_next_round.clear();

    while (to_check_this_round.size())
    {
      fs::path path = to_check_this_round.back();
      string   name = path.filename().string();
      to_check_this_round.pop_back();

      bool found   = false;
      int  inc_cnt = 0;

      if (header_path_mapping.contains(name))
      {
        found = true;
        const fs::path& path = header_path_mapping[name];

        ifstream input_stream(path);
        string line;

        while (getline(input_stream, line))
        {
          while (line.size() && line[0] >= 0 && isspace(line[0]))
          {
            line = line.substr(1);
          }

          if (line.starts_with("//"))
          {
            continue;
          }

          if (smatch match; regex_search(line, match, pattern))
          {
            string filename = fs::path(match[1].str()).filename().string();
            inc_cnt += 1;
            if (!checked.contains(filename))
            {
              checked.insert(filename);
              to_check_next_round.push_back(move(filename));
            }
          }
        }
      }
    }
  }

  string stripped_name = file_path.filename().string();
  int    num_checked   = static_cast<int>(checked.size());

  if (!list_included)
  {
    if (rc_depth == 66666)
    {
      println("{} includes transitively {} headers.", stripped_name, num_checked);
    }
    else
    {
      println("{} includes transitively {} headers. Recursion depth: {}", stripped_name, num_checked, rc_depth);
    }
  }
  else
  {
    for (const string& included : checked)
    {
      println("{}", included);
    }
  }

  return 0;
}
