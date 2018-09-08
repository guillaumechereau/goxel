#ifdef VERTEX_SHADER

/************************************************************************/
attribute highp   vec3  a_pos;
uniform   highp   mat4  u_model;
uniform   highp   mat4  u_view;
uniform   highp   mat4  u_proj;
uniform   mediump float u_pos_scale;
void main()
{
    gl_Position = u_proj * u_view * u_model * vec4(a_pos * u_pos_scale, 1.0);
}

/************************************************************************/

#endif

#ifdef FRAGMENT_SHADER

/************************************************************************/
void main() {}
/************************************************************************/

#endif
