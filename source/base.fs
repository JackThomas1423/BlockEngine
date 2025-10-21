#version 330 core
out vec4 FragColor;

in float colorIndex;

const vec3 colorLookup[4] = vec3[4](
  vec3(1.0, 0.0, 0.0),
  vec3(0.0, 1.0, 0.0),
  vec3(0.0, 0.0, 1.0),
  vec3(0.5, 0.5, 0.0)
);

void main()
{
    int index = int(colorIndex);
    FragColor = vec4(colorLookup[index], 1.0);
}