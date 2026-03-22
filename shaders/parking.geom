#version 450
layout(lines) in;
layout(line_strip,max_vertices = 4) out;

void main()
{
    vec4 p1 = gl_in[0].gl_Position;
    vec4 p2 = gl_in[1].gl_Position;

    vec4 center = (p1+p2)*0.33333;
    center.x = 0.5;
    center.y = 0.3;

    //1
    gl_Position = p1;
    EmitVertex();

    gl_Position = p2;
    EmitVertex();

    gl_Position = center;
    EmitVertex();

    gl_Position = vec4(0.0, 0.5, 0.0, 0.0);
    EmitVertex();

    EndPrimitive();
}