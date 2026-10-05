# Jalon 4 — Extension : salons de discussion

> Ce jalon est une **extension**. Ne l'abordez qu'après avoir terminé les jalons obligatoires. Toutes les exigences des [jalons précédents](README.md#parcours-du-projet) restent obligatoires.

## Objectif

Ajoutez des salons de discussion. Un utilisateur peut créer ou rejoindre un salon. Les membres d'un même salon échangent des messages entre eux. Un utilisateur peut quitter un salon ou changer de salon à tout moment.

Utilisez `MULTICAST_CREATE`, `MULTICAST_LIST`, `MULTICAST_JOIN`, `MULTICAST_SEND` et `MULTICAST_QUIT`.

## Utilisation du champ `infos`

| Type | Contenu de `infos` |
| --- | --- |
| `MULTICAST_CREATE` | Nom du salon à créer. |
| `MULTICAST_LIST` | Chaîne vide. |
| `MULTICAST_JOIN` | Nom du salon à rejoindre. |
| `MULTICAST_SEND` | Nom du salon dans lequel envoyer le message. |
| `MULTICAST_QUIT` | Nom du salon à quitter. |

## Exigences

- **Req4.1** — `/create <channel_name>` (`MULTICAST_CREATE`) crée un salon. Rejetez les noms contenant des espaces ou des caractères autres que des lettres et des chiffres.
- **Req4.2** — Le créateur rejoint automatiquement son salon. Il quitte son salon courant s'il y en a un.
- **Req4.3** — `/channel_list` (`MULTICAST_LIST`) retourne la liste des salons.
- **Req4.4** — La création d'un salon existant renvoie une erreur à l'utilisateur.
- **Req4.5** — `/join` et `/quit` (`MULTICAST_JOIN` et `MULTICAST_QUIT`) permettent respectivement de rejoindre et quitter un salon.
- **Req4.6** — Rejoindre un autre salon fait quitter le salon courant.
- **Req4.7** — Lorsque le dernier occupant quitte un salon, le serveur détruit ce salon et notifie cet utilisateur.
- **Req4.8** — Une ligne saisie sans commande est envoyée dans le salon courant (`MULTICAST_SEND`). Elle ne doit parvenir qu'aux utilisateurs présents dans ce salon.
- **Req4.9** — Les membres d'un salon sont notifiés des arrivées et départs des autres utilisateurs de ce salon.

## Exemple d'interaction

```text
%terminal_user0> /create channel_name
%terminal_user0> You have created channel channel_name
%terminal_user0[channel_name]> You have joined channel_name

%terminal_user1> /join channel_name
%terminal_user1[channel_name]> INFO> You have joined channel_name
%terminal_user0[channel_name]> INFO> user1 has joined channel_name

%terminal_user0[channel_name]> I'm downtown
%terminal_user1[channel_name]> user0> : I'm downtown

%terminal_user0[channel_name]> /quit channel_name
%terminal_user1[channel_name]> INFO> user0 has quit channel_name
%terminal_user1[channel_name]> /quit channel_name
%terminal_user1> INFO> You were the last user in this channel, channel_name has been destroyed
```
