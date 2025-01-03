// math head for GLSL
vec3 rgb2lin(vec3 rgb) { // sRGB to linear approximation
  return pow(rgb, vec3(2.2));
}

vec3 lin2rgb(vec3 lin) { // linear to sRGB approximation
  return pow(lin, vec3(1.0 / 2.2));
}

#define RECIPROCAL_PI 0.3183098861837907
#define RECIPROCAL_2PI 0.15915494309189535