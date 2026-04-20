#version 410 core
out vec4 FragColor;

flat in int fsColor;
//flat in vec3 FragPos;
//flat in vec3 Normal;
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};




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
uniform sampler2D diffuses;
uniform sampler2D speculars; 

void main()
{
  int number = 6;

  
  float shininess = (shininessPalette[number]*1.4f);



 
 
    // ambient
    vec3 ambient = light.ambient * texture(diffuses, TexCoords).rgb;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * texture(diffuses, TexCoords).rgb;  
    
    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = light.specular * spec * texture(speculars, TexCoords).rgb;  
        
    vec3 result = ambient + diffuse + specular;
 
    FragColor = vec4(result, 1.0);
}
