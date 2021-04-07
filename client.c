#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include "helpers.h"
#include "requests.h"
#include "parson.h"

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define HOST "ec2-3-8-116-10.eu-west-2.compute.amazonaws.com"
#define PORT 8080
#define URL "/api/v1/tema"

#define USER "adrianDrag20"
#define PASS "123456789"
#define TIMEOUT "Too many requests"

//#define DEBUG

char stringIP[16];

void usage(char *s) {
    printf("Unknown command: %s"
           "Commands:\n\tregister\n\tlogin\n\tenter_library\n\tget_books\n\t"
           "get_book\n\tadd_book\n\tdelete_book\n\tlogout\n\texit\n", s);
}
/**
 *  Primeste un numar dat de pointeri char* si eliberarza memoria alocata la
 * adresele respective daca e cazul
 */
void freeAll(int num, ...) {
    va_list argv;
    va_start(argv, num);
    char *p;
    for (int i = 0; i < num; i++) {
        p = va_arg(argv, char *);
        if (p)
            free(p);
    }
}
/**
 *  Toate funtciile care trimit cereri la server deschide si inchide
 * conexiunea de la o adresa deja stiuta.
 *  In cazul incare acesta intoarce o eroare aceasta este afisata.
 */

/**
 *  Creaza un obiect JSON cu username-ul si parola care va constitui payloadul,
 */
void registerRequest(char *username, char *password) {
    int sockfd;
    char *message, *response, *payload, *p;
    JSON_Value *value = json_value_init_object();
    printf("%s %s\n", username, password);

    JSON_Object *object = json_value_get_object(value);
    json_object_set_string(object, "username", username);
    json_object_set_string(object, "password", password);
    payload = json_serialize_to_string(value);
    json_value_free(value);

    sockfd = open_connection(stringIP, PORT, AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return;
    message = compute_post_request(stringIP, URL"/auth/register",
            "application/json", &payload, 1, NULL, 0, NULL);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);

    p = strstr(response, TIMEOUT);
    if (p)
        fprintf(stderr, "%s\n", p);

    p = strstr(response, "\n\r");
    value = json_parse_string(p);
    object = json_value_get_object(value);
    if (json_object_get_string(object, "error"))
        fprintf(stderr, "%s\n", json_object_get_string(object, "error"));

    close_connection(sockfd);
    freeAll(3, message, response, payload);
    json_value_free(value);
}
/**
 *  Creaza un obiect JSON cu username-ul si parola care va constitui payloadul,
 *  Intoarce cookie-ul dat de server pentru cererile urmatoare
 */
char *loginRequest(char *username, char *password) {
    int sockfd;
    char *message, *response, *payload, *p, *cookie = NULL;
    JSON_Value *value = json_value_init_object();

    JSON_Object *object = json_value_get_object(value);
    json_object_set_string(object, "username", username);
    json_object_set_string(object, "password", password);
    payload = json_serialize_to_string(value);
    json_value_free(value);

    sockfd = open_connection(stringIP, PORT, AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return NULL;
    message = compute_post_request(stringIP, URL"/auth/login",
            "application/json", &payload, 1, NULL, 0, NULL);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);

    p = strstr(response, TIMEOUT);
    if (p)
        fprintf(stderr, "%s\n", p);
    else {
        p = strstr(response, "\n\r");
        value = json_parse_string(p);
        object = json_value_get_object(value);
        if (json_object_get_string(object, "error"))
            fprintf(stderr, "%s\n", json_object_get_string(object, "error"));
        else {
            p = strstr(response, " connect.sid=");
            p = strtok(p, ";");
            cookie = (char *) malloc(strlen(p));
            strcpy(cookie, p);
        }
    }
    close_connection(sockfd);
    freeAll(3, message, response, payload);
    json_value_free(value);
    return cookie;
}
/**
 *  Intoarce token-ul jwt primit de la server.
 */
