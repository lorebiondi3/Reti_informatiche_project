#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define BUF_SIZE 1024
#define MAX_LEN_CMD 17 // "!vedi_estrazione"
#define SESSION_ID_LEN 11 // "session_id\0"
#define fflush(stdin) while((getchar())!='\n')

void  schermata_iniziale(int j);
int gestione_comando(int socket,char* buffer);
void help(char* buffer);     
void signup(int socket,char* buffer);
int login(int socket,char* buffer);
void invia_giocata(int socket,char* buffer);
int memorizza_ruote (char* buffer);
int memorizza_numeri (char* buffer);
int memorizza_importi (char* buffer);
void vedi_giocate(int socket,char* buffer);
void vedi_estrazione(int socket,char* buffer);
void vedi_vincite(int socket,char* buffer);
int esci(int socket,char* buffer);



char session_id [SESSION_ID_LEN];
char utente_loggato [BUF_SIZE];

//possono essere giocati fino a 10 numeri su al piu 11 ruote con 5 possibili tipologie di giocata
//importi[i] indica l'importo giocato sulla i-esima tipologia di giocata 
struct Schedina{
    //vettore di al piu 11 stringhe di lunghezza massima 20
    char ruote [11][20];
    //uint16_t in formato network 
    uint16_t numeri [10];
    //vettore di stringhe perche' in questo modo posso inviare importi non interi
    char importi [5][5];
    
};

struct Schedina s;

// ./lotto_client <IP server> <porta server>


int main(int argc, char** argv) {
    
    int ret,sd,i,n,len;
    struct sockaddr_in srv_addr;
    char buffer[BUF_SIZE];
    char comando [MAX_LEN_CMD];
    char parametro [MAX_LEN_CMD];
    char username [BUF_SIZE];
    char password [BUF_SIZE];
    uint16_t lmsg;
    char session_id[SESSION_ID_LEN];
    
    //lettura parametri
    // argv[1] : IP server
    // argv[2] : porta server
    
    if(argv[1]==NULL || argv[2]==NULL){
        printf("Il client si avvia con la seguente sintassi: './lotto_client <IP server> <porta server>'\n");
        exit(0);
    }
    
    int porta=atoi(argv[2]);
    
    
    sd=socket(AF_INET,SOCK_STREAM,0);
    
    memset(&srv_addr,0,sizeof(srv_addr));
    srv_addr.sin_family=AF_INET;
    srv_addr.sin_port=htons(porta);
    inet_pton(AF_INET,argv[1],&srv_addr.sin_addr);
    
    
    ret=connect(sd,(struct sockaddr*)&srv_addr,sizeof(srv_addr));
    if(ret<0){
        perror("Connessione non riuscita:\n");
        exit(-1);
    }
 
    
    //l'IP potrebbe essere bloccato
    //attendo una notifica da parte del server
    ret=recv(sd,(void*)&lmsg,sizeof(uint16_t),0);
    len=ntohs(lmsg);
    ret=recv(sd,(void*)buffer,len,0);
    if(strcmp(buffer,"IP bloccato")==0){
        //ricevo i minuti mancanti
        ret=recv(sd,(void*)&lmsg,sizeof(uint16_t),0);
        len=ntohs(lmsg);
        ret=recv(sd,(void*)buffer,len,0);
        printf("Questo IP e' bloccato,ritentare la connessione tra %s minuti\n",buffer);
        printf("Chiusura del socket %i\n",sd);
        //chiudo socket sd e termino processo client
        close(sd);
        return 0;
    }
    
    //else if(strcmp(buffer,"IP non bloccato")==0)
    //connessione accettata
    
    
    //comandi disponibili
    schermata_iniziale(0);
    

    while(1){
        
        printf("Digitare un comando:\n");
        fgets(buffer,BUF_SIZE,stdin);
        ret=gestione_comando(sd,buffer);
        if(ret==-1){
            //connessione chiusa,termino il client
            printf("Termino processo client\n");
            return 0;
        }
        
            
    }
        
}       



        
int gestione_comando(int socket,char* buffer){

    char comando[MAX_LEN_CMD];
    int ret;
    
    //controllo il primo carattere
    if(buffer[0]!= '!'){
        printf("I comandi devono iniziare con il carattere '!'\n");
        return;
    }
    
    sscanf(buffer,"%s",comando);
    
    if(strcmp(comando,"!help")==0){
            help(buffer);
            return 0;
    }
    else if(strcmp(comando,"!signup")==0){
            signup(socket,buffer);
            return 0;
    }
    else if(strcmp(comando,"!login")==0){
        ret=login(socket,buffer);
        //se ret=-1 termino il client
        return ret;
    }
    else if(strcmp(comando,"!invia_giocata")==0){
        invia_giocata(socket,buffer);
        return 0;
    }
    else if(strcmp(comando,"!vedi_giocate")==0){
        vedi_giocate(socket,buffer);
        return 0;
    }
    else if(strcmp(comando,"!vedi_estrazione")==0){
        vedi_estrazione(socket,buffer);
        return 0;
    }
    else if(strcmp(comando,"!vedi_vincite")==0){
        vedi_vincite(socket,buffer);
        return 0;
    }
    else if(strcmp(comando,"!esci")==0){
        ret=esci(socket,buffer);
        //se ret=-1 termino il client
        return ret;
    }
    
    //il comando non esiste
    printf("Il comando che hai inserito non esiste\nQuesti sono i comandi che puoi inserire:\n");
    schermata_iniziale(0);
    return 0;
    
}     
        
        
        
