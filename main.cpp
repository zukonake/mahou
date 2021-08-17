#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdlib>
#include <ctime>
#include <cassert>
#include <iostream>

using uint	= unsigned;
using u8	= uint8_t;
using u32	= uint32_t;
using u64	= uint64_t;

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

char log_buffer[0x400];

constexpr uint SCREEN_W = 80;
constexpr uint SCREEN_H = 45;

uint gameview_x = 0;
uint gameview_y = 0;
uint gameview_w = 45;
uint gameview_h = 45;

struct Screen_Tile {
	u8 val;
	u8 col; //4 bits fg, 4 bits bg
};

Screen_Tile		screen[SCREEN_H][SCREEN_W];
u64				screen_redraw[(SCREEN_H * SCREEN_W - 1) / 64 + 1];
SDL_Texture*	tileset;
SDL_Renderer*	renderer;
bool			is_quitting = false;

void blit_screen() {
	for(uint y = 0; y < SCREEN_H; y++) {
		for(uint x = 0; x < SCREEN_W; x++) {
			uint id = x + y * SCREEN_W;
			if(not (screen_redraw[id / 64] & (1ull << (id % 64)))) {
				continue;
			}
			screen_redraw[id / 64] &= ~(1ull << (id % 64));

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

void set_screen(Vec pos, Screen_Tile val) {
	if(memcmp(&screen[pos.y][pos.x], &val, sizeof(Screen_Tile)) != 0) {
		uint id = pos.x + pos.y * SCREEN_W;
		screen_redraw[id / 64] |= 1ull << (id % 64);
		screen[pos.y][pos.x] = val;
	}
}

void render_map() {
	Vec center = {(int)(gameview_x + gameview_w / 2), (int)(gameview_y + gameview_h / 2)};
	int bound_xl = -(int)gameview_w / 2;
	int bound_xh =  (int)gameview_w / 2;
	int bound_yl = -(int)gameview_h / 2;
	int bound_yh =  (int)gameview_h / 2;
	for(int y = bound_yl; y <= bound_yh; y++) {
		for(int x = bound_xl; x <= bound_xh; x++) {
			Vec screen_pos = {(int)center.x + x, (int)center.y + y};
			auto player_pos = get_entity(player_entity)->pos;
			auto map_tile = read_map({player_pos.x + x, player_pos.y + y});
			auto tile = map_tile.tile;
			if(map_tile.entity != 0) {
				Entity* entity = get_entity(map_tile.entity);
				tile = entity->type->tile;
			}
			set_screen(screen_pos, tile);
		}
	}
}

template<typename... Args>
void game_log(const char* fmt, Args... args) {
	uint len = snprintf(NULL, 0, fmt, args...) + 1;
	if(len > ARRAY_LEN(log_buffer)) len = ARRAY_LEN(log_buffer);
	memmove(log_buffer + len, log_buffer, ARRAY_LEN(log_buffer) - len);
	snprintf(log_buffer, len, fmt, args...);
}

void render_ui() {
	int bound_xh = gameview_w + gameview_x;
	for(int y = 0; y < SCREEN_H; y++) {
		set_screen({bound_xh, y}, TILE(0xBA, WHITE, BLACK));
	}
	Vec cursor = {bound_xh + 1, 0};
	for(const char* it = log_buffer; it < log_buffer + ARRAY_LEN(log_buffer); it++) {
		if(*it == 0 || *it == '\n') {
			for(; cursor.x < SCREEN_W; ++cursor.x) {
				set_screen(cursor, TILE(0x00, WHITE, BLACK));
			}
			cursor.x--;
		}

		if(cursor.x >= SCREEN_W) {
			cursor.x = bound_xh + 1;
			cursor.y++;
		}
		if(cursor.y >= SCREEN_H) {
			break;
		}
		set_screen(cursor, TILE((u8)*it, WHITE, BLACK));
		cursor.x++;
	}
}

void move_entity(uint id, Vec off) {
	assert(entity_num >= id);
	Entity* entity = get_entity(id);

	Vec pos = entity->pos;
	assert(map[pos.y][pos.x].entity == id);
	map[pos.y][pos.x].entity = 0;

	pos.x += off.x;
	pos.y += off.y;
	//assert(is_pos_valid(pos));

	//@TODO line collision test
	auto tile = read_map(pos);

	if(tile.passable && tile.entity == 0) {
		entity->pos = pos;
	} else {
		pos = entity->pos;
	}
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

Entity_Type goblin = {
	"Goblin",
	TILE('G', GREEN, BLACK),
};

uint spawn_entity(const Entity_Type* type, Vec pos) {
	assert(entity_num < ARRAY_LEN(entities));
	assert(is_pos_valid(pos));

	if(map[pos.y][pos.x].entity != 0) {
		return 0;
	}

	entities[entity_num] = {type, pos};
	entity_num++;

	map[pos.y][pos.x].entity = entity_num;

	return entity_num;
}

void spawn_player() {
	player_entity = spawn_entity(&human, {MAP_W / 2, MAP_H / 2});
}

void spawn_goblins() {
	uint num = 32;
	for(uint i = 0; i < num; ++i) {
		Vec pos = {rand() % MAP_W, rand() % MAP_H};
		spawn_entity(&goblin, pos);
	}
}

void simulate_goblins() {
	for(uint i = 0; i < entity_num; ++i) {
		uint id = i + 1;
		Entity* e = get_entity(id);
		if(e->type == &goblin) {
			bool x = rand() % 2;
			bool s = rand() % 2;
			Vec off = {0, 0};
			if(x) {
				off.x = s ? -1 : 1;
			} else {
				off.y = s ? -1 : 1;
			}
			move_entity(id, off);
		}
	}
}

//required for SDL on cygwin to work for some reason
#ifdef main
# undef main
#endif /* main */

int main()
{
	srand(time(NULL));
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
	spawn_goblins();
	memset(screen_redraw, 0xff, sizeof(screen_redraw));

	uint t = 0;
	while(not is_quitting) {
		handle_input();
		if(t % 15 == 0) {
			simulate_goblins();
		}
		render_map();
		render_ui();

		blit_screen();

		SDL_RenderPresent(renderer);
		SDL_Delay(15);
		t++;
	}

	return EXIT_SUCCESS;
}
