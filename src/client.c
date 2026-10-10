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

static int file_listen_fd = -1;

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

static int open_file_listener(unsigned short *port) {
	struct sockaddr_in address;
	socklen_t address_length = sizeof(address);
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (listen_fd < 0) {
		perror("socket for file transfer");
		return -1;
	}

	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(0);
	if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind for file transfer");
		close(listen_fd);
		return -1;
	}
	if (listen(listen_fd, 1) < 0) {
		perror("listen for file transfer");
		close(listen_fd);
		return -1;
	}
	if (getsockname(listen_fd, (struct sockaddr *)&address, &address_length) < 0) {
		perror("getsockname for file transfer");
		close(listen_fd);
		return -1;
	}

	*port = ntohs(address.sin_port);
	return listen_fd;
}

int receiving_request(int socket_fd, const struct message *request, const char *filename) {
	char answer[32];
	struct message response;
	int accepted;
	unsigned short port = 0;
	char port_payload[16];
	char reject_payload[MAX_MESSAGE_SIZE];
	const char *payload;
	size_t payload_length;

	printf("\"%s\" wants you to accept the transfer of the file named \"%s\". Do you accept? [Y/N].\n",
	       request->nick_sender, filename);
	fflush(stdout);

	for (;;) {
		if (fgets(answer, sizeof(answer), stdin) == NULL) {
			return 0;
		}
		if (answer[0] == 'Y' || answer[0] == 'y') {
			accepted = 1;
			break;
		}
		if (answer[0] == 'N' || answer[0] == 'n') {
			accepted = 0;
			break;
		}
		fprintf(stderr, "Please answer Y or N.\n");
	}

	memset(&response, 0, sizeof(response));
	snprintf(response.nick_sender, sizeof(response.nick_sender), "%s", request->infos);
	snprintf(response.infos, sizeof(response.infos), "%s", request->nick_sender);
	if (accepted) {
		if (file_listen_fd < 0) {
			file_listen_fd = open_file_listener(&port);
			if (file_listen_fd < 0) {
				return 0;
			}
		} else {
			struct sockaddr_in address;
			socklen_t address_length = sizeof(address);
			if (getsockname(file_listen_fd, (struct sockaddr *)&address, &address_length) < 0) {
				perror("getsockname for file transfer");
				return 0;
			}
			port = ntohs(address.sin_port);
		}

		snprintf(port_payload, sizeof(port_payload), "%u", (unsigned int)port);
		payload = port_payload;
		payload_length = strlen(payload);
		response.type = FILE_ACCEPT;
	} else {
		snprintf(reject_payload, sizeof(reject_payload),
		         "\"%s\" doesn't want to accept your file.", request->infos);
		payload = reject_payload;
		payload_length = strlen(payload);
		response.type = FILE_REJECT;
	}
	response.pld_len = (int)payload_length;

	if (write_in_socket(socket_fd, &response, sizeof(response)) == 0 ||
	    write_in_socket(socket_fd, (void *)payload, payload_length) == 0) {
		return 0;
	}
	return 1;
}

int receive_and_print_server_message(int socket_fd) {
    struct message s_message;
    char pld[MAX_MESSAGE_SIZE + 1];

    if (read_from_socket(socket_fd, &s_message, sizeof(s_message)) == 0)
        return 0;

    if (s_message.pld_len < 0 || s_message.pld_len > MAX_MESSAGE_SIZE)
        return 0;

    if (s_message.pld_len > 0) {
        if (read_from_socket(socket_fd, pld, (size_t)s_message.pld_len) == 0)
            return 0;

		if (s_message.type == FILE_REQUEST) {
			pld[s_message.pld_len] = '\0';
			return receiving_request(socket_fd, &s_message, pld);
		}

        write(STDOUT_FILENO, pld, (size_t)s_message.pld_len);
		printf("\n");
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
	else if (strncmp(message, "/send", 5) == 0) {
		return send_send_message(socket_fd, &s_message, message);
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
	if (file_listen_fd >= 0) {
		close(file_listen_fd);
	}
	return EXIT_SUCCESS;
}