void help(char* buffer){
    
    char comando[MAX_LEN_CMD];
    char parametro[MAX_LEN_CMD];
    int ret;
    
    
    ret=sscanf(buffer,"%s %s",comando,parametro);
    
    //comando "!help"
    if(ret==1){ 
        schermata_iniziale(0);
    }
    
    else{
        if(strcmp(parametro,"help")==0)
             schermata_iniziale(1);
         else if(strcmp(parametro,"signup")==0)
             schermata_iniziale(2);
         else if(strcmp(parametro,"login")==0)
             schermata_iniziale(3);
         else if(strcmp(parametro,"invia_giocata")==0)
             schermata_iniziale(4);
         else if(strcmp(parametro,"vedi_giocata")==0)
             schermata_iniziale(5);
         else if(strcmp(parametro,"vedi_estrazione")==0)
             schermata_iniziale(6);
         else if(strcmp(parametro,"vedi_vincite")==0)
             schermata_iniziale(7);
         else if(strcmp(parametro,"esci")==0)
             schermata_iniziale(8);
         else 
             schermata_iniziale(9);
        }
}
        
     
void signup(int socket,char* buffer){
    
    char buf [BUF_SIZE];
    char username[BUF_SIZE];
    char password [BUF_SIZE];
    uint16_t lmsg;
    int len,i,ret;
    
   
    
    ret=sscanf(buffer,"%s %s %s",buf,username,password);
    
    //controllo formato
    if(ret!=3){
        printf("Il formato del comando deve essere del tipo '!signup username password'\n");
        return;
    }
    
    
    len=strlen(buffer)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    if(ret<0){
        perror("Errore in fase di invio della lunghezza del comando:\n");
    }
  
    ret=send(socket,(void*)buffer,len,0);
    if(ret<0){
        perror("Errore in fase di invio del comando:\n");
    }
    
    while(1){
        //attendo una risposta dal server
        ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
        len=ntohs(lmsg);
        ret=recv(socket,(void*)buf,len,0);
        if(ret<0){
            perror("Errore in fase di ricezione della risposta dal server:\n");
        }

        if( strcmp(buf,"Username gia' esistente,sceglierne un'altro\n")==0 ){
         //invio un nuovo username
            printf("%s",buf);
            scanf("%s",username);
            //pulizia dello stream stdin
            fflush(stdin);
            len=strlen(username)+1;
            lmsg=htons(len);
            ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
            ret=send(socket,(void*)username,len,0);
        }
        
        else if(strcmp(buf,"Registrazione effettuata con successo\n")==0){
            printf("%s",buf);
            break;
        }
    }
    
    
    
}     

