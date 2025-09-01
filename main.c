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

GLuint VAO, VBO, EBO, shader, texture, uniform_tex0;
SDL_Window *g_window;
SDL_GLContext g_gl_context;

struct vertex { float x, y, tx, ty; };
struct vertex *vertex_buffer;
int vertex_buffer_size = 0;

GLuint *index_buffer;
int index_buffer_size = 0;
int index_buffer_count = 0;

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
    TILE_CELL_BOMBX,
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
    int barh, border, tile, ox, oy, vcount, i, j, nquad;
    struct vertex *v;

    tile = 16;
    border = 10;
    barh = 52;
    scw = border * 2 + tile * w;
    sch = barh + tile * h + border;

    /* frame (8), smile (1), numbers (6) */
    nquad = 8 + 1 + 6 + h * w;

    /* 4 vertices per quad */
    vcount = nquad * 4;

    vertex_buffer_size = vcount * sizeof(*vertex_buffer);
    vertex_buffer = malloc(vertex_buffer_size);
    if (!vertex_buffer) die("couldn't allocate vertex buffer");

    /* 6 indices per quad */
    index_buffer_count = nquad * 6;
    index_buffer_size = index_buffer_count * sizeof(*index_buffer);
    index_buffer = malloc(index_buffer_size);
    if (!index_buffer) die("couldn't allocate index buffer");

    v = vertex_buffer;

    /* bar left */
    *v++ = (struct vertex) {          0,        sch,  2, 30 };
    *v++ = (struct vertex) { border    ,        sch, 12, 30 };
    *v++ = (struct vertex) {          0, sch - barh,  2, 82 };
    *v++ = (struct vertex) { border    , sch - barh, 12, 82 };

    /* bar middle */
    *v++ = (struct vertex) {       border,        sch, 14, 30 };
    *v++ = (struct vertex) { scw - border,        sch, 30, 30 };
    *v++ = (struct vertex) {       border, sch - barh, 14, 82 };
    *v++ = (struct vertex) { scw - border, sch - barh, 30, 82 };

    /* bar right */
    *v++ = (struct vertex) { scw - border,        sch, 32, 30 };
    *v++ = (struct vertex) {          scw,        sch, 42, 30 };
    *v++ = (struct vertex) { scw - border, sch - barh, 32, 82 };
    *v++ = (struct vertex) {          scw, sch - barh, 42, 82 };

    /* bottom border left */
    *v++ = (struct vertex) {      0, border,  2, 84 };
    *v++ = (struct vertex) { border, border, 12, 84 };
    *v++ = (struct vertex) {      0,      0,  2, 94 };
    *v++ = (struct vertex) { border,      0, 12, 94 };
    
    /* bottom border middle */
    *v++ = (struct vertex) {       border, border, 14, 84 };
    *v++ = (struct vertex) { scw - border, border, 29, 84 };
    *v++ = (struct vertex) {       border,      0, 14, 94 };
    *v++ = (struct vertex) { scw - border,      0, 29, 94 };
    
    /* bottom border right */
    *v++ = (struct vertex) { scw - border, border, 32, 84 };
    *v++ = (struct vertex) {          scw, border, 42, 84 };
    *v++ = (struct vertex) { scw - border,      0, 32, 94 };
    *v++ = (struct vertex) {          scw,      0, 42, 94 };
    
    /* left border */
    *v++ = (struct vertex) {      0, sch - barh,  2, 40 };
    *v++ = (struct vertex) { border, sch - barh, 12, 40 };
    *v++ = (struct vertex) {      0,     border,  2, 72 };
    *v++ = (struct vertex) { border,     border, 12, 72 };
    
    /* right border */
    *v++ = (struct vertex) { scw - border, sch - barh,  2, 40 };
    *v++ = (struct vertex) {          scw, sch - barh, 12, 40 };
    *v++ = (struct vertex) { scw - border,     border,  2, 72 };
    *v++ = (struct vertex) {          scw,     border, 12, 72 };

    /* smile */
    *v++ = (struct vertex) { scw / 2 - 13, sch - barh / 2 + 13,  86,  2 };
    *v++ = (struct vertex) { scw / 2 + 13, sch - barh / 2 + 13, 111,  2 };
    *v++ = (struct vertex) { scw / 2 - 13, sch - barh / 2 - 13,  86, 27 };
    *v++ = (struct vertex) { scw / 2 + 13, sch - barh / 2 - 13, 111, 27 };

    /* left numbers */
    *v++ = (struct vertex) {      16,      sch - 14, 2, 96 };
    *v++ = (struct vertex) { 16 + 13,      sch - 14, 15, 96 };
    *v++ = (struct vertex) {      16, sch - 14 - 23, 2, 119 };
    *v++ = (struct vertex) { 16 + 13, sch - 14 - 23, 15, 119 };

    *v++ = (struct vertex) {      29,      sch - 14, 2, 96 };
    *v++ = (struct vertex) { 29 + 13,      sch - 14, 15, 96 };
    *v++ = (struct vertex) {      29, sch - 14 - 23, 2, 119 };
    *v++ = (struct vertex) { 29 + 13, sch - 14 - 23, 15, 119 };

    *v++ = (struct vertex) {      42,      sch - 14, 2, 96 };
    *v++ = (struct vertex) { 42 + 13,      sch - 14, 15, 96 };
    *v++ = (struct vertex) {      42, sch - 14 - 23, 2, 119 };
    *v++ = (struct vertex) { 42 + 13, sch - 14 - 23, 15, 119 };
    
    /* right numbers */
    *v++ = (struct vertex) { scw - 55,      sch - 14, 2, 96 };
    *v++ = (struct vertex) { scw - 42,      sch - 14, 15, 96 };
    *v++ = (struct vertex) { scw - 55, sch - 14 - 23, 2, 119 };
    *v++ = (struct vertex) { scw - 42, sch - 14 - 23, 15, 119 };

    *v++ = (struct vertex) { scw - 42,      sch - 14, 2, 96 };
    *v++ = (struct vertex) { scw - 29,      sch - 14, 15, 96 };
    *v++ = (struct vertex) { scw - 42, sch - 14 - 23, 2, 119 };
    *v++ = (struct vertex) { scw - 29, sch - 14 - 23, 15, 119 };

    *v++ = (struct vertex) { scw - 29,      sch - 14, 2, 96 };
    *v++ = (struct vertex) { scw - 16,      sch - 14, 15, 96 };
    *v++ = (struct vertex) { scw - 29, sch - 14 - 23, 2, 119 };
    *v++ = (struct vertex) { scw - 16, sch - 14 - 23, 15, 119 };

    /* mines */
    ox = border;
    oy = sch - barh;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            *v++ = (struct vertex) {       ox + i * tile,       oy - j * tile, 80, 30 };
            *v++ = (struct vertex) { ox + (i + 1) * tile,       oy - j * tile, 96, 30 };
            *v++ = (struct vertex) {       ox + i * tile, oy - (j + 1) * tile, 80, 46 };
            *v++ = (struct vertex) { ox + (i + 1) * tile, oy - (j + 1) * tile, 96, 46 };
        }
    }

    /* map coordinates to gl space */
    v = vertex_buffer;
    for (i = 0; i < vcount; i++) {
        v[i].x = v[i].x / (float)scw * 2.0 - 1.0;
        v[i].y = v[i].y / (float)sch * 2.0 - 1.0;
        v[i].tx = v[i].tx / 256.0;
        v[i].ty = v[i].ty / 256.0;
    }

    /* setup indices */
    for (i = 0; i < nquad; i++) {
        index_buffer[i*6 + 0] = 0 + i*4;
        index_buffer[i*6 + 1] = 1 + i*4;
        index_buffer[i*6 + 2] = 2 + i*4;
        index_buffer[i*6 + 3] = 1 + i*4;
        index_buffer[i*6 + 4] = 2 + i*4;
        index_buffer[i*6 + 5] = 3 + i*4;
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

    /* build shader program */

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

    /* VAO/VBO/EBO setup */

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    /* populate buffers */

    glBufferData(GL_ARRAY_BUFFER, vertex_buffer_size, vertex_buffer, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_buffer_size, index_buffer, GL_STATIC_DRAW);

    /* shader attributes (layout) position and color */

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    /* unbind */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    GL_ERR("create VAO/VBO/EBO");

    /* load texture */

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
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texture);
    glDeleteProgram(shader);

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
    /*glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);*/
    glBindTexture(GL_TEXTURE_2D, texture);

    GL_ERR("bind buffers");

    /* update with data */
    /*
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_buffer_size, vertex_buffer);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, index_buffer_size, index_buffer);
    */

    GL_ERR("update data");

    /* clear background */
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    GL_ERR("clear background");

    /* draw */
    glUseProgram(shader);
    glDrawElements(GL_TRIANGLES, index_buffer_count, GL_UNSIGNED_INT, NULL);

    GL_ERR("draw");

    /* unbind buffers */
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    /*glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);*/
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

