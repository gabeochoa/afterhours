#pragma once

#ifdef AFTER_HOURS_ENABLE_MCP

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

#include <nlohmann/json.hpp>

#include "../core/key_codes.h"

namespace afterhours {
namespace mcp {

struct MCPConfig {
  std::function<void *(void)> get_render_texture = nullptr;
  std::function<std::pair<int, int>()> get_screen_size = nullptr;
  std::function<void(int x, int y)> mouse_move = nullptr;
  std::function<void(int x, int y, int button)> mouse_click = nullptr;
  std::function<void(int keycode)> key_down = nullptr;
  std::function<void(int keycode)> key_up = nullptr;
  std::function<std::vector<uint8_t>()> capture_screenshot = nullptr;
  std::function<std::string()> dump_ui_tree = nullptr;
};

namespace detail {

inline MCPConfig g_config;
inline bool g_initialized = false;
inline bool g_exit_requested = false;
inline std::string g_input_buffer;
inline int g_stdout_fd = -1; // Can be set to use a different fd for output

inline std::string base64_encode(const std::vector<uint8_t> &data) {
  static const char *chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  result.reserve((data.size() + 2) / 3 * 4);

  for (size_t i = 0; i < data.size(); i += 3) {
    uint32_t n = static_cast<uint32_t>(data[i]) << 16;
    if (i + 1 < data.size()) {
      n |= static_cast<uint32_t>(data[i + 1]) << 8;
    }
    if (i + 2 < data.size()) {
      n |= static_cast<uint32_t>(data[i + 2]);
    }

    result.push_back(chars[(n >> 18) & 0x3F]);
    result.push_back(chars[(n >> 12) & 0x3F]);
    result.push_back((i + 1 < data.size()) ? chars[(n >> 6) & 0x3F] : '=');
    result.push_back((i + 2 < data.size()) ? chars[n & 0x3F] : '=');
  }

  return result;
}

inline bool has_stdin_data() {
#ifdef _WIN32
  HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
  DWORD avail = 0;
  if (PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL)) {
    return avail > 0;
  }
  return false;
#else
  struct pollfd pfd;
  pfd.fd = STDIN_FILENO;
  pfd.events = POLLIN;
  return poll(&pfd, 1, 0) > 0;
#endif
}

inline std::string read_stdin_nonblocking() {
  std::string result;
#ifdef _WIN32
  HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
  DWORD avail = 0;
  if (PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL) && avail > 0) {
    std::vector<char> buf(avail);
    DWORD read_count = 0;
    if (ReadFile(h, buf.data(), avail, &read_count, NULL)) {
      result.assign(buf.data(), read_count);
    }
  }
#else
  if (has_stdin_data()) {
    char buf[4096];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n > 0) {
      result.assign(buf, static_cast<size_t>(n));
    }
  }
#endif
  return result;
}

inline void write_stdout(const std::string &data) {
#ifdef _WIN32
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD written = 0;
  WriteFile(h, data.c_str(), static_cast<DWORD>(data.size()), &written, NULL);
#else
  // Use saved fd if available (for when stdout has been redirected)
  int fd = (g_stdout_fd >= 0) ? g_stdout_fd : STDOUT_FILENO;
  ssize_t result = write(fd, data.c_str(), data.size());
  (void)result;
#endif
}

