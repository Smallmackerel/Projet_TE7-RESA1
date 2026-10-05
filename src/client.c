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

// valide un pseudo
int is_valid_nickname_character(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

// valide un pseudo et le copie
int validate_and_copy_nickname(const char *source, size_t nickname_length, char *nickname_destination) {
	if (nickname_length == 0) {
		fprintf(stderr, "Nickname is empty\n");
		return 0;
	}
	if (nickname_length >= NICK_LEN) { // gestion taille nickname (avec le '\0')
		fprintf(stderr, "Nickname too long\n");
		return 0;
	}

	for (size_t i = 0; i < nickname_length; i++) { // gestion caractères spéciaux
		if (!is_valid_nickname_character(source[i])) {
			fprintf(stderr, "Unexpected char in nickname !\n");
			return 0;
		}
	}

	memcpy(nickname_destination, source, nickname_length);
	nickname_destination[nickname_length] = '\0';
	return 1;
}

int extract_nickname_from_argument(const char *argument, char *nickname_destination) {
	return validate_and_copy_nickname(argument, strcspn(argument, "\r\n"), nickname_destination);
}

int connect_to_server(const char *server_ip, const char *server_port) {
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

int receive_and_print_server_message(int socket_fd) {
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

// Remplit la struct message (en-tête) puis l'envoie au serveur.

int fill_and_send_message_header(int socket_fd, struct message *s_message,
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

/* Détecte la commande exacte "/quit" (avec ou sans retour à la ligne). */
int is_quit_command(const char *message) {
	return strcmp(message, "/quit") == 0 || strcmp(message, "/quit\n") == 0;
}

/* Détecte la commande exacte "/who" (avec ou sans retour à la ligne). */
int is_who_command(const char *message) {
	return strcmp(message, "/who") == 0 || strcmp(message, "/who\n") == 0;
}

// envoie quit
int send_quit_request(int socket_fd, struct message *s_message) {
	fill_and_send_message_header(socket_fd, s_message, -1, "", 0, ""); // quit, detected by pld_len = -1
	return 0;
}

/* /nick <pseudo> : demande au serveur de changer le pseudo.
 * Return 1 to keep running. */
int send_nickname_change_request(int socket_fd, struct message *s_message, char *message) {
	char nickname[NICK_LEN];

	/* Format attendu : "/nick " suivi du pseudo. */
	if (strncmp(message, "/nick ", 6) != 0) {
		fprintf(stderr, "Usage: /nick <nickname>\n");
		return 1;
	}

	/* Le pseudo commence après "/nick " (6 caractères). */
	if (!extract_nickname_from_argument(message + 6, nickname)) {
		return 1;
	}

	fill_and_send_message_header(socket_fd, s_message, 0, "", NICKNAME_NEW, nickname);
	return 1;
}

/* /who : demande au serveur la liste des utilisateurs connectés.
 * Return 1 to keep running. */
int send_nickname_list_request(int socket_fd, struct message *s_message) {
	fill_and_send_message_header(socket_fd, s_message, 0, "", NICKNAME_LIST, ""); 
	return 1;
}

/* /whois <pseudo> : demande au serveur les informations sur un utilisateur.
 * Return 1 to keep running. */
int send_nickname_info_request(int socket_fd, struct message *s_message, char *message) {
	char nickname[NICK_LEN];

	/* Format attendu : "/whois " suivi du pseudo. */
	if (strncmp(message, "/whois ", 7) != 0) {
		fprintf(stderr, "Usage: /whois <nickname>\n");
		return 1;
	}

	/* Le pseudo commence après "/whois " (7 caractères). */
	if (!extract_nickname_from_argument(message + 7, nickname)) {
		return 1;
	}

	fill_and_send_message_header(socket_fd, s_message, 0, "", NICKNAME_INFOS, nickname);
	return 1;
}

/* /msgall <message> : envoie un message à tous les utilisateurs (broadcast).
 * Return 1 to keep running, or 0 on write failure. */
int send_broadcast_message(int socket_fd, struct message *s_message, char *message) {
	char *content;
	size_t content_length;

	/* Format attendu : "/msgall " suivi du message. */
	if (strncmp(message, "/msgall ", 8) != 0) {
		fprintf(stderr, "Usage: /msgall <message>\n");
		return 1;
	}

	/* Le contenu commence après "/msgall " (8 caractères). */
	content = message + 8; // le message est envoyé tel quel, seule sa longueur réelle compte
	content_length = strlen(content);

	/* Envoi de l'en-tête (avec la longueur du payload), puis du payload. */
	fill_and_send_message_header(socket_fd, s_message, (int)content_length, "", BROADCAST_SEND, ""); // pas d'infos puis qu'on les envoies ensuite
	
	if (write_in_socket(socket_fd, content, content_length) == 0) {
		return 0;
	}
	return 1;
}

/* /msg <pseudo> <message> : envoie un message privé à un utilisateur.
 * Return 1 to keep running, or 0 on write failure. */
int send_private_message(int socket_fd, struct message *s_message, char *message) {//
	char nickname[NICK_LEN];
	char *nickname_start;
	char *nickname_end;
	char *content;
	size_t content_length;

	/* Format attendu : "/msg " suivi du pseudo, d'un espace, puis du message. */
	if (strncmp(message, "/msg ", 5) != 0) {
		fprintf(stderr, "Usage: /msg <nickname> <message>\n");
		return 1;
	}

	/* Le pseudo s'étend de la fin de "/msg " jusqu'au premier espace suivant. */
	nickname_start = message + 5;
	nickname_end = strchr(nickname_start, ' ');
	if (nickname_end == NULL) {
		fprintf(stderr, "Usage: /msg <nickname> <message>\n");
		return 1;
	}

	if (!validate_and_copy_nickname(nickname_start, (size_t)(nickname_end - nickname_start), nickname)) { // vérification que le pseudo est valide et tient dans NICK_LEN
		return 1;
	}

	/* Le contenu du message commence juste après l'espace qui suit le pseudo. */
	content = nickname_end + 1;
	content_length = strlen(content);

	/* Envoi de l'en-tête (pseudo destinataire dans infos), puis du payload. */
	fill_and_send_message_header(socket_fd, s_message, (int)content_length, "", UNICAST_SEND, nickname);
	if (write_in_socket(socket_fd, content, content_length) == 0) {
		return 0;
	}
	return 1;
}

/* Message sans commande : envoyé au serveur qui le renvoie en écho.
 * Return 1 to keep running, or 0 on write failure. */
int send_echo_message(int socket_fd, struct message *s_message, char *message, int message_size) {
	if (!fill_and_send_message_header(socket_fd, s_message, message_size, "", ECHO_SEND, "")) {
		return 0;
	}

	if (write_in_socket(socket_fd, message, (size_t)message_size) == 0) {
		return 0;
	}
	return 1;
}

// gère les entrées et détecte les commandes
int read_user_input_and_send_to_server(int socket_fd) {
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
	struct message s_message;
	
	// le NICK_SENDER n'est pas encore géré !!!
	
	/* Dispatch : le premier test qui correspond détermine la commande. */
	if (is_quit_command(message)) {
		return send_quit_request(socket_fd, &s_message);
	}
	
	else if (strncmp(message, "/nick", 5) == 0) {
		return send_nickname_change_request(socket_fd, &s_message, message);
	}
	
	else if (is_who_command(message)) {
		return send_nickname_list_request(socket_fd, &s_message);
	}
	
	else if (strncmp(message, "/whois", 6) == 0) {
		return send_nickname_info_request(socket_fd, &s_message, message);
	}
	else if (strncmp(message, "/msgall", 7) == 0) { // testé avant "/msg" car "/msgall" commence par "/msg"
		return send_broadcast_message(socket_fd, &s_message, message);
	}
	else if (strncmp(message, "/msg", 4) == 0) {//
		return send_private_message(socket_fd, &s_message, message);
	}
	else {
		return send_echo_message(socket_fd, &s_message, message, message_size);
	}
}

void run_client_poll_loop(int socket_fd) {
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

		/* Données du serveur à afficher. */
		if ((watched[1].revents & POLLIN) != 0) {
			running = receive_and_print_server_message(socket_fd);
		}

		/* Saisie de l'utilisateur à envoyer. */
		if (running && (watched[0].revents & POLLIN) != 0) {
			running = read_user_input_and_send_to_server(socket_fd);
		}

		/* Erreur ou fermeture sur stdin ou sur la socket : on arrête. */
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
	socket_fd = connect_to_server(argv[1], argv[2]);
	if (socket_fd < 0) {
		return EXIT_FAILURE;
	}
	run_client_poll_loop(socket_fd);
	close(socket_fd);
	return EXIT_SUCCESS;
}