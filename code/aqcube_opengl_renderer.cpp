#include "aqcube_renderer.h"

struct opengl_render_target
{
    render_target_type Type;
};

RENDERER_CLEAR(OpenGLRendererClear)
{
    vector3 Color = C / 255.0f;
    glClearColor(Color.R, Color.G, Color.B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

RENDERER_DRAW_BITMAP(OpenGLRendererDrawBitmap)
{
}