inline nlohmann::json get_tools_list() {
  return nlohmann::json::array({
      {{"name", "ping"},
       {"description", "Check if the MCP server is running and responsive"},
       {"inputSchema",
        {{"type", "object"}, {"properties", nlohmann::json::object()}}}},
      {{"name", "screenshot"},
       {"description", "Capture a screenshot of the current game frame"},
       {"inputSchema",
        {{"type", "object"}, {"properties", nlohmann::json::object()}}}},
      {{"name", "mouse_move"},
       {"description", "Move the mouse cursor to screen coordinates"},
       {"inputSchema",
        {{"type", "object"},
         {"properties",
          {{"x", {{"type", "integer"}, {"description", "X coordinate"}}},
           {"y", {{"type", "integer"}, {"description", "Y coordinate"}}}}},
         {"required", {"x", "y"}}}}},
      {{"name", "mouse_click"},
       {"description", "Click at the specified screen coordinates"},
       {"inputSchema",
        {{"type", "object"},
         {"properties",
          {{"x", {{"type", "integer"}}},
           {"y", {{"type", "integer"}}},
           {"button",
            {{"type", "string"},
             {"enum", {"left", "right"}},
             {"default", "left"}}}}},
         {"required", {"x", "y"}}}}},
      {{"name", "key_press"},
       {"description", "Press and release a key"},
       {"inputSchema",
        {{"type", "object"},
         {"properties",
          {{"key",
            {{"type", "string"},
             {"description",
              "Key name (e.g., 'a', 'enter', 'space', 'escape')"}}}}},
         {"required", {"key"}}}}},
      {{"name", "key_down"},
       {"description", "Press and hold a key"},
       {"inputSchema",
        {{"type", "object"},
         {"properties", {{"key", {{"type", "string"}}}}},
         {"required", {"key"}}}}},
      {{"name", "key_up"},
       {"description", "Release a held key"},
       {"inputSchema",
        {{"type", "object"},
         {"properties", {{"key", {{"type", "string"}}}}},
         {"required", {"key"}}}}},
      {{"name", "get_screen_size"},
       {"description", "Get the current game screen dimensions"},
       {"inputSchema",
        {{"type", "object"}, {"properties", nlohmann::json::object()}}}},
      {{"name", "exit"},
       {"description", "Request the application to close gracefully"},
       {"inputSchema",
        {{"type", "object"}, {"properties", nlohmann::json::object()}}}},
      {{"name", "dump_ui_tree"},
       {"description",
        "Dump the UI component tree showing positions, sizes, and hierarchy"},
       {"inputSchema",
        {{"type", "object"}, {"properties", nlohmann::json::object()}}}},
  });
}

inline nlohmann::json handle_request(const nlohmann::json &request) {
  nlohmann::json response;
  response["jsonrpc"] = "2.0";

  if (request.contains("id")) {
    response["id"] = request["id"];
  }

  std::string method = request.value("method", "");

  if (method == "initialize") {
    response["result"] = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {{"tools", nlohmann::json::object()}}},
        {"serverInfo", {{"name", "afterhours-game"}, {"version", "1.0.0"}}}};
    return response;
  }

  if (method == "notifications/initialized") {
    return nlohmann::json();
  }

  if (method == "tools/list") {
    response["result"] = {{"tools", get_tools_list()}};
    return response;
  }

  if (method == "tools/call") {
    auto params = request.value("params", nlohmann::json::object());
    std::string tool_name = params.value("name", "");
    auto arguments = params.value("arguments", nlohmann::json::object());

    if (tool_name == "ping") {
      response["result"] = {
          {"content", {{{"type", "text"}, {"text", "pong"}}}}};
      return response;
    }

    if (tool_name == "screenshot") {
      if (g_config.capture_screenshot) {
        std::vector<uint8_t> png_data = g_config.capture_screenshot();
        if (!png_data.empty()) {
          std::string base64_data = base64_encode(png_data);
          response["result"] = {{"content",
                                 {{{"type", "image"},
                                   {"data", base64_data},
                                   {"mimeType", "image/png"}}}}};
        } else {
          response["result"] = {
              {"content",
               {{{"type", "text"}, {"text", "Failed to capture screenshot"}}}}};
        }
      } else {
        response["result"] = {
            {"content",
             {{{"type", "text"}, {"text", "Screenshot not available"}}}}};
      }
      return response;
    }

    if (tool_name == "mouse_move") {
      int x = arguments.value("x", 0);
      int y = arguments.value("y", 0);
      if (g_config.mouse_move) {
        g_config.mouse_move(x, y);
        response["result"] = {
            {"content", {{{"type", "text"}, {"text", "Mouse moved"}}}}};
      } else {
        response["result"] = {
            {"content",
             {{{"type", "text"}, {"text", "Mouse move not available"}}}}};
      }
      return response;
    }

    if (tool_name == "mouse_click") {
      int x = arguments.value("x", 0);
      int y = arguments.value("y", 0);
      std::string button_str = arguments.value("button", "left");
      int button = (button_str == "right") ? 1 : 0;
      if (g_config.mouse_click) {
        g_config.mouse_click(x, y, button);
        response["result"] = {
            {"content", {{{"type", "text"}, {"text", "Mouse clicked"}}}}};
      } else {
        response["result"] = {
            {"content",
             {{{"type", "text"}, {"text", "Mouse click not available"}}}}};
      }
      return response;
    }

    if (tool_name == "key_press") {
      std::string key = arguments.value("key", "");
      int keycode = key_from_name(key);
      if (keycode > 0 && g_config.key_down && g_config.key_up) {
        g_config.key_down(keycode);
        g_config.key_up(keycode);
        response["result"] = {
            {"content", {{{"type", "text"}, {"text", "Key pressed"}}}}};
      } else {
        response["result"] = {
            {"content",
             {{{"type", "text"},
               {"text", "Invalid key or key press not available"}}}}};
      }
      return response;
    }

    if (tool_name == "key_down") {
      std::string key = arguments.value("key", "");
      int keycode = key_from_name(key);
      if (keycode > 0 && g_config.key_down) {
        g_config.key_down(keycode);
        response["result"] = {
            {"content", {{{"type", "text"}, {"text", "Key down"}}}}};
      } else {
        response["result"] = {
            {"content", {{{"type", "text"}, {"text", "Invalid key"}}}}};
      }
      return response;
    }

    if (tool_name == "key_up") {
      std::string key = arguments.value("key", "");
      int keycode = key_from_name(key);
      if (keycode > 0 && g_config.key_up) {
        g_config.key_up(keycode);
        response["result"] = {
            {"content", {{{"type", "text"}, {"text", "Key up"}}}}};
      } else {
        response["result"] = {
            {"content", {{{"type", "text"}, {"text", "Invalid key"}}}}};
      }
      return response;
    }

    if (tool_name == "get_screen_size") {
      if (g_config.get_screen_size) {
        auto [width, height] = g_config.get_screen_size();
        nlohmann::json size_json;
        size_json["width"] = width;
        size_json["height"] = height;
        response["result"] = {
            {"content", {{{"type", "text"}, {"text", size_json.dump()}}}}};
      } else {
        response["result"] = {
            {"content",
             {{{"type", "text"}, {"text", "Screen size not available"}}}}};
      }
      return response;
    }

    if (tool_name == "exit") {
      g_exit_requested = true;
      response["result"] = {
          {"content", {{{"type", "text"}, {"text", "Exit requested"}}}}};
      return response;
    }

    if (tool_name == "dump_ui_tree") {
      if (g_config.dump_ui_tree) {
        std::string tree_dump = g_config.dump_ui_tree();
        response["result"] = {
            {"content", {{{"type", "text"}, {"text", tree_dump}}}}};
      } else {
        response["result"] = {
            {"content",
             {{{"type", "text"}, {"text", "UI tree dump not available"}}}}};
      }
      return response;
    }

    response["error"] = {{"code", -32601},
                         {"message", "Unknown tool: " + tool_name}};
    return response;
  }

  response["error"] = {{"code", -32601},
                       {"message", "Unknown method: " + method}};
  return response;
}

