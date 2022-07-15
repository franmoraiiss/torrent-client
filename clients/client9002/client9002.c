#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>

#define SIZE_BUFFER 1024
#define PEER_PORT 9002
#define PEER_ADDRESS "127.0.0.1"

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
    int i, c = 0;

    int aux;
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
    BINARY_SUM:
    Responsável pela soma de dois valores binários
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
    for (int i = 0; i < 8; i++)
    {
        // Complemento de 1 da soma passa a ser o checksum
        if (total_sum[i] == 1)
        {
            packet->checksum[i] = 0;
        }
        else
        {
            packet->checksum[i] = 1;
        }
    }

    return 0;
}

/*
    SEND_MESSAGE:
    Responsável por enviar a mensagem e tratar o erro.
*/
int send_packet(FILE *file, int socket_server, struct sockaddr_in peer_address, socklen_t size_peer, char *buffer)
{
    Packet packet;
    int sequency_number = 0;

    memset(&packet, 0, sizeof(packet));

    while (!feof(file))
    {
        sequency_number++;

        packet.size = fread(packet.data, 1, 1024, file);
        packet.sequency_number = sequency_number;
        checksum(&packet);

        while (1)
        {
            memset(buffer, '\0', SIZE_BUFFER);

            // Enviando pacote
            int send_status = sendto(socket_server, &packet, sizeof(packet), 0, (struct sockaddr *)&peer_address, size_peer);

            if (send_status < 0)
            {
                perror("/!\\ - Erro ao enviar pacote");
                exit(1);
            }

            // Recebe resposta
            int recvfrom_status = recvfrom(socket_server, buffer, sizeof(buffer), 0, (struct sockaddr *)&peer_address, &size_peer);
            if (recvfrom_status < 0)
            {
                perror("/!\\ - Erro ao receber resposta");
                exit(1);
            }

            // Somente caso o valor recebido (ACK) no buffer seja '1',
            // passamos para o próximo pacote.
            if (buffer[0] == '1')
            {
                printf("> Pacote %d enviado com sucesso!\n", packet.sequency_number);
                break;
            }
            else
            {
                printf("Falha no envio do pacote %d, iniciando reenvio\n", packet.sequency_number);
            }
        }
    }
}

int create_socket_server()
{
    int socket_server;
    socket_server = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_server < 0)
    {
        perror("-> Erro ao criar socket!");
        exit(1);
    }

    // memset(&server_address, 0, sizeof(server_address));

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PEER_PORT);
    server_address.sin_addr.s_addr = inet_addr(PEER_ADDRESS);

    if (bind(socket_server, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("-> Erro ao bindar socket!");
        exit(1);
    }

    return socket_server;
}

int verifica_buffer(char *buffer)
{
    if (buffer[0] != '\0')
        return 1;
    return 0;
}

int main()
{
    FILE *file;

    int socket_server;

    struct sockaddr_in peer_address;
    socklen_t peer_struct;

    // Inicializa o servidor
    socket_server = create_socket_server();

    char *buffer;
    buffer = (char *)malloc(SIZE_BUFFER * sizeof(char));

    printf("-> Peer online <-\n\n");

    while (1)
    {
        printf("-> Aguardando solicitação!\n");

        memset(buffer, '\0', SIZE_BUFFER);

        peer_struct = sizeof(peer_address);

        // Recebe solicitação
        recvfrom(socket_server, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&peer_address, &peer_struct);

        printf("-> Arquivo requisitado: %s\n", buffer);
        if (verifica_buffer(buffer))
        {
            // "Conecta" ao banco de dados
            file = fopen(buffer, "rb");
            memset(buffer, '\0', SIZE_BUFFER);

            if (file == NULL)
            {
                buffer[0] = '0';
                // Envia o código '0' como NAK. Indicando que o peer não tem arquivo.
                sendto(socket_server, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&peer_address, peer_struct);
            }
            else
            {
                buffer[0] = '1';
                // Caso tenha o arquivo, envia um ACK de código '1'.
                sendto(socket_server, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&peer_address, peer_struct);

                // Inicia a transferência do arquivo.
                send_packet(file, socket_server, peer_address, peer_struct, buffer);
            }

            fclose(file);
        }
    }
}
