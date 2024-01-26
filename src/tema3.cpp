#include "tema3.h"

void *download_thread_func(void *arg)
{
    Client *client = (Client*) arg;
    MPI_Status status;
    send_info_to_tracker(*client);
    for (auto file : client->wanted_files) {
        req_data_from_tracker(file);
        unordered_map<int, vector<string>> data = recv_data_from_tracker();
        vector<int> peers;

        int file_size = 0;
        for (auto peer : data) {
            peers.push_back(peer.first);
            int peer_chunks = peer.second.size();
            if (peer_chunks > file_size) {
                file_size = peer_chunks;
            }
        }

        client->files.push_back(File(file, file_size));
        client->no_files++;

        int aux = 0;    
        int no_peers = data.size();
        vector<string> segments(10 , "");
        
        for (int i = 0; i < file_size; i++) {
            if (aux >= no_peers) {
                aux = 0;
            }
            if (segments.size() == 10) {
                segments.clear();
            }

            string messages = "REQ_CHUNK\n" + file + " " + data[peers[aux]][i];
            MPI_Send(messages.c_str(), messages.size(), MPI_CHAR, peers[aux], 10, MPI_COMM_WORLD);

            char chunk[HASH_SIZE + 1];
            MPI_Recv(&chunk, HASH_SIZE, MPI_CHAR, peers[aux], 20, MPI_COMM_WORLD, &status);

            segments.push_back(string(chunk));

            client->files[client->files.size() - 1].segments[i] = string(chunk);

            if (i != 0 && i % 9 == 0) {
                update_tracker(file, segments);
            }
            if (i == file_size - 1) {
                update_tracker(file, segments);
            }
            aux++;
        }

        ofstream fout("client" + to_string(client->rank) + "_" + file);
        for (auto segment : client->files[client->files.size() - 1].segments) {
            fout << segment << "\n";
        }

        string done = "DONEFILE";
        MPI_Send(done.c_str(), done.size(), MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);

    }
    string alldone = "ALL_DONE";
    MPI_Send(alldone.c_str(), alldone.size(), MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);

    return NULL;
}

void *upload_thread_func(void *arg)
{
    Client *client = (Client*) arg;

    while(true){
        char messages[64] = "";
        MPI_Status status;
        MPI_Recv(&messages, 64, MPI_CHAR, MPI_ANY_SOURCE, 10, MPI_COMM_WORLD, &status);

        stringstream ss(messages);
        string command;
        ss >> command;

        if (command == "REQ_CHUNK") {
            string file_name, chunk;
            ss >> file_name;
            ss >> chunk;
            string seg_to_send;
            for (auto file : client->files) {
                if (file.name == file_name) {
                    for (auto seg : file.segments) {
                        if (seg == chunk) {
                            seg_to_send = seg;
                            break;
                        }
                    }
                }
            }
            MPI_Send(seg_to_send.c_str(), seg_to_send.size(), MPI_CHAR, status.MPI_SOURCE, 20, MPI_COMM_WORLD);
        } else if (command == "SHUTDOWN") {
            break;
        }
    }

    return NULL;
}

void tracker(int numtasks, int rank) {

    unordered_map<string, unordered_map<int, vector<string>>> data;
    get_info_from_clients(numtasks , data);

    int client_done = 0;
    
    while (client_done < numtasks - 1) {
        char messages[MAX_MESSAGE_SIZE];
        MPI_Status status;

        MPI_Recv(&messages, MAX_MESSAGE_SIZE, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        stringstream ss(messages);
        string command;
        ss >> command;

        if (command == "REQ_INFO") {

            char filename[MAX_FILENAME];
            MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            send_info_to_client(string(filename), data, status.MPI_SOURCE);
        } else if (command == "UPD_TRAC") {

            char filename[MAX_FILENAME];
            MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            int no_segments;
            vector<string> segments;
            MPI_Recv(&no_segments, 1, MPI_INT, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            for (int i = 0; i < no_segments; i++) {
                char segment[HASH_SIZE + 1];
                MPI_Recv(&segment, HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                string seg(segment);
                segments.push_back(seg);
            }
            data.insert({filename, {{status.MPI_SOURCE, segments}}});
        } else if (command == "ALL_DONE") {
            client_done++;
        }
        command.clear();
    }

    for (int i = 1; i < numtasks; i++) {
        string stop = "SHUTDOWN";
        MPI_Send(stop.c_str(), stop.size(), MPI_CHAR, i, 10, MPI_COMM_WORLD);
    }
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    Client client = Client(rank);

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &client);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &client);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
