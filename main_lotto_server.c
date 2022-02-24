#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>


#define BUF_LEN 1024
#define NUM_RUOTE 11
#define NUMERI 10
#define TIPI_GIOCATA 5
#define SESSION_ID_LEN 11 //anche '\0'
#define MAX_NUM_GIOCATE 10 //massimo 10 giocate a sessione per client

int gestione_comando(int socket,char* buffer);
void signup(int socket,char* buffer);
int login(int socket,char* buffer);
int IP_bloccato(char ip_client[INET_ADDRSTRLEN]);
void invia_giocata(int socket);
void vedi_giocate(int socket,char* buffer);
void estrazione ();
void vedi_estrazione(int socket,char* buffer);
void controlla_vincite();
int ambi_gen(int n);
int terni_gen(int n);
int quaterne_gen(int n);
int cinquine_gen(int n);
void vedi_vincite(int socket);
int esci(int socket);


//-----------------------------STRUTTURE DATI----------------------------------------------- 

time_t rawtime;
struct tm* timeinfo;

//timer per la gestione delle estrazioni
struct timeval timer;

char session_id [SESSION_ID_LEN];
char utente_loggato [BUF_LEN];

struct Schedina{
    char ruote[11][20];
    //formato host
    int numeri [10];
    //importi[0] indica l'importo giocato sull'estratto
    float importi[5];
};

//vettore di schedine riempito da invia_giocata()
//contiene le giocate attive relative al client,ovvero le giocate in attesa della prossima estrazione
//al momento dell'estrazione viene trasferito nel file registro e resettato
struct Schedina Giocate_Attive[MAX_NUM_GIOCATE];
//indice della prima posizione libera nel vettore delle giocate attive
int libera;

//matrice dei numeri estratti,utilizzata nel controllo delle vincite
int numeri_estratti [11][5];



//-------------------------------------HANDLER-------------------------------------


//handler per il segnale SIGALRM che viene ricevuto ogni timer.tv_sec secondi dal processo figlio che gestisce le estrazioni
void gestione_estrazione(int signal){
    estrazione();
}


