#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <pthread.h>

#define flagConnect 1
#define flagDisconnect 2
#define flagMessage 3
#define size_time 5
#define buff_time 40
//Максимальная длина сообщения
#define buffMessage 512
//Максимальная длина имени пользователя
#define maxNameSize 16
#define maxMess 512
//512 байт данных
#define dataSize 512
//516 байт пакета
#define packetSize 514

//Флаг режима работы клиента 0 - принимать сообщения: 1 - отправлять сообщения
int flagMode = 0;
int flag;
//Имя клиента
char *clientName;
//Сохранение сообщений в очередь
char *buffMessRecieve[maxMess];
int countMessRecievBuf = 0;
char bufferSendMessage[buffMessage];
//Пакет от сервера
char packetFROM[packetSize+maxNameSize];
//Пакет серверу
char packetTO[packetSize];
struct sockaddr_in serv_addr;
//Сокет клиента
int sockfd;
int lengthPacket;
//Определение мьютекса
pthread_mutex_t mutex;

//Функция закрытия клиента
void stopClient(int socket){
    shutdown(socket,SHUT_RDWR);
    close(socket);
    pthread_exit(NULL);
}

void printMessage(char* name, char* message, int flagMode){
    /*
    * template for time <00:00> size 5
    * Функция для определения времени отправки сообщения
    */
    time_t timer = time(NULL);
    struct tm* timeStruct = localtime(&timer);
    char stringTime[size_time];
    bzero(stringTime, size_time);
    int length = strftime(stringTime,buff_time,"%H:%M", timeStruct);
    if (flagMode == 1){
        //Вывод сообщения
        printf("<%s>[%s]: ", stringTime, name);
    }
    else{
        //Вывод сообщения
        printf("<%s>%s\n", stringTime, message);
    }

}

//Вывод сохраненных сообщений
void printSafeMessage(){

    pthread_mutex_lock(&mutex);
    time_t timer = time(NULL);
    struct tm* timeStruct = localtime(&timer);
    char stringTime[size_time];
    bzero(stringTime, size_time);
    int length = strftime(stringTime,buff_time,"%H:%M", timeStruct);

    for (int i = 0; i < countMessRecievBuf; i++){
        printf("<%s>%s\n", stringTime, buffMessRecieve[i]);
        free(buffMessRecieve[i]);
    }

    countMessRecievBuf = 0;
    pthread_mutex_unlock(&mutex);

}

//Функция для сохранения присланных сообщений в буфер
void safeMess(char* message){

    pthread_mutex_lock(&mutex);
    if (countMessRecievBuf < maxMess){
        buffMessRecieve[countMessRecievBuf] = strdup(message);
        countMessRecievBuf++;
    }
    pthread_mutex_unlock(&mutex);

}

//Функция формирователь пакетов
void makePacket(){

    switch (flag){
        case flagConnect : {
            bzero(packetTO, packetSize);
            //2 байта на тип пакета
            flag = htons(flag);
            memcpy(packetTO, &flag, 2);
            // N байт на имя
            memcpy(packetTO + 2, clientName, strlen(clientName) + 1);
            //Формируем пакет
            break;
        }
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
            memcpy(packetTO + 2 , bufferSendMessage, strlen(bufferSendMessage));
            //Формируем пакет
            break;
        }
    }

}

//Отправляем пакет серверу
void sendPacketToServer(int length ){
    sendto(sockfd, (const char *) packetTO, length, 0, (struct sockaddr  *) &serv_addr, sizeof(serv_addr));
}

//Получаем пакет с сервера
void recivePacketFromServer(){
    bzero(packetFROM, packetSize);
    socklen_t size  = sizeof(serv_addr);
    lengthPacket = recvfrom(sockfd, (char *) packetFROM, maxMess+maxNameSize, 0, (struct sockaddr  *) &serv_addr, &size);
}

//Поток на чтение сообщения
void* readThread(void* sock){
    char *recMessage;
    int socket = *(int *) sock;

    //Получаем сообщения от сервера
    while(1){
        recivePacketFromServer();
        if(!flagMode){
            printMessage("", packetFROM+2, flagMode);
        }
        else{
            safeMess(packetFROM+2);
        }
    }

}

