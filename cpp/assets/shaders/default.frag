#version 450 core

in vec3 vFragPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 vColor;

layout(std140, binding = 0) uniform CameraBlock {
    mat4 uView;
    mat4 uProjection;
    mat4 uViewProjection;
    vec3 uCameraPos;
    float uTime;
};

layout(std140, binding = 1) uniform LightBlock {
    vec3 uSunDir;
    float _pad0;
    vec3 uSunColor;
    float _pad1;
    vec3 uAmbientColor;
    float _pad2;
    vec3 uFogColor;
    float uFogDensity;
};

uniform vec4 uAlbedo;
uniform int uUseTexture;
uniform int uAlphaMode;
uniform float uAlphaCutoff;
layout(binding = 0) uniform sampler2D uTexture;
layout(binding = 1) uniform sampler2D uSpecularTexture;
uniform int uUseSpecularTexture;
// Filtro de color de la psicosis: contrast(), saturate() y hue-rotate() del CSS del juego web
uniform float uContrast = 1.0;
uniform float uSaturation = 1.0;
uniform float uHueDegrees = 0.0;

out vec4 FragColor;

void main() {
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(-uSunDir);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uSunColor;
    vec3 ambient = uAmbientColor;
    vec3 viewDir = normalize(uCameraPos - vFragPos);
    vec3 reflectedLight = reflect(-lightDir, norm);
    float specularStrength = uUseSpecularTexture == 1
        ? texture(uSpecularTexture, vUV).r * 0.35 : 0.0;
    vec3 specular = pow(max(dot(viewDir, reflectedLight), 0.0), 24.0)
        * uSunColor * specularStrength;

    vec4 baseColor = uAlbedo * vColor;
    if (uUseTexture == 1) {
        baseColor *= texture(uTexture, vUV);
    }
    if (uAlphaMode == 1) {
        if (baseColor.a < uAlphaCutoff) discard;
        baseColor.a = 1.0;
    } else if (uAlphaMode == 0) {
        baseColor.a = 1.0;
    }

    vec3 result = (ambient + diffuse) * baseColor.rgb + specular;

    float dist = length(uCameraPos - vFragPos);
    float fogFactor = 1.0 - exp(-dist * uFogDensity);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    result = mix(result, uFogColor, fogFactor);

    if (uContrast != 1.0 || uSaturation != 1.0 || uHueDegrees != 0.0) {
        const vec3 luma = vec3(0.2126, 0.7152, 0.0722);
        result = mix(vec3(dot(result, luma)), result, uSaturation);
        const float angle = radians(uHueDegrees);
        const float c = cos(angle);
        const float s = sin(angle);
        result = vec3(
            dot(vec3(0.213 + 0.787 * c - 0.213 * s, 0.715 - 0.715 * c - 0.715 * s, 0.072 - 0.072 * c + 0.928 * s), result),
            dot(vec3(0.213 - 0.213 * c + 0.143 * s, 0.715 + 0.285 * c + 0.140 * s, 0.072 - 0.072 * c - 0.283 * s), result),
            dot(vec3(0.213 - 0.213 * c - 0.787 * s, 0.715 - 0.715 * c + 0.715 * s, 0.072 + 0.928 * c + 0.072 * s), result));
        result = clamp((result - 0.5) * uContrast + 0.5, 0.0, 1.0);
    }
    FragColor = vec4(result, baseColor.a);
}
