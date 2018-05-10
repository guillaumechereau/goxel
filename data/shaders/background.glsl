#ifdef GL_ES
    precision mediump float;
#else
    #define highp
    #define mediump
    #define lowp
#endif

varying vec4 v_color;

#ifdef VERTEX_SHADER

/************************************************************************/
attribute vec3 a_pos;
attribute vec4 a_color;

void main()
{
    gl_Position = vec4(a_pos, 1.0);
    v_color = a_color;
}
/************************************************************************/

#endif

#ifdef FRAGMENT_SHADER

/************************************************************************/
void main()
{
    gl_FragColor = v_color;
}
/************************************************************************/

#endif
