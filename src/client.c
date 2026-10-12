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
static char pending_file_path[MAX_MESSAGE_SIZE + 1];
static char pending_file_recipient[NICK_LEN];

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

//Reçoit le fichier sur la socket ouverte, FILE_SEND annonce une longueur, un FILE_SEND de longueur nulle annonce une fin de fichier


/*
 * Reçoit le fichier sur la socket dédiée. Chaque FILE_SEND annonce une
 * longueur qui est contrôlée avant toute lecture dans le tampon fixe.
 * Un FILE_SEND de longueur nulle marque la fin du fichier; l'accusé FILE_ACK
 * n'est envoyé qu'après l'affichage complet des octets reçus.
 */
int receive_file_transfer(int listen_fd, const char *filename) { // on peut peut-être l'initialiser en static...
	struct message chunk_header;
	char chunk[MAX_MESSAGE_SIZE];
	int transfer_fd;

	transfer_fd = accept(listen_fd, NULL, NULL);
	if (transfer_fd < 0) {
		perror("accept file transfer");
		return 0;
	}

	printf("Contenu reçu pour \"%s\" :\n", filename);
	//fflush(stdout);

	while(1) {
		if (read_from_socket(transfer_fd, &chunk_header, sizeof(chunk_header)) == 0) {
			fprintf(stderr, "Connexion interrompue avant la fin du fichier.\n");
			close(transfer_fd);
			return 0;
		}
		if (chunk_header.type != FILE_SEND || chunk_header.pld_len < 0 || chunk_header.pld_len > MAX_MESSAGE_SIZE) {
			fprintf(stderr, "Morceau de fichier invalide (type ou longueur).\n");
			close(transfer_fd);
			return 0;
		}
		if (chunk_header.pld_len == 0) {
			break;
		}

		if (read_from_socket(transfer_fd, chunk, (size_t)chunk_header.pld_len) == 0) {
			fprintf(stderr, "Connexion interrompue au milieu d'un morceau de fichier.\n");
			close(transfer_fd);
			return 0;
		}
		if (fwrite(chunk, 1, (size_t)chunk_header.pld_len, stdout) !=
		    (size_t)chunk_header.pld_len) {
			perror("write received file to stdout");
			close(transfer_fd);
			return 0;
		}
	}
	// if (fflush(stdout) == EOF) { // vide le tampon de réception jusqu'au bout et renvoie une erreur si non complet
	// 	perror("flush received file to stdout");
	// 	close(transfer_fd);
	// 	return 0;
	// }

	/*envoie de l'accusé de réception*/
	memset(&chunk_header, 0, sizeof(chunk_header));
	chunk_header.type = FILE_ACK;
	if (write_in_socket(transfer_fd, &chunk_header, sizeof(chunk_header)) == 0) {
		fprintf(stderr, "Impossible d'envoyer la confirmation du transfert.\n");
		close(transfer_fd);
		return 0;
	}
	printf("\nFin de la réception de \"%s\".\n", filename);
	close(transfer_fd);
	return 1;
}


//Traite un FILE_REQUEST reçue du serveur. 
// Réserve le port d'écoute avant le FILE_ACCEPT

