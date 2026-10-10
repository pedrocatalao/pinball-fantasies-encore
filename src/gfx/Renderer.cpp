#include "gfx/Renderer.h"

#include <algorithm>
#include <cmath>

#include "core/Log.h"

namespace encore {

Renderer::~Renderer() {
  if (vao_) glDeleteVertexArrays(1, &vao_);
  if (indexTex_) glDeleteTextures(1, &indexTex_);
  if (paletteTex_) glDeleteTextures(1, &paletteTex_);
  deleteTarget(scene_);
  deleteTarget(hdScene_);
  for (GLuint& t : hdTex_)
    if (t) glDeleteTextures(1, &t);
  if (hdMapTex_) glDeleteTextures(1, &hdMapTex_);
  for (GLuint& t : spriteTex_)
    if (t) glDeleteTextures(1, &t);
  for (GLuint& t : coverTex_)
    if (t) glDeleteTextures(1, &t);
}

void Renderer::deleteTarget(Target& t) {
  if (t.tex) glDeleteTextures(1, &t.tex);
  if (t.fbo) glDeleteFramebuffers(1, &t.fbo);
  t = {};
}

bool Renderer::init(const std::filesystem::path& shaderDir, int frameWidth, int frameHeight, double pixelAspect) {
  shaderDir_ = shaderDir;
  pixelAspect_ = pixelAspect;
  glGenVertexArrays(1, &vao_);
  glGenTextures(1, &indexTex_);
  glGenTextures(1, &paletteTex_);
  glBindTexture(GL_TEXTURE_2D, paletteTex_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  resizeSource(frameWidth, frameHeight);
  if (!palettePass_.load(shaderDir / "fullscreen.vert", shaderDir / "palette.frag")) return false;
  if (!postPass_.load(shaderDir / "fullscreen.vert", shaderDir / "post.frag")) return false;
  return true;
}

void Renderer::resizeSource(int frameWidth, int frameHeight) {
  frameW_ = frameWidth;
  frameH_ = frameHeight;
  glBindTexture(GL_TEXTURE_2D, indexTex_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, frameWidth, frameHeight, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);
  ensureTarget(scene_, frameWidth, frameHeight);
}

void Renderer::ensureTarget(Target& t, int w, int h) {
  if (t.tex && t.w == w && t.h == h) return;
  if (!t.tex) glGenTextures(1, &t.tex);
  if (!t.fbo) glGenFramebuffers(1, &t.fbo);
  t.w = w;
  t.h = h;
  glBindTexture(GL_TEXTURE_2D, t.tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
  glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) log::error("offscreen framebuffer incomplete");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::pollShaderReload() {
  palettePass_.reloadIfChanged();
  postPass_.reloadIfChanged();
  if (crtLoaded_) crtPass_.reloadIfChanged();
  if (hdPassLoaded_) hdPass_.reloadIfChanged();
  if (spritePassLoaded_) spritePass_.reloadIfChanged();
}

void Renderer::setSpritePicture(std::size_t slot, int width, int height, const u8* rgba) {
  if (slot >= kSprites) return;
  if (!spriteTex_[slot]) glGenTextures(1, &spriteTex_[slot]);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, spriteTex_[slot]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  spriteSize_[slot] = {width, height};
}

void Renderer::clearSpritePictures() { spriteSize_ = {}; }

void Renderer::setCoverPicture(int layer, int width, int height, const u8* rgba, int sourceWidth, int sourceHeight) {
  if (layer < 0 || layer > 1) return;
  const auto l = static_cast<std::size_t>(layer);
  if (!coverTex_[l]) glGenTextures(1, &coverTex_[l]);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, coverTex_[l]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  coverLoaded_[l] = true;
  coverSource_[l] = {sourceWidth, sourceHeight};
}

void Renderer::clearCoverPictures() { coverLoaded_ = {}; }

void Renderer::drawSprites(const HdFrame& hd) {
  if (!spritePassLoaded_) {
    spritePassLoaded_ = true;
    if (!spritePass_.load(shaderDir_ / "fullscreen.vert", shaderDir_ / "sprite.frag"))
      log::error("the flipper shader failed to load; the drawn positions are used instead");
  }
  if (!spritePass_.id()) return;
  spritePass_.use();
  glUniform1i(spritePass_.uniform("uMap"), 2);   // still bound from the picture pass
  glUniform1i(spritePass_.uniform("uSprite"), 3);
  glUniform2f(spritePass_.uniform("uFrameSize"), static_cast<float>(hd.width), static_cast<float>(hd.height));
  glUniform1f(spritePass_.uniform("uTint"), hd.spriteTint);
  const auto layer = static_cast<std::size_t>(hd.ballLayer != 0 ? 1 : 0);
  glUniform1ui(spritePass_.uniform("uHasCover"), coverLoaded_[layer] ? 1u : 0u);
  if (coverLoaded_[layer]) {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, coverTex_[layer]);
    glUniform1i(spritePass_.uniform("uCover"), 4);
    glUniform2f(spritePass_.uniform("uCoverSource"), static_cast<float>(coverSource_[layer][0]), static_cast<float>(coverSource_[layer][1]));
    glUniform1ui(spritePass_.uniform("uFirstPlayfield"), static_cast<unsigned>(HdPicture::Playfield1On));
    glUniform1ui(spritePass_.uniform("uLastPlayfield"), static_cast<unsigned>(HdPicture::Playfield4Off));
  }
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glActiveTexture(GL_TEXTURE3);
  for (const HdSprite& s : hd.sprites) {
    if (s.picture >= kSprites || !spriteTex_[s.picture] || !spriteSize_[s.picture][0]) continue;
    glBindTexture(GL_TEXTURE_2D, spriteTex_[s.picture]);
    glUniform2f(spritePass_.uniform("uPivotFrame"), s.pivotFrameX, s.pivotFrameY);
    glUniform2f(spritePass_.uniform("uPivotSprite"), s.pivotSpriteX, s.pivotSpriteY);
    glUniform2f(spritePass_.uniform("uScale"), s.scaleX, s.scaleY);
    glUniform1f(spritePass_.uniform("uAngle"), s.angle);
    glUniform2f(spritePass_.uniform("uClip"), s.clipTop, s.clipBottom);
    glUniform1ui(spritePass_.uniform("uHiddenBy"), s.hiddenBy);
    glUniform1f(spritePass_.uniform("uOpacity"), s.opacity);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
  glDisable(GL_BLEND);
  glActiveTexture(GL_TEXTURE0);
}

void Renderer::setHdPicture(HdPicture p, int width, int height, const u8* rgba) {
  const auto i = static_cast<std::size_t>(p);
  if (!hdTex_[i]) glGenTextures(1, &hdTex_[i]);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, hdTex_[i]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, p == HdPicture::LeftRepeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  hdLoaded_ |= 1u << i;
}

void Renderer::drawHd(const HdFrame& hd) {
  if (!hdPassLoaded_) {
    hdPassLoaded_ = true;
    if (!hdPass_.load(shaderDir_ / "fullscreen.vert", shaderDir_ / "hd.frag")) {
      log::error("the replacement-picture shader failed to load; showing the original pictures");
      hdEnabled_ = false;
      return;
    }
  }
  if (!hdPass_.id()) return;
  glActiveTexture(GL_TEXTURE2);
  if (!hdMapTex_) glGenTextures(1, &hdMapTex_);
  glBindTexture(GL_TEXTURE_2D, hdMapTex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
  if (hd.width != hdMapW_ || hd.height != hdMapH_) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16UI, hd.width, hd.height, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, hd.map.data());
    hdMapW_ = hd.width;
    hdMapH_ = hd.height;
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, hd.width, hd.height, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, hd.map.data());
  }
  hdPass_.use();
  glUniform1i(hdPass_.uniform("uMap"), 2);
  glUniform1i(hdPass_.uniform("uPicture"), 3);
  glUniform1i(hdPass_.uniform("uPictureLit"), 4);
  glUniform3f(hdPass_.uniform("uFadeColor"), hd.fadeColor.r / 255.0f, hd.fadeColor.g / 255.0f, hd.fadeColor.b / 255.0f);
  glActiveTexture(GL_TEXTURE3);
  for (std::size_t i = 1; i < HdFrame::kCount; ++i) {
    if (!(hd.used & hdLoaded_ & (1u << i))) continue;
    // A playfield comes as a pair: the unlit picture is drawn and the lit one blended into it
    // lamp by lamp, so the lit picture is never a pass of its own.
    const auto picture = static_cast<HdPicture>(i);
    if (picture >= HdPicture::Playfield1On && picture <= HdPicture::Playfield4On) continue;
    const bool pair = picture >= HdPicture::Playfield1Off && picture <= HdPicture::Playfield4Off;
    const std::size_t lit = i - (static_cast<std::size_t>(HdPicture::Playfield1Off) -
                                 static_cast<std::size_t>(HdPicture::Playfield1On));
    const bool hasLit = pair && (hdLoaded_ & (1u << lit));
    if (hasLit) {
      glActiveTexture(GL_TEXTURE4);
      glBindTexture(GL_TEXTURE_2D, hdTex_[lit]);
      glActiveTexture(GL_TEXTURE3);
    }
    glBindTexture(GL_TEXTURE_2D, hdTex_[i]);
    glUniform1ui(hdPass_.uniform("uHasLit"), hasLit ? 1u : 0u);
    glUniform1ui(hdPass_.uniform("uId"), static_cast<GLuint>(i));
    glUniform2f(hdPass_.uniform("uSourceSize"), hd.size[i][0], hd.size[i][1]);
    glUniform1f(hdPass_.uniform("uFade"), hd.fade[i]);
    glUniform1f(hdPass_.uniform("uRowStep"), hd.rowStep[i]);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
  glActiveTexture(GL_TEXTURE0);
}

void Renderer::setPalette(const Palette& palette) { setRowPalettes(palette.colors().data(), 1); }

void Renderer::setRowPalettes(const Rgb* colors, int rows) {
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, paletteTex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  if (rows != paletteRows_) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 256, rows, 0, GL_RGB, GL_UNSIGNED_BYTE, colors);
    paletteRows_ = rows;
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, rows, GL_RGB, GL_UNSIGNED_BYTE, colors);
  }
}

