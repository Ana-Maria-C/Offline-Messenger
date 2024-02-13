#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#define PORT 2024

extern int errno;
pthread_t th[200]; // ID-ul thread-urilor ce se vor ocupa de fiecare client

char comenzi_active[1024] = "Introduceti o comanda: \n"
                            "1. Creare cont\n"
                            "2. Log in\n"
                            "3. Trimite mesaj\n"
                            "4. Vizualizare conversatie\n"
                            "5. Raspunde la mesaj\n"
                            "6. Mesaje noi\n"
                            "7. Log out\n"
                            "8. Exit\n";

char comanda1[200] = "Creare cont";
char comanda2[200] = "Log in";
char comanda3[200] = "Trimite mesaj";
char comanda4[200] = "Vizualizare conversatie";
char comanda5[200] = "Raspunde la mesaj";
char comanda6[200] = "Mesaje noi";
char comanda7[200] = "Log out";
char comanda8[200] = "Exit";

typedef struct conect
{
    char username[50];
    int descriptor;
} C;

struct conect utilizatori[100];
int count_connected = 0;
char utilizator[50] = "";

sqlite3 *db;
int dbproject;
char *err_msg;

typedef struct thData
{
    int idThread;
    int cl;
} thData;

void messenger(int, int);

static void *treat(void *arg)
{
    struct thData thd;
    thd = *((struct thData *)arg);
    printf("[Thread %d] - Se asteapta o comanda!\n", thd.idThread);
    fflush(stdout);
    pthread_detach(pthread_self());
    messenger(thd.cl, thd.idThread);
    close((intptr_t)arg);
    return 0;
}

// verificam daca exista usernameul in baza de date (username-ul introdus de client este corect)
int validare_username(char username[50])
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT username FROM users WHERE username = ?1;", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const char *check_username = sqlite3_column_text(stmt, 0);
        if (!strcmp(check_username, username))
            return 1;
    }
    return 0;
}

void creare_cont(int cl, int idThread, char rasp[], int *conectat)
{
    char username[50];
    char password[50];
    int rc;
    bzero(username, 50);
    bzero(password, 50);
    read(cl, username, 50);
    read(cl, password, 50);
    username[strlen(username) - 1] = '\0';
    password[strlen(password) - 1] = '\0';

    // printf("aici1");
    sqlite3_stmt *stmt1;
    sqlite3_prepare_v2(db, "SELECT username, password FROM users WHERE username = ?1;", -1, &stmt1, NULL);
    sqlite3_bind_text(stmt1, 1, username, -1, SQLITE_STATIC);
    // printf("aici2");

    if ((rc = sqlite3_step(stmt1)) == SQLITE_ROW)
    {
        strcpy(rasp, "Utilizatorul exista deja! Alegeti alt username!");
    }
    else
    {
        printf("A fost creat contul:: username = %s, password = %s\n", username, password);
        sqlite3_stmt *stmt2;
        sqlite3_prepare_v2(db, "INSERT INTO USERS VALUES(?1, ?2);", -1, &stmt2, NULL);
        sqlite3_bind_text(stmt2, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt2, 2, password, -1, SQLITE_STATIC);
        sqlite3_step(stmt2);
        strcpy(rasp, "Utilizatorul a fost creat!\n");
    }
}

