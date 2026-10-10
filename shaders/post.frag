#version 410 core
// Pass 2: presentation (the CRT look is crt-lottes.frag instead).
//
// The default keeps the picture crisp. Each output pixel samples the centre of the source
// pixel it falls inside, so the image stays as sharp as nearest-neighbour, and only the
// thin band that straddles a source-pixel edge is blended. That band is what stops the
// uneven pixel sizes of a non-integer scale from shimmering.
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D uScene;      // native-resolution RGB scene
uniform vec2 uSceneSize;       // pixels
uniform vec2 uOutputSize;      // pixels of the viewport this pass renders into
uniform float uFilter;         // 0 = nearest neighbour, 1 = sharp with antialiased edges
uniform int uDisplayTop;       // the scene's row where the dot display begins; -1 for none
uniform int uDots;             // 1: the scene is at the frame's resolution, and the dots are drawn here

// The table's dot display (its last rows, from uDisplayTop down): a dot is one pixel of the
// scene on every other column and every other row, with dark pixels between. Enlarged by a
// factor that is not a whole number, single pixels and single gaps come out one and two screen
// pixels by turns, which shimmers; so each dot is drawn here as a round lamp at the screen's own
// resolution instead, in the colour of its pixel over the colour between them. (As palette.frag does
// when it draws at the screen's resolution.)
const float kDotRadius = 0.62;  // in the scene's pixels; the dots are two apart
// And a little glow: each dot lights the dark around it in its own colour, less the further
// from it and the dimmer it is (an unlit dot hardly at all), the glows of dots near each other
// adding up.
const float kGlow = 0.3;         // how bright the glow is at a dot's edge, of the dot's own
const float kGlowReach = 0.7;     // in the scene's pixels: how far it fades (to about a third)

vec3 sceneAt(ivec2 p) { return texelFetch(uScene, clamp(p, ivec2(0), ivec2(uSceneSize) - 1), 0).rgb; }

vec2 sharpUv(vec2 uv) {
  vec2 texel = uv * uSceneSize;
  vec2 scale = max(uOutputSize / uSceneSize, vec2(1.0));
  // Distance from the centre of the source pixel, in source pixels.
  vec2 centre = fract(texel) - 0.5;
  // Everything except a one-output-pixel band around the edge samples the exact centre.
  vec2 flat_ = 0.5 - 0.5 / scale;
  vec2 offset = (centre - clamp(centre, -flat_, flat_)) * scale + 0.5;
  return (floor(texel) + offset) / uSceneSize;
}

void main() {
  vec2 f = vec2(vUv.x, 1.0 - vUv.y) * uSceneSize;  // in the scene's pixels, rows top-down
  vec2 screen = 1.0 / max(fwidth(f), vec2(1e-6));   // screen pixels to a scene pixel
  if (uDots != 0 && uDisplayTop >= 0 && f.y >= float(uDisplayTop)) {
    vec2 local = f - vec2(0.0, float(uDisplayTop));
    vec2 cell = floor((local - 0.5) / 2.0 + 0.5);    // the nearest dot
    // (the texture's rows are bottom-up)
    int bottom = int(uSceneSize.y) - 1;
    ivec2 lit = ivec2(cell * 2.0) + ivec2(0, uDisplayTop);
    vec3 dark = sceneAt(ivec2(lit.x + 1, bottom - lit.y - 1));
    float d = length((local - (cell * 2.0 + 0.5)) * screen);
    float on = clamp(kDotRadius * min(screen.x, screen.y) - d + 0.5, 0.0, 1.0);
    vec3 c = mix(dark, sceneAt(ivec2(lit.x, bottom - lit.y)), on);
    vec3 glow = vec3(0.0);
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        vec2 other = cell + vec2(dx, dy);
        ivec2 at = ivec2(other * 2.0) + ivec2(0, uDisplayTop);
        float r = max(length(local - (other * 2.0 + 0.5)) - kDotRadius, 0.0) / kGlowReach;
        vec3 o = sceneAt(ivec2(at.x, bottom - at.y));
        float lum = max(o.r, max(o.g, o.b));
        glow += max(o - dark, 0.0) * exp(-r * r) * lum * lum;
      }
    fragColor = vec4(min(c + glow * kGlow * (1.0 - on), vec3(1.0)), 1.0);
    return;
  }
  vec2 uv = uFilter < 0.5 ? (floor(vUv * uSceneSize) + 0.5) / uSceneSize : sharpUv(vUv);
  fragColor = vec4(texture(uScene, uv).rgb, 1.0);
}