//handler per il segnale SIGUSR1 inviato a tutti i processi al termine di un'estrazione
//ciascun processo trasferira' le giocate contenute in Giocate_Attive (se ce ne sono) nel relativo file registro nel formato <ruote n numeri i importi e data_ora_estrazione>
//controlla le eventuali vincite per ogni giocata attraverso la funzione "controlla_vincite" e le registra nel relativo file vincite
void registra_giocate(int signal){
    
    FILE* fd;
    int ret,i,j;
    char buffer [BUF_LEN];
    char str [BUF_LEN];
    
    //ottengo ora attuale che coincide con l'ora dell'estrazione
    time(&rawtime);
    timeinfo=localtime(&rawtime);
    
    
    //non ci sono giocate attive
    if(libera==0){
        return;
    }
    
    //controlla e registra le eventuali vincite nel file "vincite_utente.txt"
    controlla_vincite();
    
    //apertura file registro di utente_loggato
    fd=fopen(utente_loggato,"a");
    
    //scorro il vettore Giocate_Attive
    for(i=0;i<libera;i++){
        //giocata i-esima
        
        //pulizia buffer
        strcpy(buffer,"");
        
        
        //leggo le ruote
        for(j=0;strlen(Giocate_Attive[i].ruote[j])!=0;j++){
            strcat(buffer,Giocate_Attive[i].ruote[j]);
            strcat(buffer," ");
        }
        //carattere 'n'
        strcat(buffer,"n ");
        //leggo i numeri
        for(j=0;Giocate_Attive[i].numeri[j]!=0;j++){
            sprintf(str,"%i",Giocate_Attive[i].numeri[j]);
            strcat(buffer,str);
            strcat(buffer," ");
        }
        //carattere 'i'
        strcat(buffer,"i ");
        //leggo gli importi
        for(j=0;j<5;j++){
            sprintf(str,"%f",Giocate_Attive[i].importi[j]);
            strcat(buffer,str);
            strcat(buffer," ");
        }
        //carattere 'e'
        strcat(buffer,"e ");
        //data_ora estrazione di riferimento
        sprintf(str,"%i-%i-%i %i::%i::%i",timeinfo->tm_mday,timeinfo->tm_mon+1,timeinfo->tm_year+1900,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
        strcat(buffer,str);
        
        
        //scrittura nel file registro
        ret=fprintf(fd,"%s\n",buffer);
    }
    
    //chiusura file registro
    fclose(fd);
    
    //resetto Giocate_Attive e libera=0
    memset(Giocate_Attive,0,sizeof(Giocate_Attive));
    libera=0;
    printf("Memorizzazione nel file registro delle giocate effettuata con successo\n");
    
}




// ./lotto_server <porta> <periodo>

int main(int argc, char** argv) {
 
    
    int ret,listener,newfd,i,len;
    int minuti_mancanti;
    unsigned int addrlen;
    uint16_t lmsg;
    pid_t pid;
    int porta;
    char buffer [BUF_LEN];
    time_t timestamp;
    
    
    fd_set master;
    fd_set read_fds;
    int fdmax;
    
    struct sockaddr_in my_addr,cl_addr;
    
    
    
    FILE* fd;
    char ip_client [INET_ADDRSTRLEN];
    
    //lettura parametri
    if(argv[1]==NULL){
        printf("Inserire un numero di porta\n");
        exit(0);
    }
    porta=atoi(argv[1]);
    
    //il periodo viene inserito (se viene specificato) in minuti
    if(argv[2]==NULL)
        timer.tv_sec=300; //estrazioni ogni 5 minuti di default  
    else
        timer.tv_sec=(atol(argv[2])*60); //conversione da stringa a minuti e poi a secondi
    
    printf("Periodo di estrazione:%i \n",timer.tv_sec/60);
    
    listener=socket(AF_INET,SOCK_STREAM,0);
    if(listener>=0){
        printf("Socket di ascolto creato\n");
    }
    
    //creazione indirizzo
    memset(&my_addr,0,sizeof(my_addr));
    my_addr.sin_family=AF_INET;
    my_addr.sin_port=htons(porta); //porta data in input
    my_addr.sin_addr.s_addr=INADDR_ANY; //raggiungibile da tutti gli indirizzi IP
     
    ret=bind(listener,(struct sockaddr*)&my_addr,sizeof(my_addr));
    ret=listen(listener,10);
    if(ret<0){
        perror("Errore in fase di bind:\n");
    }
    
    //associo un handler al segnale SIGALRM e al segnale SIGUSR1
    signal(SIGALRM,gestione_estrazione);
    signal(SIGUSR1,registra_giocate);
    
    //creazione processo figlio per la gestione delle estrazioni
    pid=fork();
    if(pid==0){
        estrazione(timer.tv_sec);
    }
    
    addrlen=sizeof(cl_addr);
    
    while(1){
        newfd=accept(listener,(struct sockaddr*)&cl_addr,&addrlen);
        if(newfd>=0){
            printf("Nuova connessione accettata,nuovo socket %d\n",newfd);
        }
        
        //------------CONTROLLO SE L'IP DEL CLIENT E' BLOCCATO-------------------

        //trasformo l'indirizzo IP in una stringa
        inet_ntop(AF_INET,(void*)&cl_addr.sin_addr,ip_client,INET_ADDRSTRLEN);

        minuti_mancanti=IP_bloccato(ip_client);
        if(minuti_mancanti==0){
            //IP non bloccato
            printf("IP non bloccato\n");
            //invio al client la notifica
            sprintf(buffer,"IP non bloccato");
            len=strlen(buffer)+1;
            lmsg=htons(len);
            ret=send(newfd,(void*)&lmsg,sizeof(uint16_t),0);
            ret=send(newfd,(void*)buffer,len,0);
            //continua regolarmente l'esecuzione
        }
        else{
            //IP bloccato
            //invio al client la notifica
            sprintf(buffer,"IP bloccato");
            len=strlen(buffer)+1;
            lmsg=htons(len);
            ret=send(newfd,(void*)&lmsg,sizeof(uint16_t),0);
            ret=send(newfd,(void*)buffer,len,0);
            //invio al client il numero di minuti che deve ancora attendere
            sprintf(buffer,"%i",minuti_mancanti);
            len=strlen(buffer)+1;
            lmsg=htons(len);
            ret=send(newfd,(void*)&lmsg,sizeof(uint16_t),0);
            ret=send(newfd,(void*)buffer,len,0);
            //chiudo socket
            printf("Chiudo il socket %d\n",newfd);
            close(newfd);
            continue;
        }
        
        //---------------------------------------------------------------------
              
        
        //creazione processo figlio per la gestione della comunicazione
        pid=fork();
        //figlio
        if(pid==0){
            //chiudo il socket di ascolto
            close(listener);
            
            while(1){
                //lunghezza del messaggio
                ret=recv(newfd,(void*)&lmsg,sizeof(uint16_t),0);
                if(ret<0){
                    perror("Lunghezza del messaggio non ricevuta:\n");
                    break;
                }

                len=ntohs(lmsg);
                if(len==0)
                    break;

                ret=recv(newfd,(void*)buffer,len,0);
                if(ret<0){
                    perror("Errore in fase di ricezione del comando:\n");
                    break;
                }

                //ricezione del messaggio "Verifica session id"
                //avviene ogni volta che il client utilizza comandi del tipo "!invia_giocata !vedi_giocate ecc..."
                if(strcmp(buffer,"Verifica session id")==0){
                    //ricevo session id
                    ret=recv(newfd,(void*)&lmsg,sizeof(uint16_t),0);
                    len=ntohs(lmsg);
                    ret=recv(newfd,(void*)buffer,len,0);
                    if(strcmp(buffer,session_id)==0){
                        //session id valido
                        sprintf(buffer,"Session id valido");
                        len=strlen(buffer)+1;
                        lmsg=htons(len);
                        ret=send(newfd,(void*)&lmsg,sizeof(uint16_t),0);
                        ret=send(newfd,(void*)buffer,len,0);
                    }
                    else{
                        //session id non valido
                        sprintf(buffer,"Session id non valido");
                        len=strlen(buffer)+1;
                        lmsg=htons(len);
                        ret=send(newfd,(void*)&lmsg,sizeof(uint16_t),0);
                        ret=send(newfd,(void*)buffer,len,0);
                    }
                    continue;
                }

                printf("Comando ricevuto: %s\n",buffer);
                ret=gestione_comando(newfd,buffer);
                //terzo tentativo di login fallito 
                if(ret==-1){
                    //salvo in un file 'IP_bloccati' l'IP e il timestamp(time_t)
                    fd=fopen("IP_bloccati.txt","a");
                    //IP del client
                    inet_ntop(AF_INET,(void*)&cl_addr.sin_addr,ip_client,INET_ADDRSTRLEN);
                    //timeinfo e' stata inizializzata dalla funzione login con il timestamp di blocco
                    //file IP_bloccati.txt ha record del tipo <IP,hour,minute,second>
                    ret=fprintf(fd,"%s %i %i %i",ip_client,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
                    fclose(fd);
                    //termino il processo figlio e chiudo il socket
                    break;
                }
                //logout avvenuto con successo
                if(ret==1){
                    //chiudo il socket TCP e termino il processo
                    break;
                }
                
            }
            //chiusura socket di comunicazione
            close(newfd);
            exit(0);
        }
        //padre
        //chiudo il socket di comunicazione
        close(newfd);
    }

    close(listener);
}
        
 
    
           
int gestione_comando(int socket,char* buffer){
    
    char comando[BUF_LEN];
    int ret;
    
    //lettura del comando
    sscanf(buffer,"%s",comando);
    
    //i controlli sul formato e la validita' dei comandi vengono effettuati lato client
    
    if(strcmp(comando,"!signup")==0){
        signup(socket,buffer);
        return 0;
    }
    else if(strcmp(comando,"!login")==0){
        ret=login(socket,buffer);
        return ret;
    }
    else if(strcmp(comando,"!invia_giocata")==0){
        invia_giocata(socket);
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
        vedi_vincite(socket);
        return 0;
    }
    else if(strcmp(comando,"!esci")==0){
        ret=esci(socket);
        return ret;
    }
    
    
}
      
//non ha return value,gli errori(username gia utilizzato) vengono gestiti internamente
void signup(int socket,char* buffer){
       
    
        FILE* fd;
        char comando[BUF_LEN];
        char username[BUF_LEN];
        char password[BUF_LEN];
        char messaggio[BUF_LEN];
        int ret,len,i,checked,n;
        uint16_t lmsg;
        char* tipo_file=".txt";
        
        n=sizeof(messaggio);

        sscanf(buffer,"%s %s %s",comando,username,password);
       
        
       
        //controllo se esiste gia un utente con lo stesso username
        while(1){
            strncat(username,tipo_file,5);
            fd=fopen(username,"r");
            if(fd!=NULL){
                printf("File registro %s gia' esistente\n",username);
                sprintf(messaggio,"Username gia' esistente,sceglierne un'altro\n");
                len=strlen(messaggio)+1;
                lmsg=htons(len);
                ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
                ret=send(socket,(void*)messaggio,len,0);
                //attendo un nuovo username
                
                ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
                len=ntohs(lmsg);
                ret=recv(socket,(void*)username,len,0);
                printf("Ricevuto il nuovo username %s\n",username);
                continue;       
            }
            //fd==NULL
            break;
        }
        
        //creo il file registro
        fd=fopen(username,"w");
        if(fd==NULL){
            perror("Errore in fase di creazione del file registro\n");
            return;
        }
        
        //scrittura della password nel file registro
        ret=fprintf(fd,"%s\n",password);
        fclose(fd);
        
        //invio un messaggio al client per la conferma dell'avvenuta registrazione
        sprintf(messaggio,"Registrazione effettuata con successo\n");
        printf("Registrazione (file registro: %s,password: %s) effettuata\n",username,password);
        len=strlen(messaggio)+1;
        lmsg=htons(len);
        ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
        ret=send(socket,(void*)messaggio,len,0);
        
}
     
        
  
//ritorna -1 se l'autenticazione fallisce dopo i tre tentativi
int login(int socket,char* buffer){
    
    char comando [BUF_LEN];
    char username [BUF_LEN];
    char password [BUF_LEN];
    char buf[BUF_LEN];
    FILE* fd;
    char* tipo_file=".txt";
    char messaggio [BUF_LEN];
    int ret,n,len,i;
    uint16_t lmsg;
    int tentativi_rimasti=3;
    
    
    
    n=sizeof(messaggio);
    
    
    sscanf(buffer,"%s %s %s",comando,username,password);
    
    //controllo la validita' delle credenziali
    
    strncat(username,tipo_file,5);
    fd=fopen(username,"r");
    
    //il file registro non esiste,utente non registrato
    if(fd==NULL){
        printf("Il file %s non esiste,utente non registrato\n",username);
        strncpy(messaggio,"Utente non registrato,effettua la registrazione\n",n);
        len=strlen(messaggio)+1;
        lmsg=htons(len);
        ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
        ret=send(socket,(void*)messaggio,len,0);
        return 0;
    }
    
    else{
        //lettura della password dal file registro
        ret=fscanf(fd,"%s",buf);
        
        
        while(1){//esco nel caso di password corretta o fine tentativi
            
            printf("Tentativo %i) password inserita: %s,password corretta: %s\n",3-tentativi_rimasti+1,password,buf);
            //password corretta
            if(strcmp(buf,password)==0){

                //genero il session_id
                srand(time(NULL));
                for(i=0;i<10;i++){
                    session_id[i]=(char)((rand()%26)+65);
                }
                session_id[i]='\0';

                printf("Autenticazione avvenuta con successo: <username %s,session_id='%s'>\n",username,session_id);
                sprintf(utente_loggato,"%s",username);

                //invio il session_id al client
                len=strlen(session_id)+1;
                lmsg=htons(len);
                ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
                ret=send(socket,(void*)session_id,len,0);
                return 0;

            }

            //autenticazione fallita
            else{
                tentativi_rimasti--;

                if(tentativi_rimasti==0){
                    //timestamp fallimento terzo tentativo
                    time(&rawtime);
                    timeinfo=localtime(&rawtime);
                    
                    //chiusura connessione
                    printf("%i::%i::%i connessione chiusa,indirizzo IP bloccato per 30 minuti\n",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
                    sprintf(messaggio,"Connessione chiusa\n");
                    len=strlen(messaggio)+1;
                    lmsg=htons(len);
                    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
                    ret=send(socket,(void*)messaggio,len,0);
                    
                    return -1;
                    
                    
                    
                }
                else{ //richiedo una nuova password
                    printf("Password errata,%i tentativi rimasti\n",tentativi_rimasti);
                    //invio messaggio di errore al client
                    strncpy(messaggio,"Password errata",n);
                    len=strlen(messaggio)+1;
                    lmsg=htons(len);
                    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
                    ret=send(socket,(void*)messaggio,len,0);
                    //attesa nuova password
                    ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
                    len=ntohs(lmsg);
                    ret=recv(socket,(void*)password,len,0);
                    
                }
            }
        }
    }
    
    
    
}      
        
        
        
//restituisce il numero di minuti per cui l'IP passato come argomento sara' ancora bloccato ,0 se l'IP non e' bloccato
int IP_bloccato(char ip_client [INET_ADDRSTRLEN]){
    
    char buffer [BUF_LEN];
    int ora,minuti,secondi;
    time_t timestamp_attuale;
    FILE* fd;
    int ret;
    int diff_sec;
    int minuti_mancanti;
    
    
    fd=fopen("IP_bloccati.txt","r");
    if(fd==NULL){
        //non e' ancora stato bloccato nessun IP
        printf("Non ci sono IP bloccati\n");
        return 0;
    }
    
    //ciclo di lettura all'interno del file contenente gli IP bloccati
    while(1){
        //leggo dal file <IP,HH,MM,SS>
        ret=fscanf(fd,"%s %i %i %i",buffer,&ora,&minuti,&secondi);
        if(ret<0){
            //ho letto tutto il file
            break;
        }
        
        if(strcmp(ip_client,buffer)==0){
            //ottengo il timestamp attuale
            time(&timestamp_attuale);
            timeinfo=localtime(&timestamp_attuale);
            diff_sec=diff_secondi(timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec,ora,minuti,secondi);
            //30 minuti=1800 secondi
            if(diff_sec<1800){
                //non sono ancora trascorsi 30 minuti,la connessione deve essere bloccata
                minuti_mancanti=(1800-diff_sec)/60;
                printf("IP %s bloccato ancora per %i minuti\n",buffer,minuti_mancanti);
                fclose(fd);
                return minuti_mancanti;
            }
        }
    }
    
    //l'IP del client non e' presente nel file
    fclose(fd);
    return 0;
    
}



//riceve la schedina e la decodifica all'interno del vettore Giocate_Attive
//al termine della prossima estrazione le giocate presenti nel vettore verranno trasferite nel file registro del client,con le eventuali vincite 
void invia_giocata(int socket){
    
    char buffer [BUF_LEN];
    char converti_importi [BUF_LEN];
    uint16_t converti_numeri;
    char s_id [BUF_LEN];
    int ret,len,pos,i;
    uint16_t lmsg;
    FILE* fd;
    
    
    //variabili per deserializzazione schedina
    uint16_t ruote;
    uint16_t numeri;
    uint16_t importi;
    int quante_ruote;
    int quanti_numeri;
    int quanti_importi;
    
    
    //attendo schedina
    ret=recv(socket,(void*)&lmsg,sizeof(uint16_t),0);
    len=ntohs(lmsg);
    ret=recv(socket,(void*)buffer,len,0);
            
    
    
    //deserializzazione schedina ricevuta in buffer
    //memorizzazione in Giocate_Attive[libera]
    pos=0;
    memcpy(&ruote,buffer+pos,sizeof(ruote));
    pos+=sizeof(ruote);
    quante_ruote=ntohs(ruote);
    for(i=0;i<quante_ruote;i++){
        strcpy(Giocate_Attive[libera].ruote[i],buffer+pos);
        pos+=strlen(Giocate_Attive[libera].ruote[i])+1;
    }
    
    memcpy(&numeri,buffer+pos,sizeof(numeri));
    pos+=sizeof(numeri);
    quanti_numeri=ntohs(numeri);
    for(i=0;i<quanti_numeri;i++){
        //uint16_t formato network
        memcpy(&converti_numeri,buffer+pos,sizeof(converti_numeri));
        //converto i numeri da uint16_t formato network a interi formato host
        Giocate_Attive[libera].numeri[i]=ntohs(converti_numeri);
        pos+=sizeof(converti_numeri);
    }
    
    memcpy(&importi,buffer+pos,sizeof(importi));
    pos+=sizeof(importi);
    quanti_importi=ntohs(importi);
    for(i=0;i<quanti_importi;i++){
        //ricevo gli importi come stringhe
        strcpy(converti_importi,buffer+pos);
        //converto gli importi da stringhe a float
        sscanf(converti_importi,"%f",&Giocate_Attive[libera].importi[i]);
        pos+=strlen(converti_importi)+1;
    }
    
    
    
    printf("Giocata dell'utente %s salvata\n",utente_loggato);
    //notifico il client
    sprintf(buffer,"Giocata avvenuta con successo!");
    len=strlen(buffer)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)buffer,len,0);
    
    //incremento l'indice 'libera'
    libera++;
    
}

//vedi_giocate 1-->invio giocate nel vettore Giocate_Attive
//vedi_giocate 0 --> invio giocate salvate nel file registro
void vedi_giocate(int socket,char* buffer){
    
    int ret,len,i,n,tipo;
    int fine_file=0;
    float importo;
    uint16_t lmsg;
    uint16_t quante_giocate;
    char buf [BUF_LEN];
    char str [BUF_LEN];
    int scorri_giocate=0;
    FILE* fd;
    
    //ottengo il tipo
    sscanf(buffer,"%s %i",buf,&tipo);
    
    //invio le giocate attive
    if(tipo==1){
        
        //invio il numero di giocate attive
        quante_giocate=htons(libera);
        ret=send(socket,(void*)&quante_giocate,sizeof(uint16_t),0);
        
        //non ci sono giocate attive
        if(libera==0){
            printf("Non ci sono giocate attive per il client %s\n",utente_loggato);
            return;
        }
        
        
        //pulizia buf
        strcpy(buf,"");
        
        //invio 'quante_giocate' schedine
        //costruisco un messaggio del tipo "ruote numeri <* tipo_giocata importo>"
        while(1){
            
            if(scorri_giocate==libera){
                //ho letto tutte le giocate attive
                break;
            }
            
            //pulizia
            strcpy(buf,"");
            
            //leggo le ruote
            for(i=0;i<11;i++){
                //ho letto tutte le ruote
                if(strlen(Giocate_Attive[scorri_giocate].ruote[i])==0){break;}
                n=strlen(Giocate_Attive[scorri_giocate].ruote[i]);
                strncat(buf,Giocate_Attive[scorri_giocate].ruote[i],n);
                strcat(buf," ");
            }   
            //leggo i numeri
            for(i=0;i<10;i++){
                //ho letto tutti i numeri
                //NOTA:i numeri vanno da 1 a 90
                if(Giocate_Attive[scorri_giocate].numeri[i]==0){break;}
                //converto il numero in stringa per effettuare la concatenazione
                sprintf(str,"%i",Giocate_Attive[scorri_giocate].numeri[i]);
                strcat(buf,str);
                strcat(buf," ");
            }
            //leggo gli importi
            for(i=0;i<5;i++){
                
                if(Giocate_Attive[scorri_giocate].importi[i]!=0){
                    //non invio le tipologie non giocate,dove importo=0
                    strcat(buf,"* ");
                    if(i==0){
                        //estratto
                        sprintf(str,"%4.2f",Giocate_Attive[scorri_giocate].importi[i]);
                        strcat(buf,str);
                        sprintf(str," estratto");
                        strcat(buf,str);
                    }
                    else if(i==1){
                        //ambo
                        sprintf(str,"%4.2f",Giocate_Attive[scorri_giocate].importi[i]);
                        strcat(buf,str);
                        sprintf(str," ambo");
                        strcat(buf,str);
                    }
                    else if(i==2){
                        //terno
                        sprintf(str,"%4.2f",Giocate_Attive[scorri_giocate].importi[i]);
                        strcat(buf,str);
                        sprintf(str," terno");
                        strcat(buf,str);
                    }
                    else if(i==3){
                        //quaterna
                        sprintf(str,"%4.2f",Giocate_Attive[scorri_giocate].importi[i]);
                        strcat(buf,str);
                        sprintf(str," quaterna");
                        strcat(buf,str);
                    }
                    else if(i==4){
                        //cinquina
                        sprintf(str,"%4.2f",Giocate_Attive[scorri_giocate].importi[i]);
                        strcat(buf,str);
                        sprintf(str," cinquina");
                        strcat(buf,str);
                    }
                }
                
                
            }
            
            //invio la giocata 
            len=strlen(buf)+1;
            lmsg=htons(len);
            ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
            ret=send(socket,(void*)buf,len,0);
            
            
            scorri_giocate++;
        }
        
        
    }
    
    //invio le giocate relative a estrazioni gia effettuate
    //tali giocate sono nel file registro dell'utente loggato e ne vengono aggiunte nuove ad ogni estrazione trasferendole da Giocate_Attive al file
    //record del tipo <ruote n numeri i importi e data_ora estrazione>
    else if(tipo==0){
        
        //apertura del file registro in lettura
        fd=fopen(utente_loggato,"r");
        if(fd==NULL){
            printf("Errore in fase di apertura del file registro\n");
            return;
        }
        
        //lettura password per far avanzare il cursore
        ret=fscanf(fd,"%s",buf);
        
        //pulizia buf
        strcpy(buf,"");
        
        //lettura prima ruota della prima giocata
        ret=fscanf(fd,"%s",buf);
        if(feof(fd)){
            //non ci sono giocate relative a estrazioni gia' effettuate
            printf("File registro vuoto\n");
            //non ci sono giocate di tipo 0
            sprintf(buf,"File registro vuoto");
            len=strlen(buf)+1;
            lmsg=htons(len);
            ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
            ret=send(socket,(void*)buf,len,0);
            return;
        }
        
        strcat(buf," ");
        
        //lettura schedine
        while(1){
               
            //lettura ruote
            while(1){
                ret=fscanf(fd,"%s",str);
                if(feof(fd)){fine_file=1;break;}
                if(strcmp(str,"n")==0){break;}
                strcat(buf,str);
                strcat(buf," ");
            }
            if(fine_file==1){break;}
            //lettura numeri
            while(1){
                ret=fscanf(fd,"%s",str);
                if(strcmp(str,"i")==0){break;}
                strcat(buf,str);
                strcat(buf," ");
            }
            //lettura importi
            for(i=0;i<5;i++){
                ret=fscanf(fd,"%f",&importo);
                if(importo==0){continue;}
                strcat(buf,"* ");
                //scrivo l'importo nella stringa str come un float a 4 cifre e 2 dopo la virgola
                sprintf(str,"%4.2f",importo);
                strcat(buf,str);
                switch(i){
                    case 0: strcat(buf," estratto ");
                    break;
                    case 1: strcat(buf," ambo");
                    break;
                    case 2: strcat(buf," terno");
                    break;
                    case 3: strcat(buf," quaterna");
                    break;
                    case 4: strcat(buf," cinquina");
                    break;
                }
            }
            
            
        
            //invio la giocata 
            len=strlen(buf)+1;
            lmsg=htons(len);
            ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
            ret=send(socket,(void*)buf,len,0);
            
            
            //lettura informazioni relative alla data dell'estrazione per avanzare cursore
            //'e'
            ret=fscanf(fd,"%s",str);
            //'data'
            ret=fscanf(fd,"%s",str);
            //'ora'
            ret=fscanf(fd,"%s",str);
            
            //pulizia buf e str
            strcpy(buf,"");
            strcpy(str,"");
            
            
        }
     
        //chiusura file registro
        fclose(fd);
        
        //ho inviato tutte le giocate,notifico il client
        sprintf(buf,"fine");
        len=strlen(buf)+1;
        lmsg=htons(len);
        ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
        ret=send(socket,(void*)buf,len,0);
    }
    
    
    
}
  

//estrae 55 numeri casuali nell'inervallo [1,90] senza reinserimento e li registra nel file "estrazione.txt" insieme alla data dell'estrazione
void estrazione(){
    
    FILE* fd;
    char buffer[BUF_LEN];
    int ret,i,j;
    int n;
    //per evitare reinserimento
    int n1,n2,n3,n4;
    
    char citta[BUF_LEN];
    const char spazio[]="                    ";
    
    
    time(&rawtime);
    timeinfo=localtime(&rawtime);
    printf("Estrazione del %i-%i-%i,ore %i::%i::%i\n",timeinfo->tm_mday,timeinfo->tm_mon+1,(timeinfo->tm_year+1900),timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
    
    fd=fopen("estrazioni.txt","a");//se e' la prima estrazione il file viene creato
    
    //carattere 'd' per separare le varie estrazioni
    fprintf(fd,"d ");
    fprintf(fd,"%i ",timeinfo->tm_mday);
    fprintf(fd,"%i ",timeinfo->tm_mon+1);
    fprintf(fd,"%i ",timeinfo->tm_year+1900);
    fprintf(fd,"%i ",timeinfo->tm_hour);
    fprintf(fd,"%i\n",timeinfo->tm_min);
    //scrittura data e ora estrazione
    //sprintf(buffer,"%i-%i-%i %i::%i::%i",timeinfo->tm_mday,timeinfo->tm_mon+1,(timeinfo->tm_year+1900),timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
    //ret=fprintf(fd,"%s\n",buffer);
    
    srand(time(NULL));
    //estraggo i 5 numeri e li scrivo nel file per tutte e 11 le ruote
    //riempio anche la matrice di estrazione "numeri_estratti" per il successivo controllo delle vincite
    for(i=0;i<11;i++){
        n1=0,n2=0,n3=0,n4=0;
        for(j=0;j<5;j++){
            n=rand()%90+1;
            //evito reinserimento
            switch(j){
                case 0:
                    n1=n;
                    ret=fprintf(fd,"%i ",n);
                    break;
                case 1:
                    while(n==n1){
                        n=rand()%90+1;
                    }
                    n2=n;
                    ret=fprintf(fd,"%i ",n);
                    break;
                case 2:
                    while(n==n1 || n==n2){
                        n=rand()%90+1;
                    }
                    n3=n;
                    ret=fprintf(fd,"%i ",n);
                    break;
                case 3:
                    while(n==n1 || n==n2 || n==n3){
                        n=rand()%90+1;
                    }
                    n4=n;
                    ret=fprintf(fd,"%i ",n);
                    break;
                case 4:
                    while(n==n1 || n==n2 || n==n3 || n==n4){
                        n=rand()%90+1;
                    }
                    ret=fprintf(fd,"%i\n",n);
                    //ultimo numero dell'ultima ruota
                    if(i==10){fprintf(fd,"\n");}
                    break;
            }
            
        }
    }
    
    fclose(fd);
    
    //notifico tutti i processi dell'avvenuta estrazione
    kill(0,SIGUSR1);
        
    //sospendo il processo gestore delle estrazioni per timer.tv_sec secondi
    alarm(timer.tv_sec);
    
}

//"!vedi_estrazione n ruota(opzionale)"
void vedi_estrazione(int socket,char* buffer){
    
    char str [BUF_LEN];
    char buf [BUF_LEN];
    int n;
    char c;
    int numero_ruota;
    char ruota[20];
    int ret,len,i,j,z;
    uint16_t lmsg;
    FILE* fd;
    
    ret=sscanf(buffer,"%s %i %s",str,&n,ruota);
    if(ret==2){
        //ruota non specificata
        sprintf(ruota,"tutte");
    }
    
    //ruota non specificata
    if(strcmp(ruota,"tutte")==0){
        
        //apertura file
        fd=fopen("estrazioni.txt","r");
        
        //posiziono il cursore alla fine del file
        fseek(fd,0,SEEK_END);
        
        //faccio risalire il cursore fino all'inizio della n-esima estrazione partendo dal fondo
        for(i=0;i<n;i++){
            
            while(1){
                
                //leggendo un carattere sposta il cursore di una posizione in avanti
                c=fgetc(fd);
                if(c=='d'){break;}
                fseek(fd,-2,SEEK_CUR);
            }
            
            if(i==n-1){
                //se sono arrivato alla n-esima estrazione posiziono il cursore sul carattere d
                fseek(fd,-1,SEEK_CUR);
            }
            else{
                //altrimenti sposto il cursore all'inizio della fine della precedente estrazione
                fseek(fd,-2,SEEK_CUR);
            }
            
        }
             
        
        //invio le n estrazioni
        for(j=0;j<n;j++){
            //pulizia buf
            strcpy(buf,"");
            
            //salto il carattere 'd'
            ret=fscanf(fd,"%s",str);
            
            //leggo data
            strcat(buf,"Estrazione del ");
            ret=fscanf(fd,"%s",str);
            strcat(buf,str);
            strcat(buf,"-");
            ret=fscanf(fd,"%s",str);
            strcat(buf,str);
            strcat(buf,"-");
            ret=fscanf(fd,"%s",str);
            strcat(buf,str);
            strcat(buf," ");
            //leggo ora
            strcat(buf," ore ");
            ret=fscanf(fd,"%s",str);
            strcat(buf,str);
            strcat(buf,":");
            ret=fscanf(fd,"%s",str);
            strcat(buf,str);
            strcat(buf,"\n");
            
            //leggo i numeri
            for(i=0;i<55;i++){
                if(i==0){strcat(buf,"Bari      ");}
                if(i==5){strcat(buf,"Cagliari  ");}
                if(i==10){strcat(buf,"Firenze   ");}
                if(i==15){strcat(buf,"Genova    ");}
                if(i==20){strcat(buf,"Milano    ");}
                if(i==25){strcat(buf,"Napoli    ");}
                if(i==30){strcat(buf,"Palermo   ");}
                if(i==35){strcat(buf,"Roma      ");}
                if(i==40){strcat(buf,"Torino    ");}
                if(i==45){strcat(buf,"Venezia   ");}
                if(i==50){strcat(buf,"Nazionale ");}
                ret=fscanf(fd,"%s",str);
                strcat(buf,str);
                if(i==4 || i==9 || i==14 || i==19 || i==24 || i==29 || i==34 || i==39 || i==44 || i==49 || i==54){strcat(buf,"\n");}
                else{strcat(buf," ");}
 
            }

            //invio l'estrazione
            len=strlen(buf)+1;
            lmsg=htons(len);
            ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
            ret=send(socket,(void*)buf,len,0);
        }
        
        
       
        //chiudo file
        fclose(fd);
        return;
    }
    
    //numeri estratti nelle ultime n estrazioni sulla ruota specificata
    else{
        //apertura file
        fd=fopen("estrazioni.txt","r");
        
        //posiziono il cursore alla fine del file
        fseek(fd,0,SEEK_END);
        
        //faccio risalire il cursore fino all'inizio della n-esima estrazione partendo dal fondo
        for(i=0;i<n;i++){
            
            while(1){
                
                //leggendo un carattere sposta il cursore di una posizione in avanti
                c=fgetc(fd);
                if(c=='d'){break;}
                fseek(fd,-2,SEEK_CUR);
            }
            
            if(i==n-1){
                //se sono arrivato alla n-esima estrazione posiziono il cursore sul carattere d
                fseek(fd,-1,SEEK_CUR);
            }
            else{
                //altrimenti sposto il cursore all'inizio della fine della precedente estrazione
                fseek(fd,-2,SEEK_CUR);
            }
            
        }
        
        //sono all'inizio della n-esima estrazione(se n=3 sono all'inizio della terzultima estrazione)
        
        if(strcmp(ruota,"bari")==0){numero_ruota=0;}
        else if(strcmp(ruota,"cagliari")==0){numero_ruota=1;}
        else if(strcmp(ruota,"firenze")==0){numero_ruota=2;}
        else if(strcmp(ruota,"genova")==0){numero_ruota=3;}
        else if(strcmp(ruota,"milano")==0){numero_ruota=4;}
        else if(strcmp(ruota,"napoli")==0){numero_ruota=5;}
        else if(strcmp(ruota,"palermo")==0){numero_ruota=6;}
        else if(strcmp(ruota,"roma")==0){numero_ruota=7;}
        else if(strcmp(ruota,"torino")==0){numero_ruota=8;}
        else if(strcmp(ruota,"venezia")==0){numero_ruota=9;}
        else if(strcmp(ruota,"nazionale")==0){numero_ruota=10;}
        
        
        //invio le n estrazioni nel formato <data,numeri>
        for(i=0;i<n;i++){
            
            //pulizia buffer
            strcpy(buf,"");
            strcpy(str,"");
            
            //salto il carattere 'd'
            ret=fscanf(fd,"%s",str);
            
            //leggo data
            strcat(buf,"Estrazione del ");
            ret=fscanf(fd,"%s",str);
            strcat(buf,str);
            strcat(buf,"-");
            ret=fscanf(fd,"%s",str);
            strcat(buf,str);
            strcat(buf,"-");
            ret=fscanf(fd,"%s",str);
            strcat(buf,str);
            strcat(buf," ");
            //leggo ora
            strcat(buf," ore ");
            ret=fscanf(fd,"%s",str);
            strcat(buf,str);
            strcat(buf,":");
            ret=fscanf(fd,"%s",str);
            strcat(buf,str);
            strcat(buf,"\n");
            
            //leggo i numeri relativi alla ruota specificata
            for(j=0;j<11;j++){
                for(z=0;z<5;z++){
                    ret=fscanf(fd,"%s",str);  
                    //ruota specificata
                    if(j==numero_ruota){
                        strcat(buf,str);
                        if(z==4){strcat(buf,"\n");}
                        else{strcat(buf," ");}
            
                    }
                }
                
            }
            
            //invio la n-esima estrazione
            len=strlen(buf)+1;
            lmsg=htons(len);
            ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
            ret=send(socket,(void*)buf,len,0);
            
        }
        
    }
    
    
    
}

//controlla le vincite delle schedine presenti nel vettore Giocate_Attive e le inserisce nel file vincite_utente.txt nel formato <data_estrazione,vincite>
void controlla_vincite(){
    
    FILE* fd;
    int i,j,z,len,ret;
    int giocata;
    char c;
    
    int ruota;
    int estrazione_ruota [5];
    //al massimo 10 numeri giocati
    int giocati [10];
    int quanti_giocati;
    int quanti_indovinati;
    //al massimo 5 numeri indovinati
    int numeri_indovinati [5];
    int index;
    float vincita;
    int numero_vinte;
    int count;
    
    char str [BUF_LEN];
    char buf [BUF_LEN];
    char data [BUF_LEN];
    char nome_file [BUF_LEN];
    
    int estratti,ambi,terne,quaterne,cinquine;
    int ambi_generabili,terne_generabili,quaterne_generabili,cinquine_generabili;
    float vincita_estratto,vincita_ambo,vincita_terno,vincita_quaterna,vincita_cinquina;
    
    int numero_ruote;
    
    //creazione nome file vincite
    sprintf(nome_file,"vincite_");
    strcat(nome_file,utente_loggato);
    
    //lettura dell'ultima estrazione
    fd=fopen("estrazioni.txt","r");
    
    
    //posiziono il cursore alla fine del file
    fseek(fd,0,SEEK_END);
    
    //posiziono il cursore sul primo numero dell'ultima estrazione
     while(1){
        c=fgetc(fd);
        if(c=='d'){break;}
        fseek(fd,-2,SEEK_CUR);
    }
    
    //ottengo data estrazione
   
    //pulizia
    memset(data,0,sizeof(data));
    memset(str,0,sizeof(str));
    
    ret=fscanf(fd,"%s",str);
    strcat(data,str);
    strcat(data," ");
    ret=fscanf(fd,"%s",str);
    strcat(data,str);
    strcat(data," ");
    ret=fscanf(fd,"%s",str);
    strcat(data,str);
    strcat(data," ");
    //ottengo ora
    ret=fscanf(fd,"%s",str);
    strcat(data,str);
    strcat(data," ");
    ret=fscanf(fd,"%s",str);
    strcat(data,str);
    strcat(data," ");
    
    //riempio la matrice delle estrazioni
    for(i=0;i<11;i++){
        for(j=0;j<5;j++){
            ret=fscanf(fd,"%i",&numeri_estratti[i][j]);
        }
    }
    
    //chiudo file estrazioni
    fclose(fd);
    
     
    //apertura(o creazione) file "vincite_utente.txt"
    fd=fopen(nome_file,"a");
    
    
    
    //analizzo le vincite di ciascuna giocata scorrendo il vettore di schedine Giocate_Attive dall'indice 0 all'indice libera
    for(giocata=0;giocata<libera;giocata++){
        
        printf("Controllo vincite giocata %d\n",giocata);
        
        //reset 
        numero_ruote=0;
        //ottengo il numero di ruote giocate
        for(i=0;i<11;i++){
            //scorro le ruote giocate
            if(strlen(Giocate_Attive[giocata].ruote[i])==0){
                //ho letto tutte le ruote giocate
                break;
            }
            numero_ruote++;
        }

        //ottengo i numeri giocati contenuti in Giocate_Attive[giocata].numeri[]
        //ottengo anche il numero di interi giocati
        
        //reset
        memset(giocati,0,sizeof(giocati));
        quanti_giocati=0;
        
        for(i=0;i<10;i++){
            if(Giocate_Attive[giocata].numeri[i]==0){
                //ho letto tutti i numeri
                break;
            }
            giocati[i]=Giocate_Attive[giocata].numeri[i];
            quanti_giocati++;
        }

        //reset
        numero_vinte=0;
        
        //analizzo le vincite su ciascuna ruota giocata
        for(i=0;i<numero_ruote;i++){


            //codifico la ruota
            if(strcmp(Giocate_Attive[giocata].ruote[i],"bari")==0){ruota=0;}
            else if(strcmp(Giocate_Attive[giocata].ruote[i],"cagliari")==0){ruota=1;}
            else if(strcmp(Giocate_Attive[giocata].ruote[i],"firenze")==0){ruota=2;}
            else if(strcmp(Giocate_Attive[giocata].ruote[i],"genova")==0){ruota=3;}
            else if(strcmp(Giocate_Attive[giocata].ruote[i],"milano")==0){ruota=4;}
            else if(strcmp(Giocate_Attive[giocata].ruote[i],"napoli")==0){ruota=5;}
            else if(strcmp(Giocate_Attive[giocata].ruote[i],"palermo")==0){ruota=6;}
            else if(strcmp(Giocate_Attive[giocata].ruote[i],"roma")==0){ruota=7;}
            else if(strcmp(Giocate_Attive[giocata].ruote[i],"torino")==0){ruota=8;}
            else if(strcmp(Giocate_Attive[giocata].ruote[i],"venezia")==0){ruota=9;}
            else if(strcmp(Giocate_Attive[giocata].ruote[i],"nazionale")==0){ruota=10;}
            else if(strcmp(Giocate_Attive[giocata].ruote[i],"tutte")==0){
            //gestione tutte le ruote
                numero_ruote=11;
                //analizzo la vincita su tutte le ruote
                for(i=0;i<11;i++){
                    
                    printf("Estrazione ruota %i\n",i);
                    //ottengo l'estrazione della ruota che mi interessa
                    for(j=0;j<5;j++){
                        estrazione_ruota[j]=numeri_estratti[i][j];
                        printf("%i\t",estrazione_ruota[j]);
                    }
                    printf("\n");
                    
                    //ottengo il numero di ambi,terne,quaterne e cinquine generabili con quanti_giocati numeri
                    ambi_generabili=ambi_gen(quanti_giocati);
                    terne_generabili=terni_gen(quanti_giocati);
                    quaterne_generabili=quaterne_gen(quanti_giocati);
                    cinquine_generabili=cinquine_gen(quanti_giocati);

                    //pulizia
                    memset(numeri_indovinati,0,sizeof(numeri_indovinati));
                    //resetto 
                    index=0;
                    quanti_indovinati=0;

                    //controllo quanti E QUALI numeri ho indovinato
                    for(j=0;j<5;j++){
                        for(z=0;z<quanti_giocati;z++){
                            if(giocati[z]==estrazione_ruota[j]){
                                numeri_indovinati[index]=giocati[z];
                                index++;
                                quanti_indovinati++;
                                break;
                            }
                        }
                    }


                    if(quanti_indovinati==0){
                        //schedina perdente
                        printf("Ruota perdente\n");
                        continue;
                    }
                    else{
                        numero_vinte++;
                        count++;
                    }

                    //stampa numeri indovinati
                    printf("Numeri indovinati:\n");
                    for(j=0;j<index;j++){
                        printf("%i\t",numeri_indovinati[j]);
                    }
                    printf("\n");



                    //ricavo il numero di estratti,ambi,terni,quaterne e cinquine indovinate a partire da quanti_presi
                    switch(quanti_indovinati){
                        case 0:
                            estratti=0;
                            ambi=0;
                            terne=0;
                            quaterne=0;
                            cinquine=0;
                            break;
                        case 1:
                            estratti=1;
                            ambi=0;
                            terne=0;
                            quaterne=0;
                            cinquine=0;
                            break;
                        case 2:
                            estratti=2;
                            ambi=1;
                            terne=0;
                            quaterne=0;
                            cinquine=0;
                            break;
                        case 3:
                            estratti=3;
                            ambi=3;
                            terne=1;
                            quaterne=0;
                            cinquine=0;
                            break;
                        case 4:
                            estratti=4;
                            ambi=6;
                            terne=4;
                            quaterne=1;
                            cinquine=0;
                            break;
                        case 5:
                            estratti=5;
                            ambi=10;
                            terne=10;
                            quaterne=5;
                            cinquine=1;
                            break;
                    }

                    //calcolo le vincite per tipologia di giocata
                    for(j=0;j<5;j++){
                        switch(j){
                            //estratto
                            case 0:
                                //nel caso di estratto si ha quanti_giocati=estratti_generabili
                                vincita_estratto=(estratti*11.23*Giocate_Attive[giocata].importi[0])/quanti_giocati;
                                //se ho giocato su piu ruote divido la vincita fra le varie ruote
                                vincita_estratto=vincita_estratto/numero_ruote;
                                break;
                            case 1:
                                vincita_ambo=(ambi*250*Giocate_Attive[giocata].importi[1])/ambi_generabili;
                                vincita_ambo=vincita_ambo/numero_ruote;
                                break;
                            case 2:
                                vincita_terno=(terne*4500*Giocate_Attive[giocata].importi[2])/terne_generabili;
                                vincita_terno=vincita_terno/numero_ruote;
                                break;
                            case 3:
                                vincita_quaterna=(quaterne*120000*Giocate_Attive[giocata].importi[3])/quaterne_generabili;
                                vincita_quaterna=vincita_quaterna/numero_ruote;
                                break;
                            case 4:
                                vincita_cinquina=(cinquine*6000000*Giocate_Attive[giocata].importi[4])/cinquine_generabili;
                                vincita_cinquina=vincita_cinquina/numero_ruote;
                                break;


                        }
                    }

                    //se sono alla prima giocata vincente (se esiste) allora scrivo la data
                    //scrittura data e ora estrazione
                    if(count==1){
                            ret=fprintf(fd,"%s\n",data);
                    }

                    //scrivo nel file delle vincite nel formato <ruota,numeri_indovinati,vittoria_tipologia_giocata>
                    strcpy(str,"");
                    strcpy(buf,"");


                    //nome della ruota
                    switch(i){
                        case 0:sprintf(buf,"bari ");
                        break;
                        case 1:sprintf(buf,"cagliari ");
                        break;
                        case 2:sprintf(buf,"firenze ");
                        break;
                        case 3:sprintf(buf,"genova ");
                        break;
                        case 4:sprintf(buf,"milano ");
                        break;
                        case 5:sprintf(buf,"napoli ");
                        break;
                        case 6:sprintf(buf,"palermo ");
                        break;
                        case 7:sprintf(buf,"roma ");
                        break;
                        case 8:sprintf(buf,"torino ");
                        break;
                        case 9:sprintf(buf,"venezia ");
                        break;
                        case 10:sprintf(buf,"nazionale ");
                        break;
                    }
                    
                    //scrittura ruota
                    ret=fprintf(fd,"%s",buf);

                    //scrittura su file dei numeri indovinati
                    for(j=0;j<quanti_indovinati;j++){
                        ret=fprintf(fd,"%i",numeri_indovinati[j]);
                        //spazio
                        ret=fprintf(fd," ");
                    }

                    //carattere 'v'
                    ret=fprintf(fd,"v");

                    //scrittura su file delle vincite per tipologia di giocata 
                    for(j=0;j<5;j++){
                        if(Giocate_Attive[giocata].importi[j]==0.000000){
                            //il client non ha scommesso su questa tipologia di giocata
                            continue;
                        }
                        //scrittura tipologia giocata
                        //0 indica estratto,1 ambo ecc...
                        fprintf(fd," ");
                        ret=fprintf(fd,"%i",j);
                        fprintf(fd," ");
                        switch(j){
                            case 0:
                                vincita=vincita_estratto;
                                break;
                            case 1:
                                vincita=vincita_ambo;
                                break;
                            case 2:
                                vincita=vincita_terno;
                                break;
                            case 3:
                                vincita=vincita_quaterna;
                                break;
                            case 4:
                                vincita=vincita_cinquina;
                                break;

                        }
                        fprintf(fd,"%f",vincita);

                    }

                    //carattere 'n' per indicare la fine della vincita sulla ruota i della giocata 'giocata'
                    fprintf(fd," n\n");

                }
            
                break;
            }
            
            
            
            //--------------------analizzo le vincite su una singola ruota--------------------------------------------------

            printf("Estrazione ruota %s\n",Giocate_Attive[giocata].ruote[i]);
            //ottengo l'estrazione della ruota che mi interessa
            for(j=0;j<5;j++){
                estrazione_ruota[j]=numeri_estratti[ruota][j];
                printf("%i\t",estrazione_ruota[j]);

            }
            printf("\n");


            //ottengo il numero di ambi,terne,quaterne e cinquine generabili con quanti_giocati numeri
            ambi_generabili=ambi_gen(quanti_giocati);
            terne_generabili=terni_gen(quanti_giocati);
            quaterne_generabili=quaterne_gen(quanti_giocati);
            cinquine_generabili=cinquine_gen(quanti_giocati);

            //pulizia
            memset(numeri_indovinati,0,sizeof(numeri_indovinati));
            //resetto 
            index=0;
            quanti_indovinati=0;

            //controllo quanti E QUALI numeri ho indovinato
            for(j=0;j<5;j++){
                for(z=0;z<quanti_giocati;z++){
                    if(giocati[z]==estrazione_ruota[j]){
                        numeri_indovinati[index]=giocati[z];
                        index++;
                        quanti_indovinati++;
                        break;
                    }
                }
            }
            

            if(quanti_indovinati==0){
                //schedina perdente
                printf("Ruota perdente\n");
                continue;
            }
            else{
                numero_vinte++;
                count++;
            }

            //stampa numeri indovinati
            printf("Numeri indovinati:\n");
            for(j=0;j<index;j++){
                printf("%i\t",numeri_indovinati[j]);
            }
            printf("\n");



            //ricavo il numero di estratti,ambi,terni,quaterne e cinquine indovinate a partire da quanti_presi
            switch(quanti_indovinati){
                case 0:
                    estratti=0;
                    ambi=0;
                    terne=0;
                    quaterne=0;
                    cinquine=0;
                    break;
                case 1:
                    estratti=1;
                    ambi=0;
                    terne=0;
                    quaterne=0;
                    cinquine=0;
                    break;
                case 2:
                    estratti=2;
                    ambi=1;
                    terne=0;
                    quaterne=0;
                    cinquine=0;
                    break;
                case 3:
                    estratti=3;
                    ambi=3;
                    terne=1;
                    quaterne=0;
                    cinquine=0;
                    break;
                case 4:
                    estratti=4;
                    ambi=6;
                    terne=4;
                    quaterne=1;
                    cinquine=0;
                    break;
                case 5:
                    estratti=5;
                    ambi=10;
                    terne=10;
                    quaterne=5;
                    cinquine=1;
                    break;
            }

            //calcolo le vincite per tipologia di giocata
            for(j=0;j<5;j++){
                switch(j){
                    //estratto
                    case 0:
                        //nel caso di estratto si ha quanti_giocati=estratti_generabili
                        vincita_estratto=(estratti*11.23*Giocate_Attive[giocata].importi[0])/quanti_giocati;
                        //se ho giocato su piu ruote divido la vincita fra le varie ruote
                        vincita_estratto=vincita_estratto/numero_ruote;
                        break;
                    case 1:
                        vincita_ambo=(ambi*250*Giocate_Attive[giocata].importi[1])/ambi_generabili;
                        vincita_ambo=vincita_ambo/numero_ruote;
                        break;
                    case 2:
                        vincita_terno=(terne*4500*Giocate_Attive[giocata].importi[2])/terne_generabili;
                        vincita_terno=vincita_terno/numero_ruote;
                        break;
                    case 3:
                        vincita_quaterna=(quaterne*120000*Giocate_Attive[giocata].importi[3])/quaterne_generabili;
                        vincita_quaterna=vincita_quaterna/numero_ruote;
                        break;
                    case 4:
                        vincita_cinquina=(cinquine*6000000*Giocate_Attive[giocata].importi[4])/cinquine_generabili;
                        vincita_cinquina=vincita_cinquina/numero_ruote;
                        break;


                }
            }
            
            //se sono alla prima giocata vincente (se esiste) allora scrivo la data
            //scrittura data e ora estrazione
            if(count==1){
                    ret=fprintf(fd,"%s\n",data);
            }
            
            //scrivo nel file delle vincite nel formato <ruota,numeri_indovinati,vittoria_tipologia_giocata>
            strcpy(str,"");
            strcpy(buf,"");

            

            sprintf(buf,"%s",Giocate_Attive[giocata].ruote[i]);
            strcat(buf," ");
            //scrittura ruota
            ret=fprintf(fd,"%s",buf);

            //scrittura su file dei numeri indovinati
            for(j=0;j<quanti_indovinati;j++){
                ret=fprintf(fd,"%i",numeri_indovinati[j]);
                //spazio
                ret=fprintf(fd," ");
            }

            //carattere 'v'
            ret=fprintf(fd,"v");

            //scrittura su file delle vincite per tipologia di giocata 
            for(j=0;j<5;j++){
                if(Giocate_Attive[giocata].importi[j]==0.000000){
                    //il client non ha scommesso su questa tipologia di giocata
                    continue;
                }
                //scrittura tipologia giocata
                //0 indica estratto,1 ambo ecc...
                fprintf(fd," ");
                ret=fprintf(fd,"%i",j);
                fprintf(fd," ");
                switch(j){
                    case 0:
                        vincita=vincita_estratto;
                        break;
                    case 1:
                        vincita=vincita_ambo;
                        break;
                    case 2:
                        vincita=vincita_terno;
                        break;
                    case 3:
                        vincita=vincita_quaterna;
                        break;
                    case 4:
                        vincita=vincita_cinquina;
                        break;

                }
                fprintf(fd,"%f",vincita);

            }

            //carattere 'n' per indicare la fine della vincita sulla ruota i della giocata 'giocata'
            fprintf(fd," n\n");

        }
        
    }
    
    //carattere 'f' per indicare la fine delle vincite relative all'estrazione
    //scrivo solo se esiste almeno una giocata vincente
    if(count>0){
        fprintf(fd,"f\n");
    }
    
    fclose(fd);
    
    
    
    
}



//non ha parametri
void vedi_vincite(int socket){
    
    //leggo e invio tutto il file vincite relativo all'utente loggato
    //tale file viene scritto dalla funzione controlla_vincite ogni volta che avviene un'estrazione
    
    int ret,i,j,len;
    int tipo;
    uint16_t lmsg;
    char buffer [BUF_LEN];
    char str [BUF_LEN];
    FILE* fd;
    
    float importo;
    float vincite_estratto,vincite_ambo,vincite_terno,vincite_quaterna,vincite_cinquina;
    
    //apro file vincite
    sprintf(str,"vincite_");
    strcat(str,utente_loggato);
    fd=fopen(str,"r");
    if(fd==NULL){
        //il client non ha mai giocato
    }
    printf("Apertura file %s\n",str);
    
    //leggo tutto il file
    while(1){
        
        //lettura data e ora
        sprintf(str,"Estrazione del ");
        strcat(buffer,str);
        ret=fscanf(fd,"%s",str);
        
        //se sono alla fine del file termino
        if(feof(fd)){
            break;
        }
        
        strcat(buffer,str);
        strcat(buffer,"-");
        ret=fscanf(fd,"%s",str);
        strcat(buffer,str);
        strcat(buffer,"-");
        ret=fscanf(fd,"%s",str);
        strcat(buffer,str);
        strcat(buffer," ");
        strcat(buffer,"ore ");
        ret=fscanf(fd,"%s",str);
        strcat(buffer,str);
        strcat(buffer,":");
        ret=fscanf(fd,"%s",str);
        strcat(buffer,str);
        strcat(buffer,"\n");
        
        
        //lettura vincite relative all'estrazione avvenuta in data "data"
        
        while(1){
         
            //lettura ruota
            ret=fscanf(fd,"%s",str);
            //se ho letto tutte le vincite relative all'estrazione leggo il carattere 'f' e termino
            if(strcmp(str,"f")==0){
                break;
            }
            strcat(buffer,str);
            strcat(buffer," ");

            //lettura numeri vincenti (fino al carattere v)
            while(1){
                ret=fscanf(fd,"%s",str);
                if(strcmp(str,"v")==0){
                    break;
                }
                strcat(buffer,str);
                strcat(buffer," ");
            }

            strcat(buffer," >>  ");
            
            printf("%s\n",buffer);

            //lettura vincite per tipologia di giocata (fino al carattere n)
            while(1){
                ret=fscanf(fd,"%s",str);
                if(strcmp(str,"n")==0){
                    break;
                }
                sscanf(str,"%i",&tipo);
                //lettura vincita
                ret=fscanf(fd,"%f",&importo);
                switch(tipo){
                    case 0:
                        strcat(buffer,"Estratto ");
                        vincite_estratto+=importo;
                        break;
                    case 1:
                        strcat(buffer,"Ambo ");
                        vincite_ambo+=importo;
                        break;
                    case 2:
                        strcat(buffer,"Terno ");
                        vincite_terno+=importo;
                        break;
                    case 3:
                        strcat(buffer,"Quaterna ");
                        vincite_quaterna+=importo;
                        break;
                    case 4:
                        strcat(buffer,"Cinquina ");
                        vincite_cinquina+=importo;
                        break;

                }
                
                //scrittura importo
                sprintf(str,"%4.2f",importo);
                strcat(buffer,str);
                strcat(buffer," ");

            }

            strcat(buffer,"\n");
            
                printf("%s\n",buffer);

        }
        
        strcat(buffer,"******************************************************\n");
        
    }
    
    //concateno il consuntivo delle vincite per tipologia di giocata
    strcat(buffer,"\n\n");
    strcat(buffer,"Vincite su ESTRATTO: ");
    sprintf(str,"%4.2f\n",vincite_estratto);
    strcat(buffer,str);
    strcat(buffer,"Vincite su AMBO: ");
    sprintf(str,"%4.2f\n",vincite_ambo);
    strcat(buffer,str);
    strcat(buffer,"Vincite su TERNO: ");
    sprintf(str,"%4.2f\n",vincite_terno);
    strcat(buffer,str);
    strcat(buffer,"Vincite su QUATERNA: ");
    sprintf(str,"%4.2f\n",vincite_quaterna);
    strcat(buffer,str);
    strcat(buffer,"Vincite su CINQUINA: ");
    sprintf(str,"%4.2f\n\n",vincite_cinquina);
    strcat(buffer,str);
    
    //invio al client
    len=strlen(buffer)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)buffer,len,0);
    
    
    
}