void log_in(int cl, int idThread, char rasp[], int *conectat, char utilizator[])
{
    int rc;
    char username[50];
    char password[50];
    bzero(username, 50);
    bzero(password, 50);
    read(cl, username, 50);
    read(cl, password, 50);
    username[strlen(username) - 1] = '\0';
    password[strlen(password) - 1] = '\0';

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT username, password FROM users WHERE username = ?1;", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    const char *check_password;
    const char *check_username;
    int check_usr = 0;
    //  verificam daca username-ul este corect
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        // printf("aici 1");
        check_username = sqlite3_column_text(stmt, 0);
        check_password = sqlite3_column_text(stmt, 1);
        if (!strcmp(check_username, username))
            {
                check_usr = 1; // am gasit utilizatorul
                break;
            }
    }

    write(cl, &check_usr, sizeof(check_usr));

    if (!check_usr)
    {
        strcpy(rasp, "    Username-ul introdus este incorect!");
    }
    else
    {
        // verificam corectitudinea parolei
        int check_pass = 0;
        if (!strcmp(check_password, password))
            check_pass = 1;
        write(cl, &check_pass, sizeof(check_pass));

        if (!check_pass)
        {
            strcpy(rasp, "Parola introdusa este gresita!");
        }
        else
        {
            // printf("aici 3");
            strcpy(rasp, "V-ati conectat!");
            strcpy(utilizatori[count_connected].username, username);
            utilizatori[count_connected++].descriptor = cl;
            *conectat = 1;
            strcpy(utilizator, username);
            printf("S-a conectat: %s\n", utilizator);

            sqlite3_stmt *msgs;
            sqlite3_prepare_v2(db, "SELECT * FROM sent_msg WHERE receiver = ?1", -1, &msgs, NULL);
            sqlite3_bind_text(msgs, 1, utilizator, -1, SQLITE_STATIC);

            int rd;
            char *old_msg = "";
            while ((rd = sqlite3_step(msgs)) == SQLITE_ROW)
            {
                const char *msg = sqlite3_column_text(msgs, 2);
                const char *from = sqlite3_column_text(msgs, 0);
                char *new_msg = (char *)malloc(strlen(old_msg) + strlen(msg) + strlen(from) + 2);
                strcpy(new_msg, old_msg);
                strcat(new_msg, "\n");
                strcat(new_msg, from);
                strcat(new_msg, ": ");
                strcat(new_msg, msg);
                old_msg = (char *)malloc(strlen(new_msg));
                strcpy(old_msg, new_msg);
            }
            sqlite3_finalize(msgs);

            int dim_old_msg = strlen(old_msg);
            if (dim_old_msg == 0)
            {
                old_msg = (char *)malloc(strlen("Nu aveti mesaje necitite!"));
                strcpy(old_msg, "Nu aveti mesaje necitite!");
                dim_old_msg = strlen(old_msg);
            }
            write(cl, &dim_old_msg, sizeof(dim_old_msg));
            write(cl, old_msg, dim_old_msg);

            sqlite3_stmt *msgs_;
            sqlite3_prepare_v2(db, "DELETE FROM sent_msg WHERE receiver = ?1", -1, &msgs_, NULL);
            sqlite3_bind_text(msgs_, 1, utilizator, -1, SQLITE_STATIC);
            sqlite3_step(msgs_);
        }
    }
    sqlite3_finalize(stmt);
}

// functia pastreaza mesajele unui user atunci cand se deconecteaza

void onmsg_to_ofmsg(char utilizator[])
{
    // cautam mesajele userului curent
    sqlite3_stmt *stmt1;
    sqlite3_prepare_v2(db, "SELECT sender, msg FROM received_msg WHERE receiver=?1;", -1, &stmt1, NULL);
    sqlite3_bind_text(stmt1, 1, utilizator, -1, SQLITE_STATIC);

    int rc;
    while ((rc = sqlite3_step(stmt1)) == SQLITE_ROW)
    {
        // copiem mesajele intr-un alt tabel pentru a nu se pierde dupa deconectare
        const char *s = sqlite3_column_text(stmt1, 0);
        const char *msg = sqlite3_column_text(stmt1, 1);

        sqlite3_stmt *cpy;
        sqlite3_prepare_v2(db, "INSERT INTO sent_msg VALUES(?1, ?2, ?3);", -1, &cpy, NULL);
        sqlite3_bind_text(cpy, 1, s, -1, SQLITE_STATIC);
        sqlite3_bind_text(cpy, 2, utilizator, -1, SQLITE_STATIC);
        sqlite3_bind_text(cpy, 3, msg, -1, SQLITE_STATIC);
        sqlite3_step(cpy);
        sqlite3_finalize(cpy);
    }
    sqlite3_finalize(stmt1);

    // stergem mesajele din tabelul cu mesaje primite
    sqlite3_stmt *stmt3;
    sqlite3_prepare_v2(db, "DELETE FROM received_msg WHERE receiver=?1;", -1, &stmt3, NULL);
    sqlite3_bind_text(stmt3, 1, utilizator, -1, SQLITE_STATIC);
    sqlite3_step(stmt3);
    sqlite3_finalize(stmt3);
}

