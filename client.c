#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef BYTES_READ
#define BYTES_READ 512
#endif

#ifndef ALLOWED_CHAR_LENGTH
#define ALLOWED_CHAR_LENGTH 4096
#endif

#ifndef MAX_SIZE
#define MAX_SIZE 20000
#endif

#ifndef PORT_X
#define PORT_X 8000
#endif

#ifndef PORT_Y
#define PORT_Y 8001
#endif

#ifndef V
#define V 1
#endif

typedef int bool;
#define true 1
#define false 0

pthread_t worker_threads[2];
char username[ALLOWED_CHAR_LENGTH];

int listenFlag = 0;

void check_username_length(char* username) {
	if (strlen(username) >= 32) {
		errno = E2BIG;
		perror("Username has to be less than 32 bytes!");
	}
}

void check_path_length(char* path) {
	if (strlen(path) >= PATH_MAX) //4096 bytes
	{
		errno = E2BIG;
		perror("Path has to be less than 4096 bytes!");
	}
}

void check_file_length(char* filename) {
	if (strlen(filename) >= NAME_MAX) {
		errno = E2BIG;
		perror("Path has to be less than 255 bytes!");
	}
}

char** splitline(char *line, char* delimiter) {
	int buffer = 64;
	int pos = 0;
	char **args = malloc(buffer * sizeof(char*));
	char *token;

	token = strtok(line, delimiter);
	while (token != NULL) {
		args[pos] = token;
		pos += 1;

		if (pos >= buffer) {
			buffer += buffer;
			args = realloc(args, buffer * sizeof(char*));
		}

		token = strtok(NULL, delimiter);
	}
	args[pos] = NULL;
	return args;
}

void sigproc()

{
	signal(SIGQUIT, sigproc);
	printf(" Trap. Quitting.\n");
	//	pthread_exit(-1);
	raise(SIGINT);
	// kill(getpid(), SIGINT);
	exit(-1);
}

int file_exists(char *path) {
	struct stat st;
	int result = stat(path, &st);
	return result;
}

int file_size(char *path) {
	struct stat st;
	stat(path, &st);
	int size = st.st_size;
	return size;
}

void strip_line_endings(char* input) {
	/* strip of /r and /n chars to take care of Windows, Unix, OSx line endings. */
	input[strcspn(input, "\r\n")] = 0;
}

void resolve_path(char* path) {
	char resolved_path[ALLOWED_CHAR_LENGTH];
	realpath(path, resolved_path);
	strcpy(path, resolved_path);
}

void init(char* username) {
	printf("\n\n================== Authenticating.... ==================\n");

	sleep(1);

	system("clear");

	printf("================== Welcome to rudiIRC! ==================\n\n");

	char *wd = NULL;
	wd = getcwd(wd, ALLOWED_CHAR_LENGTH);
	check_path_length(wd);

	printf("Hello! You are at %s\n\n", wd);
}

int read_dstmsg(char* msg) {
	char path[ALLOWED_CHAR_LENGTH];
	strcpy(path, ".msg/.");
	strcat(path, username);

	FILE* file_pointer;
	file_pointer = fopen(path, "r");

	if (file_pointer == NULL) {
		return -1;
	}

	char buffer[ALLOWED_CHAR_LENGTH];
	char * line = NULL;
	size_t len = 0;
	ssize_t read;

	int line_number = 0;

	int val = 0;
	while ((read = getline(&line, &len, file_pointer)) != -1) {
		strip_line_endings(line);
		if (line_number == 0) {
			val = atoi(line);
		} else if (line_number == 1) {
			strcpy(msg, line);
//			printf("%s\n", line);
		}
		line_number++;
	}

	if (line) {
		free(line);
	}
	fclose(file_pointer);
	return val;
}

int send_f(int socket, char* path, int size) {

	FILE * file_pointer;
	file_pointer = fopen(path, "rb");

	if (file_pointer == NULL) {
		errno = ECANCELED;
		printf("Can't access path %s\n", path);
		perror("Cannot open file!");
	}

	char * line = NULL;
	size_t len = 0;
	ssize_t read;

	char result[MAX_SIZE];

	while ((read = getline(&line, &len, file_pointer)) != -1) {
		strcat(result, line);
	}

	if (line) {
		free(line);
	}

	fclose(file_pointer);

	 printf("%s", result);
	send(socket, result, MAX_SIZE, 0);

	return 1;
}

