.PHONY: run

run : jam.exe
	./jam.exe

jam.exe : main.cpp
	g++ -g -O0 $^ -o $@ -lSDL2 -lSDL2_image -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
