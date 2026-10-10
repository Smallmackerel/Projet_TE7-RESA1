#include "msg_struct.h"
#include "common.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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

int send_send_message(int socket_fd, struct message *s_message, char* message) {//
	char nickname[NICK_LEN];
	char *nickname_start;
	char *nickname_end;
	char *content;
	size_t content_length;

	/* Format attendu : "/send " suivi du pseudo, d'un espace, puis du nom de fichier. */
	if (strncmp(message, "/send ", 6) != 0) {
		fprintf(stderr, "Usage: /send <nickname> <file_name>\n");
		return 1;
	}

	/* Le pseudo s'étend de la fin de "/send " jusqu'au premier espace suivant. */
	nickname_start = message + 6;
	nickname_end = strchr(nickname_start, ' ');
	if (nickname_end == NULL) {
		fprintf(stderr, "Usage: /send <nickname> <file_name>\n");
		return 1;
	}

	if (!validate_and_copy_nickname(nickname_start, (size_t)(nickname_end - nickname_start), nickname)) { // vérification que le pseudo est valide et tient dans NICK_LEN
		return 1;
	}

	/* Le nom du fichier commence juste après l'espace qui suit le pseudo. */
	content = nickname_end + 1;
	int int_content = atoi(content);
	content_length = sizeof(int_content);

	/* Envoi de l'en-tête (pseudo destinataire dans infos), puis du payload. */
	fill_and_send_message_header(socket_fd, s_message, (int)content_length, "", FILE_REQUEST, nickname);
	if (write_in_socket(socket_fd, content, content_length) == 0) {
		return 0;
	}
	return 1;
}