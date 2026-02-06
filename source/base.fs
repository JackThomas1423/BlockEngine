#version 410 core
out vec4 FragColor;

flat in int fsColor;
//struct Material {
//   vec3 ambient;
//   vec3 diffuse;
//};

//struct Light {
//   vec3 position;
//   vec3 ambient;
///   vec3 diffuse;
//};

in vec3 Normal; 
in vec3 FragPos; 
//in vec3 color;

uniform vec3 lightColor;
uniform vec3 lightPos;
//uniform vec3 viewPos;
//uniform Material material;
//uniform Light light;

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
/*
const vec3 diffusePalette[3] = SetVec3[3](
  SetVec3("material.diffuse", 0.07568f,0.61424f, 0.07568f),
  SetVec3("material.diffuse", 0.54f,0.89f,	0.63f),
  SetVec3("material.diffuse", 0.18275f,	0.17f,	0.22525f)
);
const vec3 ambientPalette[3] = SetVec3[3](
  SetVec3("material.ambient", 0.0215f,0.1745f, 0.0215f),
  SetVec3("material.ambient", 0.135f,0.2225f, 0.1575f),
  SetVec3("material.ambient", 0.05375f,0.05f, 0.06625f)
);*/

//AmbientNDiffuse
//1.emerald
//2.jade
//3.obsidian   

void main()
{
    //ambient
    vec3 colors = (colorPalette[(fsColor - 1) % 8]);
    float ambientStrength = 0.5;
    vec3 ambient = ambientStrength * lightColor;
    // diffuse
   vec3 norm = normalize(Normal);
   vec3 lightDir = normalize(lightPos - FragPos);
   float diff = max(dot(norm, lightDir), 0.0);
   vec3 diffuse = diff * lightColor;


    vec3 result =(ambient + diffuse) * colors;

    FragColor = (result, 1.0);
}