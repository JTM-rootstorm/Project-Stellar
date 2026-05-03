#include <algorithm>
#include <cassert>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct FgdClass {
  std::string kind;
  std::vector<std::string> bases;
  std::set<std::string> local_keys;
};

struct FgdFile {
  std::map<std::string, FgdClass> classes;
};

std::string trim(std::string value) {
  const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
    return std::isspace(c) != 0;
  });
  const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
    return std::isspace(c) != 0;
  }).base();
  if (begin >= end) {
    return {};
  }
  return std::string(begin, end);
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path);
  assert(input.good());
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::vector<std::string> split_bases(std::string_view text) {
  std::vector<std::string> result;
  std::string token;
  for (char c : text) {
    if (c == ',') {
      token = trim(token);
      if (!token.empty()) {
        result.push_back(token);
      }
      token.clear();
    } else {
      token.push_back(c);
    }
  }
  token = trim(token);
  if (!token.empty()) {
    result.push_back(token);
  }
  return result;
}

std::string parse_class_name(std::string_view line) {
  const std::size_t equals = line.find('=');
  assert(equals != std::string_view::npos);
  std::size_t begin = equals + 1;
  while (begin < line.size() && std::isspace(static_cast<unsigned char>(line[begin])) != 0) {
    ++begin;
  }
  std::size_t end = begin;
  while (end < line.size()) {
    const char c = line[end];
    if (std::isspace(static_cast<unsigned char>(c)) != 0 || c == ':') {
      break;
    }
    ++end;
  }
  return std::string(line.substr(begin, end - begin));
}

std::vector<std::string> parse_header_bases(std::string_view line) {
  const std::string_view needle = "base(";
  const std::size_t begin = line.find(needle);
  if (begin == std::string_view::npos) {
    return {};
  }
  const std::size_t value_begin = begin + needle.size();
  const std::size_t end = line.find(')', value_begin);
  assert(end != std::string_view::npos);
  return split_bases(line.substr(value_begin, end - value_begin));
}

