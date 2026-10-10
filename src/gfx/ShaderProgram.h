#pragma once
#include "gfx/Gl.h"

#include <filesystem>
#include <string>

namespace encore {

/// A vertex+fragment program loaded from files, with source-change hot reloading.
class ShaderProgram {
 public:
  ShaderProgram() = default;
  ~ShaderProgram();
  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;

  bool load(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
  /// Recompiles if either source file changed on disk. Returns true if reloaded.
  bool reloadIfChanged();
  void use() const;
  GLuint id() const { return program_; }
  GLint uniform(const char* name) const;

 private:
  bool compileFromFiles();
  GLuint program_ = 0;
  std::filesystem::path vertexPath_, fragmentPath_;
  std::filesystem::file_time_type vertexTime_{}, fragmentTime_{};
  std::string error_;
};

}  // namespace encore
