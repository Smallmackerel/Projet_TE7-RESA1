# ETE7-RESA1 — Projet de programmation réseau

Ce projet consiste à développer en binôme une application de messagerie instantanée client/serveur en C. Elle doit permettre des échanges entre deux utilisateurs, entre plusieurs utilisateurs et, selon les jalons, des échanges de fichiers. Le projet a pour objectif principal de mettre en pratique l'API sockets POSIX et les communications TCP/IP.

Vous construirez l'application progressivement. **Un jalon doit être entièrement terminé et soumis avant de commencer le suivant.** Vous avez 14h40 en séances encadrées. L'intégralité du projet est réalisable sur ce créneau là. Vous ne devez PAS travailler à la maison sur ce projet, il est à réaliser UNIQUEMENT en séance.

## Parcours du projet

Lisez d'abord les règles communes ci-dessous, puis avancez dans l'ordre des jalons.

| Étape | Objectif | Spécification |
| --- | --- | --- |
| Jalon 1 | Client/serveur TCP IPv4, écho et multi-clients | [jalon1.md](jalon1.md) |
| Jalon 2 | Utilisateurs, pseudos et messagerie | [jalon2.md](jalon2.md) |
| Jalon 3 | Transfert de fichiers pair à pair | [jalon3.md](jalon3.md) |
| Jalon 4 | Salons de discussion | [jalon4.md](jalon4.md) |

Les [ressources](ressources.md) regroupent les conseils de débogage et le rappel de C fourni avec le sujet. Le [contrat de protocole](protocole.md) définit le format des trames, les limites et les erreurs de communication à respecter.

## Dépôt de départ

Après avoir récupéré le dépôt, vous y trouverez :

- `sample-jalon1/` et `sample-jalon2/` : squelettes de code et Makefiles de base pour les jalons 1 et 2 ;
- `travail/` : répertoire de développement. C'est ce répertoire qui est évalué pour les soumissions intermédiaires ;
- `rendu_final/` : répertoire destiné au rendu final ;
- `info.txt` : à compléter avec les noms, prénoms et logins GitHub du binôme.

## Dépôt GitHub

Pour pousser votre code, créez un *Personal Access Token* GitHub en suivant [le guide officiel](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/creating-a-personal-access-token). Lors d'un `git push`, utilisez ce jeton à la place du mot de passe.

## Soumettre un jalon

Chaque jalon est soumis sur **Moodle** sous la forme d'une archive qui contiendra le code attendu dans le bon répertoire : `travail/` pour les jalons intermédiaires, `rendu_final/` pour le rendu final.

**Ne soumettez qu les fichiers sources. N'ajoutez pas de fichiers autre dans votre archive sur moodle** (pas de fichier compilé, pas de fichier pdf, etc.). 

### Échéances du projet et des jalons

Chaque jalon devra être livré à la fin d'une séance en classe au sur moodle Vous devrez faire une démonstration de votre code aux encadrants lors de la dernière séance. Toute production du code a posteriori ne sera pas prise en compte.

| Livrable | Échéance |
| --- | --- |
| Jalon 1 | Au plus tard à la fin de la première séance |
| Jalon 2 | Au plus tard à la fin de la seconde séance |
| Jalon 3 | Au plus tard à la fin de la troisième séance | 
| Jalon 4 | Au plus tard à la fin de la quatrième séance |
| Démonstration final | Pendant la quatrième séance |

Ces échances indique **les dates au plus tard** auxquelles il vous faut soumettre votre travail sur moodle. En termes d'attente moyenne, voici les durée de travail attendue sur chaque jalon pour avancer correctement au niveau attendu : 

| Tache | Durée de travail | Date de fin de developpement souhaitée |
| --- | --- | --- | 
| Jalon 1 | 2h | Milieu de la première séance |
| Jalon 2 | 4h | Milieu de la seconde séance |
| Jalon 3 | 3h | Milieu de la troisième séance | 
| Jalon 4 | 3h | lors de la 4ieme séance |
| Démonstration final | 30 min | Pendant la quatrième séance |


### IA et production logiciel dans ce projet 

L'utilisation d'outils IA pour le développement et la production de code n'est pas interdite dans le cadre de ce projet. En revanche, les outils IA empêchent, dans le cadre de l'apprentissage via ce projet, de monter en compétence et en connaissance. Pour cette raison, il est plus que fortement recommandé de **s'interdire les outils IA** pour produire ou développer. La seule utilisation cohérente et tolérée pouvant avoir un impact positif est l'utilisation des IA pour clarifier le fonctionnement des fonctions que nous avons vu ensemble en cours (en aucun cas pour proposer du code que vous reprendriez par la suite). 


## Rendu final

Copiez le code de `travail/` vers `rendu_final/`. Le rendu final doit impérativement compiler avec un Makefile, sans erreur ni avertissement, sur les machines de l'ENSEIRB-MATMECA. Vous pouvez repartir des Makefiles fournis dans `sample-jalon1/` et `sample-jalon2/`.

Pour développer et tester sur les machines de l'école, connectez-vous à `ssh.enseirb.fr`, puis à une machine de TP. La commande `netgroup <numéro_de_salle>` permet d'identifier les machines disponibles, par exemple `netgroup I101`.

## Évaluation

L'évaluation porte notamment sur :

- le respect des modalités et échéances de soumission
- l'implémentation des fonctionnalités demandées
- la robustesse face à des messages non implémentés ou malformés, côté client comme côté serveur
- la libération de la mémoire et la fermeture des sockets, notamment vérifiées avec `valgrind` et `lsof`.

### Bonus et malus

| Critère | Effet |
| --- | --- |
| Consignes non respectées : échéances, `info.txt`, Makefile, erreurs ou avertissements importants à la compilation | -2 points |
| Mauvaise utilisation des primitives de lecture et d'écriture sur sockets | -2 points |
| Code peu lisible : indentation, `main()` trop long, absence de factorisation ou de fonctions | -2 points |
| Mémoire allouée mais non libérée | -2 points |
| Descripteurs de sockets ou fichiers non fermés | -2 points |
| Fonctionnement en IPv4 et IPv6 | +0,25 point |

Chaque réalisation de jalon donne des points dans la note finale. Pour obtenir les points d'un jalon, toutes les fonctionnalités et spécificités des jalons précédents doivent être implémentées sans erreur.

*Surprise* : ajoutez des fonctionnalités au-delà des jalons 1 à 4.