char *accessRequest(char *cookie, char *token) {
    int sockfd;
    char *message, *response, *p;

    sockfd = open_connection(stringIP, PORT, AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return NULL;
    message = compute_get_request(
            stringIP, URL"/library/access", NULL, cookie, NULL);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);

    // gasetse inceputul payloadului si in pune intr-un obiect JSON
    p = strstr(response, "\n\r");
    JSON_Value *value = json_parse_string(p);
    JSON_Object *object = json_value_get_object(value);
    if (json_object_get_string(object, "error"))
        fprintf(stderr, "%s\n", json_object_get_string(object, "error"));
    else {
        token = malloc(strlen(p));
        strcpy(token, (char *) json_object_get_string(object, "token"));
    }
    json_value_free(value);

    close_connection(sockfd);
    freeAll(2, message, response);
    return token;
}

void getAllBooks(char *cookie, char *token) {
    int sockfd;
    char *message;
    char *response;

    sockfd = open_connection(stringIP, PORT, AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return;
    message = compute_get_request(stringIP, URL"/library/books", NULL, cookie, token);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);

    // gasetse inceputul payloadului si in pune intr-un obiect JSON
    char *p = strstr(response, "\n\r");
    JSON_Value *value = json_parse_string(p);
    JSON_Object *object = json_value_get_object(value);
    if (json_object_get_string(object, "error"))
        fprintf(stderr, "%s\n", json_object_get_string(object, "error"));
    else
        printf("%s\n", json_serialize_to_string_pretty(value));

    close_connection(sockfd);
    freeAll(2, message, response);
}

void getBook(char *cookie, char *token, int id) {
    int sockfd;
    char *message, *response, *p, url[30];
    sprintf(url, "%s%d", URL"/library/books/", id);

    sockfd = open_connection(stringIP, PORT, AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return;
    message = compute_get_request(stringIP, url, NULL, cookie, token);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);

    // gasetse inceputul payloadului si in pune intr-un obiect JSON
    p = strstr(response, "\n\r");
    JSON_Value *value = json_parse_string(p);
    JSON_Object *object = json_value_get_object(value);
    if (json_object_get_string(object, "error"))
        fprintf(stderr, "%s\n", json_object_get_string(object, "error"));
    else
        printf("%s\n", json_serialize_to_string_pretty(value));

    close_connection(sockfd);
    freeAll(2, message, response);
}
/**
 *  Primeste o valoare json care este legat la un obiect procesat in main
 */
void addBook(char *cookie, char *token, JSON_Value *value) {
    int sockfd;
    char *message, *response, *p, *payload = json_serialize_to_string(value);

    sockfd = open_connection(stringIP, PORT, AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return;
    message = compute_post_request(stringIP, URL"/library/books",
            "application/json", &payload, 1, &cookie, 1, token);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);

    // gasetse inceputul payloadului si in pune intr-un obiect JSON
    p = strstr(response, "\n\r");
    JSON_Value *value2 = json_parse_string(p);
    JSON_Object *object = json_value_get_object(value2);
    if (json_object_get_string(object, "error"))
        fprintf(stderr, "%s\n", json_object_get_string(object, "error"));

    json_value_free(value);
    json_value_free(value2);
    freeAll(3, message, response, payload);
}

void deleteBook(char *cookie, char *token, int id) {
    int sockfd;
    char *message, *response, *p, url[30];
    sprintf(url, "%s%d", URL"/library/books/", id);

    sockfd = open_connection(stringIP, PORT, AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return;
    message = compute_delete_request(stringIP, url, NULL, cookie, token);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);

    // gasetse inceputul payloadului si in pune intr-un obiect JSON
    p = strstr(response, "\n\r");
    JSON_Value *value = json_parse_string(p);
    JSON_Object *object = json_value_get_object(value);

    if (json_object_get_string(object, "error"))
        fprintf(stderr, "%s\n", json_object_get_string(object, "error"));

    json_value_free(value);
    freeAll(2, message, response);
}

void logoutRequest(char *cookie, char *token) {
    int sockfd;
    char *message, *response, *p;

    sockfd = open_connection(stringIP, PORT, AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return;
    message = compute_get_request(
            stringIP, URL"/auth/logout", NULL, cookie, NULL);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);

    // gasetse inceputul payloadului si in pune intr-un obiect JSON
    p = strstr(response, TIMEOUT);
    if (p)
        fprintf(stderr, "%s\n", p);
    else {
        p = strstr(response, "\n\r");
        JSON_Value *value = json_parse_string(p);
        JSON_Object *object = json_value_get_object(value);
        if (json_object_get_string(object, "error"))
            fprintf(stderr, "%s\n", json_object_get_string(object, "error"));

        json_value_free(value);
    }
    close_connection(sockfd);
    freeAll(4, message, response, cookie, token);
    // ca dupa exit ca nu se incerce sa fie elibrate le setez la NULL
    cookie = NULL;
    token = NULL;
}

