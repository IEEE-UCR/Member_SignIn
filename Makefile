all: main.c
	gcc -I/usr/include/mysql -DBIG_JOINS=1 -fno-strict-aliasing -g -DNDEBUG -L/usr/lib/x86_64-linux-gnu -lmysqlclient -lpthread -lz -lm -ldl main.c