void send_msg(int cl, int idThread, char rasp[], char utilizator[])
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "CREATE TABLE sent_msg(SENDER TEXT NOT NULL, RECEIVER TEXT NOT NULL, MSG TEXT NOT NULL)", -1, &stmt, NULL);
    sqlite3_step(stmt);

    sqlite3_stmt *stmt8;
    sqlite3_prepare_v2(db, "CREATE TABLE received_msg(SENDER TEXT NOT NULL, RECEIVER TEXT NOT NULL, MSG TEXT NOT NULL)", -1, &stmt8, NULL);
    sqlite3_step(stmt8);

    sqlite3_stmt *stmt3;
    sqlite3_prepare_v2(db, "CREATE TABLE CONVERSATII(USER1 TEXT, USER2 TEXT, COUNT INTEGER, CONV TEXT, primary key(USER1, USER2))", -1, &stmt3, NULL);
    sqlite3_step(stmt3);

    char to_username[50];
    bzero(to_username, 50);
    read(cl, to_username, 50);

    to_username[strlen(to_username) - 1] = '\0';
    int aux = validare_username(to_username);

    write(cl, &aux, sizeof(validare_username(to_username)));

    if (!validare_username(to_username))
    {
        strcpy(rasp, "Username-ul introdus nu exista!");
    }
    else
    {
        int dimensiune;
        char mesaj[1000];

        read(cl, &dimensiune, sizeof(dimensiune));
        read(cl, mesaj, dimensiune);

        int este_conectat = 0;
        for (int i = 0; i < count_connected && !este_conectat; i++)
        {
            if (!strcmp(to_username, utilizatori[i].username))
            {
                este_conectat = 1;
            }
        }
        // daca user-ul  cu care vrem sa comunicam este conectat
        if (este_conectat)
        {
            sqlite3_stmt *stmt9;
            sqlite3_prepare_v2(db, "INSERT INTO received_msg VALUES(?1, ?2, ?3);", -1, &stmt9, NULL);
            sqlite3_bind_text(stmt9, 1, utilizator, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt9, 2, to_username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt9, 3, mesaj, -1, SQLITE_STATIC);
            sqlite3_step(stmt9);
        }
        else // daca user-ul  cu care vrem sa comunicam nu este conectat
        {
            sqlite3_stmt *stmt2;
            printf("%s to %s mesajul: %s\n", utilizator, to_username, mesaj);
            sqlite3_prepare_v2(db, "INSERT INTO sent_msg VALUES(?1, ?2, ?3);", -1, &stmt2, NULL);
            sqlite3_bind_text(stmt2, 1, utilizator, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt2, 2, to_username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt2, 3, mesaj, -1, SQLITE_STATIC);
            sqlite3_step(stmt2);
        }

        strcpy(rasp, "Mesajul a fost trimis!");

        // adaugam mesajul trimis la conversatia clinetilor (il inseram in tabela)
        sqlite3_stmt *stmt4;
        sqlite3_prepare_v2(db, "SELECT conv, count FROM CONVERSATII WHERE (USER1=?1 AND USER2=?2) OR (USER1=?2 AND USER2=?1);", -1, &stmt4, NULL);
        sqlite3_bind_text(stmt4, 1, utilizator, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt4, 2, to_username, -1, SQLITE_STATIC);

        int rc;
        if (!((rc = sqlite3_step(stmt4)) == SQLITE_ROW)) // incepem conversatia(id-ul mesajului trimis va fi 1))
        {
            int id_msg = 1;
            sqlite3_stmt *stmt5;
            sqlite3_prepare_v2(db, "INSERT INTO CONVERSATII VALUES(?1, ?2, ?3, ?4);", -1, &stmt5, NULL);
            char *number = "1";
            char *new_msg = (char *)malloc(strlen(mesaj) + strlen(utilizator) + strlen(number) + 2 + strlen("ID_mesaj=") + strlen(" from ") + 3);
            strcpy(new_msg, "ID_mesaj=");
            strcat(new_msg, number);
            strcat(new_msg, " from ");
            strcat(new_msg, utilizator);
            strcat(new_msg, ": ");
            strcat(new_msg, mesaj);

            sqlite3_bind_text(stmt5, 1, utilizator, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt5, 2, to_username, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt5, 3, id_msg);
            sqlite3_bind_text(stmt5, 4, new_msg, -1, SQLITE_STATIC);
            sqlite3_step(stmt5);
        }
        else // cei doi clienti au mai vorbit
        {
            char nr[10];
            int id_msg = sqlite3_column_int(stmt4, 1);
            char *number;
            char *conv = (char *)sqlite3_column_text(stmt4, 0);

            // incrementam id-ul mesajului nou trimis
            id_msg++;

            sprintf(nr, "%d", id_msg);
            number = (char *)malloc(strlen(nr + 1));
            strcpy(number, nr);

            char *new_msg = (char *)malloc(strlen(utilizator) + strlen(mesaj) + strlen(number) + 2 + strlen("ID_mesaj=") + strlen(" from "));
            strcpy(new_msg, "ID_mesaj=");
            strcat(new_msg, number);
            strcat(new_msg, " from ");
            strcat(new_msg, utilizator);
            strcat(new_msg, ": ");
            strcat(new_msg, mesaj);

            char *new_conv = (char *)malloc(strlen(conv) + strlen(new_msg));
            strcpy(new_conv, conv);
            strcat(new_conv, "\n");
            strcat(new_conv, new_msg);

            sqlite3_stmt *stmt5;
            sqlite3_prepare_v2(db, "UPDATE CONVERSATII SET CONV=?1, COUNT=?2 WHERE (USER1=?3 AND USER2=?4) OR (USER1=?4 AND USER2=?3);", -1, &stmt5, NULL);
            sqlite3_bind_text(stmt5, 1, new_conv, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt5, 2, id_msg);
            sqlite3_bind_text(stmt5, 3, utilizator, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt5, 4, to_username, -1, SQLITE_STATIC);
            sqlite3_step(stmt5);
        }
    }
}