int main(int argc, char *argv[]) {

    struct addrinfo hints, *result;
    char stringPort[10];
    sprintf(stringPort, "%d", PORT);

    hints.ai_addr = NULL;
    hints.ai_addrlen = 0;
    hints.ai_flags = 0;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_next = NULL;
    hints.ai_canonname = NULL;

    int ret = getaddrinfo(HOST, NULL, &hints, &result);
    DIE(ret < 0, gai_strerror(ret));

    inet_ntop(AF_INET, &((struct sockaddr_in *) result->ai_addr)->sin_addr.s_addr, stringIP, 16);
    freeaddrinfo(result);

    char input[30];
    int status = 1;
    //  in cazul in care nu se logeaza sau nu se intra in bibleoteca
    //  nu se vor elibera aceste piontere
    char *loginCookie = NULL, *token = NULL;

    while (status) {
        scanf("%s", input);
        if (strncmp(input, "register", 8) == 0) {
            char username[50] = USER;
            char password[50] = PASS;
#ifndef DEBUG
            printf("username=");
            scanf("%s", username);
            printf("password=");
            scanf("%s", password);
#endif
            registerRequest(username, password);
            continue;
        }

        if (strncmp(input, "login", 5) == 0) {
            char username[50] = USER;
            char password[50] = PASS;
#ifndef DEBUG
            printf("username=");
            scanf("%s", username);
            printf("password=");
            scanf("%s", password);
#endif
            loginCookie = loginRequest(username, password);
            continue;
        }

        if (strncmp(input, "enter_library", 13) == 0) {
            token = accessRequest(loginCookie, token);
            continue;
        }

        if (strncmp(input, "get_books", 9) == 0) {
            getAllBooks(loginCookie, token);
            continue;
        }

        if (strncmp(input, "get_book", 8) == 0) {
            int id;
            char s[10], *ptr;

            printf("id=");
            scanf("%s", s);
            while ((id = (int) strtol(s, &ptr, 10)) == 0) {
                printf("Invalid number: %s. Please enter a valid number.\nid=", s);
                scanf("%s", s);
            }
            getBook(loginCookie, token, id);
            continue;
        }

        if (strncmp(input, "add_book", 8) == 0) {
            char title[50] = "TestBook";
            char author[50] = "student";
            char genre[50] = "Comedy";
            char publisher[50] = "UPB";
            char s[10], *ptr;
            int page_count = 10;
#ifndef DEBUG
            printf("title=");
            scanf("%s", title);
            printf("author=");
            scanf("%s", author);
            printf("genre=");
            scanf("%s", genre);
            printf("publisher=");
            scanf("%s", publisher);
            printf("page_count=");
            scanf("%s", s);
            while ((page_count = (int) strtol(s, &ptr, 10)) == 0) {
                printf("Invalid number: %s. Please enter a valid number.\npage_count=", s);
                scanf("%s", s);
            }
#endif
            JSON_Value *value = json_value_init_object();
            JSON_Object *object = json_value_get_object(value);
            json_object_set_string(object, "title", title);
            json_object_set_string(object, "author", author);
            json_object_set_string(object, "publisher", publisher);
            json_object_set_string(object, "genre", genre);
            json_object_set_number(object, "page_count", page_count);

            addBook(loginCookie, token, value);
            continue;
        }

        if (strncmp(input, "delete_book", 11) == 0) {
            int id;
            char s[10], *ptr;

            printf("id=");
            scanf("%s", s);
            while ((id = (int) strtol(s, &ptr, 10)) == 0) {
                printf("Invalid number: %s. Please enter a valid number.\nid=", s);
                scanf("%s", s);
            }
            deleteBook(loginCookie, token, id);
            continue;
        }

        if (strncmp(input, "logout", 6) == 0) {
            logoutRequest(loginCookie, token);
            continue;
        }

        if (strncmp(input, "exit", 4) == 0) {
            status = 0;
            continue;
        }
        usage(input);
    }
    freeAll(2, token, loginCookie);

    return 0;
}
