build:
	@g++ -o build/main.out -lncurses src/main.cpp

debug:
	@g++ -g -o build/main.out -lncurses src/main.cpp && gdb ./main.out

run:
	@./build/main.out

valgrind: build
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./main.out 
