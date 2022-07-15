#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>

#define SERVER_PORT 9000
#define IP_ADDRESS "127.0.0.1"

#define PEER_A_PORT 9001
#define FOREIGN_PEER_IP_ADDRESS "127.0.0.1"

#define BUFFER_SIZE 1024

typedef struct Peer
{
    int peer_port;
    char file[20];
} Peer;

typedef struct Packet
{
    int sequency_number;
    int checksum[8];
    int size;
    char data[1024];
} Packet;

/*
    BINARY_SUM:
    Responsável pela soma de dois valores binários
*/
void binary_sum(int result[], int binary[])
{
    int aux;
    int i, c = 0;

    for (i = 7; i >= 0; i--)
    {
        aux = result[i];
        result[i] = ((aux ^ binary[i]) ^ c);
        c = ((aux & binary[i]) | (aux & c)) | (binary[i] & c);
    }

    if (c == 1)
    {
        int aux;
        for (i = 7; i >= 0; i--)
        {
            aux = result[i];
            result[i] = ((aux ^ 0) ^ c);
            c = ((aux & 0) | (aux & c)) | (0 & c);
        }
    }
}

/*
    CHECKSUM:
    Responsável por verificar se o arquivo enviado é o mesmo
    recebido após uma transferência. Assim, é possível verificar
    se o mesmo foi ou não corrompido/alterado.
*/
int checksum(Packet *packet)
{
    if (packet == NULL)
        return 0;

    int binary[8];
    int total_sum[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    for (int i = 0; i < packet->size; ++i)
    {
        // Cada posição é transformada em uma palavra de 8 bits
        char ch = packet->data[i];
        for (int j = 7; j >= 0; --j)
        {
            if (ch & (1 << j))
            {
                binary[7 - j] = 1;
            }
            else
            {
                binary[7 - j] = 0;
            }
        }

        // Soma cada palavra das 1024 posições
        binary_sum(total_sum, binary);
    }

    // Soma com o checksum recebido para verificar a validade
    binary_sum(total_sum, packet->checksum);

    // Verifica se o pacote foi corrompido
    for (int i = 0; i < 8; i++)
    {
        // Caso uma das posições da soma, seja diferente de 1,
        // significa que corrompeu. Retornamos 0.
        if (total_sum[i] != 1)
            return 0;
    }

    // Se todas as posições forem 1, o pacote é válido. Retornamos 1.
    return 1;
}

/*
    RECEIVE_MESSAGE:
    Responsável por receber a mensagem e tratar o erro
*/
void receive_message(int socket_server, struct sockaddr_in address, char *buffer)
{
    socklen_t addrlen = sizeof(address);

    while (1)
    {
        int recvfrom_status = recvfrom(socket_server, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&address, &addrlen);
        if (recvfrom_status == -1)
        {
            perror("/!\\ - Erro ao receber mensagem!\n\n");
            exit(1);
        }
        else
        {
            break;
        }
    }
}

/*
    SEND_MESSAGE:
    Responsável por enviar a mensagem e tratar o erro.
*/
void send_message(int socket_server, struct sockaddr_in address, char *buffer, int message_type)
{
    socklen_t addrlen = sizeof(address);

    int sendto_status;
    Peer current_peer;

    // Se o tipo da mensagem (message_type) for 1, significa que estamos enviando
    // ao servidor, o nome do arquivo desejado. Se o tipo da mensagem for 2,
    // significa que estamos enviando ao servidor os dados com o nome do arquivo
    // e a nossa porta atual para receber o pacote. (PEER_A_PORT).
    switch (message_type)
    {
    case 1:
        sendto_status = sendto(socket_server, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&address, addrlen);
        if (sendto_status == -1)
        {
            perror("/!\\ - Erro ao enviar dados! \n");
            exit(1);
        }
        break;

    case 2:
        current_peer.peer_port = PEER_A_PORT;
        strcpy(current_peer.file, buffer);

        sendto_status = sendto(socket_server, &current_peer, sizeof(current_peer), 0, (struct sockaddr *)&address, addrlen);
        if (sendto_status == -1)
        {
            perror("/!\\ - Erro ao enviar dados! \n");
            exit(1);
        }
        break;

    default:
        break;
    }
}

/*
    RECEIVE_PACKET:
    Recebe do peer estrangeiro, os pacotes contendo os dados do arquivo
    desejado e os salva em um arquivo.
*/
void receive_packet(int socket_server, struct sockaddr_in address, char *file_name)
{
    FILE *file;
    Packet packet;
    int recvfrom_status;

    // Definir ack e nak
    char ack = '1';
    char nak = '0';

    int counter = 0;

    file = fopen(file_name, "wb");
    if (file == NULL)
    {
        perror("/!\\ - Não foi possível criar o arquivo! \n");
        exit(1);
    }

    socklen_t addrlen = sizeof(address);

    sleep(1);
    printf("\n\n>> Iniciando transferência <<\n");
    for (int i = 3; i > 0; i--)
    {
        printf("%d\n", i);
        sleep(1);
    }

    while (1)
    {
        memset(&packet, 0, sizeof(Packet));

        // Recebe pacote do peer com os dados
        recvfrom_status = recvfrom(socket_server, &packet, sizeof(packet), 0, (struct sockaddr *)&address, &addrlen);
        if (recvfrom_status == -1)
        {
            perror("/!\\ - Erro ao receber pacote!\n");
            exit(1);
        }

        // Verifica o checksum
        if (checksum(&packet) == 1 && packet.sequency_number == counter + 1)
        {
            printf("-> Pacote %d recebido!\n", packet.sequency_number);

            fwrite(packet.data, 1, packet.size, file);
            sendto(socket_server, &ack, sizeof(ack), 0, (struct sockaddr *)&address, addrlen);
            counter++;

            // Se o tamanho do pacote for meno que 1024, significa que é o último pacote
            if (packet.size < 1024)
                break;
        }
        else
        {
            printf("Pacote %d corrompido no campinho, aguardando reenvio! \n", packet.sequency_number);
            sendto(socket_server, &nak, sizeof(nak), 0, (struct sockaddr *)&address, addrlen);
        }
    }

    fclose(file);
}

int main(int argc, char *argv[])
{
    // Ao iniciar verificamos o arquivo solicitado pelo cliente
    if (argc == 1)
    {
        perror("/!\\ - Insira o arquivo ao executar o cliente. (Ex.: ./cliente arquivo.pdf)\n");
        exit(1);
    }

    int socket_server;
    int FOREING_PEER_PORT;

    struct sockaddr_in server_address;
    struct sockaddr_in foreign_peer_address;

    // Alocação do buffer
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    strcpy(buffer, argv[1]); // Buffer recebe o nome do arquivo

    socket_server = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_server == -1)
    {
        perror("/!\\ - Erro ao criar socket!\n");
        exit(1);
    }

    memset(&server_address, 0, sizeof(server_address));

    // Inicializa a struct com os dados do servidor
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = inet_addr(IP_ADDRESS);

    // Envia mensagem ao servidor para procurar no banco
    // se algum peer possui o arquivo desejado
    send_message(socket_server, server_address, buffer, 1);
    printf("-> Requisição enviada ao servidor! \n\n");

    memset(buffer, '\0', BUFFER_SIZE); // Reinicia o buffer

    // A resposta do servidor, vem com a porta do peer que possui o endereço.
    // Caso não haja nenhum peer com o arquivo desejado, o buffer é diferente de 1.
    receive_message(socket_server, server_address, buffer);
    if (buffer[0] == '1')
    {
        FOREING_PEER_PORT = atoi(&buffer[1]);
        printf(">> Arquivo %s encontrado!\n>> Peer: { IP: %s, PORTA: %d }.\n\n", argv[1], FOREIGN_PEER_IP_ADDRESS, FOREING_PEER_PORT);
        memset(buffer, '\0', BUFFER_SIZE);
    }
    else
    {
        perror("/!\\ - Nenhum peer possui o arquivo desejado.\n");
        exit(1);
    }

    memset(&foreign_peer_address, 0, sizeof(foreign_peer_address));

    // Inicializa os peer (estrangeiro) com o endereço
    foreign_peer_address.sin_family = AF_INET;
    foreign_peer_address.sin_port = htons(FOREING_PEER_PORT);
    foreign_peer_address.sin_addr.s_addr = inet_addr(FOREIGN_PEER_IP_ADDRESS);

    strcpy(buffer, argv[1]);

    // Envia para o peer estrangeiro, o nome do arquivo desejado
    send_message(socket_server, foreign_peer_address, buffer, 1);
    printf("-> Arquivo solicitado ao peer { IP: %s, PORTA: %d }.\n", FOREIGN_PEER_IP_ADDRESS, FOREING_PEER_PORT);

    memset(buffer, '\0', BUFFER_SIZE);
    receive_message(socket_server, foreign_peer_address, buffer);

    // Caso a posição inicial do buffer seja 1, inica a transferência do arquivo.
    if (buffer[0] == '1')
    {

        printf("-> Transferindo...\n");

        // Passa o nome do arquivo e recebe em pacotes do peer estrangeiro.
        strcpy(buffer, argv[1]);
        receive_packet(socket_server, foreign_peer_address, buffer);

        // Passa para o servidor, que o peer atual, também possui o novo arquivo.
        send_message(socket_server, server_address, buffer, 2);

        memset(buffer, '\0', BUFFER_SIZE);
        // Confirma que o servidor inseriu o peer atual no banco de dados
        receive_message(socket_server, server_address, buffer);
        if (buffer[0] == '1')
        {
            printf("\n-> Agora você está presente na base de dados :)\n");
        }
        else
        {
            perror("\n-> Falha ao ser cadastrado na base de dados :(\n");
            exit(1);
        }
    }
    else
    {
        printf("/!\\ - Peer { IP: %s, PORTA: %d }, não conseguiu enviar o arquivo! \n", FOREIGN_PEER_IP_ADDRESS, FOREING_PEER_PORT);
        exit(1);
    }

    free(buffer);

    return 0;
}