void Renderer::draw(const Framebuffer& frame, int windowWidth, int windowHeight, const HdFrame* hd) {
  if (frame.width() != frameW_ || frame.height() != frameH_) resizeSource(frame.width(), frame.height());
  glBindVertexArray(vao_);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  // Upload the indexed frame and the palette.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, indexTex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameW_, frameH_, GL_RED_INTEGER, GL_UNSIGNED_BYTE, frame.data());
  const double targetAspect = (frameW_ * pixelAspect_) / frameH_;
  int vw = windowWidth, vh = static_cast<int>(std::lround(windowWidth / targetAspect));
  if (vh > windowHeight) { vh = windowHeight; vw = static_cast<int>(std::lround(windowHeight * targetAspect)); }
  viewport_ = {(windowWidth - vw) / 2, (windowHeight - vh) / 2, vw, vh};

  // Pass 1: palette lookup into the scene texture: native resolution, or the window's when
  // replacement pictures are drawn into it (pass 1b).
  const bool withHd = hdEnabled_ && hd && (hd->used & hdLoaded_) && hd->width == frameW_ && hd->height == frameH_;
  Target& scene = withHd ? hdScene_ : scene_;
  if (withHd) ensureTarget(hdScene_, std::max(vw, 1), std::max(vh, 1));
  glBindFramebuffer(GL_FRAMEBUFFER, scene.fbo);
  glViewport(0, 0, scene.w, scene.h);
  palettePass_.use();
  glUniform1i(palettePass_.uniform("uIndices"), 0);
  glUniform1i(palettePass_.uniform("uPalette"), 1);
  // (the dots are drawn by the first pass that is at the screen's resolution: this one with the
  // replacement pictures, the last one without them)
  const int displayTop = displayRows_ > 0 ? frameH_ - displayRows_ : -1;
  glUniform1i(palettePass_.uniform("uDisplayTop"), displayTop);
  glUniform1i(palettePass_.uniform("uDots"), withHd ? 1 : 0);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  if (withHd) {
    drawHd(*hd);
    if (!hd->sprites.empty()) drawSprites(*hd);
  }

  // Pass 2: present with aspect-correct letterboxing.
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, windowWidth, windowHeight);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(viewport_.x, viewport_.y, viewport_.w, viewport_.h);
  if (crt_ && !crtLoaded_) {
    crtLoaded_ = true;
    if (!crtPass_.load(shaderDir_ / "fullscreen.vert", shaderDir_ / "crt-lottes.frag")) {
      log::error("the CRT shader failed to load; showing the picture without it");
      crt_ = false;
    }
  }
  if (crt_) {
    crtPass_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene.tex);
    glUniform1i(crtPass_.uniform("uScene"), 0);
    glUniform2f(crtPass_.uniform("uSceneSize"), static_cast<float>(scene.w), static_cast<float>(scene.h));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    return;
  }
  postPass_.use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, scene.tex);
  glUniform1i(postPass_.uniform("uScene"), 0);
  glUniform2f(postPass_.uniform("uSceneSize"), static_cast<float>(scene.w), static_cast<float>(scene.h));
  glUniform2f(postPass_.uniform("uOutputSize"), static_cast<float>(vw), static_cast<float>(vh));
  glUniform1f(postPass_.uniform("uFilter"), smoothEdges_ ? 1.0f : 0.0f);
  glUniform1i(postPass_.uniform("uDisplayTop"), displayTop);
  glUniform1i(postPass_.uniform("uDots"), withHd ? 0 : 1);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
}

}  // namespace encore
