// vérifier la taille de déclaration des tableaux
// s'occuper du nick_sender
// gestion de la date
// merge des codes serveurs et clients


#include "common.h"
#include "msg_struct.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_MESSAGE_SIZE 4096

int is_valid_nickname_char(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

int extract_nickname(const char *argument, char *nick_name) {
	size_t nick_len = strcspn(argument, "\r\n");
 
	if (nick_len == 0) {
		fprintf(stderr, "Nickname is empty\n");
		return 0;
	}
	if (nick_len >= NICK_LEN) { // gestion taille nickname (avec le '\0')
		fprintf(stderr, "Nickname too long\n");
		return 0;
	}
 
	for (size_t i = 0; i < nick_len; i++) { // gestion caractères spéciaux
		if (!is_valid_nickname_char(argument[i])) {
			fprintf(stderr, "Unexpected char in nickname !\n");
			return 0;
		}
	}

	memcpy(nick_name, argument, nick_len);
	nick_name[nick_len] = '\0';
	return 1;
}

int handle_nick_command(int socket_fd, struct message *s_message, char *message) {
	char nick_name[NICK_LEN];
 
	/* Format attendu : "/nick " suivi du pseudo. */
	if (strncmp(message, "/nick ", 6) != 0) {
		fprintf(stderr, "Usage: /nick <nickname>\n");
		return 1;
	}
 
	if (!extract_nickname(message + 6, nick_name)) {
		return 1;
	}
 
	s_message_completion(socket_fd, s_message, 0, "", NICKNAME_NEW, nick_name);
	return 1;
}
 
/* Traitement de /whois <pseudo>. Return 1 to keep running. */
int handle_whois_command(int socket_fd, struct message *s_message, char *message) {
	char nick_name[NICK_LEN];
 
	/* Format attendu : "/whois " suivi du pseudo. */
	if (strncmp(message, "/whois ", 7) != 0) {
		fprintf(stderr, "Usage: /whois <nickname>\n");
		return 1;
	}
 
	if (!extract_nickname(message + 7, nick_name)) {
		return 1;
	}
 
	s_message_completion(socket_fd, s_message, 0, "", NICKNAME_INFOS, nick_name);
	return 1;
}
 
	
int setup_connection(const char *server_ip, const char *server_port) {
	int socket_fd;
	int result;
	struct sockaddr_in server_address;

	printf("Using server IPv4 address %s.\n", server_ip);
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	result = inet_aton(server_ip, &server_address.sin_addr);
	if (result == 0) {
		fprintf(stderr, "Invalid IPv4 address: %s\n", server_ip);
		return -1;
	}

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	die(socket_fd, "socket");
	printf("TCP socket created.\n");

	server_address.sin_port = htons((unsigned short)atoi(server_port));
	result = connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address));
	die(result, "connect");
	printf("Connected to %s:%s.\n", inet_ntoa(server_address.sin_addr), server_port);
	return socket_fd;
}

/* Return 1 to keep running, or 0 if the server disconnects or sends an invalid message. */
int read_server_message(int socket_fd) {
    struct message s_message;
    char pld[MAX_MESSAGE_SIZE];

    if (read_from_socket(socket_fd, &s_message, sizeof(s_message)) == 0)
        return 0;

    if (s_message.pld_len < 0 || s_message.pld_len > MAX_MESSAGE_SIZE)
        return 0;

    if (s_message.pld_len > 0) {
        if (read_from_socket(socket_fd, pld, (size_t)s_message.pld_len) == 0)
            return 0;

        write(STDOUT_FILENO, pld, (size_t)s_message.pld_len);
    }

    return 1;
}

int s_message_completion(int socket_fd, struct message *s_message,
                         int pld_len, const char *nick_sender,
                         enum msg_type type, const char *infos) 
	{
	// complète la struct message
	memset(s_message, 0, sizeof(*s_message));
    s_message->pld_len = pld_len;
    s_message->type = type;

    snprintf(s_message->nick_sender, sizeof(s_message->nick_sender),
             "%s", nick_sender);
    snprintf(s_message->infos, sizeof(s_message->infos),
             "%s", infos);

    return write_in_socket(socket_fd, s_message, sizeof(*s_message)) != 0;
}