//restiuisce -1 in caso di chiusura connessione
int login(int socket,char* buffer){
        
    char buf[BUF_SIZE];
    char username[BUF_SIZE];
    char password[BUF_SIZE];
    uint16_t lmsg;
    int ret,len;
    
    ret=sscanf(buffer,"%s %s %s",buf,username,password);
    
    //controllo formato
    if(ret!=3){
        printf("Il formato del comando deve essere del tipo '!login username passsword'\n");
        return;
    }
    
    //invio il comando al server
    len=strlen(buffer)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)buffer,len,0);
    
    while(1){
        //attendo risposta dal server
        ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
        len=ntohs(lmsg);
        ret=recv(socket,(void*)buf,len,0);

        if(strcmp(buf,"Utente non registrato,effettua la registrazione\n")==0){
            //l utente specificato non e' ancora registrato
            printf("%s",buf);
            schermata_iniziale(2);
            return 0; 
        }

        if(strcmp(buf,"Password errata")==0){
            //invio una nuova password
            printf("Password errata,inserire una nuova password:\n");
            scanf("%s",password);
            //pulizia dello stream stdin
            fflush(stdin);
            len=strlen(password)+1;
            lmsg=htons(len);
            ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
            ret=send(socket,(void*)password,len,0);
            continue;
        }

        if(strcmp(buf,"Connessione chiusa\n")==0){
            printf("Autenticazione fallita.Connessione bloccata per 30 minuti\n");
            //chiusura socket
            close(socket);
            return -1;
        }
        
        else{//autenticazione riuscita,ricevo il session_id
            strncpy(session_id,buf,11); //copio anche il terminatore di stringa
            printf("Login dell'utente %s effettuato con successo,session_id: '%s'\n",username,session_id);
            sprintf(utente_loggato,"%s",username);
            return 0;
        }
        
    }
    
}    

void invia_giocata(int socket,char* buffer){
        
    int ret,len,pos,i;
    uint16_t lmsg;
    char buf [BUF_SIZE];
    char comando [BUF_SIZE];
    
    //per lo split del comando inserito da tastiera
    char buffer_ruote[BUF_SIZE];
    char buffer_numeri[BUF_SIZE];
    char buffer_importi[BUF_SIZE];
    const char* delimiter="-,\n";
    char* token;
    int num_token=0;
    int quante_ruote=0;
    int quanti_numeri=0;
    int quanti_importi=0;
    //utilizzo tipi che possono essere trasferiti
    uint16_t ruote;
    uint16_t numeri;
    uint16_t importi;
    
    //invia session id e attendi accettazione del server
    sprintf(buf,"Verifica session id");
    len=strlen(buf)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)buf,len,0);
    
    //invio session id
    len=strlen(session_id)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)session_id,len,0);
    
    //attendo risposta del server
    ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
    len=ntohs(lmsg);
    ret=recv(socket,(void*)buf,len,0);
    
    //se il session id non e' valido dai messaggio di errore e torna a "digita comando"
    if(strcmp(buf,"Session id non valido")==0){
        printf("Session id non valido,effettua prima il login\n");
        //schermata help comando login
        schermata_iniziale(3);
        return;
    }
    
    //se il session id e' valido invia il comando !invia_giocata schedina
    printf("Session id valido,invio della schedina al server\n");
    
    //invio comando "!invia_giocata schedina" contenuto in buffer al server
    len=strlen(buffer)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)buffer,len,0);
    
    
    //memorizzo la schedina negli appositi campi della struttura 
    
    //divido la stringa "!invia_giocata -r milano roma -n 15 4 -i 4.5 6" 
    //in 4 token: "!invia_giocata"(non mi interessa) "r milano roma " "n 15 4 " "i 4.5 6"
    //elimino anche il carattere '\n' dall'ultimo importo inserito(6 nell'esempio)
    token=strtok(buffer,delimiter);
    while(token!=NULL){
        num_token++;
        if(num_token==2){sprintf(buffer_ruote,"%s",token);}
        if(num_token==3){sprintf(buffer_numeri,"%s",token);}
        if(num_token==4){sprintf(buffer_importi,"%s",token);}
        token=strtok(NULL,delimiter);
    }
    
    //pulizia schedina
    memset(&s,0,sizeof(s));
    //pulizia buf
    memset(buf,0,sizeof(buf));
    
    //memorizzo i dati nella struct schedina s
    quante_ruote=memorizza_ruote(buffer_ruote);
    quanti_numeri=memorizza_numeri(buffer_numeri);
    quanti_importi=memorizza_importi(buffer_importi);
    
    
    //BINARY PROTOCOL
    //invio la schedina nel seguente formato:
    // "!invia_giocata <quante_ruote> ruote <quanti_numeri> numeri <quanti_importi> importi"
    
    //conversione in formato network per il trasferimento
    ruote=htons(quante_ruote);
    numeri=htons(quanti_numeri);
    importi=htons(quanti_importi);
    
    //serializzazione della schedina
    pos=0;
    memcpy(buf+pos,&ruote,sizeof(ruote));
    pos+=sizeof(ruote);
    for(i=0;i<quante_ruote;i++){
        strcpy(buf+pos,s.ruote[i]); //copia anche '\0'
        pos+=strlen(s.ruote[i])+1; //strlen non considera '\0'
        
    }
    
    
    memcpy(buf+pos,&numeri,sizeof(numeri));
    pos+=sizeof(numeri);
    for(i=0;i<quanti_numeri;i++){
        //il vettore 'numeri' e' un vettore di uint16_t in formato network gia' pronti per essere inviati
        memcpy(buf+pos,&s.numeri[i],sizeof(s.numeri[i]));
        pos+=sizeof(s.numeri[i]);
    }
    
    
    memcpy(buf+pos,&importi,sizeof(importi));
    pos+=sizeof(importi);
    for(i=0;i<quanti_importi;i++){
        //il vettore di importi e' un vettore di stringhe
        strcpy(buf+pos,s.importi[i]);
        pos+=strlen(s.importi[i])+1;
    }
    
    
    
    //invio la schedina serializzata in buf al server
    len=pos+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)buf,len,0);
    
    
    //attendo notifica del server
    ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
    len=ntohs(lmsg);
    ret=recv(socket,(void*)buf,len,0);
    if(strcmp(buf,"Giocata avvenuta con successo!")==0){
        printf("%s\n",buf);
    }
    
    
}


