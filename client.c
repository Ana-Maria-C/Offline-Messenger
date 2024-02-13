#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>

extern int errno;

char comanda1[200] = "Creare cont";
char comanda2[200] = "Log in";
char comanda3[200] = "Trimite mesaj";
char comanda4[200] = "Vizualizare conversatie";
char comanda5[200] = "Raspunde la mesaj";
char comanda6[200] = "Mesaje noi";
char comanda7[200] = "Log out";
char comanda8[200] = "Exit";

int port;
char utilizator[50];

void creare_cont(int soc)
{
    char username[50];
    char password[50];
    printf("Alegeti un username: \n");
    bzero(username, 50);
    read(0, username, 50);
    write(soc, username, 50);

    printf("Introduceti o parola: \n");
    bzero(password, 50);
    read(0, password, 50);
    write(soc, password, 50);
}

void log_in(int soc)
{
    char username[50];
    char password[50];
    printf("Introduceti username-ul: \n");
    bzero(username, 50);
    read(0, username, 50);
    write(soc, username, 50);

    printf("Introduceti parola: \n");
    bzero(password, 50);
    read(0, password, 50);
    write(soc, password, 50);

    int aux_u, aux_p;
    read(soc, &aux_u, sizeof(aux_u));
    read(soc, &aux_p, sizeof(aux_p));

 
    if (aux_u && aux_p)
    {
        strcpy(utilizator, username);
        int dim_msg;
        read(soc, &dim_msg, sizeof(dim_msg));
        char *old_msg = (char *)malloc(dim_msg);
        read(soc, old_msg, dim_msg);
        printf("Mesaje necitite: %s\n", old_msg);
    }

}

void send_msg(int soc)
{
    char to_username[50];
    int aux = 0;
    bzero(to_username, 50);
    printf("Introduceti utilizatorul caruia doriti sa ii transmiteti mesajul: \n");
    read(0, to_username, 50);
    write(soc, to_username, 50);
    read(soc, &aux, sizeof(aux));

    if (aux) // daca username-ul introdus este corect
    {
        char mesaj[2000];
        bzero(mesaj, 2000);
        printf("Introduceti mesajul: \n");
        read(0, mesaj, 2000);

        int dim_msg = strlen(mesaj);
        mesaj[dim_msg - 1] = '\0';
        write(soc, &dim_msg, sizeof(dim_msg));
        write(soc, mesaj, dim_msg);
    }
}

void answer_msg(int soc)
{
    char user_conv[50];
    int ok=0;
    bzero(user_conv, 50);

    printf("\nIntroduceti username-ul utilizatorului: \n");
    read(0, user_conv, 50);
    write(soc, user_conv, 50);
    read(soc, &ok, sizeof(ok));

    if (ok)
    {
        int rs;
        read(soc, &rs, sizeof(rs));

        if (rs==100)
        {
            int dim_conv;
            int nr_total_msg;
            int id_msg;
            read(soc, &dim_conv, sizeof(dim_conv));
            char *conversatie = (char *)malloc(dim_conv + 1);
            read(soc, conversatie, dim_conv);

            read(soc, &nr_total_msg, sizeof(nr_total_msg));
            printf("%s\nIntroduceti ID-ul mesajului la care doriti sa raspundeti: \n", conversatie);

            scanf("%d", &id_msg);
            write(soc, &id_msg, sizeof(id_msg));

            if (id_msg <= nr_total_msg)
            {
                char reply_msg[1024];
                bzero(reply_msg, 1024);
                printf("Introduceti raspunsul pentru mesajul cu ID-ul: %d\n", id_msg);
                read(0, reply_msg, 1024);
                int dim_reply_msg = strlen(reply_msg);
                write(soc, reply_msg, 1024);
            }
            else
            {
                printf("ID-ul introdus este incorect!.\n");
                return;
            }
        }
    }
}


void show_conv(int soc)
{
    char user_conv[50];
    int aux=0;
    bzero(user_conv, 50);
    printf("Introduceti utilizatorul a carui conversatie doriri sa o vedeti: \n");
    read(0, user_conv, 50);
    write(soc, user_conv, 50);
    read(soc, &aux, sizeof(aux));
    user_conv[strlen(user_conv) - 1] = '\0';

    if (aux)
    {
        int dim_conv;
        read(soc, &dim_conv, sizeof(dim_conv));
        char *conversatie = (char *)malloc(dim_conv + 1);
        read(soc, conversatie, dim_conv);
        printf("\nConversatia cu %s: \n%s\n", user_conv, conversatie);
    }
}