int receiving_request(int socket_fd, const struct message *request, const char *filename) {
	char answer[32];
	size_t answer_length;
	char answer_character;
	ssize_t bytes_read;
	struct message response;
	int accepted;
	unsigned short port = 0;
	char port_payload[16];
	char reject_payload[MAX_MESSAGE_SIZE];
	const char *payload;
	size_t payload_length;

	printf("\"%s\" wants you to accept the transfer of the file named \"%s\". Do you accept? [Y/N].\n",
	       request->nick_sender, filename);

	while(1) {
		
		// Lit stdin avec read() dans la boucle poll.
		// lecture caractère par caractère
		answer_length = 0;
		while(1) {
			bytes_read = read(STDIN_FILENO, &answer_character, 1);
			if (bytes_read < 0) {
				perror("read transfer decision");
				return 0;
			}
			if (bytes_read == 0) {
				return 0;
			}
			if (answer_character == '\n') {
				break;
			}
			if (answer_length + 1 < sizeof(answer)) {
				answer[answer_length++] = answer_character;
			}
		}
		answer[answer_length] = '\0';
		if (answer_length == 0) {
			fprintf(stderr, "Please answer Y or N.\n");
			continue;
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
		if (accepted && file_listen_fd >= 0) {
			close(file_listen_fd);
			file_listen_fd = -1;
		}
		return 0;
	}
	if (accepted) {
		int transfer_succeeded = receive_file_transfer(file_listen_fd, filename);
		close(file_listen_fd);
		file_listen_fd = -1;
		if (!transfer_succeeded) {
			fprintf(stderr, "Le transfert de fichier a échoué.\n");
		}
	}
	return 1;
}

/*
 * Ouvre le fichier demandé, envoie son contenu par trames FILE_SEND d'au plus
 * MAX_MESSAGE_SIZE octets, puis attend un FILE_ACK sans charge utile avant de
 * fermer la connexion directe. L'adresse IP vient du serveur et le port de la
 * réponse FILE_ACCEPT.
 */
static int send_file_to_peer(const struct message *accept_message,
                             const char *port_payload) {
	struct sockaddr_in peer_address;
	struct message chunk_header;
	char chunk[MAX_MESSAGE_SIZE];
	char *port_end;
	unsigned long port;
	const char *port_character;
	FILE *file;
	int transfer_fd;

	if (pending_file_path[0] == '\0' ||
	    strcmp(accept_message->nick_sender, pending_file_recipient) != 0) {
		fprintf(stderr, "Réponse FILE_ACCEPT inattendue; aucun fichier en attente.\n");
		return 1;
	}
	port = strtoul(port_payload, &port_end, 10);
	for (port_character = port_payload; *port_character != '\0'; port_character++) {
		if (*port_character < '0' || *port_character > '9') {
			break;
		}
	}
	if (port_payload[0] == '\0' || *port_end != '\0' ||
	    *port_character != '\0' || port == 0 || port > 65535) {
		fprintf(stderr, "Port de transfert invalide.\n");
		pending_file_path[0] = '\0';
		pending_file_recipient[0] = '\0';
		return 1;
	}

	memset(&peer_address, 0, sizeof(peer_address));
	peer_address.sin_family = AF_INET;
	peer_address.sin_port = htons((unsigned short)port);
	if (inet_pton(AF_INET, accept_message->infos, &peer_address.sin_addr) != 1) {
		fprintf(stderr, "Adresse IPv4 de transfert invalide : %s\n", accept_message->infos);
		pending_file_path[0] = '\0';
		pending_file_recipient[0] = '\0';
		return 1;
	}

	file = fopen(pending_file_path, "rb");
	if (file == NULL) {
		perror("open file for transfer");
		pending_file_path[0] = '\0';
		pending_file_recipient[0] = '\0';
		return 1;
	}
	transfer_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (transfer_fd < 0) {
		perror("socket for file transfer");
		fclose(file);
		pending_file_path[0] = '\0';
		pending_file_recipient[0] = '\0';
		return 1;
	}
	if (connect(transfer_fd, (struct sockaddr *)&peer_address, sizeof(peer_address)) < 0) {
		perror("connect for file transfer");
		close(transfer_fd);
		fclose(file);
		pending_file_path[0] = '\0';
		pending_file_recipient[0] = '\0';
		return 1;
	}

	printf("Envoi de \"%s\" à %s...\n", pending_file_path, pending_file_recipient);
	fflush(stdout);
	while (!feof(file)) {
		size_t chunk_length = fread(chunk, 1, sizeof(chunk), file);
		if (ferror(file)) {
			perror("read file for transfer");
			close(transfer_fd);
			fclose(file);
			pending_file_path[0] = '\0';
			pending_file_recipient[0] = '\0';
			return 1;
		}
		if (chunk_length == 0) {
			break;
		}

		memset(&chunk_header, 0, sizeof(chunk_header));
		chunk_header.type = FILE_SEND;
		chunk_header.pld_len = (int)chunk_length;
		if (write_in_socket(transfer_fd, &chunk_header, sizeof(chunk_header)) == 0 ||
		    write_in_socket(transfer_fd, chunk, chunk_length) == 0) {
			fprintf(stderr, "Échec de l'envoi d'un morceau du fichier.\n");
			close(transfer_fd);
			fclose(file);
			pending_file_path[0] = '\0';
			pending_file_recipient[0] = '\0';
			return 1;
		}
	}
	fclose(file);

	/* Une trame vide distingue sans ambiguïté la fin du fichier. */
	memset(&chunk_header, 0, sizeof(chunk_header));
	chunk_header.type = FILE_SEND;
	if (write_in_socket(transfer_fd, &chunk_header, sizeof(chunk_header)) == 0) {
		fprintf(stderr, "Impossible de signaler la fin du fichier.\n");
		close(transfer_fd);
		pending_file_path[0] = '\0';
		pending_file_recipient[0] = '\0';
		return 1;
	}

	if (read_from_socket(transfer_fd, &chunk_header, sizeof(chunk_header)) == 0 ||
	    chunk_header.type != FILE_ACK || chunk_header.pld_len != 0) {
		fprintf(stderr, "Confirmation de transfert invalide ou absente.\n");
		close(transfer_fd);
		pending_file_path[0] = '\0';
		pending_file_recipient[0] = '\0';
		return 1;
	}
	printf("Transfert de \"%s\" confirmé par %s.\n",
	       pending_file_path, pending_file_recipient);
	fflush(stdout);
	close(transfer_fd);
	pending_file_path[0] = '\0';
	pending_file_recipient[0] = '\0';
	return 1;
}

/*
 * Lit et valide l'en-tête serveur avant de lire son payload dans un tampon fixe.
 * Les FILE_REQUEST sont transmis à receiving_request; les FILE_ACCEPT déclenchent
 * l'envoi du fichier en attente. Les autres payloads sont affichés comme avant.
 */
int receive_and_print_server_message(int socket_fd) {
    struct message s_message;
    char pld[MAX_MESSAGE_SIZE + 1];

    if (read_from_socket(socket_fd, &s_message, sizeof(s_message)) == 0)
        return 0;

    if (s_message.pld_len < 0 || s_message.pld_len > MAX_MESSAGE_SIZE) {
		fprintf(stderr, "Longueur de message serveur invalide : %d\n", s_message.pld_len);
        return 0;
	}
	if (s_message.type == FILE_REQUEST && s_message.pld_len == 0) {
		fprintf(stderr, "Demande de transfert sans nom de fichier.\n");
		return 1;
	}

    if (s_message.pld_len > 0) {
        if (read_from_socket(socket_fd, pld, (size_t)s_message.pld_len) == 0)
            return 0;

		if (s_message.type == FILE_REQUEST) {
			pld[s_message.pld_len] = '\0';
			return receiving_request(socket_fd, &s_message, pld);
		}
		if (s_message.type == FILE_ACCEPT) {
			pld[s_message.pld_len] = '\0';
			return send_file_to_peer(&s_message, pld);
		}

        write(STDOUT_FILENO, pld, (size_t)s_message.pld_len);
		printf("\n");
		if (s_message.type == FILE_REJECT &&
		    strcmp(s_message.nick_sender, pending_file_recipient) == 0) {
			pending_file_path[0] = '\0';
			pending_file_recipient[0] = '\0';
		}
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
		if (pending_file_path[0] != '\0') {
			fprintf(stderr, "Une demande de transfert est déjà en attente.\n");
			return 1;
		}
		return send_send_message(socket_fd, &s_message, message,
		                         pending_file_path, sizeof(pending_file_path),
		                         pending_file_recipient, sizeof(pending_file_recipient));
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