void show_conv(int cl, char rasp[], char utilizator[])
{
    // printf("a ajuns1");
    char user_conv[50];
    int rc;
    bzero(user_conv, 50);

    read(cl, user_conv, 50);

    user_conv[strlen(user_conv) - 1] = '\0';
    int aux = validare_username(user_conv);

    write(cl, &aux, sizeof(validare_username(user_conv)));
    // printf("a ajuns2");

    if (!validare_username(user_conv))
    {
        strcpy(rasp, "Username-ul introdus nu exista!");
        // printf("a ajuns3");
    }
    else // daca usernamul exista, cautam conversatia
    {
        sqlite3_stmt *stmt2;
        sqlite3_prepare_v2(db, "SELECT conv FROM CONVERSATII WHERE (USER1=?1 AND USER2=?2) OR (USER1=?2 AND USER2=?1);", -1, &stmt2, NULL);
        sqlite3_bind_text(stmt2, 1, utilizator, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt2, 2, user_conv, -1, SQLITE_STATIC);

        if (!((rc = sqlite3_step(stmt2)) == SQLITE_ROW))
        {
            int dimensiune_conv = strlen("Nu aveti inca o conversatie cu user-ul introdus");

            char *conversatie = (char *)malloc(dimensiune_conv);
            strcpy(conversatie, "Nu aveti inca o conversatie cu user-ul introdus!");
            write(cl, &dimensiune_conv, sizeof(dimensiune_conv));
            write(cl, conversatie, dimensiune_conv);
        }
        else
        {
            char *conv = (char *)sqlite3_column_text(stmt2, 0);

            char *conversatie = (char *)malloc(strlen(conv) + 1);
            strcpy(conversatie, conv);
            int dimensiune_conv = strlen(conv);

            write(cl, &dimensiune_conv, sizeof(dimensiune_conv));
            write(cl, conversatie, dimensiune_conv);
        }
        strcpy(rasp, "Aceasta este conversatia cu ");
        strcat(rasp, user_conv);
    }
}

