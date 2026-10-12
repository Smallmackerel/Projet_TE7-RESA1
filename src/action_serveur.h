#ifndef ACTION_SERVEUR_H
#define ACTION_SERVEUR_H

#include <stddef.h>



#include "common.h"
#include  "client_list.h"
#include "msg_struct.h"
#include "action_serveur.h"



void file_request_send(struct client_info **clients, int client_fd, struct message received, char* payload);
void file_response_forward(struct client_info **clients, int client_fd,struct message received, const char *payload);

void echo_send(int client_fd, struct message received,char* payload);

void unicast_send(struct client_info **clients, int client_fd, struct message received,char* payload);

void broadcast_send(struct client_info **clients, int client_fd,struct message received,char* payload);



void nickname_infos(struct client_info **clients, int client_fd,struct message received);


void nickname_list(struct client_info **clients, int client_fd);


//ajoute/modifie un nom.
void nickname_new(struct message msg, struct client_info **clients,int client_fd);

//fonction qui choisi l'action a realiser
void action(struct message msg, struct client_info **clients,int client_fd,char* payload);

#endif