int recv_f(int socket, char* filename, int size, char* dst_user) {

	char path[ALLOWED_CHAR_LENGTH];
	strcpy(path, "Downloads/");
	strcat(path, dst_user);
	strcat(path, "/");
	strcat(path, filename);

	FILE* file_pointer;
	file_pointer = fopen(path, "wb");

	if (file_pointer == NULL) {
		errno = ECANCELED;
		printf("Can't access path %s\n", path);
		perror("Cannot open file!");
		return -1;
	}

	strcpy(path, "");

	char file_buffer[MAX_SIZE];
	recv(socket, file_buffer, MAX_SIZE, 0);

	printf("recv \n");
	char *usage = "Usage %s, [options] ... ";
	fprintf(file_pointer, file_buffer, usage);

	fclose(file_pointer);
	return 1;
}

int send_file(int socket, char* path, int size) {

	if (file_exists(path) != 0) {
		errno = ECANCELED;
		printf("Can't access path %s\n", path);
		perror("Cannot open file!");
		return -1;
	}

	FILE* file_pointer;
	file_pointer = fopen(path, "rb");

	if (file_pointer == NULL) {
		printf("Can't access path %s\n", path);
		perror("Cannot open file!");
		return -1;
	}

	char buffer[BYTES_READ];

	int i = BYTES_READ;
	while (i <= size) {

		fread(buffer, sizeof(char), BYTES_READ, file_pointer);

		usleep(300);
		send(socket, buffer, BYTES_READ, 0);

		strcpy(buffer, "");
		i += BYTES_READ;

		fseek(file_pointer, i, SEEK_SET);
	}
	puts("");
	fclose(file_pointer);

	return 1;
}

int recv_file(int socket, char* filename, int size, char* dst_user) {

	char path[ALLOWED_CHAR_LENGTH];
	strcpy(path, "Downloads/");
	strcat(path, dst_user);
	strcat(path, "/");
	strcat(path, filename);

	FILE* file_pointer;
	file_pointer = fopen(path, "wb");

	if (file_pointer == NULL) {
		errno = ECANCELED;
		printf("Can't access path %s\n", path);
		perror("Cannot open file!");
		return -1;
	}

	strcpy(path, "");

	char file_buffer[ALLOWED_CHAR_LENGTH];
	char buffer[BYTES_READ];

	int i = BYTES_READ;
	while (i <= size) {

		usleep(300);
		recv(socket, buffer, BYTES_READ, 0);
		strcat(file_buffer, buffer);

		strcpy(buffer, "");
//		sleep(1);

		i += BYTES_READ;
	}

	printf("bu\n");
	char *usage = "Usage %s, [options] ... ";
	fprintf(file_pointer, file_buffer, usage);

	fclose(file_pointer);
	return 1;
}

void reset_dstmsg() {
	char path[ALLOWED_CHAR_LENGTH];
	strcpy(path, ".msg/.");
	strcat(path, username);

	FILE* file_pointer;
	file_pointer = fopen(path, "w+");

	if (file_pointer == NULL) {
		return;
	}

	char buffer[ALLOWED_CHAR_LENGTH];
	strcpy(buffer, "0\n\n");

	char *usage = "Usage %s, [options] ... ";
	fprintf(file_pointer, buffer, usage);

	fclose(file_pointer);
}

