# Jalon 1 — Client/serveur TCP et serveur multi-clients

> Prérequis : lisez les règles de dépôt, soumission et évaluation dans le [README](README.md). À la fin du jalon, soumettez une release `jalon1` contenant votre code dans `travail/`.

## Objectif

Implémentez un modèle client/serveur TCP en IPv4. Le client envoie une chaîne saisie au clavier, le serveur la renvoie uniquement à ce même client. Le serveur doit gérer plusieurs clients simultanément avec un unique processus et `poll()`.

## Fonctionnement attendu

Le client crée une socket TCP avec le nom de domaine et le port fournis, puis se connecte au serveur. Le serveur crée une socket d'écoute, l'associe à un port, accepte les connexions, reçoit les données et répond aux clients.

Le client doit surveiller simultanément son entrée standard et sa socket avec la fonction poll car il peut a tout moment recevoir des données depuis STDIN (descripteur de fichier qui vaut 0 par défaut) envoyées par l'utilisateur ou des données depuis sa socket connectée a serveur. Le serveur doit stocker, dans une liste chaînée, le descripteur de chaque socket cliente ainsi que l'adresse et le port source obtenus par `accept()`. Cette liste sera utilisée dans le jalon2. 

## Interface des programmes

```text
./client <server_name> <server_port>
./server <server_port>
```

## Exigences

- **Req1.1** — Créer un client TCP qui se connecte au serveur indiqué par le nom de domaine et le port passés en argument.
- **Req1.2** — Créer un serveur avec une socket d'écoute. Le port est passé en argument. Le serveur accepte les connexions et traite les données entrantes.
- **Req1.3** — Le serveur accepte et sert plusieurs clients simultanément.
- **Req1.4** — Le client lit une chaîne au clavier, envoie ensuite la taille de la chaine au serveur (envoyer les octets d'un `int`), puis envoie la chaine au serveur. Le client doit ensuite recevoir la réponse du serveur avec ce meme protocole de communication.
- **Req1.5** — Le client traite en même temps les chaînes saisies au clavier et les messages provenant du serveur, avec `poll()`.
- **Req1.6** — Lorsqu'il reçoit une chaîne d'un client, le serveur la renvoie uniquement à ce client.
- **Req1.7** — La connexion se ferme lorsque le client envoie `/quit`. Client et serveur ferment leurs sockets et libèrent la mémoire associée aux structures de données.
- **Req1.8** — Le serveur conserve les informations de chaque client, descripteur de fichier et adresse/port issus de `accept()`, dans une liste chaînée.

## Vérification avant soumission

- Lancez plusieurs clients sur le même serveur.
- Vérifiez qu'un message n'est renvoyé qu'à son expéditeur.
- Vérifiez `/quit`, y compris la fermeture des descripteurs et la libération de la liste chaînée.
- Consultez les [ressources de débogage](ressources.md), notamment `gdb`, `lsof` et `valgrind`.
