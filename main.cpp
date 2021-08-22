#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdlib>
#include <ctime>
#include <cassert>
#include <unordered_map>
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

bool operator==(const Vec& a, const Vec& b) {
	return a.x == b.x && a.y == b.y;
}

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
#define TILE(ch, fg, bg) {(u8)(ch), COL((fg), (bg))}

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
uint gameview_w = 45; //@TODO this crashes if even LUL
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

template<typename... Args>
void game_log(const char* fmt, Args... args) {
	uint len = snprintf(NULL, 0, fmt, args...) + 1;
	if(len > ARRAY_LEN(log_buffer)) len = ARRAY_LEN(log_buffer);
	memmove(log_buffer + len, log_buffer, ARRAY_LEN(log_buffer) - len);
	snprintf(log_buffer, len, fmt, args...);
}

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
	uint				max_hp;
	uint				dmg;
};

struct Entity {
	const Entity_Type*	type;
	Vec					pos;
	uint				max_hp;
	int					hp;
	uint				dmg;
};

size_t	entity_num = 0;
Entity	entities[0x1000];
uint	player_entity = 0;

Entity* get_entity(uint id) {
	if(id == 0) return NULL;
	id -= 1;
	if(entity_num <= id) return NULL;
	Entity *e = &entities[id];
	if(e->type == NULL) {
		return NULL;
	}
	return e;
}

constexpr uint MAP_W = 80;
constexpr uint MAP_H = 45;

struct Map_Tile {
	Screen_Tile tile;
	bool		passable;
	uint		entity;
};

#define CHUNK_W 16
#define CHUNK_MAX_ENTITIES 1024

struct Map_Chunk {
	uint		last_access;

	Map_Tile	data[CHUNK_W][CHUNK_W];
};

class VecHasher {
public:
    size_t operator()(const Vec& p) const
    {
        return ((u64)p.x & 0xffffffff) | (((u64)p.y & 0xffffffff) << 32ull);
    }
};

uint game_turn_number = 0;
std::unordered_map<Vec, Map_Chunk*, VecHasher> map_chunks;

float fract(float x) {
	return x - floor(x);
}

float frand(float x, float y){
    return fract(sin(x * 12.9898f + y * 78.233) * 43758.5453f);
}

float lerp(float a, float b, float f) {
	return f * (b - a) + a;
}

float value_noise(float x, float y) {
	float lx = floor(x);
	float ly = floor(y);
	float fx = x - lx;
	float fy = y - ly;

	float v00 = frand(lx + 0.f, ly + 0.f);
	float v01 = frand(lx + 1.f, ly + 0.f);
	float v10 = frand(lx + 0.f, ly + 1.f);
	float v11 = frand(lx + 1.f, ly + 1.f);

	return lerp(lerp(v00, v01, fx), lerp(v10, v11, fx), fy);
}

float cerp(float a, float b, float c, float d, float f) {
    float p = (d - c) - (a - b);
    
    return f * (f * (f * p + ((a - b) - p)) + (c - a)) + b;
}

float cubic_noise(float x, float y) {
	float lx = floor(x);
	float ly = floor(y);
	float fx = x - lx;
	float fy = y - ly;

	float vy[4];
	for(uint iy = 0; iy < 4; ++iy) {
		float offy = (float)iy - 1.f;
		float vx[4];
		for(uint ix = 0; ix < 4; ++ix) {
			float offx = (float)ix - 1.f;
			vx[ix] = frand(lx + offx, ly + offy);
		}
		vy[iy] = cerp(vx[0], vx[1], vx[2], vx[3], fx);
	}

	return cerp(vy[0], vy[1], vy[2], vy[3], fy);
}

void generate_chunk(Vec p, Map_Chunk* chunk) {
	game_log("generating chunk %d, %d ...", p.x, p.y);
	chunk->last_access = game_turn_number;
	for(uint y = 0; y < CHUNK_W; ++y) {
		for(uint x = 0; x < CHUNK_W; ++x) {
			auto* tile = &chunk->data[y][x];
			tile->entity = 0;
			tile->passable = true;
			Vec mpos = {(int)(p.x * CHUNK_W + x), (int)(p.y * CHUNK_W + y)};
			float scale = 0.1f;
			float v = cubic_noise((float)mpos.x * scale, (float)mpos.y * scale);
			if(v < 0.5) {
				tile->tile = TILE(5, GREEN, BLACK);
			} else {
				tile->tile = TILE(',', YELLOW, BLACK);
			}
		}
	}
}

void unload_chunks() {
	if(game_turn_number < 10) {
		return;
	}
	for(auto it = map_chunks.begin(); it != map_chunks.end(); ) {
		auto p = it->first;
		auto* chunk = it->second;
		if(chunk->last_access < game_turn_number - 10) {
			it = map_chunks.erase(it);
			game_log("unloading chunk %d, %d ...", p.x, p.y);
			//unload_chunk(pos);
		} else ++it;
	}
}

