#pragma once
// Comment-preserving splice for settings files (R-142): keep the original
// text and patch only changed value spans, so comments/formatting elsewhere
// stay byte-stable. Parse side tolerates // /* */ comments + trailing commas.
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace afterhours::settings_preserve {
using Json = nlohmann::json;

inline void skip_ws_comments(const std::string &s, size_t &i) {
  while (i < s.size()) {
    if (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r') { ++i; continue; }
    if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/') {
      while (i < s.size() && s[i] != '\n') ++i;
      continue;
    }
    if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*') {
      i += 2; while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) ++i;
      i = (i + 1 < s.size()) ? i + 2 : s.size(); continue;
    }
    break;
  }
}

inline size_t scan_string(const std::string &s, size_t i) {
  ++i; // opening quote
  while (i < s.size()) {
    if (s[i] == '\\') { i += 2; continue; }
    if (s[i] == '"') return i + 1;
    ++i;
  }
  return s.size();
}

inline std::string decode_key(const std::string &s, size_t b, size_t e) {
  // b/e span the quoted string; decode only common escapes (keys are simple).
  std::string out;
  for (size_t i = b + 1; i + 1 < e; ++i) {
    if (s[i] == '\\' && i + 1 < e) {
      char c = s[++i];
      out += (c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c);
    } else out += s[i];
  }
  return out;
}

inline size_t scan_value(const std::string &s, size_t i) {
  skip_ws_comments(s, i);
  if (i >= s.size()) return i;
  if (s[i] == '"') return scan_string(s, i);
  if (s[i] == '{' || s[i] == '[') {
    char open = s[i], close = (open == '{' ? '}' : ']');
    int depth = 0;
    while (i < s.size()) {
      if (s[i] == '"') { i = scan_string(s, i); continue; }
      if (s[i] == '/' ) { size_t j = i; skip_ws_comments(s, j); if (j != i) { i = j; continue; } }
      if (s[i] == open) ++depth;
      else if (s[i] == close && --depth == 0) return i + 1;
      ++i;
    }
    return s.size();
  }
  while (i < s.size() && std::string(",}] \t\n\r").find(s[i]) == std::string::npos) ++i;
  return i;
}

struct Member { std::string key; size_t ks, vs, ve; };
struct ObjInfo { std::vector<Member> members; size_t close = 0; bool ok = false; };

inline ObjInfo parse_object(const std::string &s, size_t start) {
  ObjInfo o; size_t i = start; skip_ws_comments(s, i);
  if (i >= s.size() || s[i] != '{') return o;
  ++i;
  while (true) {
    skip_ws_comments(s, i);
    if (i < s.size() && s[i] == '}') { o.close = i; o.ok = true; return o; }
    if (i >= s.size() || s[i] != '"') return ObjInfo{};
    size_t ks = i, ke = scan_string(s, i);
    std::string key = decode_key(s, ks, ke);
    i = ke; skip_ws_comments(s, i);
    if (i >= s.size() || s[i] != ':') return ObjInfo{};
    ++i; skip_ws_comments(s, i);
    size_t vs = i, ve = scan_value(s, i);
    o.members.push_back({key, ks, vs, ve});
    i = ve; skip_ws_comments(s, i);
    if (i < s.size() && s[i] == ',') { ++i; continue; }
    if (i < s.size() && s[i] == '}') { o.close = i; o.ok = true; return o; }
    return ObjInfo{};
  }
}

inline std::vector<std::pair<size_t, size_t>> array_elems(const std::string &s, size_t start) {
  std::vector<std::pair<size_t, size_t>> out; size_t i = start; skip_ws_comments(s, i);
  if (i >= s.size() || s[i] != '[') return out;
  ++i; skip_ws_comments(s, i);
  if (i < s.size() && s[i] == ']') return out;
  while (i < s.size()) {
    size_t vs = i, ve = scan_value(s, i); out.push_back({vs, ve});
    i = ve; skip_ws_comments(s, i);
    if (i < s.size() && s[i] == ',') { ++i; skip_ws_comments(s, i); continue; }
    break;
  }
  return out;
}

