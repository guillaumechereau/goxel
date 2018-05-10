#ifdef GL_ES
    precision mediump float;
#else
    #define highp
    #define mediump
    #define lowp
#endif

varying lowp vec2 v_pos_data;
uniform      mat4 u_model;
uniform      mat4 u_view;
uniform      mat4 u_proj;
uniform lowp vec2 u_block_id;

#ifdef VERTEX_SHADER

/************************************************************************/
attribute vec3 a_pos;
attribute vec2 a_pos_data;

void main()
{
    vec3 pos = a_pos;
    gl_Position = u_proj * u_view * u_model * vec4(pos, 1.0);
    v_pos_data = a_pos_data;
}
/************************************************************************/

#endif

#ifdef FRAGMENT_SHADER

/************************************************************************/
void main()
{
    gl_FragColor.rg = u_block_id;
    gl_FragColor.ba = v_pos_data;
}
/************************************************************************/

#endif
