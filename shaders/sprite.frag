#version 410 core
// Draws a flipper as one picture turned to the angle the table computes, instead of the
// original's twenty-one drawn positions, and the ball as a picture of its own. Runs over the
// scene, at the window's resolution.
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D uSprite;   // the flipper at rest, RGBA, top row first
uniform usampler2D uMap;     // the replacement-picture map: its low bit marks what covers the flipper
uniform vec2 uFrameSize;     // screen pixels
uniform vec2 uPivotFrame;    // the hinge, in screen pixels
uniform vec2 uPivotSprite;   // the hinge, as a fraction of the picture
uniform vec2 uScale;         // fractions of the picture per screen pixel
uniform float uAngle;        // radians away from the resting position
uniform float uTint;         // the table's fade
uniform vec2 uClip;          // the screen rows it may be drawn in
uniform uint uHiddenBy;      // pixels whose flags match these are left alone (the ball only)
uniform float uOpacity;      // 1 for the ball itself, less for the ghosts behind it
uniform sampler2D uCover;    // what hides the ball, drawn to the replacement playfield (App::loadFlipperPictures)
uniform uint uHasCover;      // there is such a picture for where the ball is (on the playfield or the ramps)
uniform vec2 uCoverSource;   // the original playfield's size, which the picture covers edge to edge
uniform uint uFirstPlayfield, uLastPlayfield;  // the playfield pictures' numbers

// Under clear plastic the ball is seen whole, but greyed: this much of its light and shade
// gives way to an even grey.
const float kHaze = 0.42;
const vec3 kHazeGrey = vec3(0.55);
// In a picture of what hides the ball, the levels taken as black and as white.
const float kMaskBlack = 0.12, kMaskWhite = 0.92;

void main() {
  vec2 f = vec2(vUv.x, 1.0 - vUv.y) * uFrameSize;  // screen rows are top-down
  if (f.y < uClip.x || f.y >= uClip.y) discard;    // never over the dot matrix
  ivec2 p = clamp(ivec2(floor(f)), ivec2(0), ivec2(uFrameSize) - 1);
  // How much artwork is in front: all of it hides the picture, a dithered cover lets it
  // through. Read smoothly between the screen's pixels, so no trace of the dither is left.
  float cover = 0.0, plastic = 0.0;
  if (uHiddenBy != 0u) {
    vec2 c = f - 0.5;
    ivec2 q = ivec2(floor(c));
    vec2 t = fract(c);
    ivec2 hi = ivec2(uFrameSize) - 1;
    float c00 = float((texelFetch(uMap, clamp(q, ivec2(0), hi), 0).a >> 8u) & 0xffu);
    float c10 = float((texelFetch(uMap, clamp(q + ivec2(1, 0), ivec2(0), hi), 0).a >> 8u) & 0xffu);
    float c01 = float((texelFetch(uMap, clamp(q + ivec2(0, 1), ivec2(0), hi), 0).a >> 8u) & 0xffu);
    float c11 = float((texelFetch(uMap, clamp(q + ivec2(1, 1), ivec2(0), hi), 0).a >> 8u) & 0xffu);
    cover = mix(mix(c00, c10, t.x), mix(c01, c11, t.x), t.y) / 255.0;
    // A picture of what hides the ball, where there is one, says it instead, at its own
    // resolution: white shows the ball, black hides it, and pure red is clear plastic, which
    // shows it greyed. Its red is how much of the ball shows, and red less green how much
    // plastic it is seen through, so the soft edges between the three read as they look.
    uvec4 m = texelFetch(uMap, p, 0);
    uint id = m.b & 0xffu;
    if (uHasCover != 0u && id >= uFirstPlayfield && id <= uLastPlayfield) {
      vec2 step = vec2((m.b & 0x100u) != 0u ? 0.5 : 1.0, (m.b & 0x200u) != 0u ? 0.5 : 1.0);
      vec3 drawn = texture(uCover, (vec2(m.rg) / 8.0 + fract(f) * step) / uCoverSource).rgb;
      // (what is nearly black is black, and nearly white white: a picture's black is seldom 0)
      float shows = clamp((drawn.r - kMaskBlack) / (kMaskWhite - kMaskBlack), 0.0, 1.0);
      cover = 1.0 - shows;
      plastic = clamp((drawn.r - drawn.g - kMaskBlack) / (kMaskWhite - kMaskBlack), 0.0, 1.0);
    }
    if (cover > 0.99) discard;
  }
  vec2 d = f - uPivotFrame;
  float c = cos(uAngle), s = sin(uAngle);
  vec2 sp = vec2(c * d.x + s * d.y, -s * d.x + c * d.y) * uScale + uPivotSprite;
  if (any(lessThan(sp, vec2(0.0))) || any(greaterThanEqual(sp, vec2(1.0)))) discard;
  vec4 t = texture(uSprite, sp);
  float alpha = t.a * (1.0 - cover) * uOpacity;
  if (alpha < 0.02) discard;
  fragColor = vec4(mix(t.rgb, kHazeGrey, kHaze * plastic) * uTint, alpha);
}
