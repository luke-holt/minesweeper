#include <glad/glad.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_opengl.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TILEMAP_IMPLEMENTATION
#include "tilemap.h"

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
    t = state[3]; s = state[0];
    state[3] = state[2];
    state[2] = state[1];
    state[1] = s;
    t ^= t << 11; t ^= t >> 8;
    return state[0] = t ^ s ^ (s >> 19);
}
static inline float randf(void){ return (float)xorshift128(xorshift128_state) / (float)XORSHIFT128_RAND_MAX; }

#define GL_ERR(msg) check_gl_err(true, __LINE__, msg);
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
        default: str = "unknown gl error"; break;
        }
        printf("main.c:line(%d):glerr(0x%x):%s: -> %s\n", line, err, str, msg);
        error_occurred = true;
    }
    if (error_occurred) exit(1);
}

struct vertex { float x, y, tx, ty; };

enum {
    GAME_STATE_IDLE,
    GAME_STATE_ONGOING,
    GAME_STATE_WON,
    GAME_STATE_LOST,
};

struct gamestate {
    int state;            /* idle, ongoing, won, lost */
    int time;             /* game time in seconds */
    int rem;              /* remaining mines */
    int w, h;             /* width, height */
    int hot;              /* hot tile */
    bool infield;         /* mouse in frame */
    bool down;            /* mouse pressed */
    bool up;              /* mouse released */
    unsigned char *field; /* minefield */
    bool *bombs;          /* location of mines */
};

static void die(const char *fmt, ...);
static void render(void);
static void window_init(void);
static void teardown(void);
static void tilemap_init(int w, int h);
static void game_init(int w, int h, int nbomb);
static void game_update(void);
static void quad_update_texture(struct vertex *v, int tex);

/* GLOBAL DATA */
int scw, sch;

GLuint VAO, VBO, EBO, shader, texture, uniform_tex0;
SDL_Window *window;
SDL_GLContext glctx;

struct vertex *vertex_buffer;
int vertex_buffer_size = 0;

GLuint *index_buffer;
int index_buffer_size = 0;
int index_buffer_count = 0;

struct vertex *mine_vertices;
struct vertex *bomb_counter_vertices;
struct vertex *timer_vertices;
struct vertex *smile_vertices;

struct gamestate state;