void vedi_giocate(int socket,char* buffer){
    
    char str [BUF_SIZE];
    int tipo;
    int ret,len,i;
    uint16_t lmsg;
    int numero_giocate;
    
    sscanf(buffer,"%s %i",str,&tipo);
    
    //controllo formato
    if(tipo!=0 && tipo!=1){
        printf("Tipo non valido\n");
        schermata_iniziale(5);
        return;
    }
    
    //invio il session_id
    sprintf(str,"Verifica session id");
    len=strlen(str)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)str,len,0);
    
    //invio session id
    len=strlen(session_id)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)session_id,len,0);
    
    //attendo risposta del server
    ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
    len=ntohs(lmsg);
    ret=recv(socket,(void*)str,len,0);
    
    if(strcmp(str,"Session id non valido")==0){
        printf("Session id non valido,effettua prima il login\n");
        //schermata help comando login
        schermata_iniziale(3);
        return;
    }
    
    printf("Session id valido\n");
    
    
    //invio il comando
    len=strlen(buffer)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)buffer,len,0);
    
    
    //giocate attive
    if(tipo==1){
        //ricevo il numero di giocate attive
        ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
        numero_giocate=ntohs(lmsg);
        if(numero_giocate==0){
            //non ci sono giocate attive
            printf("Non ci sono giocate attive\n");
            return;
        }
        
        //se numero_giocate > 0 ricevo e stampo numero_giocate schedine
        for(i=1;i<=numero_giocate;i++){
            ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
            len=ntohs(lmsg);
            ret=recv(socket,(void*)str,len,0);
            
            printf("%i) ",i);
            printf("%s\n",str);
        }
        return;
    }
    
    //giocate relative ad estrazioni gia' effettuate
    else if(tipo==0){
        
        ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
        len=ntohs(lmsg);
        ret=recv(socket,(void*)str,len,0);
        
        if(strcmp(str,"File registro vuoto")==0){
            printf("Non sono presenti giocate relative ad estrazioni gia' effettuate\n");
            return;
        }
        //altrimenti ho ricevuto la prima giocata e la stampo
        numero_giocate=1;
        printf("%i) ",numero_giocate);
        printf("%s\n",str);
        
        //ricevo tutte le altre giocate
        while(1){
            
            ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
            len=ntohs(lmsg);
            ret=recv(socket,(void*)str,len,0);
            
            //ho ricevuto tutte le giocate,termino
            if(strcmp(str,"fine")==0){
                return;
            }
            
            numero_giocate++;
            //stampo la giocata
            printf("%i) ",numero_giocate);
            printf("%s\n",str);
        }
        
    }
   
    
}


