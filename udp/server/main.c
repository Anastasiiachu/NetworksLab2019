#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#define flagConnect 1
#define flagDisconnect 2
#define flagMessage 3
#define size_time 5
#define buff_time 40
//Максимальная длина сообщения
#define buffMessage 512
//Максимальная длина имени пользователя
#define maxNameSize 16
#define port 5001
//Максимальное  разрешенное количество клиентов
#define maxClients 10
//512 байт данных
#define dataSize 512
//516 байт пакета
#define packetSize 514
//Создаем сокет сервера
int sockfd;
struct sockaddr_in serv_addr, cli_addr;

//Структура клиента
typedef struct list{
    //Храним адрес клиента
    struct sockaddr_in addr;
    char *name;
    struct list *prev;
    struct list *nextClient;
} client;

//Пакет от сервера
char packetFROM[packetSize];
//Пакет серверу
char packetTO[packetSize];
int lengthPacket;
int flag;
char *nameCl;
//Массив клиентов
client *initClient = NULL;

//Счетчик клиентов
//Инициализация текущего счетчика клиентов
int countClients = 0;

void printMessage(char* name, char* message){
    /*
    * template for time <00:00> size 5
    * Функция для определения времени отправки сообщения
    */
    time_t timer = time(NULL);
    struct tm* timeStruct = localtime(&timer);
    char stringTime[size_time];
    //bzero(stringTime, size_time);
    int length = strftime(stringTime,buff_time,"%H:%M", timeStruct);
    //Вывод сообщения
    printf("<%s>[%s]: %s\n", stringTime, name, message);

}

void printServerLog(char* message, int state){
    /*
    * template for time <00:00> size 5
    * Функция для определения времени отправки сообщения
    */
    time_t timer = time(NULL);
    struct tm* timeStruct = localtime(&timer);
    char stringTime[size_time];
    //bzero(stringTime, size_time);
    int length = strftime(stringTime,buff_time,"%H:%M", timeStruct);

    //Определяем что отобразить
    switch(state){
        case 0: {
            //Вывод сообщения
            printf("<%s>: %s\n", stringTime, message);
            break;
        }
        case 1: {
            printf("<%s>:Клиент %s инициализирован\n", stringTime, message);
            break;
        }
        case 2: {
            printf("<%s>Сообщение : %s\n", stringTime, message);
            break;
        }
        case 3: {
            printf("<%s>:Имя клиента = %s\n", stringTime, message);
            break;
        }
        case 4: {
            printf("<%s>:Клиент %s отправил сообщение\n", stringTime, message);
        }
    }
}


//Функция закрытие клиента
void closeClient(struct sockaddr_in addr){
    client *socket;
    socket = initClient;;

    while (socket != NULL) {
        if (socket->addr.sin_port == addr.sin_port){
            if(socket->prev !=NULL){
                socket->prev->nextClient = socket->nextClient;
            }
            if(socket->nextClient!=NULL){
                socket->nextClient->prev = socket->prev;
            }
        }
        socket = socket->nextClient;
    }
}


void findClient(){

    client *tmpClients;
    tmpClients = initClient;
    while (tmpClients != NULL) {
        if (tmpClients->addr.sin_port == cli_addr.sin_port){
            nameCl = tmpClients->name;
        }
        tmpClients = tmpClients->nextClient;
    }

}
//Функция закрытия сервера
void closeServer(){

    //Отключить всех клиентов
    for (int i =0; i < countClients; i ++){
        //closeClient(clients);
    }
    close(sockfd);
    exit(1);

}

//Отправка данных клиенту
void sendPacketToClient(int length, struct sockaddr_in addr){
    int lengthSend = sendto(sockfd, (char *) packetTO, length, 0, (struct sockaddr *) &addr, sizeof(addr));
    if (lengthSend < 0){
        printf("ERROR");
        closeClient(addr);
    }
}


void makePacket(){

    switch (flag){
        case flagDisconnect : {
            bzero(packetTO, packetSize);
            //2 байта на тип пакета
            flag = htons(flag);
            memcpy(packetTO, &flag, 2);
            //Формируем пакет
            break;
        }
            //Формируем пакет данных
        case flagMessage : {
            bzero(packetTO, packetSize);
            //2 байта на тип пакета
            flag = htons(flag);
            memcpy(packetTO,  &flag, 2);
            //N байт на данные
            char sendM[maxNameSize + buffMessage];
            bzero(sendM, maxNameSize+buffMessage);
            strcat(sendM,"[");
            strcat(sendM,nameCl);
            strcat(sendM,"]: ");
            strcat(sendM, packetFROM + 2);
            memcpy(packetTO + 2 , sendM, strlen(sendM));
            //Формируем пакет
            break;
        }
    }

}
//Обработка сигнала выхода от пользователя
void signalExit(int sig){
    closeServer();
}

int main(int argc, char *argv[]) {

    int newsockfd;
    uint16_t portno;
    unsigned int clilen;
    char bufferMessage[buffMessage];
    ssize_t n;

    signal(SIGINT, signalExit);

    /*Сокет для прослушивания других клиентов */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
    }

    /*Инициализируем сервер*/
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = port;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    /* Now bind the host address using bind() call.*/
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    /*Слушаем клиентов */
    printf("Сервер запущен. Готов слушать\n");

    listen(sockfd, maxClients);
    clilen = sizeof(cli_addr);
    client *tmpClients;
    //Работа сервера
    while (1){

        /* Сокет для приёма новых клиентов */
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        client* newClientInfo = (client*) malloc(sizeof(client));

        //Принимаем очередной пакет от клиента
        socklen_t size  = sizeof(serv_addr);
        lengthPacket = recvfrom(sockfd, (char *) packetFROM, packetSize, 0, (struct sockaddr  *) &cli_addr, &size);

        flag = ntohs(*(uint16_t *) packetFROM);
        switch(flag){
            case flagConnect : {
                printServerLog("Вошел новый клиент", 0);
                //Получаем имя клиента
                //Выделяем память для клиента
                char *nameClient = (char *) malloc(maxNameSize);

                //Получаем имя клиента
                bzero(nameClient,maxNameSize + 1);
                nameClient =strcat(nameClient,packetFROM + 2);

                printServerLog(nameClient,3);

                tmpClients = initClient;
                newClientInfo->addr= cli_addr;
                newClientInfo->name = nameClient;
                newClientInfo->nextClient = NULL;
                if (initClient == NULL) {
                    newClientInfo->prev = NULL;
                    initClient=newClientInfo;
                } else {
                    while (tmpClients->nextClient != NULL) {
                        tmpClients = tmpClients->nextClient;
                    }
                    newClientInfo->prev = tmpClients;
                    tmpClients->nextClient = newClientInfo;
                }
                break;
            }
            case flagDisconnect: {
                break;
            }
            case flagMessage :{
                findClient();
                makePacket();
                tmpClients = initClient;
                while (tmpClients != NULL) {
                    if (tmpClients->addr.sin_port != cli_addr.sin_port ){
                        sendPacketToClient(maxNameSize+buffMessage, tmpClients->addr);
                    }
                    tmpClients = tmpClients->nextClient;
                }
                break;
            }
        }
    }
    return 0;
}