void answer_msg(int cl, int idThread, char rasp[], char utilizator[])
{
    char user_conv[50];
    bzero(user_conv, 50);

    read(cl, user_conv, 50);
    user_conv[strlen(user_conv) - 1] = '\0';
    int aux = validare_username(user_conv);

    write(cl, &aux, sizeof(validare_username(user_conv)));

    if (!validare_username(user_conv))
    {
        strcpy(rasp, "Username-ul introdus nu exista!");
    }
    else
    {
        sqlite3_stmt *conv;
        sqlite3_prepare_v2(db, "SELECT conv, count FROM CONVERSATII WHERE (USER1=?1 AND USER2=?2) OR (USER1=?2 AND USER2=?1);", -1, &conv, NULL);
        sqlite3_bind_text(conv, 1, utilizator, -1, SQLITE_STATIC);
        sqlite3_bind_text(conv, 2, user_conv, -1, SQLITE_STATIC);

        int rs = sqlite3_step(conv);
        write(cl, &rs, sizeof(rs));

        // pentru a raspunde la un mesaj intre cei doi clienti trebuie sa existe deja o conversatie

        if (!(rs == SQLITE_ROW)) // verificam daca exista o conversatie intre cei doi clienti
        {
            strcpy(rasp, "Nu exista inca o conversatie cu utilizatorul ");
            strcat(rasp, user_conv);
        }
        else // exista o conversatie intre clienti
        {
            char *mesaje = (char *)sqlite3_column_text(conv, 0);
            int nr_total_msg = sqlite3_column_int(conv, 1);
            char *conversatie = (char *)malloc(strlen(mesaje) + 1);
            strcpy(conversatie, mesaje);
            int id_msg;
            int dimensiune_conv = strlen(mesaje);

            write(cl, &dimensiune_conv, sizeof(dimensiune_conv));
            write(cl, conversatie, dimensiune_conv);
            write(cl, &nr_total_msg, sizeof(nr_total_msg));
            read(cl, &id_msg, sizeof(id_msg));

            if (id_msg > nr_total_msg) // exista mesajul cu id-ul selectat de client
            {
                strcpy(rasp, "Nu exista mesajul cu ID-ul ");
                strcat(rasp, id_msg);
            }
            else
            {
                char reply_msg[1024];
                bzero(reply_msg, 1024);
                read(cl, reply_msg, 1024);
                reply_msg[strlen(reply_msg) - 1] = '\0';
                printf("%s\n", reply_msg);

                int este_conectat = 0;
                for (int i = 0; i < count_connected; i++)
                {
                    if (strcmp(user_conv, utilizatori[i].username) == 0)
                    {
                        este_conectat = 1;
                        break;
                    }
                }

                char *number;
                char nr[10];
                sprintf(nr, "%d", id_msg);
                number = (char *)malloc(strlen(nr));
                strcpy(number, nr);

                char *mesaj = malloc(strlen(reply_msg) + strlen(number) + strlen("Raspuns la mesajul cu ID-ul= ") + 2);
                strcpy(mesaj, "Raspuns la mesajul cu ID-ul= ");
                strcat(mesaj, number);
                strcat(mesaj, ": ");
                strcat(mesaj, reply_msg);

                if (este_conectat)
                {
                    // daca utilizatorul caruia vrem sa trimitem mesajul este conectat vom adauga mesajul in tabelul received_msg
                    sqlite3_stmt *stmt9;
                    sqlite3_prepare_v2(db, "INSERT INTO received_msg VALUES(?1, ?2, ?3);", -1, &stmt9, NULL);
                    sqlite3_bind_text(stmt9, 1, utilizator, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt9, 2, user_conv, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt9, 3, mesaj, -1, SQLITE_STATIC);
                    sqlite3_step(stmt9);
                }
                else
                {
                    // daca utilizatorul caruia vrem sa ii trimitem mesajul nu e conectat vom pastra mesajul in tabelul send_msg
                    sqlite3_stmt *stmt2;
                    sqlite3_prepare_v2(db, "INSERT INTO sent_msg VALUES(?1, ?2, ?3);", -1, &stmt2, NULL);
                    sqlite3_bind_text(stmt2, 1, utilizator, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt2, 2, user_conv, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt2, 3, mesaj, -1, SQLITE_STATIC);
                    sqlite3_step(stmt2);
                }

                sqlite3_stmt *stmt4;
                sqlite3_prepare_v2(db, "SELECT conv, count FROM CONVERSATII WHERE (USER1=?1 AND USER2=?2) OR (USER1=?2 AND USER2=?1);", -1, &stmt4, NULL);
                sqlite3_bind_text(stmt4, 1, utilizator, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt4, 2, user_conv, -1, SQLITE_STATIC);

                int rc_;
                if ((rc_ = sqlite3_step(stmt4)) == SQLITE_ROW)
                {
                    char nr[10];
                    char *number;
                    char *conv = (char *)sqlite3_column_text(stmt4, 0);
                    int id_msg = sqlite3_column_int(stmt4, 1);
                    id_msg++;

                    sprintf(nr, "%d", id_msg);
                    number = (char *)malloc(strlen(nr + 1));
                    strcpy(number, nr);

                    char *new_msg = (char *)malloc(strlen(utilizator) + strlen(mesaj) + strlen(number) + 2 + strlen("ID_mesaj=") + strlen("from "));

                    strcpy(new_msg, "ID_mesaj=");
                    strcat(new_msg, number);
                    strcat(new_msg, "from ");
                    strcat(new_msg, utilizator);
                    strcat(new_msg, ": ");
                    strcat(new_msg, mesaj);

                    char *new_conv = (char *)malloc(strlen(conv) + strlen(new_msg));

                    strcpy(new_conv, conv);
                    strcat(new_conv, "\n");
                    strcat(new_conv, new_msg);
                    printf("Noul mesaj ce va fi adaugat in tabelul CONVERSATII este: %s\n", new_conv);

                    sqlite3_stmt *stmt5;

                    sqlite3_prepare_v2(db, "UPDATE CONVERSATII SET CONV=?1, COUNT=?2 WHERE (USER1=?3 AND USER2=?4) OR (USER1=?4 AND USER2=?3);", -1, &stmt5, NULL);
                    sqlite3_bind_text(stmt5, 1, new_conv, -1, SQLITE_STATIC);
                    sqlite3_bind_int(stmt5, 2, id_msg);
                    sqlite3_bind_text(stmt5, 3, utilizator, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt5, 4, user_conv, -1, SQLITE_STATIC);
                    sqlite3_step(stmt5);
                }
            }
        }

        strcpy(rasp, "Mesajul a fost trimis!\n");
    }
}