std::string parse_key_name(std::string_view line) {
  std::size_t begin = 0;
  while (begin < line.size() && std::isspace(static_cast<unsigned char>(line[begin])) != 0) {
    ++begin;
  }
  if (begin >= line.size() || line[begin] == '/' || line[begin] == '[' || line[begin] == ']') {
    return {};
  }
  std::size_t end = begin;
  while (end < line.size()) {
    const char c = line[end];
    if (!(std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.')) {
      break;
    }
    ++end;
  }
  if (end == begin || end >= line.size() || line[end] != '(') {
    return {};
  }
  return std::string(line.substr(begin, end - begin));
}

FgdFile parse_fgd(const std::filesystem::path &path) {
  std::istringstream input(read_file(path));
  FgdFile file;
  std::string current;
  int block_depth = 0;
  std::string line;
  while (std::getline(input, line)) {
    const std::string stripped = trim(line);
    if (stripped.starts_with("@BaseClass")) {
      current = parse_class_name(stripped);
      file.classes[current].kind = "BaseClass";
      block_depth = 0;
    } else if (stripped.starts_with("@PointClass") || stripped.starts_with("@SolidClass")) {
      current = parse_class_name(stripped);
      auto &entry = file.classes[current];
      entry.kind = stripped.starts_with("@PointClass") ? "PointClass" : "SolidClass";
      entry.bases = parse_header_bases(stripped);
      block_depth = 0;
    }

    if (stripped == "[") {
      ++block_depth;
      continue;
    }
    if (stripped == "]") {
      block_depth = std::max(0, block_depth - 1);
      continue;
    }
    if (block_depth == 1 && !current.empty()) {
      if (std::string key = parse_key_name(stripped); !key.empty()) {
        file.classes[current].local_keys.insert(std::move(key));
      }
    }
  }
  return file;
}

void collect_keys(const FgdFile &file, const std::string &name, std::set<std::string> &keys) {
  const auto found = file.classes.find(name);
  assert(found != file.classes.end());
  keys.insert(found->second.local_keys.begin(), found->second.local_keys.end());
  for (const std::string &base : found->second.bases) {
    collect_keys(file, base, keys);
  }
}

void collect_key_counts(const FgdFile &file, const std::string &name,
                        std::map<std::string, int> &counts) {
  const auto found = file.classes.find(name);
  assert(found != file.classes.end());
  for (const std::string &key : found->second.local_keys) {
    ++counts[key];
  }
  for (const std::string &base : found->second.bases) {
    collect_key_counts(file, base, counts);
  }
}

std::map<std::string, std::set<std::string>> concrete_contract(const FgdFile &file) {
  std::map<std::string, std::set<std::string>> result;
  for (const auto &[name, entry] : file.classes) {
    if (entry.kind == "PointClass" || entry.kind == "SolidClass") {
      collect_keys(file, name, result[name]);
    }
  }
  return result;
}

void assert_has_keys(const std::map<std::string, std::set<std::string>> &classes,
                     const std::string &classname, std::initializer_list<const char *> keys) {
  const auto found = classes.find(classname);
  assert(found != classes.end());
  for (const char *key : keys) {
    if (!found->second.contains(key)) {
      std::cerr << "FGD class '" << classname << "' is missing key '" << key << "'\n";
    }
    assert(found->second.contains(key));
  }
}

void assert_lacks_keys(const std::map<std::string, std::set<std::string>> &classes,
                       const std::string &classname, std::initializer_list<const char *> keys) {
  const auto found = classes.find(classname);
  assert(found != classes.end());
  for (const char *key : keys) {
    if (found->second.contains(key)) {
      std::cerr << "FGD class '" << classname << "' exposes unsupported key '" << key << "'\n";
    }
    assert(!found->second.contains(key));
  }
}

void fgd_files_have_identical_concrete_class_key_contracts() {
  const std::filesystem::path root = STELLAR_SOURCE_DIR;
  const auto trenchbroom = concrete_contract(
      parse_fgd(root / "tools/trenchbroom/Stellar/stellar_entities.fgd"));
  const auto bsp = concrete_contract(parse_fgd(root / "tools/bsp/stellar_entities.fgd"));
  assert(trenchbroom == bsp);
}

void required_editor_categories_are_exposed_with_expected_keys() {
  const std::filesystem::path root = STELLAR_SOURCE_DIR;
  const auto classes = concrete_contract(
      parse_fgd(root / "tools/trenchbroom/Stellar/stellar_entities.fgd"));

  assert_has_keys(classes, "worldspawn",
                  {"message", "wad", "_stellar_lighting_mode",
                   "_stellar_global_light", "_stellar_global_color",
                   "_stellar_global_intensity"});
  assert_has_keys(classes, "info_player_start", {"targetname", "origin", "angle"});
  assert_has_keys(classes, "info_player_deathmatch", {"targetname", "origin", "angle"});
  assert_has_keys(classes, "info_stellar_spawn",
                  {"targetname", "origin", "archetype", "_stellar_script", "_stellar_table"});
  assert_has_keys(classes, "light", {"targetname", "origin", "_light", "light", "style"});
  assert_has_keys(classes, "light_spot",
                  {"targetname", "target", "origin", "angles", "pitch", "_cone", "_cone2"});
  assert_has_keys(classes, "light_environment",
                  {"targetname", "origin", "angle", "pitch", "_light"});
  assert_has_keys(classes, "stellar_global_light",
                  {"targetname", "origin", "_stellar_color", "_stellar_intensity",
                   "_stellar_enabled"});
  assert_has_keys(classes, "trigger_stellar",
                  {"targetname", "target", "delay", "_stellar_script", "_stellar_once"});
  assert_has_keys(classes, "trigger_multiple", {"targetname", "target", "delay"});
  assert_has_keys(classes, "trigger_once", {"targetname", "target", "delay"});
  assert_has_keys(classes, "trigger_stellar_point", {"targetname", "target", "delay"});
  assert_has_keys(classes, "trigger_multiple_point",
                  {"targetname", "target", "delay", "origin", "_stellar_extents",
                   "_stellar_script"});
  assert_has_keys(classes, "trigger_once_point", {"targetname", "target", "delay"});
  assert_has_keys(classes, "stellar_object_collider",
                  {"targetname", "archetype", "_stellar_collider", "_stellar_enabled"});
  assert_has_keys(classes, "stellar_object_collider_point",
                  {"origin", "_stellar_extents", "_stellar_collider", "_stellar_enabled"});
  assert_has_keys(classes, "stellar_sprite",
                  {"targetname", "origin", "_stellar_sprite", "_stellar_size", "_stellar_alpha"});
  assert_has_keys(classes, "env_sprite", {"targetname", "origin", "_stellar_sprite"});
  assert_has_keys(classes, "func_wall", {"targetname", "archetype", "_stellar_collision"});
  assert_has_keys(classes, "func_illusionary", {"targetname", "_stellar_collision"});
  assert_has_keys(classes, "func_detail", {"targetname", "_stellar_collision"});
  assert_has_keys(classes, "func_door",
                  {"targetname", "angle", "angles", "speed", "wait", "lip", "dmg",
                   "spawnflags", "_stellar_collision"});
  assert_has_keys(classes, "func_button",
                  {"targetname", "target", "delay", "angle", "angles", "speed", "wait",
                   "lip", "spawnflags", "_stellar_collision"});
  assert_has_keys(classes, "target_stellar_relay", {"targetname", "target", "delay", "origin"});
  assert_has_keys(classes, "info_null", {"targetname", "origin"});
}

void unsupported_target_fields_are_not_exposed() {
  const std::filesystem::path root = STELLAR_SOURCE_DIR;
  const auto classes = concrete_contract(
      parse_fgd(root / "tools/trenchbroom/Stellar/stellar_entities.fgd"));

  for (const std::string &classname : {"func_door"}) {
    assert_lacks_keys(classes, classname, {"target", "killtarget", "message", "delay"});
  }
  for (const std::string &classname : {"target_stellar_relay"}) {
    assert_lacks_keys(classes, classname, {"killtarget", "message", "spawnflags"});
  }
  for (const std::string &classname : {"light", "light_environment", "info_player_start",
                                      "stellar_sprite", "env_sprite",
                                      "stellar_object_collider",
                                      "stellar_object_collider_point"}) {
    assert_lacks_keys(classes, classname, {"target"});
  }
}

void targetname_and_routing_bases_are_intentional() {
  const std::filesystem::path root = STELLAR_SOURCE_DIR;
  const FgdFile fgd = parse_fgd(root / "tools/trenchbroom/Stellar/stellar_entities.fgd");

  assert(fgd.classes.at("Targetname").local_keys.contains("targetname"));
  assert(!fgd.classes.at("LightKeys").local_keys.contains("targetname"));
  assert(fgd.classes.at("FiresTarget").local_keys.contains("target"));
  assert(fgd.classes.at("FiresTarget").local_keys.contains("delay"));
  assert(fgd.classes.at("KillTarget").local_keys.contains("killtarget"));
  assert(fgd.classes.at("MessageText").local_keys.contains("message"));
  assert(fgd.classes.at("AimTarget").local_keys.contains("target"));
  assert(!fgd.classes.contains("TargetRouting"));
}

void concrete_classes_do_not_duplicate_inherited_fields() {
  const std::filesystem::path root = STELLAR_SOURCE_DIR;
  const FgdFile fgd = parse_fgd(root / "tools/trenchbroom/Stellar/stellar_entities.fgd");
  for (const auto &[name, entry] : fgd.classes) {
    if (entry.kind != "PointClass" && entry.kind != "SolidClass") {
      continue;
    }
    std::map<std::string, int> counts;
    collect_key_counts(fgd, name, counts);
    for (const auto &[key, count] : counts) {
      if (count > 1) {
        std::cerr << "FGD class '" << name << "' exposes duplicate key '" << key << "'\n";
      }
      assert(count == 1);
    }
  }
}

void alias_policy_stays_importer_supported() {
  const std::filesystem::path root = STELLAR_SOURCE_DIR;
  const std::string text = read_file(root / "tools/trenchbroom/Stellar/stellar_entities.fgd");
  const std::vector<std::string> supported_aliases = {
      "_stellar_script",   "_stellar_table",    "_stellar_extents", "_stellar_once",
      "_stellar_sprite",   "_stellar_texture",  "_stellar_size",    "_stellar_alpha",
      "_stellar_collider", "_stellar_enabled",  "_stellar_collision",
      "_stellar_lighting_mode", "_stellar_global_light", "_stellar_global_color",
      "_stellar_global_intensity", "_stellar_color", "_stellar_intensity"};
  for (const std::string &alias : supported_aliases) {
    assert(text.find(alias) != std::string::npos);
  }
  const std::vector<std::string> unsupported_plain_aliases = {
      "\n    stellar_script(",   "\n    stellar_table(",     "\n    stellar_extents(",
      "\n    stellar_once(",     "\n    stellar_sprite(",    "\n    stellar_texture(",
      "\n    stellar_size(",     "\n    stellar_alpha(",     "\n    stellar_collider(",
      "\n    stellar_enabled(",  "\n    stellar_collision(", "\n    stellar_lighting_mode(",
      "\n    stellar_global_light(", "\n    stellar_global_color(",
      "\n    stellar_global_intensity(", "\n    stellar_color(", "\n    stellar_intensity("};
  for (const std::string &alias : unsupported_plain_aliases) {
    assert(text.find(alias) == std::string::npos);
  }
}

void point_and_solid_categories_are_intentional() {
  const std::filesystem::path root = STELLAR_SOURCE_DIR;
  const FgdFile fgd = parse_fgd(root / "tools/trenchbroom/Stellar/stellar_entities.fgd");
  for (const std::string &classname : {"trigger_stellar", "trigger_multiple", "trigger_once",
                                      "stellar_object_collider", "func_wall", "func_illusionary",
                                      "func_detail", "func_door", "func_button", "worldspawn"}) {
    assert(fgd.classes.at(classname).kind == "SolidClass");
  }
  for (const std::string &classname : {"info_player_start", "info_player_deathmatch",
                                      "info_stellar_spawn", "trigger_stellar_point",
                                      "trigger_multiple_point", "trigger_once_point",
                                      "stellar_object_collider_point", "stellar_sprite",
                                      "env_sprite", "light", "light_spot", "light_environment",
                                      "stellar_global_light", "target_stellar_relay",
                                      "info_null"}) {
    assert(fgd.classes.at(classname).kind == "PointClass");
  }
}

}  // namespace

int main() {
  fgd_files_have_identical_concrete_class_key_contracts();
  required_editor_categories_are_exposed_with_expected_keys();
  unsupported_target_fields_are_not_exposed();
  targetname_and_routing_bases_are_intentional();
  concrete_classes_do_not_duplicate_inherited_fields();
  alias_policy_stays_importer_supported();
  point_and_solid_categories_are_intentional();
  return 0;
}
