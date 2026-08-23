#version 330 core

#define MAX_POINT_LIGHTS 8

in vec2 UV;
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform int uUseTexture;
uniform vec3 uTint;

uniform vec3 uLightColor;
uniform int uNumPointLights;
uniform vec3 uPointLightPos[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColor[MAX_POINT_LIGHTS];

void main() {
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * uLightColor;

    vec3 norm = normalize(Normal);
    vec3 diffuse = vec3(0.0);

    for (int i = 0; i < uNumPointLights; i++) {
        vec3 lightDir = normalize(uPointLightPos[i] - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        diffuse += diff * uPointLightColor[i];
    }

    vec3 base = uUseTexture == 1 ? texture(uTexture, UV).rgb : vec3(1.0);
    vec3 result = (ambient + diffuse) * base * uTint;
    FragColor = vec4(result, 1.0);
}