// Return 1 to keep running, or 0 when stdin closes or the user quits. 
int get_and_send_user_message(int socket_fd) {
	char message[MAX_MESSAGE_SIZE + 1];
	ssize_t bytes_read;
	int message_size;

	/* Read up to one message from stdin, then add a terminator for strcmp. */
	bytes_read = read(STDIN_FILENO, message, MAX_MESSAGE_SIZE);
	die(bytes_read, "read stdin");
	if (bytes_read == 0) {
		return 0;
	}

	message_size = bytes_read;
	message[message_size] = '\0';
	struct message * s_message = malloc(sizeof(struct message));
	
	// le NICK_SENDER n'est pas encore géré !!!
	
	if (strcmp(message, "/quit") == 0 || strcmp(message, "/quit\n") == 0) {
		s_message_completion(socket_fd, s_message, -1, "", 0, ""); // quit, detected by pld_len = -1
	}
	
	else if (strncmp(message, "/nick", 5) == 0) {
		char nick_name[NICK_LEN];
		char *nick_start;
		size_t nick_len;
	
		if (strncmp(message, "/nick ", 6) != 0) {
			fprintf(stderr, "Usage: /nick <nickname>\n");
			return 1;
		}
	
		/* Le pseudo s'étend jusqu'au retour à la ligne (ou jusqu'à la fin de la saisie). */
		nick_start = message + 6;
		nick_len = strcspn(nick_start, "\r\n");
	
		if (nick_len == 0) {
			fprintf(stderr, "Usage: /nick <nickname>\n");
			return 1;
		}
		if (nick_len >= NICK_LEN) { // gestion taille nickname (avec le '\0')
			fprintf(stderr, "Nickname too long\n");
			return 1;
		}
	
		for (size_t i = 0; i < nick_len; i++) { // gestion caractères spéciaux
			if (!is_valid_nickname_char(nick_start[i])) {
				fprintf(stderr, "Unexpected char in nickname !\n");
				return 1;
			}
		}
	
		memcpy(nick_name, nick_start, nick_len);
		nick_name[nick_len] = '\0';
	
		s_message_completion(socket_fd, s_message, 0, "", NICKNAME_NEW, nick_name);
		}
		
		else if (strcmp(message, "/who") == 0 || strcmp(message, "/who\n") == 0) {
			s_message_completion(socket_fd, s_message, 0, "", NICKNAME_LIST, ""); 
	}
	
	else if (strncmp(message, "/whois", 6) == 0) {
		char nick_name[NICK_LEN];
		if(message_size +1 > 6 + NICK_LEN){ // gestion taille nickname
			fprintf(stdout,"Nickname wanted is too long (spaces might be the reason)");
			return 1;
		}
		strcpy(nick_name, message + 6);
		for (int i = 0; i< (int)strlen(nick_name); i++){ // gestion caracères spéciaux
			if(!(((65 <= nick_name[i]) && (nick_name[i] <= 90)) || ((97 <= nick_name[i]) && (nick_name[i] <= 122)) || ((48 <= nick_name[i]) && (nick_name[i] <= 57)))){ // Ascii encoding for char check
				fprintf(stderr, "Unexpected char in pseudo !");
				return 1;
			}
		}
		s_message_completion(socket_fd, s_message, 0, "", NICKNAME_INFOS, nick_name); 
	}
	else if (strncmp(message, "/msgall", 7) == 0) {
		char message_to_send_all[MAX_MESSAGE_SIZE];
		message_to_send_all[MAX_MESSAGE_SIZE] = '\0';
		strcpy(message_to_send_all, message + 8); // pas de vérification à effectuer
		s_message_completion(socket_fd, s_message, MAX_MESSAGE_SIZE, "", BROADCAST_SEND, ""); // pas d'infos puis qu'on les envoies ensuite
		
		if (write_in_socket(socket_fd, &message_to_send_all, MAX_MESSAGE_SIZE) == 0) {
			return 0;
		}
	}
	else if (strncmp(message, "/msg", 4) == 0) {//
		char nickname[NICK_LEN];
		char *nick_start;
		char *nick_end;
		char *content;
		size_t nick_len;
		size_t content_len;

		if (strncmp(message, "/msg ", 5) != 0) {
			fprintf(stdout, "Usage: /msg <nickname> <message>\n");
			return 1;
		}

		nick_start = message + 5;
		nick_end = strchr(nick_start, ' ');
		if (nick_end == NULL) {
			fprintf(stdout, "Usage: /msg <nickname> <message>\n");
			return 1;
		}

		nick_len = (size_t)(nick_end - nick_start);
		if (nick_len == 0 || nick_len >= NICK_LEN) { // vérification que le pseudo tient dans NICK_LEN (avec le '\0')
			fprintf(stdout, "Incorrect nickname\n");
			return 1;
		}
		memcpy(nickname, nick_start, nick_len);
		nickname[nick_len] = '\0';

		/* Le contenu du message commence juste après l'espace qui suit le pseudo. */
		content = nick_end + 1;
		content_len = strlen(content);

		s_message_completion(socket_fd, s_message, (int)content_len, "", UNICAST_SEND, nickname);
		if (write_in_socket(socket_fd, content, content_len) == 0) {
			return 0;
		}
	}
	else {
		if (!s_message_completion(socket_fd, s_message, message_size, "", ECHO_SEND, "")) {
			free(s_message);
			return 0;
		}

		if (write_in_socket(socket_fd, message, (size_t)message_size) == 0) {
			free(s_message);
			return 0;
		}
	}

	return 1;
}

void client_poll_loop(int socket_fd) {
	struct pollfd watched[2];
	int running = 1;

	/* Initialize once; poll() fills revents after each call. */
	watched[0].fd = STDIN_FILENO;
	watched[0].events = POLLIN;
	watched[1].fd = socket_fd;
	watched[1].events = POLLIN;

	while (running) {
		int ready = poll(watched, 2, -1);
		die(ready, "poll");

		if ((watched[1].revents & POLLIN) != 0) {
			running = read_server_message(socket_fd);
		}

		if (running && (watched[0].revents & POLLIN) != 0) {
			running = get_and_send_user_message(socket_fd);
		}

		if ((watched[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 || (watched[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			running = 0;
		}
	}
}

int main(int argc, char **argv) {
	int socket_fd;

	if (argc != 3) {
		fprintf(stderr, "Usage: ./client <server_ipv4> <server_port>\n");
		return EXIT_FAILURE;
	}
	socket_fd = setup_connection(argv[1], argv[2]);
	if (socket_fd < 0) {
		return EXIT_FAILURE;
	}
	client_poll_loop(socket_fd);
	close(socket_fd);
	return EXIT_SUCCESS;
}