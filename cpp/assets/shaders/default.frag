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

    FragColor = vec4(result, baseColor.a);
}