void interface(char* username, int socket) {

	char m[ALLOWED_CHAR_LENGTH];
	if (read_dstmsg(m) == 1) {
		printf("Message: %s \n", m);
		strcpy(m, "\n");
		reset_dstmsg();
	}

	printf("$ ");

	char cmd[ALLOWED_CHAR_LENGTH];
	fgets(cmd, ALLOWED_CHAR_LENGTH, stdin);
	strip_line_endings(cmd);
	send(socket, cmd, ALLOWED_CHAR_LENGTH, 0);

	char success[ALLOWED_CHAR_LENGTH];
	recv(socket, success, ALLOWED_CHAR_LENGTH, 0);

	if (strcmp(success, "1") == 0) {
		// /who
		char result[ALLOWED_CHAR_LENGTH];
		recv(socket, result, ALLOWED_CHAR_LENGTH, 0);
		printf("%s", result);

	} else if (strcmp(success, "2") == 0) {
		// /msg

		printf("Message: ");
		char msg[ALLOWED_CHAR_LENGTH];
		fgets(msg, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(msg);

		send(socket, msg, ALLOWED_CHAR_LENGTH, 0);

		printf("Send to: ");
		char user[ALLOWED_CHAR_LENGTH];
		fgets(user, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(user);

		send(socket, user, ALLOWED_CHAR_LENGTH, 0);

		char rc[ALLOWED_CHAR_LENGTH];
		recv(socket, rc, ALLOWED_CHAR_LENGTH, 0);

	} else if (strcmp(success, "3") == 0) {
		// /create_grp

		printf("Name of the group: ");
		char grp[ALLOWED_CHAR_LENGTH];
		fgets(grp, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(grp);

		send(socket, grp, ALLOWED_CHAR_LENGTH, 0);

		char rc[ALLOWED_CHAR_LENGTH];
		recv(socket, rc, ALLOWED_CHAR_LENGTH, 0);
		printf("result %s", rc);

	} else if (strcmp(success, "4") == 0) {
		// /join_grp

		printf("Name of the group: ");
		char grp[ALLOWED_CHAR_LENGTH];
		fgets(grp, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(grp);

		send(socket, grp, ALLOWED_CHAR_LENGTH, 0);

		char rc[ALLOWED_CHAR_LENGTH];
		recv(socket, rc, ALLOWED_CHAR_LENGTH, 0);
		printf("result %s", rc);

	} else if (strcmp(success, "5") == 0) {
		// /send

		printf("Send to: ");
		char user[ALLOWED_CHAR_LENGTH];
		fgets(user, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(user);
		send(socket, user, ALLOWED_CHAR_LENGTH, 0);

		printf("Filename: ");
		char filename[ALLOWED_CHAR_LENGTH];
		fgets(filename, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(filename);
		send(socket, filename, ALLOWED_CHAR_LENGTH, 0);

		int size = file_size(filename);
		send(socket, &size, sizeof(int), 0);

		if (V != 1) {
			send_file(socket, filename, size);
		} else {
			printf("Sending!\n");
			send_f(socket, filename, size);
		}
		char rc[ALLOWED_CHAR_LENGTH];
		recv(socket, rc, ALLOWED_CHAR_LENGTH, 0);
		printf("result %s\n", rc);

	} else if (strcmp(success, "6") == 0) {
		// /msg_grp

		printf("Message: ");
		char msg[ALLOWED_CHAR_LENGTH];
		fgets(msg, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(msg);

		send(socket, msg, ALLOWED_CHAR_LENGTH, 0);

		printf("Name of the group: ");
		char grp[ALLOWED_CHAR_LENGTH];
		fgets(grp, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(grp);

		send(socket, grp, ALLOWED_CHAR_LENGTH, 0);

		char rc[ALLOWED_CHAR_LENGTH];
		recv(socket, rc, ALLOWED_CHAR_LENGTH, 0);
		printf("result %s", rc);

	} else if (strcmp(success, "7") == 0) {
		// /recv
		printf("Receiving pending file ....\n");
		printf("Which file do you wan to receive?\n");

		char filename[ALLOWED_CHAR_LENGTH];
		fgets(filename, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(filename);

		send(socket, filename, ALLOWED_CHAR_LENGTH, 0);

		int size = file_size(filename);

		if (V != 1) {
			recv_file(socket, filename, size, username);
		} else {
			recv_f(socket, filename, size, username);
		}

	} 
	// else {
		// printf("Command not supported!\n");
	// }
}

/*
 void *connection_handler1(void *connection_socket) {

 int* _socket = (int *) connection_socket;

 while (1) {
 interface(username, _socket);
 }

 return 0;
 }
 */

/*
 void *connection_handler(void *connection_socket) {

 int* _socket = (int *) connection_socket;
 // // where socketfd is the socket you want to make non-blocking
 // int status = fcntl(_socket, F_SETFL, fcntl(_socket, F_GETFL, 0) | O_NONBLOCK);
 //
 // if (status == -1){
 //   perror("calling fcntl");
 //   // handle the error.  By the way, I've never seen fcntl fail in this way
 // }

 char rc[ALLOWED_CHAR_LENGTH];
 while (1) {
 recv(_socket, rc, ALLOWED_CHAR_LENGTH, 0);
 printf("%s", rc);
 strcpy(rc, "");
 }
 return 0;
 }
 */

int main(int argc, char const *argv[]) {

// signal(SIGQUIT, sigproc);
	if (V != 1) {

		int client_socket_descriptor0;
		struct sockaddr_in server_address0;
		socklen_t address_size0;

		client_socket_descriptor0 = socket(PF_INET, SOCK_STREAM, 0);
		if (client_socket_descriptor0 == -1) {
			errno = EBADR;
			perror("Error creating socket!");
		} else {
			printf("socket initialized...\n");
		}

		server_address0.sin_family = AF_INET;
		server_address0.sin_addr.s_addr = inet_addr("127.0.0.1");
		server_address0.sin_port = htons(PORT_X);

		memset(server_address0.sin_zero, '\0',
				sizeof(server_address0.sin_zero));
		address_size0 = sizeof(server_address0);

		if (connect(client_socket_descriptor0,
				(struct sockaddr *) &server_address0, address_size0) < 0) {
			errno = ECONNREFUSED;
			perror("Error connecting to the server!\n");
			exit(-1);
		} else {
			printf("you are connected to the server...\n");
		}

		printf("Hello! Please enter your username to login.\n\n> ");
		fgets(username, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(username);

		printf("pw > ");
		char pw[ALLOWED_CHAR_LENGTH];
		fgets(pw, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(pw);

		check_username_length(username);

		if (send(client_socket_descriptor0, username, ALLOWED_CHAR_LENGTH, 0)
				< 0) {
			perror("Send failed");
			exit(-1);
		}

		char success0[ALLOWED_CHAR_LENGTH];

		if (recv(client_socket_descriptor0, success0, ALLOWED_CHAR_LENGTH, 0)
				< 0) {
			perror("Recv failed");
			exit(-1);
		}

		printf("You are %s\n", success0);
		close(client_socket_descriptor0);
	} else {
		printf("Phase 1 complete!\n");

		int client_socket_descriptor1;
		struct sockaddr_in server_address1;
		socklen_t address_size1;

		client_socket_descriptor1 = socket(PF_INET, SOCK_STREAM, 0);
		if (client_socket_descriptor1 == -1) {
			errno = EBADR;
			perror("Error creating socket!");
		} else {
			printf("socket initialized...\n");
		}

		server_address1.sin_family = AF_INET;
		server_address1.sin_addr.s_addr = inet_addr("127.0.0.1");
		server_address1.sin_port = htons(PORT_Y);

		memset(server_address1.sin_zero, '\0',
				sizeof(server_address1.sin_zero));
		address_size1 = sizeof(server_address1);

		if (connect(client_socket_descriptor1,
				(struct sockaddr *) &server_address1, address_size1) < 0) {
			errno = ECONNREFUSED;
			perror("Error connecting to the server!\n");
			exit(-1);
		} else {
			printf("you are connected to the server...\n");
		}

		printf("Hello! Please enter your username to login.\n> ");
		fgets(username, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(username);

		check_username_length(username);

		printf("Password: ");
		char pw[ALLOWED_CHAR_LENGTH];
		fgets(pw, ALLOWED_CHAR_LENGTH, stdin);
		strip_line_endings(pw);

		check_username_length(pw);

		if (strcmp(username, pw) != 0) {
			printf("wrong password!\n");
			exit(-1);
		}

		if (send(client_socket_descriptor1, username, ALLOWED_CHAR_LENGTH, 0)
				< 0) {
			perror("Send failed");
			exit(-1);
		}

		char success0[ALLOWED_CHAR_LENGTH];

		if (recv(client_socket_descriptor1, success0, ALLOWED_CHAR_LENGTH, 0)
				< 0) {
			perror("Recv failed");
			exit(-1);
		}

		printf("You are %s\n", success0);

		char success1[ALLOWED_CHAR_LENGTH];

		if (recv(client_socket_descriptor1, success1, ALLOWED_CHAR_LENGTH, 0)
				< 0) {
			perror("Recv failed");
			exit(-1);
		}

		strip_line_endings(success1);

		if (strcmp(success1, "connected") == 0) {

			printf("You are %s\n", success1);

			init(username);

			while (1) {
				interface(username, client_socket_descriptor1);
			}
//		pthread_create(&worker_threads[0], NULL, connection_handler,
//				(void *) client_socket_descriptor1);
//
//		pthread_create(&worker_threads[1], NULL, connection_handler1,
//				(void *) client_socket_descriptor1);
//
//		pthread_join(worker_threads[0], NULL);
//
//		pthread_join(worker_threads[1], NULL);
		} else {
			errno = ECONNREFUSED;
			perror("Invalid username! Please try again!\n");
			exit(-1);
		}

		close(client_socket_descriptor1);
	}
	return 0;
}
