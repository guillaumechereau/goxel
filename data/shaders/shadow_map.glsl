#ifdef VERTEX_SHADER

/************************************************************************/
attribute vec3 a_pos;
uniform   mat4 u_model;
uniform   mat4 u_view;
uniform   mat4 u_proj;
uniform   float u_pos_scale;
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