void messenger(int cl, int idThread)
{

    if (write(cl, comenzi_active, 1024) <= 0)
    {
        printf("[Thread %d] ", idThread);
        perror("[Thread]Eroareee la scriere catre client.\n");
    }

    char comanda[200];
    int este_conectat = 0;
    char utilizator[50] = "";
    bzero(comanda, 200);

    while (read(cl, comanda, 200))
    {
        int c = 0; // comanda clientului
        int i;

        char rasp[200];
        bzero(rasp, 200);
        printf("[Thread %d]A fost introdusa comanda: %s\n", idThread, comanda);

        for (i = 0; i < strlen(comanda1) && comanda[i] == comanda1[i]; i++)
            ;
        if (i == strlen(comanda1))
            c = 1;

        for (i = 0; i < strlen(comanda2) && comanda[i] == comanda2[i]; i++)
            ;
        if (i == strlen(comanda2))
            c = 2;

        for (i = 0; i < strlen(comanda3) && comanda[i] == comanda3[i]; i++)
            ;
        if (i == strlen(comanda3))
            c = 3;

        for (i = 0; i < strlen(comanda4) && comanda[i] == comanda4[i]; i++)
            ;
        if (i == strlen(comanda4))
            c = 4;

        for (i = 0; i < strlen(comanda5) && comanda[i] == comanda5[i]; i++)
            ;
        if (i == strlen(comanda5))
            c = 5;

        for (i = 0; i < strlen(comanda6) && comanda[i] == comanda6[i]; i++)
            ;
        if (i == strlen(comanda6))
            c = 6;

        for (i = 0; i < strlen(comanda7) && comanda[i] == comanda7[i]; i++)
            ;
        if (i == strlen(comanda7))
            c = 7;

        for (i = 0; i < strlen(comanda8) && comanda[i] == comanda8[i]; i++)
            ;
        if (i == strlen(comanda8))
            c = 8;

        // printf("c= %d", c);

        switch (c)
        {

        case 1: // "Creare cont"

            if (!este_conectat)
            {
                creare_cont(cl, idThread, rasp, &este_conectat);
            }
            else
            {
                strcpy(rasp, utilizator);
                strcat(rasp, " este conectat. Pentru a crea un alt cont trebuie sa va deconectati mai intai!");
            }
            break;

        case 2: //"Log in"

            write(cl, &este_conectat, sizeof(este_conectat));

            if (este_conectat)
            {
                strcpy(rasp, utilizator);
                strcat(rasp, " sunteti deja conectat!");
            }
            else
            {
                log_in(cl, idThread, rasp, &este_conectat, utilizator);
            }

            break;

        case 3: // "Trimite mesaj"

            write(cl, &este_conectat, sizeof(este_conectat));

            if (este_conectat)
            {
                send_msg(cl, idThread, rasp, utilizator);
            }
            else
            {
                strcpy(rasp, "Nu sunteti conectat!");
            }

            break;

        case 4: // "Vizualizare conversatie"

            write(cl, &este_conectat, sizeof(este_conectat));

            // printf("aici");
            if (este_conectat)
            {
                // printf("este conectat");
                show_conv(cl, rasp, utilizator);
            }
            else
            {
                // printf("nu este conectat");
                strcpy(rasp, "Nu sunteti conectat!");
            }

            break;

        case 5: //"Raspunde la mesaj"

            write(cl, &este_conectat, sizeof(este_conectat));

            if (este_conectat)
            {
                answer_msg(cl, idThread, rasp, utilizator);
            }
            else
            {
                strcpy(rasp, "Nu sunteti conectat!");
            }

            break;

        case 6: //"Mesaje noi"

            write(cl, &este_conectat, sizeof(este_conectat));
        
            break;

        case 7: // "Log out"

            if (!este_conectat)
            {
                strcpy(rasp, "Nu sunteti conectat!");
                //printf("nu e conectat");
            }
            else
            {
                int id_utilizator = -1;
                onmsg_to_ofmsg(utilizator);
                printf("Utilizatorul %s s-a deconectat\n", utilizator);

                for (int i = 0; i < count_connected; ++i)
                {
                    if (!strcmp(utilizatori[i].username, utilizator))
                    {
                        id_utilizator = i;
                        break;
                    }
                }
                if (id_utilizator != -1)
                {

                    for (int i = id_utilizator; i < count_connected - 1; i++)
                    {
                        strcpy(utilizatori[i].username, utilizatori[i + 1].username);
                        utilizatori[i].descriptor = utilizatori[i + 1].descriptor;
                    }
                    count_connected--;
                }
                strcpy(utilizator, "");
                strcpy(rasp, "V-ati deconectat!");
                este_conectat = 0;
            }

            break;

        case 8:; // "Exit"

            int id_utilizator = -1;
            for (int i = 0; i < count_connected && id_utilizator == -1; i++)
            {
                if (!strcmp(utilizatori[i].username, utilizator))
                {
                    id_utilizator = i;
                }
            }

            if (id_utilizator != -1)
            {
                onmsg_to_ofmsg(utilizator);
                for (int i = id_utilizator; i < count_connected - 1; i++)
                {
                    strcpy(utilizatori[i].username, utilizatori[i + 1].username);
                    utilizatori[i].descriptor = utilizatori[i + 1].descriptor;
                }
                count_connected--;
            }
            strcpy(utilizator, " ");
            este_conectat = 0;
            pthread_exit(th[idThread]);
            break;

        default:

            strcpy(rasp, "Comanda introdusa nu exista! Alegeti una dintre comenzile de mai sus");
            int dim_com = strlen(comenzi_active);
            write(cl, &dim_com, sizeof(dim_com));
            write(cl, comenzi_active, dim_com);
            break;
        }

        // printf("aici4");
        //printf("%s", rasp);
        write(cl, rasp, 200);
        write(cl, &este_conectat, sizeof(este_conectat));

        // verificam daca utilizatorul a primit mesaje noi
        if (este_conectat)
        {
            int rc;
            char *old_msg = "";
            sqlite3_stmt *rec1;
            sqlite3_prepare_v2(db, "SELECT * FROM received_msg WHERE receiver = ?1", -1, &rec1, NULL);
            sqlite3_bind_text(rec1, 1, utilizator, -1, SQLITE_STATIC);

            while ((rc = sqlite3_step(rec1)) == SQLITE_ROW)
            {
                const char *msg = sqlite3_column_text(rec1, 2);
                const char *from = sqlite3_column_text(rec1, 0);
                char *new_msg = (char *)malloc(strlen(old_msg) + strlen(msg) + strlen(from) + 3);
                strcpy(new_msg, old_msg);
                strcat(new_msg, "\n");
                strcat(new_msg, from);
                strcat(new_msg, ": ");
                strcat(new_msg, msg);
                old_msg = (char *)malloc(strlen(new_msg));
                strcpy(old_msg, new_msg);
            }
            sqlite3_finalize(rec1);

            int dim_old_msg = strlen(old_msg);

            // clientul nu a mai primit niciun mesaj

            if (dim_old_msg == 0)
            {
                old_msg = (char *)malloc(strlen("Nu aveti mesaje necitite!"));
                strcpy(old_msg, "Nu aveti mesaje necitite!");
                dim_old_msg = strlen(old_msg);
            }

            write(cl, &dim_old_msg, sizeof(dim_old_msg));
            write(cl, old_msg, dim_old_msg);

            sqlite3_stmt *rec2;
            sqlite3_prepare_v2(db, "DELETE FROM received_msg WHERE receiver = ?1", -1, &rec2, NULL);
            sqlite3_bind_text(rec2, 1, utilizator, -1, SQLITE_STATIC);
            sqlite3_step(rec2);
        }
    }

    bzero(comanda, 200);
}


