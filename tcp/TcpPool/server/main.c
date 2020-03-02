#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>

#define size_time 5
#define buff_time 40
//Максимальная длина сообщения
#define buffMessage 512
//Максимальная длина имени пользователя
#define maxNameSize 16
#define port 5001
//Максимальное  разрешенное количество клиентов
#define maxClients 10

//Определение мьютекса
pthread_mutex_t mutex;
//Создаем сокет сервера
int sockfd;

//Структура клиента
typedef struct list{
    //У каждого клиента есть сокет, имя
    int socket;
    char *name;
    struct list *prev;
    struct list *nextClient;
} client;


//Массив клиентов
client *initClient = NULL;

struct pollfd poolClients[maxClients];

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
void closeClient(client* socket){
    int sock = socket->socket;
    close(socket->socket);
    if(socket->prev !=NULL){
        socket->prev->nextClient = socket->nextClient;
    }
    if(socket->nextClient!=NULL){
        socket->nextClient->prev = socket->prev;
    }

    for (int i = 1; i < countClients; i++){
        if (poolClients[i].fd == sock)
            for (int j = i; j < countClients; j++){
                poolClients[j] = poolClients[j+1];
            }

    }

    countClients--;

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

//Отправка сообщений всем клиентам клиентам, кроме себя
void sendMessageClients(char* message, int socket, char* name){

    int n;
    pthread_mutex_lock(&mutex);
    char* sendM = (char *) malloc((strlen(message)+ strlen(name))*sizeof(char));
    strcat(sendM,"[");
    strcat(sendM,name);
    strcat(sendM,"]: ");
    strcat(sendM, message);
    int length = strlen(sendM);
    client *tempClient = initClient;

    while(tempClient != NULL){
        if (tempClient->socket != socket){
            n = write(tempClient->socket, &length, sizeof(int));
            if (n <= 0){
                closeClient(tempClient);
            }
            n = write(tempClient->socket, sendM, length);
            if (n <= 0){
                closeClient(tempClient);
            }
        }
        tempClient = tempClient->nextClient;
    }

    free(sendM);
    pthread_mutex_unlock(&mutex);

}



//Функция для приема сообщений от клиентов
void reciveMessage(client *socket, char* bufferMessage,  char * name){
    int n;
    int length = 0;
    //Получаем размер сообщения
    n = read(socket->socket, &length, sizeof(int));
    printf("n afret %d\n",n);
    if (n <= 0) {
        perror("ERROR reading from socket\n");
        closeClient(socket);
    }

    if(length > 0){
        printf("Размер введенного сообщения %d\n", length);
        //Получаем само сообщение
        n = read(socket->socket, bufferMessage, length);
        if (n <= 0) {
            perror("ERROR reading from socket\n");
            closeClient(socket);
        }
        if (n > 0){
            printf("length %d\n", length);
            printMessage(name, bufferMessage);
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
    struct sockaddr_in serv_addr, cli_addr;
    ssize_t n;
    int status;

    signal(SIGINT, signalExit);
    //Инициализация мьютекса
    pthread_mutex_init(&mutex,NULL);

    //Инициализация массива клиентов
    //initMas();

    //Идентификатор потока
    pthread_t clientTid;

    /* Сокет для прослушивания других клиентов */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

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
    printServerLog("Сервер запущен. Готов слушать",0);

    listen(sockfd, maxClients);
    clilen = sizeof(cli_addr);

    poolClients[countClients].fd = sockfd;
    poolClients[countClients].events = POLLIN;
    client *tmpClients;
    countClients++;

    //Работа сервера
    while (1){
        status = poll(poolClients,(unsigned int) maxClients, 10000);

        for (int i = 0; i < countClients; i++){
            if (poolClients[i].revents == 0)
                continue;
            if (poolClients[i].fd == sockfd){
                /* Сокет для приёма новых клиентов */
                newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
                client* newClientInfo = (client*) malloc(sizeof(client));
                printServerLog("Вошел новый клиент", 0);
                //Получаем имя клиента
                int length = 0;
                bzero(bufferMessage, buffMessage);

                //Получаем размер сообщения
                n = read(newsockfd, &length, sizeof(int));

                //Вывод размера введенного сообщения
                printf("Размер введенного сообщения %d\n", length);

                //Выделяем память для клиента
                char *nameClient = (char *) malloc(length);

                //Получаем имя клиента
                bzero(nameClient,length + 1);
                n = read(newsockfd, nameClient, length);

                printServerLog(nameClient,3);

                tmpClients = initClient;
                newClientInfo->socket = newsockfd;
                newClientInfo->name = nameClient;
                newClientInfo->nextClient = NULL;
                poolClients[countClients].fd = newsockfd;
                poolClients[countClients].events = POLLIN;

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
                countClients++;
            }
            else{
                tmpClients = initClient;
                while (tmpClients->socket != poolClients[i].fd) {
                    tmpClients = tmpClients->nextClient;
                }
                bzero(bufferMessage, buffMessage);
                reciveMessage(tmpClients, bufferMessage,tmpClients->name);
                //Теперь отправляем сообщение
                sendMessageClients(bufferMessage, tmpClients->socket, tmpClients->name);
            }
        }
    }
    return 0;
}