void vedi_estrazione(int socket,char* buffer){
    
    char buf [BUF_SIZE];
    char str [BUF_SIZE];
    int ret,i,j,len;
    uint16_t lmsg;
    
    char ruota[BUF_SIZE];
    int n;
    
    //invio il session_id
    sprintf(str,"Verifica session id");
    len=strlen(str)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)str,len,0);
    
    
    len=strlen(session_id)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)session_id,len,0);
    
    //attendo risposta del server
    ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
    len=ntohs(lmsg);
    ret=recv(socket,(void*)str,len,0);
    
    if(strcmp(str,"Session id non valido")==0){
        printf("Session id non valido,effettua prima il login\n");
        //schermata help comando login
        schermata_iniziale(3);
        return;
    }
    
    printf("Session id valido\n");
    
    
    
    ret=sscanf(buffer,"%s %i %s",buf,&n,ruota);
    if(ret<2){
        printf("Formaro non valido\n");
        schermata_iniziale(6);
        return;
    }
    
    if(ret==2){
        sprintf(ruota,"tutte");
    }
    
    //invia comando
    len=strlen(buffer)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)buffer,len,0);

    for(i=0;i<n;i++){
        //ricevo un'estrazione pronta per essere stampata
        ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
        len=ntohs(lmsg);
        ret=recv(socket,(void*)buf,len,0);

        //stampo l'estrazione
        printf("%s\n",buf);

    }
    
    
    
    
}


void vedi_vincite(int socket,char* buffer){
    
    int ret,len,i;
    char str [BUF_SIZE];
    char buf [BUF_SIZE];
    uint16_t lmsg;
    
    //controllo formato
    ret=sscanf(buffer,"%s %s",str,buf);
    if(ret==2){
        printf("Formato del comando non corretto\n");
        schermata_iniziale(7);
        return;
    }
    
    
    //invio il session_id
    sprintf(str,"Verifica session id");
    len=strlen(str)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)str,len,0);
    
    //invio session id
    len=strlen(session_id)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)session_id,len,0);
    
    //attendo risposta del server
    ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
    len=ntohs(lmsg);
    ret=recv(socket,(void*)str,len,0);
    
    if(strcmp(str,"Session id non valido")==0){
        printf("Session id non valido,effettua prima il login\n");
        //schermata help comando login
        schermata_iniziale(3);
        return;
    }
    
    printf("Session id valido\n");
    
    //invio il comando
    len=strlen(buffer)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)buffer,len,0);
    
    //ricevo le vincite e le stampo
    ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
    len=ntohs(lmsg);
    ret=recv(socket,(void*)buffer,len,0);
    
    printf("%s\n",buffer);
    
}

//restituisce -1 in caso di logout avvenuto con successo
int esci(int socket,char* buffer){
    
    
    int i,len,ret;
    uint16_t lmsg;
    
    
    //invio il comando
    len=strlen(buffer)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)buffer,len,0);
    
    //attendo risposta del server
    ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
    len=ntohs(lmsg);
    ret=recv(socket,(void*)buffer,len,0);
    
    if(strcmp(buffer,"Attendi la prossima estrazione prima di effettuare il logout")==0){
        printf("%s\n",buffer);
        return 0;
    }
   
    //chiudo socket TCP
    close(socket);
    printf("Logout effettuato con successo\n");
    return -1;
    
    
    
}




//----------------------------FUNZIONI DI UTILITA'--------------------------------------------------