inline void process_message(const std::string &message) {
  try {
    nlohmann::json request = nlohmann::json::parse(message);
    nlohmann::json response = handle_request(request);
    if (!response.empty()) {
      std::string response_str = response.dump() + "\n";
      write_stdout(response_str);
    }
  } catch (const std::exception &e) {
    nlohmann::json error_response;
    error_response["jsonrpc"] = "2.0";
    error_response["error"] = {{"code", -32700}, {"message", "Parse error"}};
    std::string response_str = error_response.dump() + "\n";
    write_stdout(response_str);
  }
}

} // namespace detail

inline void init(const MCPConfig &config, int stdout_fd = -1) {
  detail::g_config = config;
  detail::g_initialized = true;
  detail::g_stdout_fd = stdout_fd;

#ifndef _WIN32
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
#endif
}

inline void update() {
  if (!detail::g_initialized) {
    return;
  }

  std::string new_data = detail::read_stdin_nonblocking();
  if (!new_data.empty()) {
    detail::g_input_buffer += new_data;
  }

  size_t pos;
  while ((pos = detail::g_input_buffer.find('\n')) != std::string::npos) {
    std::string message = detail::g_input_buffer.substr(0, pos);
    detail::g_input_buffer = detail::g_input_buffer.substr(pos + 1);

    if (!message.empty()) {
      detail::process_message(message);
    }
  }
}

inline void shutdown() {
  detail::g_initialized = false;
  detail::g_input_buffer.clear();
}

inline bool exit_requested() { return detail::g_exit_requested; }

} // namespace mcp
} // namespace afterhours

#endif // AFTER_HOURS_ENABLE_MCP