Map_Chunk* guarantee_chunk(Vec p) {
	Map_Chunk* chunk;
	if(map_chunks.count(p) == 0) {
		chunk = new Map_Chunk();
		map_chunks[p] = chunk;

		generate_chunk(p, chunk);
	}
	chunk = map_chunks[p];
	chunk->last_access = game_turn_number;
	return chunk;
}

void split_pos(Vec p, Vec* chunk_p, Vec* inner_p) {
	assert(CHUNK_W == 16);
	chunk_p->x = p.x >> 4;
	chunk_p->y = p.y >> 4;
	inner_p->x = p.x & 0xf;
	inner_p->y = p.y & 0xf;
}

template<typename F>
Map_Tile* set_map(Vec p) {
	Vec cp;
	Vec ip;
	split_pos(p, &cp, &ip);
	auto* chunk = guarantee_chunk(cp);
	return &chunk->data[ip.y][ip.x];
}

Map_Tile read_map(Vec p) {
	static Vec cached_chunk_pos;
	static Map_Chunk* cached_chunk = NULL;

	Vec cp;
	Vec ip;
	split_pos(p, &cp, &ip);
	Map_Chunk* chunk;
	if(cached_chunk != NULL && cached_chunk_pos == cp) {
		chunk = cached_chunk;
	} else {
		chunk = guarantee_chunk(cp);
	}
	return chunk->data[ip.y][ip.x];
}

/*void generate_map() {
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
}*/

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

void attack_entity(uint att_id, uint def_id) {
	/*Entity* attacker = get_entity(att_id);
	Entity* defender = get_entity(def_id);
	assert(att_id != def_id);
	if(att_id == player_entity) {
		game_log("you hit %s", defender->type->name);
	}
	if(def_id == player_entity) {
		game_log("you got hit by %s", attacker->type->name);
	}
	defender->hp -= attacker->dmg;
	if(defender->hp < 0) {
		defender->hp = 0;
		game_log("%s died!", defender->type->name);
		map[defender->pos.y][defender->pos.x].entity = 0;
		defender->type = NULL;
	}*/
	//@TODO
}

void move_entity(uint id, Vec off) {
	assert(entity_num >= id);
	Entity* entity = get_entity(id);
	assert(entity != NULL);

	Vec pos = entity->pos;
	//assert(map[pos.y][pos.x].entity == id);
	//map[pos.y][pos.x].entity = 0;

	pos.x += off.x;
	pos.y += off.y;

	//@TODO line collision test
	auto tile = read_map(pos);

	/*if(tile.entity != 0) {
		attack_entity(id, tile.entity);
	}

	if(tile.passable && tile.entity == 0) {*/
		entity->pos = pos;
	/*} else {
		pos = entity->pos;
	}
	map[pos.y][pos.x].entity = id;*/
	//@TODO
}


Entity_Type human = {
	"Human",
	TILE('H', YELLOW, BLACK),
	.max_hp = 10,
	.dmg = 3,
};

Entity_Type goblin = {
	"Goblin",
	TILE('G', GREEN, BLACK),
	.max_hp = 7,
	.dmg = 2,
};

uint spawn_entity(const Entity_Type* type, Vec pos) {
	assert(entity_num < ARRAY_LEN(entities));

	/*if(map[pos.y][pos.x].entity != 0) {
		return 0;
	}*/

	auto* entity = &entities[entity_num];
	entity->type = type;
	entity->pos = pos;
	entity->max_hp = type->max_hp;
	entity->hp = entity->max_hp;
	entity->dmg = type->dmg;
	entity_num++;

	//map[pos.y][pos.x].entity = entity_num;

	return entity_num;
}

void spawn_player() {
	player_entity = spawn_entity(&human, {MAP_W / 2, MAP_H / 2});
}

void spawn_goblins() {
	uint num = 32;
	for(uint i = 0; i < num; ++i) {
		Vec pos = {(int)(rand() % MAP_W), (int)(rand() % MAP_H)};
		spawn_entity(&goblin, pos);
	}
}

/*void simulate_goblins() {
	for(uint i = 0; i < entity_num; ++i) {
		uint id = i + 1;
		Entity* e = get_entity(id);
		if(e != NULL) {
			if(e->hp < e->max_hp / 3) {
				auto* tile = &map[e->pos.y][e->pos.x].tile;
				tile->col &= 0x00;
				tile->col |= BLACK << 4;
				tile->col |= RED;
			}
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
}*/

void handle_input() {
	SDL_Event e;
    while(SDL_PollEvent(&e) != 0 ) {

        if(e.type == SDL_QUIT) {
            is_quitting = true;
        } else if(e.type == SDL_KEYDOWN) {
			//simulate_goblins();
			game_turn_number++;
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
            unload_chunks();
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

	//generate_map();
	spawn_player();
	//spawn_goblins();
	memset(screen_redraw, 0xff, sizeof(screen_redraw));

	uint t = 0;
	while(not is_quitting) {
		handle_input();
		render_map();
		render_ui();

		blit_screen();

		SDL_RenderPresent(renderer);
		SDL_Delay(15);
		t++;
	}

	return EXIT_SUCCESS;
}
