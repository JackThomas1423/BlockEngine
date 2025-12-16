#version 410 core
out vec4 FragColor;

flat in int fsColor;

const vec3 colorPalette[8] = vec3[8](
    vec3(1.0, 0.0, 0.0), // Red
    vec3(0.0, 1.0, 0.0), // Green
    vec3(0.0, 0.0, 1.0), // Blue
    vec3(1.0, 1.0, 0.0), // Yellow
    vec3(1.0, 0.0, 1.0), // Magenta
    vec3(0.0, 1.0, 1.0), // Cyan
    vec3(1.0, 1.0, 1.0), // White
    vec3(0.0, 0.0, 0.0)  // Black
);


void main()
{
    FragColor = vec4(colorPalette[(fsColor - 1) % 8], 1.0);
}