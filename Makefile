all: main.c
	gcc `mysql_config --cflags` main.c `mysql_config --libs` -Wall
