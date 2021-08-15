#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdlib>
#include <cassert>
#include <iostream>

using uint	= unsigned;
using u8	= uint8_t;
using u32	= uint32_t;

///zero-overhead defer mechanism in c++, neat
struct DeferDummy {};
template<typename F>
struct Deferrer
{
	F f;
	~Deferrer() { f(); }
};

template<typename F> Deferrer<F> operator*(DeferDummy, F f) { return {f}; }
#define LUX_DEFER_0(LINE) zz_defer##LINE
#define LUX_DEFER_1(LINE) LUX_DEFER_0(LINE)
#define LUX_DEFER auto LUX_DEFER_1(__LINE__) = DeferDummy { } *[&]()ate<typename F> Deferrer<F> operator*(DeferDummy, F f) { return {f}; }
#define DEFER_0(LINE) zz_defer##LINE
#define DEFER_1(LINE) DEFER_0(LINE)
/* use like this:
 * DEFER { some_stuff(); asdf(); };
 * 
 * this will cause some_stuff and asdf to get called when the scope ends
 * DEFER is a destructor so, first defer to be defined, is going to be the last to be
 * called
 */
#define DEFER auto DEFER_1(__LINE__) = DeferDummy { } *[&]()

#define ARRAY_LEN(x) (sizeof(x) / sizeof(*x))

struct Vec {
	int x, y;
};

enum Color {
	BLACK		= 0x0,
	BLUE		= 0x1,
	GREEN		= 0x2,
	CYAN		= 0x3,
	RED			= 0x4,
	MAGENTA		= 0x5,
	BROWN		= 0x6,
	LGRAY		= 0x7,
	DGRAY		= 0x8,
	LBLUE		= 0x9,
	LGREEN		= 0xA,
	LCYAN		= 0xB,
	LRED		= 0xC,
	LMAGENTA	= 0xD,
	YELLOW		= 0xE,
	WHITE		= 0xF
};

#define COL(fg, bg) ((fg << 4) | bg)
#define TILE(ch, fg, bg) {ch, COL(fg, bg)}

struct Color_Mapping {
	u8 r, g, b;
};

//based on DF Lee's Colour Scheme v2
Color_Mapping COLOR_PALLETTE[16] = {
	{ 21,  19,  15}, //BLACK
	{ 45,  90, 160}, //BLUE
	{ 80, 135,  20}, //GREEN
	{ 25, 140, 140}, //CYAN,
	{160,  20,  10}, //RED
	{135,  60, 130}, //MAGENTA
	{150,  75,  55}, //BROWN
	{178, 175, 172}, //LGRAY
	{116, 110, 113}, //DGRAY
	{105, 135, 225}, //LBLUE
	{125, 185,  55}, //LGREEN
	{ 60, 205, 190}, //LCYAN
	{220,  50,  20}, //LRED
	{190, 110, 185}, //LMAGENTA
	{230, 170,  30}, //YELLOW
	{232, 227, 232}, //WHITE
};

constexpr uint SCREEN_W = 80;
constexpr uint SCREEN_H = 45;

struct Screen_Tile {
	u8 val;
	u8 col; //4 bits fg, 4 bits bg
};

//@TODO add redraw flag
Screen_Tile		screen[SCREEN_H][SCREEN_W];
SDL_Texture*	tileset;
SDL_Renderer*	renderer;
bool			is_quitting = false;

void blit_screen() {
	for(uint y = 0; y < SCREEN_H; y++) {
		for(uint x = 0; x < SCREEN_W; x++) {
			auto* tile = &screen[y][x];

			Color bg = (Color)(tile->col & 0x0f);
			Color fg = (Color)((tile->col & 0xf0) >> 4);

			SDL_Rect src;
			src.x = (tile->val % 16) * 16;
			src.y = (tile->val / 16) * 16;
			src.w = 16;
			src.h = 16;

			SDL_Rect dst;
			dst.x = x * 16;
			dst.y = y * 16;
			dst.w = 16;
			dst.h = 16;

			Color_Mapping bg_cm = COLOR_PALLETTE[bg];

			//render bg color
			SDL_SetRenderDrawColor(renderer, bg_cm.r, bg_cm.g, bg_cm.b, 0xff);
			SDL_RenderFillRect(renderer, &dst);

			Color_Mapping fg_cm = COLOR_PALLETTE[fg];

			//render fg
			SDL_SetTextureColorMod(tileset, fg_cm.r, fg_cm.g, fg_cm.b);
			SDL_RenderCopy(renderer, tileset, &src, &dst);
		}
	}
}

struct Entity_Type {
	const char*	name;
	Screen_Tile	tile;
};

struct Entity {
	const Entity_Type*	type;
	Vec					pos;
};

size_t	entity_num = 0;
Entity	entities[0x1000];
uint	player_entity = 0;

Entity* get_entity(uint id) {
	if(id == 0) return NULL;
	id -= 1;
	if(entity_num <= id) return NULL;
	return &entities[id];
}

constexpr uint MAP_W = 80;
constexpr uint MAP_H = 45;

