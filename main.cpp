#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdlib>
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
#define DEFER auto DEFER_1(__LINE__) = DeferDummy { } *[&]()
/* use like this:
 * DEFER { some_stuff(); asdf(); };
 * 
 * this will cause some_stuff and asdf to get called when the scope ends
 * DEFER is a destructor so, first defer to be defined, is going to be the last to be
 * called
 */

enum Color {
	BLACK,
	BLUE,
	GREEN,
	CYAN,
	RED,
	MAGENTA,
	BROWN,
	LGRAY,
	DGRAY,
	LBLUE,
	LGREEN,
	LCYAN,
	LRED,
	LMAGENTA,
	YELLOW,
	WHITE
};

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

constexpr uint SCREEN_W = 800/16;
constexpr uint SCREEN_H = 600/16;

struct Screen_Tile {
	u8 val;
	u8 col; //4 bits fg, 4 bits bg
};

Screen_Tile screen[SCREEN_H][SCREEN_W];
SDL_Texture*	tileset;
SDL_Renderer*	renderer;

void render_screen() {
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

	SDL_Window* win = SDL_CreateWindow("Mahou", 100, 100, 800, 600, SDL_WINDOW_SHOWN);
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


	for (int i = 0; i < 20; i++) {
		//clear screen
		memset(screen, 0, sizeof(screen));

		screen[0][0] = {'P', 0x17};
		screen[0][1] = {'o', 0x26};
		screen[0][2] = {'g', 0x35};
		screen[0][3] = {'g', 0x44};
		screen[0][4] = {'e', 0x53};
		screen[0][5] = {'r', 0x62};
		screen[0][6] = {'s', 0x71};

		SDL_RenderClear(renderer);
		render_screen();
		SDL_RenderPresent(renderer);
		SDL_Delay(100);
	}

	return EXIT_SUCCESS;
}
