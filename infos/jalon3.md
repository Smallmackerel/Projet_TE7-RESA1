# Jalon 3 — Transferts de fichiers pair à pair

> Prérequis : toutes les exigences des [jalons 1](jalon1.md) et [2](jalon2.md) restent obligatoires. Ce jalon introduit une seconde connexion TCP temporaire entre deux clients.

## Objectif

Permettez à un utilisateur d'envoyer un fichier directement à un autre utilisateur en pair à pair. Le serveur ne transporte pas le fichier. Il met seulement en relation l'émetteur et le récepteur.

Utilisez `FILE_REQUEST`, `FILE_ACCEPT`, `FILE_REJECT`, `FILE_SEND` et `FILE_ACK`. Respectez le [contrat de protocole](protocole.md), notamment la limite de taille d'une trame et la nécessité de transférer les fichiers par morceaux.

## Déroulement d'un transfert

1. L'émetteur envoie au serveur une demande `/send` contenant le pseudo du récepteur et le nom du fichier (`FILE_REQUEST`).
2. Le serveur transmet la demande au récepteur.
3. Le récepteur accepte ou refuse.
4. Sa réponse transite par le serveur (`FILE_ACCEPT` ou `FILE_REJECT`).
5. En cas de refus, l'émetteur est informé et aucun fichier n'est envoyé.
6. En cas d'acceptation, la réponse inclut l'adresse IP et le port d'écoute temporaire du récepteur.
7. L'émetteur se connecte directement au récepteur et transmet le fichier (`FILE_SEND`).
8. Le récepteur confirme la bonne réception (`FILE_ACK`).

Le périmètre attendu est un environnement de TP où les clients sont directement joignables. Les mécanismes de traversée de NAT ne font pas partie du projet.

## Utilisation du champ `infos`

| Type | Contenu de `infos` et du payload |
| --- | --- |
| `FILE_REQUEST` | `infos` contient le pseudo du destinataire ; le payload contient le nom du fichier. |
| `FILE_ACCEPT` | `infos` contient le pseudo de l'émetteur initial ; le payload contient l'adresse et le port d'écoute du récepteur, par exemple `127.0.0.1:8081`. |
| `FILE_REJECT` | `infos` contient le pseudo de l'émetteur initial. |
| `FILE_SEND` | `infos` contient le nom du fichier ; le payload contient les données binaires du fichier. |
| `FILE_ACK` | `infos` contient le nom du fichier correctement reçu. |

Le payload de `FILE_SEND` n'est pas nécessairement du texte. Il peut contenir des octets nuls. N'utilisez donc pas de fonctions propres aux chaînes de caractères, telles que `strlen()`, `strcpy()` ou `strcmp()`, pour traiter ce payload.

## Exigences

- **Req3.1** — Un émetteur peut envoyer un fichier à un récepteur.
- **Req3.2** — Le récepteur doit approuver le transfert.
- **Req3.3** — Si le récepteur accepte, l'émetteur se connecte directement à lui le temps de transmettre le fichier.
- **Req3.4** — Si le récepteur refuse, l'émetteur est informé.
- **Req3.5** — L'émetteur et le récepteur reçoivent confirmation de la réussite du transfert.
- **Req3.6** — Le transfert de fichiers se fait par morceaux. Le programme valide les longueurs annoncées avant toute allocation ou lecture.

## Exemple d'interaction

```text
%terminal_user1> /send user2 "/home/user/file.txt"
%terminal_user2> user1 wants you to accept the transfer of the file named "file.txt". Do you accept? [Y/N]
%terminal_user2> Y
%terminal_user1> user2 accepted file transfer.
%terminal_user1> Connecting to user2 and sending the file...
%terminal_user2> Receiving the file from user1...
%terminal_user2> file.txt saved in .resa1/inbox/file.txt
%terminal_user1> user2 has received the file.
```