//argv[1] - name ; argv[2] - host; argv[3] - port
int main(int argc, char *argv[]) {

    uint16_t portno;
    struct hostent *server;
    //Структуры для изменения режима работы терминала
    struct termios initial_settings, new_settings;
    //Идентификатор потока
    pthread_t readThr;
    char pressButton;


    //Инициализация мьютекса
    pthread_mutex_init(&mutex,NULL);
    //Начальное состояние консоли
    tcgetattr(fileno(stdin), &initial_settings);

    //Проверка на корректность введенных данных
    if (argc < 3) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }

    //Имя пользователя чата
    clientName = argv[1];

    //Инициализция номера порта
    portno = (uint16_t) atoi(argv[3]);
    /*Создание сокета*/
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Ошибка при открытии сокета");
        stopClient(sockfd);
    }

    if (strlen(clientName) > maxNameSize){
        fprintf(stderr, "Имя пользователя слишком длинное. Пожалуйста введите имя пользователя повторно\n");
        bzero(clientName,maxNameSize);
        fgets(clientName,maxNameSize, stdin);
        fflush(stdin);
        clientName[strlen(clientName)-1] = 0;
    }

    printf("Здравствуй %s! Добро пожаловать в чат. Для отправки сообщения используй клавишу -m\nДля тогого чтобы выйти из чата нажмите -q\n",clientName);

    //Инициализируем соединение с сервером
    server = gethostbyname(argv[2]);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка при открытии сокета");
        stopClient(sockfd);
    }
    //Проверяем что хост существует и корректный
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    //Инициализируем настройки клиента
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy(server->h_addr, (char *) &serv_addr.sin_addr.s_addr, (size_t) server->h_length);
    serv_addr.sin_port = htons(portno);

    //отправка имени серверу
    flag = 1;
    makePacket();
    sendPacketToServer(strlen(clientName) + 2);

    //Создаем поток на чтение данных с сервера
    if(pthread_create(&readThr, NULL, readThread, &sockfd) < 0 ){
        printf("ERROR");
        exit(1);
    }

    //Отправка сообщений другим клиентам
    while(1){
        //Если флаг в 1 то вывод заблокирован если флаг в 0 вывод разрешен
        if(!flagMode){
            new_settings = initial_settings;
            new_settings.c_lflag &= ~ICANON;
            new_settings.c_lflag &= ~ECHO;
            new_settings.c_cc[VMIN] = 0;
            new_settings.c_cc[VTIME] = 0;
            tcsetattr(fileno(stdin), TCSANOW, &new_settings);
            //Ожидаем нажатие клавиши
            read(0, &pressButton, 1);
            //Проверка нажатой клавиши
            if(pressButton == 'm'){
                flagMode = 1;
                printMessage(clientName, "", flagMode);
            }
            //Обрабатываем событие на выход клиент
            if(pressButton == 'q'){
                tcsetattr(fileno(stdin), TCSANOW, &initial_settings);
                stopClient(sockfd);
                exit(0);
            }
        }
        else{

            bzero(bufferSendMessage,buffMessage);
            //Восстанавливаем исходный терминал
            tcsetattr(fileno(stdin), TCSANOW, &initial_settings);
            //Получаем сообщение c консоли
            fgets(bufferSendMessage,buffMessage,stdin);
            int lengthMess = strlen(bufferSendMessage);
            fflush(stdin);
            //Убираем последний пробел
            bufferSendMessage[strlen(bufferSendMessage)-1] = 0;
            if (lengthMess > 1) {
                flag = 3;
                makePacket();
                sendPacketToServer(packetSize);
                flag = 0;
            }
            //Сбрасываем кнопку нажатия сообщения
            pressButton = 0;
            if (countMessRecievBuf > 0){
                printSafeMessage();
            }
            //Устанавливаем флаг снова в ноль
            flagMode = 0;

        }
    }
    return 0;
}