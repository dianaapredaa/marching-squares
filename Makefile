build: tema1_par.c
	gcc main.c helpers.c -o main -g -lm -lpthread -Wall -Wextra
clean:
	rm -rf main