int
main(int argc, char *argv[])
{
    bool ret, quit;
    SDL_Event e;
    int w, h, i;

    w = 9;
    h = 9;

    game_init(w, h, 10);
    tilemap_init(w, h);
    window_init();

    SDL_StartTextInput(window);

    quit = false;
    while (!quit) {

        /* input */

        while (SDL_PollEvent(&e)) {

            switch (e.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;

            case SDL_EVENT_MOUSE_MOTION:
                i = (((int)e.motion.x - 10) / 16) + (((int)e.motion.y - 52) / 16) * w;
                state.infield = (i >= 0 && i < (w * h));
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                i = (((int)e.motion.x - 10) / 16) + (((int)e.motion.y - 52) / 16) * w;
                state.infield = (i >= 0 && i < (w * h));
                state.down = (e.button.button == SDL_BUTTON_LEFT);
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                i = (((int)e.motion.x - 10) / 16) + (((int)e.motion.y - 52) / 16) * w;
                state.infield = (i >= 0 && i < (w * h));
                state.up = (e.button.button == SDL_BUTTON_LEFT);
                break;

            case SDL_EVENT_TEXT_INPUT:
                break;

            default:
                break;
            }
        }

        game_update();
        render();

        ret = SDL_GL_SwapWindow(window);
        sdl_err(ret);
    }

    ret = SDL_StopTextInput(window);
    sdl_err(ret);

    teardown();

    return 0;
}

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
    v[0] = (struct vertex) {          0,        sch };
    v[1] = (struct vertex) { border    ,        sch };
    v[2] = (struct vertex) {          0, sch - barh };
    v[3] = (struct vertex) { border    , sch - barh };
    quad_update_texture(v, TILE_FRAME_TOP_LEFT);
    v += 4;

    /* bar middle */
    v[0] = (struct vertex) {       border,        sch };
    v[1] = (struct vertex) { scw - border,        sch };
    v[2] = (struct vertex) {       border, sch - barh };
    v[3] = (struct vertex) { scw - border, sch - barh };
    quad_update_texture(v, TILE_FRAME_TOP_MID);
    v += 4;

    /* bar right */
    v[0] = (struct vertex) { scw - border,        sch };
    v[1] = (struct vertex) {          scw,        sch };
    v[2] = (struct vertex) { scw - border, sch - barh };
    v[3] = (struct vertex) {          scw, sch - barh };
    quad_update_texture(v, TILE_FRAME_TOP_RIGHT);
    v += 4;

    /* bottom border left */
    v[0] = (struct vertex) {      0, border };
    v[1] = (struct vertex) { border, border };
    v[2] = (struct vertex) {      0,      0 };
    v[3] = (struct vertex) { border,      0 };
    quad_update_texture(v, TILE_FRAME_BOT_LEFT);
    v += 4;
    
    /* bottom border middle */
    v[0] = (struct vertex) {       border, border };
    v[1] = (struct vertex) { scw - border, border };
    v[2] = (struct vertex) {       border,      0 };
    v[3] = (struct vertex) { scw - border,      0 };
    quad_update_texture(v, TILE_FRAME_BOT_MID);
    v += 4;
    
    /* bottom border right */
    v[0] = (struct vertex) { scw - border, border };
    v[1] = (struct vertex) {          scw, border };
    v[2] = (struct vertex) { scw - border,      0 };
    v[3] = (struct vertex) {          scw,      0 };
    quad_update_texture(v, TILE_FRAME_BOT_RIGHT);
    v += 4;
    
    /* left border */
    v[0] = (struct vertex) {      0, sch - barh };
    v[1] = (struct vertex) { border, sch - barh };
    v[2] = (struct vertex) {      0,     border };
    v[3] = (struct vertex) { border,     border };
    quad_update_texture(v, TILE_FRAME_SIDE_LEFT);
    v += 4;
    
    /* right border */
    v[0] = (struct vertex) { scw - border, sch - barh };
    v[1] = (struct vertex) {          scw, sch - barh };
    v[2] = (struct vertex) { scw - border,     border };
    v[3] = (struct vertex) {          scw,     border };
    quad_update_texture(v, TILE_FRAME_SIDE_RIGHT);
    v += 4;

    /* smile */
    smile_vertices = v;
    v[0] = (struct vertex) { scw / 2 - 13, sch - barh / 2 + 13 };
    v[1] = (struct vertex) { scw / 2 + 13, sch - barh / 2 + 13 };
    v[2] = (struct vertex) { scw / 2 - 13, sch - barh / 2 - 13 };
    v[3] = (struct vertex) { scw / 2 + 13, sch - barh / 2 - 13 };
    quad_update_texture(v, TILE_SMILE_COOL);
    v += 4;

    /* bomb counter */
    bomb_counter_vertices = v;
    v[0] = (struct vertex) { 16,      sch - 14 };
    v[1] = (struct vertex) { 29,      sch - 14 };
    v[2] = (struct vertex) { 16, sch - 14 - 23 };
    v[3] = (struct vertex) { 29, sch - 14 - 23 };
    quad_update_texture(v, TILE_NUM_0);
    v += 4;

    v[0] = (struct vertex) { 29, sch - 14 };
    v[1] = (struct vertex) { 42, sch - 14 };
    v[2] = (struct vertex) { 29, sch - 37 };
    v[3] = (struct vertex) { 42, sch - 37 };
    quad_update_texture(v, TILE_NUM_1);
    v += 4;

    v[0] = (struct vertex) { 42,      sch - 14 };
    v[1] = (struct vertex) { 55,      sch - 14 };
    v[2] = (struct vertex) { 42, sch - 14 - 23 };
    v[3] = (struct vertex) { 55, sch - 14 - 23 };
    quad_update_texture(v, TILE_NUM_2);
    v += 4;
    
    /* timer */
    timer_vertices = v;
    v[0] = (struct vertex) { scw - 55,      sch - 14 };
    v[1] = (struct vertex) { scw - 42,      sch - 14 };
    v[2] = (struct vertex) { scw - 55, sch - 14 - 23 };
    v[3] = (struct vertex) { scw - 42, sch - 14 - 23 };
    quad_update_texture(v, TILE_NUM_3);
    v += 4;

    v[0] = (struct vertex) { scw - 42,      sch - 14 };
    v[1] = (struct vertex) { scw - 29,      sch - 14 };
    v[2] = (struct vertex) { scw - 42, sch - 14 - 23 };
    v[3] = (struct vertex) { scw - 29, sch - 14 - 23 };
    quad_update_texture(v, TILE_NUM_4);
    v += 4;

    v[0] = (struct vertex) { scw - 29,      sch - 14 };
    v[1] = (struct vertex) { scw - 16,      sch - 14 };
    v[2] = (struct vertex) { scw - 29, sch - 14 - 23 };
    v[3] = (struct vertex) { scw - 16, sch - 14 - 23 };
    quad_update_texture(v, TILE_NUM_5);
    v += 4;

    /* mines */
    mine_vertices = v;
    ox = border;
    oy = sch - barh;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            v[0] = (struct vertex) {       ox + i * tile,       oy - j * tile };
            v[1] = (struct vertex) { ox + (i + 1) * tile,       oy - j * tile };
            v[2] = (struct vertex) {       ox + i * tile, oy - (j + 1) * tile };
            v[3] = (struct vertex) { ox + (i + 1) * tile, oy - (j + 1) * tile };
            quad_update_texture(v, TILE_CELL_UNKNOWN);
            v += 4;
        }
    }

    /* map vertex coordinates to gl space */
    v = vertex_buffer;
    for (i = 0; i < vcount; i++) {
        v[i].x = v[i].x / (float)scw * 2.0 - 1.0;
        v[i].y = v[i].y / (float)sch * 2.0 - 1.0;
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
window_init(void)
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

    window = SDL_CreateWindow("minesweeper", scw, sch, SDL_WINDOW_OPENGL);
    sdl_err(window != NULL);

    glctx = SDL_GL_CreateContext(window);
    sdl_err(glctx != NULL);

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

    /* populate buffers */

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertex_buffer_size, NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_buffer_size, NULL, GL_DYNAMIC_DRAW);

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

    SDL_DestroyWindow(window);
    SDL_Quit();

    free(vertex_buffer);
    free(index_buffer);
    free(state.field);
}