struct Map_Tile {
	Screen_Tile tile;
	bool		passable;
	uint		entity;
};

Map_Tile map[100][100];

bool is_pos_valid(Vec p) {
	return not (p.x < 0 || p.y < 0 || p.x >= MAP_W || p.y >= MAP_H);
}

Map_Tile read_map(Vec p) {
	const Map_Tile border = {{0xDB, 0xf0}, false};
	if(not is_pos_valid(p)) {
		return border;
	}
	return map[p.y][p.x];
}

void generate_map() {
	const Map_Tile floor = {{'.', 0xf0}, true};
	const Map_Tile wall  = {{'#', 0xf0}, false};
	for(uint y = 0; y < MAP_H; y++) {
		for(uint x = 0; x < MAP_W; x++) {
			if(rand() % 10 == 0) {
				map[y][x] = wall;
			} else {
				map[y][x] = floor;
			}
		}
	}
}

void render_map() {
	for(int y = -15; y <= 15; y++) {
		for(int x = -15; x <= 15; x++) {
			Vec screen_pos = {(int)SCREEN_W / 2 + x, (int)SCREEN_H / 2 + y};
			auto player_pos = get_entity(player_entity)->pos;
			auto map_tile = read_map({player_pos.x + x, player_pos.y + y});
			auto tile = map_tile.tile;
			if(map_tile.entity != 0) {
				Entity* entity = get_entity(map_tile.entity);
				tile = entity->type->tile;
			}
			screen[screen_pos.y][screen_pos.x] = tile;
		}
	}
}

void move_entity(uint id, Vec off) {
	assert(entity_num >= id);
	Entity* entity = get_entity(id);

	Vec pos = entity->pos;
	assert(map[pos.y][pos.x].entity == id);
	map[pos.y][pos.x].entity = 0;
	entity->pos.x += off.x;
	entity->pos.y += off.y;

	pos = entity->pos;
	assert(is_pos_valid(pos));
	map[pos.y][pos.x].entity = id;
}

void handle_input() {
	SDL_Event e;
    while(SDL_PollEvent(&e) != 0 ) {

        if(e.type == SDL_QUIT) {
            is_quitting = true;
        } else if(e.type == SDL_KEYDOWN) {
            switch(e.key.keysym.sym) {
                case SDLK_UP:
                move_entity(player_entity, {0, -1});
                break;

                case SDLK_DOWN:
                move_entity(player_entity, {0, 1});
                break;

                case SDLK_LEFT:
                move_entity(player_entity, {-1, 0});
                break;

                case SDLK_RIGHT:
                move_entity(player_entity, {1, 0});
                break;

                default: break;
            }
        }
    }
}

Entity_Type human = {
	"Human",
	TILE('H', YELLOW, BLACK),
};

uint spawn_entity(const Entity_Type* type, Vec pos) {
	assert(entity_num < ARRAY_LEN(entities));
	assert(is_pos_valid(pos));
	//@TODO what if entity already exists on this spot?

	entities[entity_num] = {type, pos};
	entity_num++;

	assert(map[pos.y][pos.x].entity == 0);
	map[pos.y][pos.x].entity = entity_num;

	return entity_num;
}

void spawn_player() {
	player_entity = spawn_entity(&human, {MAP_W / 2, MAP_H / 2});
}

//required for SDL on cygwin to work for some reason
#ifdef main
# undef main
#endif /* main */

int main()
{
	using std::cerr;
	using std::endl;

	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		cerr << "SDL_Init Error: " << SDL_GetError() << endl;
		return EXIT_FAILURE;
	}
	DEFER { SDL_Quit(); };

	SDL_Window* win = SDL_CreateWindow("Mahou", 100, 100, SCREEN_W*16, SCREEN_H*16, SDL_WINDOW_SHOWN);
	if (win == nullptr) {
		cerr << "SDL_CreateWindow Error: " << SDL_GetError() << endl;
		return EXIT_FAILURE;
	}
	DEFER { SDL_DestroyWindow(win); };

	renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == nullptr) {
		cerr << "SDL_CreateRenderer Error" << SDL_GetError() << endl;
		return EXIT_FAILURE;
	}
	DEFER { SDL_DestroyRenderer(renderer); };

	SDL_Surface* bmp = IMG_Load("curses_square_16x16.png");
	if (bmp == nullptr) {
		cerr << "SDL_LoadBMP Error: " << SDL_GetError() << endl;
		return EXIT_FAILURE;
	}
	DEFER { SDL_FreeSurface(bmp); };

	tileset = SDL_CreateTextureFromSurface(renderer, bmp);
	if (tileset == nullptr) {
		cerr << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << endl;
		return EXIT_FAILURE;
	}
	DEFER { SDL_DestroyTexture(tileset); };

	generate_map();
	spawn_player();

	while(not is_quitting) {
		handle_input();
		memset(screen, 0, sizeof(screen));
		render_map();

		SDL_RenderClear(renderer);
		blit_screen();

		SDL_RenderPresent(renderer);
		SDL_Delay(15);
	}

	return EXIT_SUCCESS;
}
