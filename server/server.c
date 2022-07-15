#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024
#define SERVER_PORT 9000
#define IP_ADDRESS "127.0.0.1"

typedef struct Peer
{
	int port;
	char file[50];
} Peer;

/*
	CREATE_SOCKET_SERVER:
	Responsável por incializar o socket e definir
	o IP e a porta do servidor, tratando os erros.
*/
int create_socket_server()
{
	// Inicializa o socket, configurando como IPV4 (AF_INET) e UDP (SOCK_DGRAM).
	int server_socket;
	server_socket = socket(AF_INET, SOCK_DGRAM, 0);

	if (server_socket < 0)
	{
		perror("-> Erro ao criar socket!");
		exit(1);
	}

	// Configura o endereço do servidor (127.0.0.1) e a porta (9000).
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(SERVER_PORT);
	server_address.sin_addr.s_addr = inet_addr(IP_ADDRESS);

	// Faz o bind das informações do endereço com o socket
	int bind_status = bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));
	if (bind_status < 0)
	{
		perror("-> Erro ao bindar socket!");
		exit(1);
	}

	return server_socket;
}

/*
	FIND_PEER:
	Responsável por procurar no banco de dados, se outro peer
	possui o arquivo desejado. Caso exista, retorna a porta do peer.
 */
int find_peer(FILE *database, char *seek_file, char *seek_port)
{
	char port[5];
	char file[50];

	fseek(database, 0, SEEK_SET);
	while (fscanf(database, "%s %s", file, port) != EOF)
	{
		// Verifica se tem o arquivo desejado no banco de dados
		if (strcmp(file, seek_file) == 0)
		{
			// Envia a porta de quem possui o arquivo
			strcpy(seek_port, port);
			return 1;
		}
	}

	// Caso ninguém tenha o arquivo
	return 0;
}

/*
	UPDATE_DATABASE:
	Responsável por atualizar o banco de dados com os dados
	do novo	peer (arquivo que possui e sua porta).
*/
int update_database(FILE *database, struct Peer peer)
{
	char port[5];
	char file[50];

	while (fscanf(database, "%s %s", file, port) != EOF)
	{
		// Compara se o nome do arquivo do banco é igual ao arquivo do peer,
		// e se a porta é igual a porta do peer
		if (strcmp(file, peer.file) == 0 && peer.port == atoi(port))
			return 0;
	}

	// Caso não haja os dados no banco, inserimos os dados do peer.
	fprintf(database, "%s %d\n", peer.file, peer.port);
	fflush(database);

	return 0;
}

/*
	CHECK_BUFFER:
	Verificar em um buffer, se já chegou ao
	fim do conteúdo (definido por '\0'), em
	um determinado array de caracteres.
*/
int check_buffer(char *buffer)
{
	if (buffer[0] != '\0')
		return 1;

	return 0;
}

int main()
{
	// "Conecta" com o banco de dados
	FILE *database;
	database = fopen("database.txt", "r+b");
	if (database == NULL)
	{
		perror("Erro ao abrir banco de dados.");
		exit(1);
	}

	// Inicializa o servidor
	int socket_server;
	socket_server = create_socket_server();
	printf("-> Servidor rodando: { IP: %s, PORTA: %d } <--\n\n", IP_ADDRESS, SERVER_PORT);

	// Aloca o buffer com o tamanho necessário
	// e cria variável para receber o nome do
	// peer que possui o arquivo.
	char peer_with_file[50];
	char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

	// Inicializa o endereço do cliente
	struct sockaddr_in client_address;
	socklen_t size_of_client;

	while (1)
	{
		// Seta todo o buffer com '\0'
		memset(buffer, '\0', BUFFER_SIZE);

		size_of_client = sizeof(client_address);

		// Recebe as informações do cliente, e coloca
		// no buffer. Essa primeira informação é o nome
		// do arquivo desejado
		recvfrom(socket_server, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &size_of_client);

		if (check_buffer(buffer))
		{
			// Caso o arquivo requisitado esteja ligado a algum
			// cliente na base de dados, envia a porta desse cliente
			// e espera um pedido do cliente A para atualizar o banco

			// Verificar no banco de dados, se algum peer possui
			// o arquivo desejado e retorna na variavel 'peer_with_file',
			// a porta do respectivo peer.
			if (find_peer(database, buffer, peer_with_file))
			{
				memset(buffer, '\0', BUFFER_SIZE); // Reinicia o buffer com '\0'
				buffer[0] = '1';									 // Seta a mensagem para ser de tipo 1
				strcat(buffer, peer_with_file);		 // Concatena buffer com o nome do peer

				// printf("-> Enviando resposta ao cliente\n");

				// Envia ao peer a porta do outro peer que possui
				// o arquivo desejado.
				sendto(socket_server, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, size_of_client);

				// Aguarda conexão do peer que possui o arquivo com o cliente
				printf("-> Esperando resposta\n");
				Peer foreign_peer;
				while (1)
				{
					memset(&foreign_peer, 0, sizeof(Peer));

					// Após a conexão, recebe mensagem do peer inicial, para atualizar o banco
					// com os dados (nome do arquivo e porta), pois agora esse peer
					// possui o arquivo.
					recvfrom(socket_server, &foreign_peer, sizeof(foreign_peer), 0, (struct sockaddr *)&client_address, &size_of_client);

					update_database(database, foreign_peer);

					// Envia mensagem ao peer, com o buffer[0] setado como 1, dizendo que
					// ele está presendo agora no banco de dados para transferir arquivos
					sendto(socket_server, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_address, size_of_client);

					printf("-> Banco de dados atualizado com o novo peer. \n");
					break;
				}
			}
			else
			{
				// Se nenhum peer tiver o arquivo, vai retorna um NAK,
				// representado por 0 no buffer.
				memset(buffer, '\0', BUFFER_SIZE);
				buffer[0] = '0';
				// Envia o NAK para o peer.
				sendto(socket_server, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, size_of_client);
			}
		}
	}
}
