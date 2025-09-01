#ifndef TILEMAP_H
#define TILEMAP_H

enum {
    TILE_SMILE_DEAD, TILE_SMILE_COOL, TILE_SMILE_SCARED,
    TILE_SMILE_HAPPY, TILE_SMILE_PRESSED,

    TILE_NUM_0, TILE_NUM_1, TILE_NUM_2,
    TILE_NUM_3, TILE_NUM_4, TILE_NUM_5,
    TILE_NUM_6, TILE_NUM_7, TILE_NUM_8,
    TILE_NUM_9,

    TILE_CELL_BOMB, TILE_CELL_BOMBRED, TILE_CELL_UNKOWN, TILE_CELL_EMPTY,
    TILE_CELL_FLAG, TILE_CELL_1, TILE_CELL_2, TILE_CELL_3, TILE_CELL_4,
    TILE_CELL_BOMBX, TILE_CELL_5, TILE_CELL_6, TILE_CELL_7, TILE_CELL_8,

    TILE_BAR_LEFT, TILE_BAR_MID, TILE_BAR_RIGHT,
    TILE_BOT_LEFT, TILE_BOT_MID, TILE_BOT_RIGHT,
};

struct tilecoords { unsigned char x1, y1, x2, y2, x3, y3, x4, y4; };

struct tilecoords tilecoords_get(int tile);

#endif

#ifdef TILEMAP_IMPLEMENTATION

static const struct tilecoords __tilemap_coords[] = {
/*   tile coordinates        {  x0, y0,  x1, y1,  x2,  y2,  x3,  y3 } */
    [TILE_SMILE_DEAD]      = {   2,  2,  28,  2,   2,  28,  28,  28 },
    [TILE_SMILE_COOL]      = {  30,  2,  56,  2,  30,  28,  56,  28 },
    [TILE_SMILE_SCARED]    = {  58,  2,  84,  2,  58,  28,  84,  28 },
    [TILE_SMILE_HAPPY]     = {  86,  2, 112,  2,  86,  28, 112,  28 },
    [TILE_SMILE_PRESSED]   = { 114,  2, 140,  2, 114,  28, 140,  28 },
    [TILE_NUM_0]           = {   2, 96,  15, 96,   2, 119,  15, 119 },
    [TILE_NUM_1]           = {  17, 96,  30, 96,  17, 119,  30, 119 },
    [TILE_NUM_2]           = {  32, 96,  45, 96,  32, 119,  55, 119 },
    [TILE_NUM_3]           = {  47, 96,  60, 96,  47, 119,  60, 119 },
    [TILE_NUM_4]           = {  62, 96,  75, 96,  62, 119,  75, 119 },
    [TILE_NUM_5]           = {  77, 96,  90, 96,  77, 119,  90, 119 },
    [TILE_NUM_6]           = {  92, 96, 105, 96,  92, 119, 105, 119 },
    [TILE_NUM_7]           = { 107, 96, 120, 96, 107, 119, 120, 119 },
    [TILE_NUM_8]           = { 122, 96, 135, 96, 122, 119, 135, 119 },
    [TILE_NUM_9]           = { 137, 96, 150, 96, 137, 119, 150, 119 },
    [TILE_CELL_BOMB]       = {  44, 30,  60, 30,  44,  46,  60,  46 },
    [TILE_CELL_BOMBRED]    = {  62, 30,  78, 30,  62,  46,  78,  46 },
    [TILE_CELL_UNKOWN]     = {  80, 30,  96, 30,  80,  46,  96,  46 },
    [TILE_CELL_EMPTY]      = {  98, 30, 114, 30,  98,  46, 114,  46 },
    [TILE_CELL_FLAG]       = { 116, 30, 132, 30, 116,  46, 132,  46 },
    [TILE_CELL_1]          = {  44, 48,  60, 48,  44,  64,  60,  64 },
    [TILE_CELL_2]          = {  62, 48,  78, 48,  62,  64,  78,  64 },
    [TILE_CELL_3]          = {  80, 48,  96, 48,  80,  64,  96,  64 },
    [TILE_CELL_4]          = {  98, 48, 114, 48,  98,  64, 114,  64 },
    [TILE_CELL_BOMBX]      = { 116, 48, 132, 48, 116,  64, 132,  64 },
    [TILE_CELL_5]          = {  44, 66,  60, 66,  44,  82,  60,  82 },
    [TILE_CELL_6]          = {  62, 66,  78, 66,  62,  82,  78,  82 },
    [TILE_CELL_7]          = {  80, 66,  96, 66,  80,  82,  96,  82 },
    [TILE_CELL_8]          = {  98, 66, 114, 66,  98,  82, 114,  82 },
    [TILE_FRAME_TOP_LEFT]  = {   2, 30,  12, 30,   2,  82,  12,  82 },
    [TILE_FRAME_TOP_MID]   = {  14, 30,  30, 30,  14,  82,  30,  82 },
    [TILE_FRAME_TOP_RIGHT] = {  32, 30,  42, 30,  32,  82,  42,  82 },
    [TILE_FRAME_BOT_LEFT]  = {   2, 84,  12, 84,   2,  94,  12,  94 },
    [TILE_FRAME_BOT_MID]   = {  14, 84,  30, 84,  14,  94,  30,  94 },
    [TILE_FRAME_BOT_RIGHT] = {  32, 84,  42, 84,  32,  94,  42,  94 },
};

struct tilecoords
tilecoords_get(int tile)
{
    return __tilemap_coords[tile];
}

#endif