// Blank trailing commas (offset-preserving) so nlohmann can parse.
inline std::string sanitized(const std::string &s) {
  std::string out = s; size_t i = 0;
  while (i < s.size()) {
    if (s[i] == '"') { i = scan_string(s, i); continue; }
    if (s[i] == '/' ) { size_t j = i; skip_ws_comments(s, j); if (j != i) { i = j; continue; } }
    if (s[i] == ',') {
      size_t j = i + 1; skip_ws_comments(s, j);
      if (j < s.size() && (s[j] == '}' || s[j] == ']')) out[i] = ' ';
    }
    ++i;
  }
  return out;
}

inline Json parse_tolerant(const std::string &text) {
  return Json::parse(sanitized(text), nullptr, true, true);
}

struct Edit { size_t b, e; std::string text; };

inline void collect(const std::string &s, const Json &oldj, const Json &newj,
                    size_t vs, size_t ve, std::vector<Edit> &edits) {
  if (oldj == newj) return;
  if (oldj.is_object() && newj.is_object()) {
    ObjInfo o = parse_object(s, vs);
    if (!o.ok) { edits.push_back({vs, ve, newj.dump(2)}); return; }
    std::string inserts;
    for (auto &[key, value] : newj.items()) {
      auto it = std::find_if(o.members.begin(), o.members.end(),
                             [&](const Member &m) { return m.key == key; });
      if (it == o.members.end()) {
        if (!inserts.empty()) inserts += ", ";
        inserts += Json(key).dump() + ": " + value.dump();
      } else collect(s, oldj[key], value, it->vs, it->ve, edits);
    }
    for (auto &m : o.members) {
      if (newj.contains(m.key)) continue;
      size_t b = m.ks, e = m.ve; size_t j = e; skip_ws_comments(s, j);
      if (j < s.size() && s[j] == ',') e = j + 1; // drop member + following comma
      edits.push_back({b, e, ""});
    }
    if (!inserts.empty()) {
      size_t j = o.close; // decide comma from last non-ws/comment char
      size_t k = o.close;
      while (k > vs && std::string(" \t\n\r").find(s[k - 1]) != std::string::npos) --k;
      bool empty = (k > vs && s[k - 1] == '{');
      bool trailing = (k > vs && s[k - 1] == ',');
      std::string ins = empty ? inserts : trailing ? " " + inserts : ", " + inserts;
      edits.push_back({o.close, o.close, ins});
    }
    return;
  }
  if (oldj.is_array() && newj.is_array() && oldj.size() == newj.size()) {
    auto elems = array_elems(s, vs);
    if (elems.size() == oldj.size()) {
      for (size_t idx = 0; idx < elems.size(); ++idx)
        collect(s, oldj[idx], newj[idx], elems[idx].first, elems[idx].second, edits);
      return;
    }
  }
  edits.push_back({vs, ve, newj.dump()});
}

// Returns spliced text, or empty on failure (caller falls back to dump).
inline std::string splice(const std::string &original, const Json &newj) {
  try {
    Json oldj = parse_tolerant(original);
    size_t vs = 0; skip_ws_comments(original, vs);
    size_t ve = scan_value(original, vs);
    std::vector<Edit> edits;
    collect(original, oldj, newj, vs, ve, edits);
    std::sort(edits.begin(), edits.end(),
              [](const Edit &a, const Edit &b) { return a.b > b.b; });
    std::string out = original;
    for (auto &e : edits) out.replace(e.b, e.e - e.b, e.text);
    // Self-check: spliced text must parse back to the new values.
    if (parse_tolerant(out) != newj) return "";
    return out;
  } catch (...) { return ""; }
}
} // namespace afterhours::settings_preserve
