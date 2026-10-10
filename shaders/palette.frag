#version 410 core
// Pass 1: palette lookup. Converts the 8-bit indexed framebuffer to RGB at native resolution.
//
// The palette texture holds one row per scanline, so a screen can change its colours
// part-way down exactly as the original did with its raster interrupt. When the texture
// has a single row, every scanline shares it.
in vec2 vUv;
out vec4 fragColor;
uniform usampler2D uIndices;   // R8UI, native resolution
uniform sampler2D uPalette;    // 256 x rows RGB
uniform int uDisplayTop;       // the frame's row where the dot display begins; -1 for none
uniform int uDots;             // 1: this pass is at the screen's resolution, and draws the dots

// The table's dot display (its last rows, from uDisplayTop down): a dot is one pixel of the
// frame on every other column and every other row, with dark pixels between. Enlarged by a
// factor that is not a whole number, single pixels and single gaps come out one and two screen
// pixels by turns, which shimmers; so each dot is drawn here as a round lamp at the screen's own
// resolution instead, in the colour of its pixel over the colour between them.
const float kDotRadius = 0.62;  // in the frame's pixels; the dots are two apart

vec3 colourAt(ivec2 p) {
  ivec2 size = textureSize(uIndices, 0);
  p = clamp(p, ivec2(0), size - 1);
  uint index = texelFetch(uIndices, p, 0).r;
  int rows = textureSize(uPalette, 0).y;
  int row = rows > 1 ? clamp(p.y, 0, rows - 1) : 0;
  return texelFetch(uPalette, ivec2(int(index), row), 0).rgb;
}

void main() {
  ivec2 size = textureSize(uIndices, 0);
  vec2 f = vec2(vUv.x, 1.0 - vUv.y) * vec2(size);  // in the frame's pixels, rows top-down
  vec2 screen = 1.0 / max(fwidth(f), vec2(1e-6));   // screen pixels to a frame pixel
  if (uDots != 0 && uDisplayTop >= 0 && f.y >= float(uDisplayTop)) {
    vec2 local = f - vec2(0.0, float(uDisplayTop));
    vec2 cell = floor((local - 0.5) / 2.0 + 0.5);    // the nearest dot
    ivec2 lit = ivec2(cell * 2.0) + ivec2(0, uDisplayTop);
    float d = length((local - (cell * 2.0 + 0.5)) * screen);
    float on = clamp(kDotRadius * min(screen.x, screen.y) - d + 0.5, 0.0, 1.0);
    fragColor = vec4(mix(colourAt(lit + ivec2(1, 1)), colourAt(lit), on), 1.0);
    return;
  }
  fragColor = vec4(colourAt(ivec2(floor(f))), 1.0);
}
