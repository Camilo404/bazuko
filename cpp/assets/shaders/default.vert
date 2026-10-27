#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aJoints;
layout(location = 5) in vec4 aWeights;

layout(std140, binding = 0) uniform CameraBlock {
    mat4 uView;
    mat4 uProjection;
    mat4 uViewProjection;
    vec3 uCameraPos;
    float uTime;
};

uniform mat4 uModel;
uniform int uSkinned;
uniform int uBoneCount;
layout(std140, binding = 2) uniform SkinBlock {
    mat4 uBones[128];
};

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vUV;
out vec4 vColor;

void main() {
    vec4 localPos = vec4(aPos, 1.0);
    vec3 localNormal = aNormal;
    if (uSkinned == 1) {
        ivec4 joints = clamp(ivec4(aJoints), ivec4(0), ivec4(max(uBoneCount - 1, 0)));
        float weightSum = dot(aWeights, vec4(1.0));
        if (weightSum > 0.0001) {
            vec4 weights = aWeights / weightSum;
            mat4 skin = uBones[joints.x] * weights.x + uBones[joints.y] * weights.y + uBones[joints.z] * weights.z + uBones[joints.w] * weights.w;
            localPos = skin * localPos;
            localNormal = mat3(skin) * localNormal;
        }
    }
    vec4 worldPos = uModel * localPos;
    vFragPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * localNormal;
    vUV = aUV;
    vColor = aColor;

    gl_Position = uViewProjection * worldPos;
}
