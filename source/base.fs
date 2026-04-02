#version 410 core
out vec4 FragColor;

flat in int fsColor;
flat in vec3 FragPos;
flat in vec3 Normal;

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

const vec3 colorPalette[14] = vec3[14](
    vec3(1.0f, 0.843f, 0.0f),
    vec3(1.0f, 0.882f, 0.4f),
    vec3(0.550f, 0.55f, 0.55f),
    vec3(0.753f, 0.753f, 0.753f), 
    vec3(0.900f, 0.9f, 0.98f),
    vec3(0.314f, 0.784f, 0.471f), 
    vec3(0.0f, 0.659f, 0.42f), 
    vec3(0.053f, 0.05f, 0.066f),  
    vec3(1.0f, 0.894f, 0.882f), 
    vec3(0.878f, 0.067f, 0.373f), 
    vec3(0.251f, 0.878f, 0.816f), 
    vec3(0.02f, 0.02f, 0.02f),
    vec3(0.01f, 0.01f, 0.01f),
    vec3(0.0f, 0.0f, 1.0f)
);
const vec3 ambientPalette[14] = vec3[14](
  vec3(0.24725f, 0.1995f, 0.0745f),
  vec3(0.24725f, 0.2245f, 0.0645f),
  vec3(0.105882f, 0.058824f, 0.113725f),
  vec3(0.19225f, 0.19225f, 0.19225f),
  vec3(0.23125f, 0.23125f, 0.23125f),
  vec3(0.0215f, 0.1745f, 0.0215f),
  vec3(0.135f, 0.2225f, 0.1575f),
  vec3(0.05375f, 0.05f, 0.06625f),
  vec3(0.25f, 0.20725f, 0.20725f),
  vec3(0.1745f, 0.01175f, 0.01175f),
  vec3(0.1f, 0.18725f, 0.1745f),
  vec3(0.0f, 0.0f, 0.0f),
  vec3(0.02f, 0.02f, 0.02f),
  vec3(0.0, 0.1, 0.2)
);
const vec3 diffusePalette[14] = vec3[14](
  vec3(0.75164f, 0.60648f, 0.22648f),
  vec3(0.34615f, 0.3143f, 0.0903f),
  vec3(0.427451f, 0.470588f, 0.541176f),
  vec3(0.50754f, 0.50754f, 0.50754f),
  vec3(0.2775f, 0.2775f, 0.2775f),
  vec3(0.07568f, 0.61424f, 0.07568f),
  vec3(0.54f, 0.89f, 0.63f),
  vec3(0.18275f, 0.17f, 0.22525f),
  vec3(1.0f, 0.829f, 0.829f),
  vec3(0.61424f, 0.04136f, 0.04136f),
  vec3(0.396f, 0.74151f, 0.69102f),
  vec3(0.01f, 0.01f, 0.01f),
  vec3(0.01f, 0.01f, 0.01f),
  vec3(0.0, 0.3, 0.5)
);
const vec3 specularPalette[14] = vec3[14](
  vec3(0.628281f, 0.555802f, 0.366065f),
  vec3(0.797357f, 0.723991f, 0.208006f),
  vec3(0.333333f, 0.333333f, 0.521569f),
  vec3(0.508273f, 0.508273f, 0.508273f),
  vec3(0.773911f, 0.773911f, 0.773911f),
  vec3(0.633f, 0.727811f, 0.633f),
  vec3(0.316228f, 0.316228f, 0.316228f),
  vec3(0.332741f, 0.328634f, 0.346435f),
  vec3(0.296648f, 0.296648f, 0.296648f),
  vec3(0.727811f, 0.626959f, 0.626959f),
  vec3(0.297254f, 0.30829f, 0.306678f),
  vec3(0.50f, 0.50f, 0.50f),
  vec3(0.4f, 0.4f, 0.4f),
  vec3(0.8, 0.9, 1.0)

);
const float shininessPalette[14] = float[14](
  float(51.2f),
  float(83.2f),
  float(9.84615f),
  float(51.2f),
  float(89.6f),
  float(76.8f),
  float(12.8f),
  float(38.4f),
  float(11.264f),
  float(76.8f),
  float(12.8f),
  float(32),
  float(10),
  float(64)
  
);
/*AmbientNDiffuseNspecularNshininessNcolor
 1.Gold
 2.Polished Gold
 3.Pewter
 4.Silver
 5.Polished Silver
 6.Emerald
 7.Jade
 8.Obsidian
 9.Pearl
 10.Ruby
 11.Turquoise
 12.Black Plastic
 13.Black Rubber
 14.water
   */
  
uniform Light light; 

uniform vec3 viewPos;


void main()
{
  int number = 6;
  vec3 ambients = (ambientPalette[number]);
  vec3 diffuses = (diffusePalette[number]);
  vec3 speculars = (specularPalette[number]);
  float shininess = (shininessPalette[number]*1.4f);



  
  vec3 colors = (colorPalette[number]);
   // ambient
    vec3 ambient = light.ambient * ambients;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * diffuses);
    
    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = light.specular * (spec * speculars);  
       

        
    vec3 result = (ambient + diffuse + specular) * colors;
    FragColor = vec4(result, 1.0);
}
