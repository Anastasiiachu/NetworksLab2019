#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <pthread.h>

#define size_time 5
#define buff_time 40
//Максимальная длина сообщения
#define buffMessage 512
//Максимальная длина имени пользователя
#define maxNameSize 16
#define maxMess 512

//Флаг режима работы клиента 0 - принимать сообщения: 1 - отправлять сообщения
int flagMode = 0;

//Сохранение сообщений в очередь
char *buffMessRecieve[maxMess];
int countMessRecievBuf = 0;

//Определение мьютекса
pthread_mutex_t mutex;

//Функция закрытия клиента
void stopClient(int socket){
    shutdown(socket,SHUT_RDWR);
    close(socket);
    pthread_exit(NULL);
    exit(1);
}

//Получение сообщений с сервера
char *recieveMessage(int socket){

    int n;
    int length = 0;
    //Получаем размер сообщения
    n = read(socket, &length, sizeof(int));
    if (n <= 0) {
        perror("ERROR reading from socket\n");
        stopClient(socket);
    }

    //Выделяем память на прием сообщения
    char *result = (char*) malloc(length*sizeof(char));
    if(length > 0){
        //Получаем само сообщение
        n = read(socket, result, length);
        if (n <= 0) {
            perror("ERROR reading from socket\n");
            stopClient(socket);

        }
    }

    return result;
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

//Отправка сообщения серверу
void sendMessage(int sizeMess, char* message, int socket){

    //Отправляем размер сообщения
    int n;
    n = write(socket, &sizeMess, sizeof(int));
    if (n < 0) {
        perror("ERROR writing to socket");
        stopClient(socket);
    }

    //Отправляем само сообщение
    n = write(socket, message, sizeMess);
    //printf("Записано сообщение длиной %d\n",n);
    if (n < 0) {
        perror("ERROR writing to socket");
        stopClient(socket);
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

//Поток на чтение сообщения
void* readThread(void* sock){
    char *recMessage;
    int socket = *(int *) sock;

    //Получаем сообщения от сервера
    while(1){
        recMessage = recieveMessage(socket);
        if(!flagMode){
            printMessage("", recMessage, flagMode);
        }
        else{
            safeMess(recMessage);
        }
        free(recMessage);
    }

}

//argv[1] - name ; argv[2] - host; argv[3] - port
//argc - argument count
int main(int argc, char *argv[]) {

    int sockfd, n;
    uint16_t portno;
    struct sockaddr_in serv_addr; //адрес сервера
    struct hostent *server; //хост сервера

    //Структуры для изменения режима работы терминала (по нажатию что-то происходит)
    struct termios initial_settings, new_settings;
    //Идентификатор потока, есть поток на чтение и поток на запись (у клиента 2 потока)
    pthread_t readThr;
    char pressButton;
    //Имя клиента
    char *clientName;

    char bufferSendMessage[buffMessage];

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

    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy(server->h_addr, (char *) &serv_addr.sin_addr.s_addr, (size_t) server->h_length);
    serv_addr.sin_port = htons(portno);


    /* Подключаемся к серверу */
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        stopClient(sockfd);
    }

    //отправка имени серверу
    sendMessage(strlen(clientName), clientName, sockfd);

    //Создаем поток на чтение данных с сервера
    if(pthread_create(&readThr, NULL, readThread, &sockfd) < 0 ){
        printf("ERROR");
        exit(1);
    }

    //Отправка сообщений другим клиентам
    while(1){
        //Если флаг в 1 то вывод заблокирован если флаг в 0 вывод разрешен
        if(!flagMode){

            /*Изменение режима работы консоли
            * Написание этого фрагмента программы использовалось с помощью сайта:
            * http://rus-linux.net/nlib.php?name=/MyLDP/BOOKS/Linux-tools/10/ltfwp-10-22.html
            */
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
                break;
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

            bufferSendMessage[strlen(bufferSendMessage)-1] = 0;

            if (lengthMess > 1)
                sendMessage(lengthMess, bufferSendMessage, sockfd);

            pressButton = 0;

            if (countMessRecievBuf > 0){
                printSafeMessage();
            }

            flagMode = 0;

        }
    }

    return 0;
}