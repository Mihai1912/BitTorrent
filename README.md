# Tema #3 Protocolul BitTorrent

## Popescu Mihai-Costel 334CD

### Structuri folosite:
```
struct File{
    string name;    // numele fisierului
    int no_segments;    // numarul de segmente din fisier
    vector<string> segments;    // segmentele fisierului
}
```

```
struct Client{
    int rank;   // rank-ul clientului
    int no_files;   // numarul de fisiere detinute
    vector<File> files; // fisierele detinute
    int no_wanted_files;    // numarul de fisiere dorite
    vector<string> wanted_files;    // fisierele dorite
}
```

### Descrierea programului
 - Pentru inceput am citit fisierul de intrare corespunzator fiecarui 
 client, pentru a vedea ce fisiere detine fiecare client si ce fisiere 
 doreste sa descarce.
 - Apoi am transmis datele citite catre thread-urile de download si 
 upload.
 - In thread-ul de download am realizat comunicarea cu tracker pentru al 
 informa ce fisiere detine fiecare client. Informarea se realizeaza prin 
 functia ```send_info_to_tracker(*client)``` care trimite numarul de 
 fisiere pe care le detine tracker-ului apoi pentru fiecare fisier se 
 trimite numele fisierului si numarul de segmente din care este alcatuit 
 fisierul, respectiv trimiterea pe rand a fiecarui fisier.

 #### Descrierea procesului de descarcare a unui fisier
-  Pentru fiecare fisier dorit clientul cere trackerului informatii 
despre fisierul dorit, aceasta actiune se realizeaza cu ajutorul 
functiei ```req_data_from_tracker(file)``` care trimite un mesaj 
de req cu numele fisierului dorit. Tracker-ul ii raspunde cu numarul 
clientrilor care au fisierul, apoi cu rank-ul acestora, numarul de 
segmente si apoi cu segmentele, acest lucru se realizeaza in funtia 
```recv_data_from_tracker``` care va returna un unordered_map cara are 
ca si cheie rank-ul peer ului si ca valoare vectorul de segmente 
detinute de acesta. Caut numarul maxim de segmente primite pentru a stii 
cate segmente are fisierul intreg, iar apoi pentru fiecare segment cer pe 
rand fiecarui client care are fisierul dorit, iar cand termin lista de 
clienti se incepe de la capat, iar acest cilcu este facut pana cand se 
reuseste descarcarea intreg fisierului(exemplu de descarcare : 
seg-1 -> client1 , seg-2 -> client2 , seg3 -> client1 , seg4 -> client2 , 
pentru 2 peers). Pe parcursul descarcarii 
segmentelor acestea sunt bagate in vectorul de segmente al 
utilozatorilui ce descarca, iar din 10 in 10 segmente se informeaza 
tracker-ul pentru a stii ce segmente a descarcat clientul. La sfarsitul 
descarcarii se scriu segmentele 
in fisierul de output, si se trimite un mesaj tracker-ului ca s-a 
termint de descarcat respectivul fisier. Dupa 
terminarea de descarcat a tuturor fisierelor clientul informeaza 
tracker-ul ca terminat de descarcat toate fisierle dorite, iar trackerul 
incrementeaza valoare ```clients_done``` ce reprezinta numarul de 
clienti ce au terminat de descarcat fisierele dorite.

#### Descrierea procesului de upload a unui fisier
- In thread-ul de upload se cilceaza intr-un ```while(true)``` pentru a 
astepta mesajele de la alti clienti, dupa primirea unui mesaj, mesajul 
este parsat,daca este de tip REQ_CHUNK se incepe cautarea segmentului dorit de 
client, daca acesta este gasit se trimte segmentul dorit la client, daca 
nu se asteapta un alt mesaj. Daca mesajul este SHUTDOWN asta inseamna ca 
toti clientii au termiant de descarcat fisierele pe care le voiau si
bucla infinita se incheie si se opreste thread ul.

Iar in tracker dupa terminarea de descarcat fisiere din partea tuturor 
clientilor este trimtis un mesaj catre thread ul de uploate pentru 
incetarea rularii.