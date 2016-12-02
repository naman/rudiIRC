all:
	mkdir -p .groups
	mkdir -p .msg
	mkdir -p .files/naman .files/u3 .files/u2 .files/root .files/u1
	mkdir -p Downloads/naman Downloads/u3 Downloads/u2 Downloads/root Downloads/u1
	touch .logged_in
	gcc server.c -o server -lm -w -lpthread
	gcc client.c -o client -lm -w -lpthread

clean:
	rm client server