static void
render(void)
{
    /* update vbo */
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_buffer_size, vertex_buffer);
    GL_ERR("update vbo");

    /* update ebo */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, index_buffer_size, index_buffer);
    GL_ERR("update ebo");

    /* clear background */
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    GL_ERR("clear color");

    /* draw */
    glUseProgram(shader);
    glBindVertexArray(VAO);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawElements(GL_TRIANGLES, index_buffer_count, GL_UNSIGNED_INT, NULL);
    GL_ERR("draw elements");

    /* unbind buffers */
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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

static void
game_init(int w, int h, int nbomb)
{
    int i;
    state.state = GAME_STATE_IDLE;
    state.time = 0;
    state.rem = nbomb;
    state.w = w;
    state.h = h;
    state.hot = 0;
    state.down = false;
    state.field = malloc(sizeof(*state.field) * w * h);
    state.bombs = malloc(sizeof(*state.bombs) * w * h);
    if (!state.field || !state.bombs) die("malloc failed\n");
    memset(state.field, TILE_CELL_UNKNOWN, sizeof(*state.field) * w * h);
    memset(state.bombs, TILE_CELL_UNKNOWN, sizeof(*state.bombs) * w * h);
    do {
        i = (randf() * (w * h - 1));
        if (state.bombs[i]) {
            state.bombs[i] = true;
            nbomb--;
        }
    } while (nbomb);
}

static void
game_update(void)
{
    int i;

    /* TODO(luke) continue writing game logic */

    if (state.up) {
        state.up = false;
        state.down = false;
    }

    if (state.down) {
        if (state.infield) {
            quad_update_texture(mine_vertices + state.hot * 4, TILE_CELL_EMPTY);
        }
    }

    for (i = 0; i < state.w * state.h; i++) {
        quad_update_texture(mine_vertices + i * 4, state.field[i]);
    }

    if (state.infield && state.up) {
        state.up = false;

        if (state.bombs[state.hot]) {
            state.state = GAME_STATE_LOST;
            state.field[state.hot] = TILE_CELL_BOMB;
        }

        quad_update_texture(mine_vertices + state.hot * 4, TILE_CELL_EMPTY);

        state.field[state.hot] = TILE_CELL_EMPTY;
    }

    quad_update_texture(smile_vertices, TILE_SMILE_HAPPY);

    quad_update_texture(bomb_counter_vertices + 0, TILE_NUM_0);
    quad_update_texture(bomb_counter_vertices + 4, TILE_NUM_0);
    quad_update_texture(bomb_counter_vertices + 8, TILE_NUM_0);

    quad_update_texture(timer_vertices + 0, TILE_NUM_0);
    quad_update_texture(timer_vertices + 4, TILE_NUM_0);
    quad_update_texture(timer_vertices + 8, TILE_NUM_0);
}

static void
quad_update_texture(struct vertex *v, int tex)
{
    struct tilecoords tc;
    tc = tilemap_get_tilecoords(tex);
    v[0].tx = (float)tc.x0 / 256.0; v[0].ty = (float)tc.y0 / 256.0;
    v[1].tx = (float)tc.x1 / 256.0; v[1].ty = (float)tc.y1 / 256.0;
    v[2].tx = (float)tc.x2 / 256.0; v[2].ty = (float)tc.y2 / 256.0;
    v[3].tx = (float)tc.x3 / 256.0; v[3].ty = (float)tc.y3 / 256.0;
}
