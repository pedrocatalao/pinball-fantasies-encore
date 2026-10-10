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
  vec2 uv = uFilter < 0.5 ? (floor(vUv * uSceneSize) + 0.5) / uSceneSize : sharpUv(vUv);
  fragColor = vec4(texture(uScene, uv).rgb, 1.0);
}
