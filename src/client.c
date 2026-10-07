#include "common.h"
#include "msg_struct.h"
#include "command.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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