#include <glad/glad.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_opengl.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

const char *vertex_shader_source = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 color;\n"
    "out vec4 vColor;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "    vColor = vec4(color.x, color.y, color.z, 1.0);\n"
    "}\0";

const char *fragment_shader_source = "#version 330 core\n"
    "in vec4 vColor;\n"
    "out vec4 color;\n"
    "void main()\n"
    "{\n"
    "    color = vColor;\n"
    "}\0";

static void sdl_err(bool rc) {
    if (!rc) {
        printf("%s", SDL_GetError());
        exit(1);
    }
}

#define XORSHIFT128_RAND_MAX ((uint32_t)0xFFFFFFFFu)
uint32_t xorshift128_state[4] = { 0x01234567, 0x89abcdef, 0xfedcba98, 0x7654321 };
uint32_t xorshift128(uint32_t *state);
uint32_t xorshift128(uint32_t *state) {
    uint32_t t, s;
    t = state[3];
    s = state[0];
    state[3] = state[2];
    state[2] = state[1];
    state[1] = s;
    t ^= t << 11;
    t ^= t >> 8;
    state[0] = t ^ s ^ (s >> 19);
    return state[0];
}
float randf(void) { return (float)xorshift128(xorshift128_state) / (float)XORSHIFT128_RAND_MAX; }

#ifndef ENABLE_GL_ERR
#define ENABLE_GL_ERR true
#endif
#define GL_ERR(msg) check_gl_err(ENABLE_GL_ERR, __LINE__, msg);
static void check_gl_err(bool enable, int line, const char *msg) {
    GLenum err;
    bool error_occurred;
    const char *str;

    if (!enable) return;

    error_occurred = false;
    while ((err = glGetError()) != GL_NO_ERROR) {
        switch (err) {
        case 0: str = "GL_NO_ERROR"; break;
        case 0x0500: str = "GL_INVALID_ENUM"; break;
        case 0x0501: str = "GL_INVALID_VALUE"; break;
        case 0x0502: str = "GL_INVALID_OPERATION"; break;
        case 0x0503: str = "GL_STACK_OVERFLOW"; break;
        case 0x0504: str = "GL_STACK_UNDERFLOW"; break;
        case 0x0505: str = "GL_OUT_OF_MEMORY"; break;
        default: str = "unknown error"; break;
        }
        printf("main.c:line(%d):glerr(0x%x):%s: -> %s\n", line, err, str, msg);
        error_occurred = true;
    }

    if (error_occurred) exit(1);
}

static void render(void);
static void init(void);
static void teardown(void);

/* GLOBAL DATA */
const int scw = 640;
const int sch = 480;

GLuint VAO, VBO, EBO, shader_program;
SDL_Window *g_window;
SDL_GLContext g_gl_context;

GLfloat *vertex_buffer;
GLuint *index_buffer;
int vertex_buffer_size = 0;
int index_buffer_size = 0;
int index_buffer_count = 0;

