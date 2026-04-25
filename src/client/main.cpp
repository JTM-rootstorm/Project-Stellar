#include "stellar/platform/Window.hpp"
#include "stellar/platform/Input.hpp"

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using stellar::platform::Error;
using stellar::platform::Input;
using stellar::platform::Window;

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kTargetFps = 60;
constexpr int kFrameDelayMs = 1000 / kTargetFps;
constexpr float kRotationSpeed = 45.0f; // degrees per second

// Cube vertices: position (x,y,z) + color (r,g,b)
const float kCubeVertices[] = {
    // Front face (GREEN)
    -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
    // Back face (GREEN)
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
    // Left face (RED)
    -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
    // Right face (RED)
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
    // Top face (BLUE)
    -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
    // Bottom face (BLUE)
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
};

const unsigned int kCubeIndices[] = {
     0,  1,  2,   0,  2,  3,  // front
     4,  5,  6,   4,  6,  7,  // back
     8,  9, 10,   8, 10, 11,  // left
    12, 13, 14,  12, 14, 15,  // right
    16, 17, 18,  16, 18, 19,  // top
    20, 21, 22,  20, 22, 23   // bottom
};

constexpr const char* kVertexShader = R"(
#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;

uniform mat4 u_mvp;

out vec3 v_color;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_color = a_color;
}
)";

constexpr const char* kFragmentShader = R"(
#version 330 core

in vec3 v_color;
out vec4 frag_color;

void main() {
    frag_color = vec4(v_color, 1.0);
}
)";

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

GLuint create_shader_program(const char* vertex_src, const char* fragment_src) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_src);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

} // anonymous namespace

int main(int /*argc*/, char* /*argv*/[]) {
    Window window;
    if (auto result = window.create(kWindowWidth, kWindowHeight,
                                    "Stellar Engine");
        !result) {
        std::fprintf(stderr, "Window creation failed: %s\n",
                     result.error().message.c_str());
        return EXIT_FAILURE;
    }

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::fprintf(stderr, "Failed to initialize GLEW\n");
        window.destroy();
        return EXIT_FAILURE;
    }

    // Create shader program
    GLuint shader_program = create_shader_program(kVertexShader, kFragmentShader);
    glUseProgram(shader_program);

    // Get uniform location
    GLint mvp_loc = glGetUniformLocation(shader_program, "u_mvp");

    // Create VAO and VBO
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVertices), kCubeVertices,
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kCubeIndices), kCubeIndices,
                 GL_STATIC_DRAW);

    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          nullptr);
    glEnableVertexAttribArray(0);

    // Color attribute (location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Disable face culling so we can see all faces
    glDisable(GL_CULL_FACE);

    Input input;
    Uint32 frame_start, frame_time;

    while (!window.should_close()) {
        frame_start = SDL_GetTicks();

        window.process_input(input);

        // Calculate rotation angle based on elapsed time
        float elapsed_seconds = frame_start / 1000.0f;
        float rotation_angle = elapsed_seconds * kRotationSpeed;

        // Set viewport
        glViewport(0, 0, kWindowWidth, kWindowHeight);

        // Clear buffers
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Create GLM matrices
        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight),
            0.1f, 100.0f);

        glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 3.0f),  // camera position
            glm::vec3(0.0f, 0.0f, 0.0f),  // look at origin
            glm::vec3(0.0f, 1.0f, 0.0f)   // up vector
        );

        glm::mat4 model = glm::rotate(
            glm::mat4(1.0f),
            glm::radians(rotation_angle),
            glm::vec3(0.0f, 1.0f, 0.0f)  // rotate around Y-axis
        );

        glm::mat4 mvp = projection * view * model;

        // Draw cube
        glUseProgram(shader_program);
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, glm::value_ptr(mvp));

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        window.swap_buffers();

        input.reset_frame_state();

        // Frame rate limiting
        frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < kFrameDelayMs) {
            SDL_Delay(kFrameDelayMs - frame_time);
        }
    }

    // Cleanup
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(shader_program);

    window.destroy();
    return EXIT_SUCCESS;
}