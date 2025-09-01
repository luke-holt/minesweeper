#include <glad/glad.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_opengl.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const char *vertex_shader_source = "#version 330 core\n"
    "layout (location = 0) in vec2 pos;\n"
    "layout (location = 1) in vec2 texcoord;\n"
    "out vec2 vTexCoord;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(pos.x, pos.y, 1.0, 1.0);\n"
    "    vTexCoord = texcoord;\n"
    "}\0";

const char *fragment_shader_source = "#version 330 core\n"
    "out vec4 fColor;\n"
    "in vec2 vTexCoord;\n"
    "uniform sampler2D tex0;\n"
    "void main()\n"
    "{\n"
    "    fColor = texture(tex0, vTexCoord);\n"
    "}\0";

static void
sdl_err(int rc) {
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

static void die(const char *fmt, ...);

static void render(void);
static void init(void);
static void teardown(void);

static void tilemap_init(int w, int h);

/* GLOBAL DATA */
int scw, sch;

GLuint VAO, VBO, shader, texture, uniform_tex0;
SDL_Window *g_window;
SDL_GLContext g_gl_context;

struct vertex { float x, y, tx, ty; };
struct vertex *vertex_buffer;
int vertex_buffer_size = 0;

/*
GLuint *index_buffer;
int index_buffer_size = 0;
int index_buffer_count = 0;
*/

int
main(int argc, char *argv[])
{
    bool ret, quit;
    SDL_Event e;

    tilemap_init(9, 9);

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

enum {
    TILE_CELL_UNKOWN,
    TILE_CELL_EMPTY,
    TILE_CELL_1,
    TILE_CELL_2,
    TILE_CELL_3,
    TILE_CELL_4,
    TILE_CELL_5,
    TILE_CELL_6,
    TILE_CELL_7,
    TILE_CELL_8,
    TILE_CELL_BOMB,
    TILE_CELL_BOMBRED,
    TILE_SMILE_NORMAL,
    TILE_SMILE_CLICK,
    TILE_SMILE_WIN,
    TILE_SMILE_LOSE,
    TILE_NUM_0,
    TILE_NUM_1,
    TILE_NUM_2,
    TILE_NUM_3,
    TILE_NUM_4,
    TILE_NUM_5,
    TILE_NUM_6,
    TILE_NUM_7,
    TILE_NUM_8,
    TILE_NUM_9,
    TILE_BAR_LEFT,
    TILE_BAR_MID,
    TILE_BAR_RIGHT,
    TILE_BOT_LEFT,
    TILE_BOT_MID,
    TILE_BOT_RIGHT,
};

void
tilemap_init(int w, int h)
{
    int barh, border, tile, ox, oy, vcount, i, j;
    struct vertex *v;

    tile = 16;
    border = 10;
    barh = 52;
    scw = border * 2 + tile * w;
    sch = barh + tile * h + border;

    vcount = 36 + (h + 1) * (w + 1) * 8;

    vertex_buffer_size = vcount * sizeof(*vertex_buffer);
    vertex_buffer = malloc(vertex_buffer_size);
    if (!vertex_buffer) die("couldn't allocate vertex buffer");

    v = vertex_buffer;

    /* bar left */
    *v++ = (struct vertex) {          0,            sch,  2, 30 };
    *v++ = (struct vertex) { border - 1,            sch, 11, 30 };
    *v++ = (struct vertex) {          0, sch - barh + 1,  2, 81 };
    *v++ = (struct vertex) { border - 1, sch - barh + 1, 11, 81 };

    /* bar middle */
    *v++ = (struct vertex) {       border,            sch, 14, 30 };
    *v++ = (struct vertex) { scw - border,            sch, 29, 30 };
    *v++ = (struct vertex) {       border, sch - barh + 1, 14, 81 };
    *v++ = (struct vertex) { scw - border, sch - barh + 1, 29, 81 };

    /* bar right */
    *v++ = (struct vertex) { scw - border + 1,            sch, 32, 30 };
    *v++ = (struct vertex) {              scw,            sch, 41, 30 };
    *v++ = (struct vertex) { scw - border + 1, sch - barh + 1, 32, 81 };
    *v++ = (struct vertex) {              scw, sch - barh + 1, 41, 81 };

    /* bottom border left */
    *v++ = (struct vertex) {          0, border - 1,  2, 30 };
    *v++ = (struct vertex) { border - 1, border - 1, 11, 30 };
    *v++ = (struct vertex) {          0,          0,  2, 81 };
    *v++ = (struct vertex) { border - 1,          0, 11, 81 };
    
    /* bottom border middle */
    *v++ = (struct vertex) {       border, border - 1, 14, 30 };
    *v++ = (struct vertex) { scw - border, border - 1, 29, 30 };
    *v++ = (struct vertex) {       border,          0, 14, 81 };
    *v++ = (struct vertex) { scw - border,          0, 29, 81 };
    
    /* bottom border right */
    *v++ = (struct vertex) { scw - border + 1, border - 1, 32, 30 };
    *v++ = (struct vertex) {              scw, border - 1, 41, 30 };
    *v++ = (struct vertex) { scw - border + 1,          0, 32, 81 };
    *v++ = (struct vertex) {              scw,          0, 41, 81 };
    
    /* left border */
    *v++ = (struct vertex) {          0, sch - barh,  2, 40 };
    *v++ = (struct vertex) { border - 1, sch - barh, 11, 40 };
    *v++ = (struct vertex) {          0,     border,  2, 71 };
    *v++ = (struct vertex) { border - 1,     border, 11, 71 };
    
    /* right border */
    *v++ = (struct vertex) { scw - border + 1, sch - barh,  2, 40 };
    *v++ = (struct vertex) {              scw, sch - barh, 11, 40 };
    *v++ = (struct vertex) { scw - border + 1,     border,  2, 71 };
    *v++ = (struct vertex) {              scw,     border, 11, 71 };

    /* smile */
    *v++ = (struct vertex) { scw / 2 - 13, sch - barh / 2 + 13,  86,  2 };
    *v++ = (struct vertex) { scw / 2 + 13, sch - barh / 2 + 13, 111,  2 };
    *v++ = (struct vertex) { scw / 2 - 13, sch - barh / 2 - 13,  86, 27 };
    *v++ = (struct vertex) { scw / 2 + 13, sch - barh / 2 - 13, 111, 27 };

    /*
    ox = border;
    oy = sch - barh;
    for (j = 0; j < h+1; j++) {
        for (i = 0; i < w+1; i++) {
            *v++ = (struct vertex) {           ox + i * tile,           oy - j * tile, 80, 30 };
            *v++ = (struct vertex) { ox + (i + 1) * tile - 1,           oy - j * tile, 95, 30 };
            *v++ = (struct vertex) {           ox + i * tile, oy - (j + 1) * tile + 1, 80, 45 };
            *v++ = (struct vertex) { ox + (i + 1) * tile - 1, oy - (j + 1) * tile + 1, 95, 45 };
        }
    }
    */

    /* map coordinates to [0, 1] */
    v = vertex_buffer;
    for (i = 0; i < vcount; i++) {
        v[i].x = 2.0 * v[i].x / (float)scw - 1.0;
        v[i].y = 2.0 * v[i].y / (float)sch - 1.0;
        v[i].tx = v[i].tx / 256.0;
        v[i].ty = v[i].ty / 256.0;
    }
}

static void
init(void)
{
    GLuint vertex_shader, fragment_shader;
    int ret, w, h, ch;
    GLenum fmt;
    void *image;

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

    shader = glCreateProgram();
    glAttachShader(shader, vertex_shader);
    glAttachShader(shader, fragment_shader);
    glLinkProgram(shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    GL_ERR("compile shader");

    /* VAO/VBO setup */

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    /* populate buffers */
    glBufferData(GL_ARRAY_BUFFER, vertex_buffer_size, NULL, GL_DYNAMIC_DRAW);

    /* shader attributes (layout) position and color */

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    /* unbind */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GL_ERR("create VAO/VBO");

    /* load texture */

    stbi_set_flip_vertically_on_load(true);
    image = stbi_load("tilemap.png", &w, &h, &ch, 0);
    if (!image) die("stbi_load: could not load image `%s`\n", "tilemap.png");

    switch (ch) {
    case 1: fmt = GL_RED; break;
    case 3: fmt = GL_RGB; break;
    case 5: fmt = GL_RGBA; break;
    default: die("unsupported channel count: %d", ch);;
    }

    /* texture setup */

    glActiveTexture(GL_TEXTURE0);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    uniform_tex0 = glGetUniformLocation(shader, "tex0");
    glUseProgram(shader);
    glUniform1i(uniform_tex0, 0);

    stbi_image_free(image);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void
teardown(void)
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &texture);
    glDeleteProgram(shader);

    GL_ERR("cleanup");

    SDL_DestroyWindow(g_window);
    SDL_Quit();

    free(vertex_buffer);
}

static void
render(void)
{
    /* bind buffers */
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindTexture(GL_TEXTURE_2D, texture);

    GL_ERR("bind buffers");

    /* update with data */
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_buffer_size, vertex_buffer);

    GL_ERR("update data");

    /* clear background */
    glClearColor(1.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    GL_ERR("clear background");

    /* draw */
    glUseProgram(shader);
    glDrawArrays(GL_TRIANGLES, 0, vertex_buffer_size / sizeof(*vertex_buffer));

    GL_ERR("draw");

    /* unbind buffers */
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GL_ERR("unbind buffers");
}

static void
die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	exit(1);
}

