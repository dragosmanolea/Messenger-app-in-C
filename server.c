#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno;
	char buffer[BUFLEN];
	char mesaj_deconectare[BUFLEN];
	char clienti_conectat[BUFLEN];
	char mesaj_new_user[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	while (1) {
		tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) {
						fdmax = newsockfd;
					}

					printf("Noua conexiune de la %s, port %d, socket client %d\n",
							inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);

					// ii trimit noului client lista de clienti conectati
					memset(clienti_conectat, 0, BUFLEN);
					strcpy(clienti_conectat, "  Clienti conectati: ");
					memset(mesaj_new_user, 0, BUFLEN);
					strcpy(mesaj_new_user, "  S-a conectat user: ");
					char socket_new_user = newsockfd + '0';
					strncat(mesaj_new_user, &socket_new_user, 1);
					for (int j = sockfd + 1; j <= fdmax; ++j) {
						if (j != newsockfd) {
							char socket_id_conectat = j + '0';
							strncat(clienti_conectat, &socket_id_conectat, 1);
							strcat(clienti_conectat, ", ");
							send(j, mesaj_new_user, strlen(mesaj_new_user), 0);
						}
					}
					send(newsockfd, clienti_conectat, strlen(clienti_conectat), 0);
				} else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						// conexiunea s-a inchis
						printf("Socket-ul client %d a inchis conexiunea\n", i);
						memset(mesaj_deconectare, 0, BUFLEN);
						strcpy(mesaj_deconectare, "  S-a deconectat utilizatorul: ");
						char socket_id_deconectat = i + '0';
						strncat(mesaj_deconectare, &socket_id_deconectat, 1);
						printf("%s\n", mesaj_deconectare);
						for (int j = sockfd + 1; j <= fdmax; ++j) { // sockfd e socketul de listen
							if (i != j && FD_ISSET(j, &read_fds)) {
								send(j, mesaj_deconectare, strlen(mesaj_deconectare), 0);
							}
						}
						close(i);

						// se scoate din multimea de citire socketul inchis
						FD_CLR(i, &read_fds);
					} else {
						printf ("S-a primit de la clientul de pe socketul %d mesajul: %s\n", i, buffer);
						int destinatarfd = buffer[0] - '0';
						printf("trimit catre %d\n", destinatarfd);
						send(destinatarfd, buffer, strlen(buffer), 0);
					}
				}
			}
		}
	}

	close(sockfd);

	return 0;
}
