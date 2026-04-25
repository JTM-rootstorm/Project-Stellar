#include "stellar/graphics/window.hpp"
#include <iostream>
#include <stdexcept>

namespace stellar::graphics {

Window::Window(int width, int height, const char* title)
    : m_window(nullptr), m_gl_context(nullptr), 
      m_width(width), m_height(height), m_running(true) 
{
    init_sdl();
    init_gl();
    create_cube_shader();
}

Window::~Window() {
    SDL_GL_DeleteContext(m_gl_context);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void Window::init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error("SDL initialization failed: " + std::string(SDL_GetError()));
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    m_window = SDL_CreateWindow(
        "Stellar Engine - Spinning Cube",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        m_width, m_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!m_window) {
        throw std::runtime_error("Window creation failed: " + std::string(SDL_GetError()));
    }

    m_gl_context = SDL_GL_CreateContext(m_window);
    if (!m_gl_context) {
        throw std::runtime_error("OpenGL context creation failed: " + std::string(SDL_GetError()));
    }
}

void Window::init_gl() {
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        throw std::runtime_error("GLEW initialization failed");
    }

    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
}

void Window::create_cube_shader() {
    const char* vertex_shader_src = R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aColor;
        
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        
        out vec3 vertexColor;
        
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
            vertexColor = aColor;
        }
    )";

    const char* fragment_shader_src = R"(
        #version 450 core
        
        in vec3 vertexColor;
        out vec4 FragColor;
        
        void main() {
            FragColor = vec4(vertexColor, 1.0);
        }
    )";

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_src, nullptr);
    glCompileShader(vertex_shader);

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_src, nullptr);
    glCompileShader(fragment_shader);

    m_shader_program = glCreateProgram();
    glAttachShader(m_shader_program, vertex_shader);
    glAttachShader(m_shader_program, fragment_shader);
    glLinkProgram(m_shader_program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // Cube vertices with colors
    float vertices[] = {
        // Positions         // Colors
        -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, // 0 - Red
         0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, // 1 - Green
         0.5f,  0.5f, -0.5f, 0.0f, 0.0f, 1.0f, // 2 - Blue
        -0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.0f, // 3 - Yellow
        -0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 1.0f, // 4 - Magenta
         0.5f, -0.5f,  0.5f, 0.0f, 1.0f, 1.0f, // 5 - Cyan
         0.5f,  0.5f,  0.5f, 0.5f, 0.5f, 0.5f, // 6 - Gray
        -0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 1.0f  // 7 - White
    };

    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0, // Front face
        1, 5, 6, 6, 2, 1, // Right face
        5, 4, 7, 7, 6, 5, // Back face
        4, 0, 3, 3, 7, 4, // Left face
        3, 2, 6, 6, 7, 3, // Top face
        4, 5, 1, 1, 0, 4  // Bottom face
    };

    GLuint vbo, ebo;
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Window::draw_cube(float rotation) {
    glUseProgram(m_shader_program);

    // Model matrix (rotate cube)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(rotation), glm::vec3(1.0f, 1.0f, 1.0f));

    // View matrix (camera positioning)
    glm::mat4 view = glm::mat4(1.0f);
    view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));

    // Projection matrix
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 
        static_cast<float>(m_width) / static_cast<float>(m_height), 0.1f, 100.0f);

    // Set uniform matrix values
    GLint model_loc = glGetUniformLocation(m_shader_program, "model");
    GLint view_loc = glGetUniformLocation(m_shader_program, "view");
    GLint proj_loc = glGetUniformLocation(m_shader_program, "projection");

    glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(proj_loc, 1, GL_FALSE, glm::value_ptr(projection));

    // Draw the cube
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
}

void Window::handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                m_running = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    m_running = false;
                }
                break;
        }
    }
}

void Window::render() {
    static float rotation = 0.0f;
    rotation += 1.0f;
    if (rotation >= 360.0f) rotation -= 360.0f;

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_cube(rotation);

    SDL_GL_SwapWindow(m_window);
}

void Window::run() {
    while (m_running) {
        handle_events();
        render();
        SDL_Delay(16);  // Cap at roughly 60 FPS
    }
}

} // namespace stellar::graphics