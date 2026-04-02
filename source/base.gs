#version 410 core
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

//out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

in VS_OUT {
    int length;  // This is the greedy mesh "width" (expansion along v axis)
    int height;  // This is the greedy mesh "height" (expansion along u axis)
    int face;
    int color;
    int lod;
    vec3 FragPos;
    vec3 Normal;
} gs_in[];

flat out int fsColor;
flat out vec3 FragPos;
flat out vec3 Normal;

const vec3 cubeFaces[24] = vec3[24](
    // Front Face (Z = 0.5)
    vec3(-0.5, 0.5, 0.5), vec3(-0.5, -0.5, 0.5),
    vec3(0.5, 0.5, 0.5), vec3(0.5, -0.5, 0.5),
    // Back Face (Z = -0.5)
    vec3(0.5, 0.5, -0.5), vec3(0.5, -0.5, -0.5),
    vec3(-0.5, 0.5, -0.5), vec3(-0.5, -0.5, -0.5),
    // Top Face (Y = 0.5)
    vec3(-0.5, 0.5, -0.5), vec3(-0.5, 0.5, 0.5),
    vec3(0.5, 0.5, -0.5), vec3(0.5, 0.5, 0.5),
    // Bottom Face (Y = -0.5)
    vec3(-0.5, -0.5, 0.5), vec3(-0.5, -0.5, -0.5),
    vec3(0.5, -0.5, 0.5), vec3(0.5, -0.5, -0.5),
    // Right Face (X = 0.5)
    vec3(0.5, 0.5, 0.5), vec3(0.5, -0.5, 0.5),
    vec3(0.5, 0.5, -0.5), vec3(0.5, -0.5, -0.5),
    // Left Face (X = -0.5)
    vec3(-0.5, 0.5, -0.5), vec3(-0.5, -0.5, -0.5),
    vec3(-0.5, 0.5, 0.5), vec3(-0.5, -0.5, 0.5)
);

void main() {
    mat4 pv = projection * view * model;
    vec3 basePos = gl_in[0].gl_Position.xyz;
    int index = gs_in[0].face * 4;
    
    // length = v-axis expansion, height = u-axis expansion
    float lengthV = float(gs_in[0].length);
    float heightU = float(gs_in[0].height);

    fsColor = gs_in[0].color;
    Normal = gs_in[0].Normal;
    FragPos = gs_in[0].FragPos;

    int face = gs_in[0].face;
    
    vec3 scale = vec3(1.0);
    float lod_scale = pow(2,gs_in[0].lod) + 1.0;
    
    // Face ordering: FRONT=0, BACK=1, TOP=2, BOTTOM=3, RIGHT=4, LEFT=5
    // For each face: d=depth axis, u=height axis, v=length axis
    if (face == 0 || face == 1) {
        // FRONT/BACK: d=2(Z), u=0(X), v=1(Y)
        // height expands in u(X), length expands in v(Y)
        scale = vec3(heightU, lengthV, lod_scale - 1.0);
    } else if (face == 2 || face == 3) {
        // TOP/BOTTOM: d=1(Y), u=0(X), v=2(Z)
        // height expands in u(X), length expands in v(Z)
        scale = vec3(heightU, lod_scale - 1.0, lengthV);
    } else {
        // LEFT/RIGHT: d=0(X), u=1(Y), v=2(Z)
        // height expands in u(Y), length expands in v(Z)
        scale = vec3(lod_scale - 1.0, heightU, lengthV);
    }

    // Transform vertices from centered cube space to world space
    // The localVerthex is in [-0.5, 0.5] range, so we need to:
    // 1. Scale it by the mesh dimensions
    // 2. Shift it so it starts at basePos (which is the corner, not center)
    for (int i = 0; i < 4; i++) {
        vec3 localVertex = cubeFaces[index + i] + 0.5;
        // Scale and shift: multiply by scale, add 0.5 to get [0,1] range, then add basePos
        vec3 worldVertex = basePos + localVertex * scale;
        //FragPos = vec3(model * vec4(worldVertex, 1.0));
        gl_Position = pv * vec4(worldVertex, 1.0);
        EmitVertex();
    }
    EndPrimitive();
}