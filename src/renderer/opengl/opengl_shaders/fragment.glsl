#version 330 core

in vec2 UV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform int uUseTexture;
uniform vec3 uTint;

void main() {
    // vec3 base = uUseTexture == 1 ? texture(uTexture, UV).rgb : vec3(FragColor); Error glitch effect?
    vec3 base = uUseTexture == 1 ? texture(uTexture, UV).rgb : vec3(1.0);
    FragColor = vec4(base * uTint, 1.0);
}