#version 410 core
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

in VS_OUT {
    int length;
    int height;
    int face;
    int color;
} gs_in[];

flat out int fsColor;

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
    mat4 pv = projection * view;
    vec3 basePos = gl_in[0].gl_Position.xyz;
    int index = gs_in[0].face * 4;
    
    float scaleLength = float(gs_in[0].length);
    float scaleHeight = float(gs_in[0].height);

    fsColor = gs_in[0].color;

    int face = gs_in[0].face;
    
    // The mesher gives us the MINIMUM corner position (x, y, z)
    // It expands by +width and +height from that corner
    // We need to position vertices correctly based on the face direction
    
    vec3 scale = vec3(1.0);
    
    // Face ordering: FRONT=0, BACK=1, TOP=2, BOTTOM=3, RIGHT=4, LEFT=5
    // For each face, determine which axes the width/height map to
    if (face == 0 || face == 1) {
        // FRONT/BACK: d=2(Z), u=0(X), v=1(Y)
        // height expands in X, length expands in Y
        scale = vec3(scaleHeight, scaleLength, 1.0);
    } else if (face == 2 || face == 3) {
        // TOP/BOTTOM: d=1(Y), u=0(X), v=2(Z)
        // height expands in X, length expands in Z
        scale = vec3(scaleHeight, 1.0, scaleLength);
    } else {
        // LEFT/RIGHT: d=0(X), u=1(Y), v=2(Z)
        // height expands in Y, length expands in Z
        scale = vec3(1.0, scaleHeight, scaleLength);
    }

    // The cubeFaces are defined from -0.5 to +0.5
    // We need to transform them to start at basePos and expand in positive directions
    // Transform: vertex_world = basePos + (vertex_local + 0.5) * scale
    for (int i = 0; i < 4; i++) {
        vec3 localVertex = cubeFaces[index + i];
        // Shift from [-0.5, 0.5] to [0, 1] range, then scale
        vec3 worldVertex = basePos + (localVertex + 0.5) * scale;
        gl_Position = pv * model * vec4(worldVertex, 1.0);
        EmitVertex();
    }
    EndPrimitive();
}