int main()
{
    struct sockaddr_in server; // structura pentru server
    struct sockaddr_in from;

    int i = 0;
    int soc;

    dbproject = sqlite3_open("projectdb.db", &db);

    // cream tabelul pentru utilizatori in baza de date
    char *sql_;
    sql_ = "CREATE TABLE USERS(" \
           "USERNAME TEXT PRIMARY KEY NOT NULL, " \
           "PASSWORD TEXT             NOT NULL);";

    dbproject = sqlite3_exec(db, sql_, NULL, 0, &err_msg);

    // cream socket

    if ((soc = socket(AF_INET, SOCK_STREAM, 6)) == -1)
    {
        perror("Eroare la socket.\n");
        return errno;
    }

    int ok = 1;
    setsockopt(soc, IPPROTO_TCP, SO_REUSEADDR, &ok, sizeof(ok));
    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    // atasam socketul
    if (bind(soc, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("Eroare la bind.\n");
        return errno;
    }

    if (listen(soc, 2) == -1)
    {
        perror("Eroare la listen.\n");
        return errno;
    }

    // folosind thread-uri pentru a prelua clientii, in mod concurent

    while (1)
    {
        thData *tdr;
        int client;
        int dim = sizeof(from);

        printf("Asteptam la portul %d...\n", PORT);
        fflush(stdout);

        if ((client = accept(soc, (struct sockaddr *)&from, &dim)) < 0)
        {
            perror("Eroare la accept.\n");
            continue;
        }

        tdr = (struct thData *)malloc(sizeof(struct thData));
        tdr->idThread = i++;
        tdr->cl = client;

        pthread_create(&th[i], NULL, &treat, tdr);
    }
}

