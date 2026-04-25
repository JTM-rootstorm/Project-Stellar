#pragma once

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace stellar::graphics {

class Window {
public:
    Window(int width, int height, const char* title);
    ~Window();

    void run();

private:
    void init_sdl();
    void init_gl();
    void render();
    void handle_events();
    void create_cube_shader();
    void draw_cube(float rotation);

    SDL_Window* m_window;
    SDL_GLContext m_gl_context;
    
    int m_width;
    int m_height;
    bool m_running;

    // OpenGL shader and render objects
    GLuint m_shader_program;
    GLuint m_vao;
    GLuint m_vbo;
};