int main(int argc, char *argv[])
{
    int soc;
    struct sockaddr_in server;

    if (argc != 3)
    {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    if ((soc = socket(AF_INET, SOCK_STREAM, 6)) == -1)
    {
        perror("Eroare la socket.\n");
        return -1;
    }

    port = atoi(argv[2]);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    if (connect(soc, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("Eroare la connect.\n");
        return -1;
    }

    char msg[1024];
    bzero(msg, 1024);
    if (read(soc, msg, 1024) < 0)
    {
        perror("Eroare1 la citire de la server.\n");
        return 0;
    }
    printf("%s\n", msg);

    char comanda[200];
    bzero(comanda, 200);
    while (read(0, comanda, 200))
    {
        comanda[strlen(comanda) - 1] = '\0';
        fflush(stdout);
        write(soc, comanda, 200);

        int este_conectat = 0;
        char rasp[200];
        bzero(rasp, 200);

        int c = 0; // comanda clientului
        int i;

        for (i = 0; i < strlen(comanda1) && comanda[i] == comanda1[i]; i++);
            if (i == strlen(comanda1))
                c = 1;

        for (i = 0; i < strlen(comanda2) && comanda[i] == comanda2[i]; i++);
            if (i == strlen(comanda2))
                c = 2;

        for (i = 0; i < strlen(comanda3) && comanda[i] == comanda3[i]; i++);
            if (i == strlen(comanda3))
                c = 3;

        for (i = 0; i < strlen(comanda4) && comanda[i] == comanda4[i]; i++);
            if (i == strlen(comanda4))
                c = 4;

        for (i = 0; i < strlen(comanda5) && comanda[i] == comanda5[i]; i++);
            if (i == strlen(comanda5))
                c = 5;

        for (i = 0; i < strlen(comanda6) && comanda[i] == comanda6[i]; i++);
            if (i == strlen(comanda6))
                c = 6;

        for (i = 0; i < strlen(comanda7) && comanda[i] == comanda7[i]; i++);
            if (i == strlen(comanda7))
                c = 7;

        for (i = 0; i < strlen(comanda8) && comanda[i] == comanda8[i]; i++)
            ;
        if (i == strlen(comanda8))
            c = 8;

        //printf("c= %d\n",c);

        switch (c)
        {
        case 1: // creare cont

            //printf("cont nou\n");
            creare_cont(soc);
            break;

        case 2: // log in

            read(soc, &este_conectat, sizeof(este_conectat));

            if(!este_conectat)
            {
                log_in(soc);
            }

            break;

        case 3: // trmite mesaj

            read(soc, &este_conectat, sizeof(este_conectat));

            if(este_conectat)
            {
                send_msg(soc);
            }

            break;

        case 4: // vizualizare conversatie

            read(soc, &este_conectat, sizeof(este_conectat));

            if(este_conectat)
            {
                show_conv(soc);
            }
            
            break;

        case 5: // Raspunde la mesaj

            read(soc, &este_conectat, sizeof(este_conectat));

            if(este_conectat)
            {
                answer_msg(soc);
            }
            
            break;

        case 6: // Mesaje noi

            read(soc, &este_conectat, sizeof(este_conectat));

            break;

        case 7: // log out

            if(este_conectat)
            {
                strcpy(utilizator, " ");
            }
            
            break;

        case 8: // exit

            close(soc);
            return 0;
            break;

        default: ;
            
            //printf("1");
            int dim;
            read(soc, &dim, sizeof(dim));
            char *lista = (char *)malloc(dim + 1);
            read(soc, lista, dim);
            printf("%s\n", lista);
            break;
        }

        bzero(rasp, 200);
        read(soc, rasp, 200);
        //printf("%d\n", strlen(rasp));

        printf("%s\n", rasp);
        fflush(stdout);

        int conect = 0;
        read(soc, &conect, sizeof(conect));

        if (conect)
        {
            int dim_new_msg;
            read(soc, &dim_new_msg, sizeof(dim_new_msg));
            char *new_msg = (char *)malloc(dim_new_msg);
            read(soc, new_msg, dim_new_msg);
            if (strcmp(new_msg, "Nu aveti mesaje necitite!"))
                printf("Mesaje necitite: %s\n", new_msg);
        }
        bzero(comanda, 200);
    }
    close(soc);
}