int
main(int argc, char *argv[])
{
    bool ret, quit;
    SDL_Event e;
    int i, j;

    /* 6 values per vertex (position, color). 10x10 vertices (9x9 tiles). */
    vertex_buffer_size = sizeof(*vertex_buffer) * 6 * 10 * 10;
    vertex_buffer = malloc(vertex_buffer_size);

    /* 3 indices per triangle. 2 triangles per tile. 9x9 tiles. */
    index_buffer_count = 3 * 2 * 9 * 9;
    index_buffer_size = sizeof(*index_buffer) * index_buffer_count;
    index_buffer = malloc(index_buffer_size);

    if (!vertex_buffer || !index_buffer) {
        printf("could not allocate vertex buffer and index buffer\n");
        exit(1);
    }

#if 1

    /* set up vertex buffer */
    for (j = 0; j < 10; j++) {
        for (i = 0; i < 10; i++) {
            /* 0:sc -> -1:1 => (pixel / scw) * 2 - 1  */
            *(vertex_buffer + (j * 10 + i) * 6 + 0) = (float)(i * 20 + 20) / (float)scw * 2.0 - 1.0;
            *(vertex_buffer + (j * 10 + i) * 6 + 1) = (float)(j * 20 + 20) / (float)sch * 2.0 - 1.0;
            *(vertex_buffer + (j * 10 + i) * 6 + 2) = 0.0;
            *(vertex_buffer + (j * 10 + i) * 6 + 3) = randf();
            *(vertex_buffer + (j * 10 + i) * 6 + 4) = randf();
            *(vertex_buffer + (j * 10 + i) * 6 + 5) = randf();
        }
    }

    /* set up index buffer */
    for (j = 0; j < 9; j++) {
        for (i = 0; i < 9; i++) {
            *(index_buffer + (j * 9 + i) * 6 + 0) = (j * 10 + i) + 0;
            *(index_buffer + (j * 9 + i) * 6 + 1) = (j * 10 + i) + 1;
            *(index_buffer + (j * 9 + i) * 6 + 2) = (j * 10 + i) + 10;

            *(index_buffer + (j * 9 + i) * 6 + 3) = (j * 10 + i) + 1;
            *(index_buffer + (j * 9 + i) * 6 + 4) = (j * 10 + i) + 10;
            *(index_buffer + (j * 9 + i) * 6 + 5) = (j * 10 + i) + 11;
        }
    }

#else

    vertex_buffer[0*6 + 0] = 0.5;
    vertex_buffer[0*6 + 1] = 0.5;
    vertex_buffer[0*6 + 2] = 0.0;
    vertex_buffer[0*6 + 3] = randf();
    vertex_buffer[0*6 + 4] = randf();
    vertex_buffer[0*6 + 5] = randf();

    vertex_buffer[1*6 + 0] = -0.5;
    vertex_buffer[1*6 + 1] = 0.0;
    vertex_buffer[1*6 + 2] = 0.0;
    vertex_buffer[1*6 + 3] = randf();
    vertex_buffer[1*6 + 4] = randf();
    vertex_buffer[1*6 + 5] = randf();

    vertex_buffer[2*6 + 0] = 0.5;
    vertex_buffer[2*6 + 1] = -0.5;
    vertex_buffer[2*6 + 2] = 0.0;
    vertex_buffer[2*6 + 3] = randf();
    vertex_buffer[2*6 + 4] = randf();
    vertex_buffer[2*6 + 5] = randf();

    vertex_buffer_size = sizeof(*vertex_buffer) * 6 * 3;

    index_buffer[0] = 0;
    index_buffer[1] = 1;
    index_buffer[2] = 2;
    index_buffer_size = sizeof(*index_buffer) * 3;
    index_buffer_count = 3;
#endif

    init();

    SDL_StartTextInput(g_window);

    quit = false;
    while (!quit) {

        /* input */

        while (SDL_PollEvent(&e)) {

            switch (e.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (e.button.button == SDL_BUTTON_LEFT);
                if (e.button.button == SDL_BUTTON_MIDDLE);
                if (e.button.button == SDL_BUTTON_RIGHT);
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (e.button.button == SDL_BUTTON_LEFT);
                if (e.button.button == SDL_BUTTON_MIDDLE);
                if (e.button.button == SDL_BUTTON_RIGHT);
                break;

            case SDL_EVENT_TEXT_INPUT:
                break;

            default:
                break;
            }
        }

        render();

        /* swap buffers */

        ret = SDL_GL_SwapWindow(g_window);
        sdl_err(ret);
    }

    ret = SDL_StopTextInput(g_window);
    sdl_err(ret);

    teardown();

    return 0;
}

static void
init(void)
{
    GLuint vertex_shader, fragment_shader;
    int ret;

    /* init SDL */

    ret = SDL_SetAppMetadata("minesweeper", "v0.0", NULL);
    sdl_err(ret);

    ret = SDL_InitSubSystem(SDL_INIT_VIDEO);
    sdl_err(ret);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    /* SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); */
    /* SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); */

    g_window = SDL_CreateWindow("minesweeper", scw, sch, SDL_WINDOW_OPENGL);
    sdl_err(g_window != NULL);

    g_gl_context = SDL_GL_CreateContext(g_window);
    sdl_err(g_gl_context != NULL);

    ret = SDL_GL_SetSwapInterval(-1);
    sdl_err(ret);

    gladLoadGL();

    /* init opengl */

    glViewport(0, 0, scw, sch);

    /* create shader program */

    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    GL_ERR("create vertex shader");

    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    GL_ERR("create fragment shader");

    shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    GL_ERR("compile shader");

    /* VAO/VBO/EBO setup */

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    /* populate buffers */
    glBufferData(GL_ARRAY_BUFFER, vertex_buffer_size, NULL, GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_buffer_size, NULL, GL_DYNAMIC_DRAW);

    /* shader attributes (layout) position and color */
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    /* unbind */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    GL_ERR("create VAO/VBO");
}

static void
teardown(void)
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shader_program);
    GL_ERR("cleanup");

    SDL_DestroyWindow(g_window);
    SDL_Quit();

    free(vertex_buffer);
    free(index_buffer);
}

static void
render(void)
{
    /* bind buffers */
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    GL_ERR("bind buffers");

    /* update with data */
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_buffer_size, vertex_buffer);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, index_buffer_size, index_buffer);

    GL_ERR("update data");

    /* clear background */
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    GL_ERR("clear background");

    /* draw */
    glUseProgram(shader_program);
    glDrawElements(GL_TRIANGLES, index_buffer_count, GL_UNSIGNED_INT, 0);

    GL_ERR("draw elements");

    /* unbind buffers */
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    GL_ERR("unbind buffers");
}

