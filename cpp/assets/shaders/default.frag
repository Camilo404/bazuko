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
    int uLampCount;
    int _pad3a;
    int _pad3b;
    int _pad3c;
    vec4 uLampPosition[32];
    vec4 uLampColor[32];
};

uniform vec4 uAlbedo;
uniform int uUseTexture;
uniform int uUnlit = 0;   // bombillas: sin iluminacion
uniform int uAlphaMode;
uniform float uAlphaCutoff;
layout(binding = 0) uniform sampler2D uTexture;
layout(binding = 1) uniform sampler2D uSpecularTexture;
uniform int uUseSpecularTexture;
// Filtro de color de la psicosis: contrast(), saturate() y hue-rotate() del CSS del juego web
uniform float uWetness = 0.0;   // lluvia: suelo oscuro y brillante
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
    // farolas encendidas: luz puntual calida con caida suave hasta su alcance
    for (int i = 0; i < uLampCount; ++i) {
        const vec3 toLamp = uLampPosition[i].xyz - vFragPos;
        const float lampDistance = length(toLamp);
        const float lampRange = uLampPosition[i].w;
        if (lampDistance >= lampRange) continue;
        float falloff = 1.0 - lampDistance / lampRange;
        falloff *= falloff;
        const float facingLamp = max(dot(norm, toLamp / max(lampDistance, 0.001)), 0.0) * 0.8 + 0.2;
        result += baseColor.rgb * uLampColor[i].rgb * uLampColor[i].w * falloff * facingLamp;
    }
    if (uWetness > 0.0) {
        // superficies que miran hacia arriba (calles, andenes): mas oscuras, con brillo del sol y reflejo del cielo
        const float wet = uWetness * smoothstep(0.75, 0.95, norm.y);
        const float facing = max(dot(norm, viewDir), 0.0);
        result *= 1.0 - 0.2 * wet;
        result += pow(max(dot(viewDir, reflectedLight), 0.0), 40.0) * uSunColor * 0.7 * wet;
        result = mix(result, uFogColor, 0.2 * wet * pow(1.0 - facing, 3.0));
    }

    float dist = length(uCameraPos - vFragPos);
    float fogFactor = 1.0 - exp(-dist * uFogDensity);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    result = mix(result, uFogColor, fogFactor);
    // bombillas y halos: casi no los apaga la niebla, se ven encendidos desde lejos
    if (uUnlit == 1) result = mix(baseColor.rgb, uFogColor, fogFactor * 0.25);

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