int esci(int socket){
    
    int ret,i,len;
    uint16_t lmsg;
    char buffer [BUF_LEN];
    
    printf("Logout dell'utente %s\n",utente_loggato);
            
    //invalida il session_id
    memset(session_id,0,sizeof(session_id));
    memset(utente_loggato,0,sizeof(utente_loggato));
    
    //chiude file ??
    
    //se ci sono giocate attive devi attendere la prossima estrazione prima di effettuare il logout
    if(libera>0){
        //invia messaggio di errore al client
        sprintf(buffer,"Attendi la prossima estrazione prima di effettuare il logout");
        len=strlen(buffer)+1;
        lmsg=htons(len);
        ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
        ret=send(socket,(void*)buffer,len,0);
        return 0;
    }
    
    //invia un messaggio di avvenuto logout
    sprintf(buffer,"Logout avvenuto con successo");
    len=strlen(buffer)+1;
    lmsg=htons(len);
    ret=send(socket,(void*)&lmsg,sizeof(uint16_t),0);
    ret=send(socket,(void*)buffer,len,0);
    printf("%s\n",buffer);
    return 1;
    
    
}





//-----------------------------------FUNZIONI DI UTILITA'-------------------------------------------

//differenza in secondi tra due orari (ora1,min1,sec1) e (ora2,min2,sec2)
//(ora1,min1,sec1) sempre piu recente,o uguale, di (ora2,min2,sec2)
int diff_secondi(int ora1,int min1,int sec1,int ora2,int min2,int sec2){
    
    int diff;
    
    sec1 +=ora1*3600 + min1*60;
    sec2 +=ora2*3600 + min2*60;
    diff = sec1-sec2;
    return diff;

}


//restituisce il numero di ambi generabili con n numeri
int ambi_gen(int n){
    switch(n){
        case 2:
            return 1;
        case 3:
            return 3;
        case 4:
            return 6;
        case 5:
            return 10;
        case 6:
            return 15;
        case 7:
            return 21;
        case 8:
            return 28;
        case 9:
            return 36;
        case 10:
            return 45;     
    }
}

int terni_gen(int n){
    switch(n){
        case 3:
            return 1;
        case 4:
            return 4;
        case 5:
            return 10;
        case 6:
            return 20;
        case 7:
            return 35;
        case 8:
            return 56;
        case 9:
            return 84;
        case 10:
            return 120;     
    }
}

int quaterne_gen(int n){
    switch(n){
        case 4:
            return 1;
        case 5:
            return 5;
        case 6:
            return 15;
        case 7:
            return 35;
        case 8:
            return 70;
        case 9:
            return 126;
        case 10:
            return 210;     
    }
}

int cinquine_gen(int n){
    switch(n){
        case 5:
            return 1;
        case 6:
            return 6;
        case 7:
            return 21;
        case 8:
            return 56;
        case 9:
            return 126;
        case 10:
            return 252;     
    }
}