//se j=0 mostra la schermata iniziale con tutti i comandi,altrimenti mostra l'help relativo al comando j
void  schermata_iniziale(int j){
      
     int i;
     if(j==0){
         
        for(i=1;i<=30;i++){printf("*");}
        printf(" GIOCO DEL LOTTO ");
        for(i=1;i<=30;i++){
            if(i==30){ printf("*\n");}
            else printf("*");
        }
        printf("Sono disponibili i seguenti comandi:\n\n");
        for(i=1;i<=8;i++){
            printf("%d) ",i);
            switch(i){
                case 1: 
                    printf("!help <comando> --> mostra i dettagli di un comando\n");
                    break; 
                case 2: 
                    printf("!signup <username> <password> --> crea un nuovo utente\n");
                    break;
                case 3: 
                    printf("!login <username> <password> --> autentica un utente\n");
                    break;
                case 4: 
                    printf("!invia_giocata g --> invia una giocata g al server\n");
                    break;
                case 5: 
                    printf("!vedi_giocata tipo --> visualizza le giocate precedenti dove tipo= {0,1} "
                            "e permette di visualizzare le giocate passate '0' oppure le giocate attive '1' (ancora non estratte)\n");
                    break;
                case 6:
                    printf("!vedi_estrazione <n> <ruota> --> mostra i numeri delle ultime n estrazioni sulla ruota specificata\n");
                    break;
                case 7:
                    printf("!vedi_vincite --> mostra tutte le vincite,l'estrazione in cui sono state realizzate e un consuntivo per tipologia di giocata\n");
                    break;
                case 8: 
                    printf("!esci --> termina il client\n\n");
                    break;

                }
            }
     }
     else{ //mostro i dettagli del comando specifico j
        switch(j){
            printf("%d) ",j);
                case 1: 
                    printf("!help <comando> --> mostra i dettagli di un comando\n");
                    break; 
                case 2: 
                    printf("!signup <username> <password> --> crea un nuovo utente\n");
                    break;
                case 3: 
                    printf("!login <username> <password> --> autentica un utente\n");
                    break;
                case 4: 
                    printf("!invia_giocata g --> invia una giocata g al server\n");
                    break;
                case 5: 
                    printf("!vedi_giocata tipo --> visualizza le giocate precedenti dove tipo= {0,1} "
                            "e permette di visualizzare le giocate passate '0' oppure le giocate attive '1' (ancora non estratte)\n");
                    break;
                case 6:
                    printf("!vedi_estrazione <n> <ruota> --> mostra i numeri delle ultime n estrazioni sulla ruota specificata\n");
                    break;
                case 7:
                    printf("!vedi_vincite --> mostra tutte le vincite,l'estrazione in cui sono state realizzate e un consuntivo per tipologia di giocata\n");
                    break;
                case 8: 
                    printf("!esci --> termina il client\n\n");
                    break;

                }
                
            
        } 
        
     }

//ricevo in ingresso un buffer del tipo "r milano roma"
//riempie il vettore relativo alle ruote della struttura schedina 
//nell'esempio ottengo: ruote[0]="milano",ruote[1]="roma"
//ritorna il numero di ruote giocate
//nel caso in cui si giochino tutte le ruote ("tutte") ritorna 1
int memorizza_ruote (char* buffer){
    
    char* inner_token;
    const char* inner_delimiter=" ";
    int i=0;
    int j=0;
    inner_token=strtok(buffer,inner_delimiter);
    while(inner_token!=NULL){
            //non inserisco "r"
            if(i>0){
                    strcpy(s.ruote[j],inner_token);
                    j++;
                    }
            inner_token=strtok(NULL,inner_delimiter);
            i++;
    }
    return j;
}

//ricevo in ingresso un buffer del tipo "n 15 19 33"
//riempie il vettore relativo ai numeri giocati della struttura schedina 
//ritorna quanti numeri sono stati giocati
int memorizza_numeri(char* buffer){

    char* inner_token;
    const char* inner_delimiter=" ";
    int i=0;
    int j=0;
    int numero;
    inner_token=strtok(buffer,inner_delimiter);
    while(inner_token!=NULL){
            //non inserisco "n"
            if(i>0){
                    //effettuo la conversione da stringa a intero e successivamente da intero a uint16_t in formato network
                numero=atoi(inner_token);
                s.numeri[j]=htons(numero);
                j++;
            }
            inner_token=strtok(NULL,inner_delimiter);
            i++;
    }
    return j;
}

//ricevo in ingresso un buffer del tipo "i 0 5 4.6"
//riempie il vettore relativo agli importi giocati sulle varie tipologie di giocata della schedina
//ritorna il numero di importi giocati
int memorizza_importi(char* buffer){

    char* inner_token;
    const char* inner_delimiter=" ";
    int i=0;
    int j=0;
    inner_token=strtok(buffer,inner_delimiter);
    while(inner_token!=NULL){
            //non inserisco "i"
            if(i>0){
                    strcpy(s.importi[j],inner_token); //stringa
                    j++;
                    }
            inner_token=strtok(NULL,inner_delimiter);
            i++;
    }
    return j;
}

