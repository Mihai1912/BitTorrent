#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <sstream>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100
#define MAX_MESSAGE_SIZE 10

#define START_DOWNLOAD 10
#define SEND_INFO_TRACKER_TAG 11

using namespace std;

struct File{
    string name;
    int no_segments;
    vector<string> segments;

    File(string name, int no_segments){
        this->name = name;
        this->no_segments = no_segments;
        segments = vector<string>(no_segments);
    }

    File(){}
};

struct Client{
    int rank;
    int no_files;
    vector<File> files;
    int no_wanted_files;
    vector<string> wanted_files;

    Client(int rank){
        this->rank = rank;
        ifstream fin("in" + to_string(rank) + ".txt");
        fin >> no_files;
        for (int i = 0; i < no_files; i++){
            File file;
            fin >> file.name;
            fin >> file.no_segments;
            for (int j = 0; j < file.no_segments; j++){
                string segment;
                fin >> segment;
                file.segments.push_back(segment);
            }
            files.push_back(file);
        }
        fin >> no_wanted_files;
        for (int i = 0; i < no_wanted_files; i++){
            string wanted_file;
            fin >> wanted_file;
            wanted_files.push_back(wanted_file);
        }
    }

    Client(){}
};


void send_info_to_tracker(Client client){
    MPI_Send(&client.no_files, 1, MPI_INT, TRACKER_RANK, SEND_INFO_TRACKER_TAG, MPI_COMM_WORLD);
    for (auto file : client.files){
        MPI_Send(file.name.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, SEND_INFO_TRACKER_TAG, MPI_COMM_WORLD);
        MPI_Send(&file.no_segments, 1, MPI_INT, TRACKER_RANK, SEND_INFO_TRACKER_TAG, MPI_COMM_WORLD);
        for (int i = 0; i < file.no_segments; i++){
            MPI_Send(file.segments[i].c_str(), HASH_SIZE, MPI_CHAR, TRACKER_RANK, SEND_INFO_TRACKER_TAG, MPI_COMM_WORLD);
        }
    }
}

void get_info_from_clients(int numtasks, unordered_map<string, unordered_map<int, vector<string>>> &data){
    int no_files, no_segments;
    for (int i = 1; i < numtasks; i++) {
        MPI_Recv(&no_files, 1, MPI_INT, i, SEND_INFO_TRACKER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for (int j = 0; j < no_files; j++) {
            char name[MAX_FILENAME + 1];
            MPI_Recv(&name, MAX_FILENAME, MPI_CHAR, i, SEND_INFO_TRACKER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            string file_name(name);
            MPI_Recv(&no_segments, 1, MPI_INT, i, SEND_INFO_TRACKER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            vector<string> segments;
            for (int k = 0; k < no_segments; k++) {
                char segment[HASH_SIZE + 1];
                MPI_Recv(&segment, HASH_SIZE, MPI_CHAR, i, SEND_INFO_TRACKER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                string seg(segment);
                segments.push_back(seg);
            }
            data[file_name][i] = segments;
        }
    }
}

void req_data_from_tracker(string filename){
    string messages = "REQ_INFO";
    MPI_Send(messages.c_str(), messages.size() + 1, MPI_CHAR, TRACKER_RANK, 30, MPI_COMM_WORLD);
    MPI_Send(filename.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 30, MPI_COMM_WORLD);
}

void send_info_to_client(string filename, unordered_map<string, unordered_map<int, vector<string>>> data, int client_rank){
    int no_peers = data[filename].size();
    MPI_Send(&no_peers, 1, MPI_INT, client_rank, 40, MPI_COMM_WORLD);
    for (auto peer : data[filename]){
        MPI_Send(&peer.first, 1, MPI_INT, client_rank, 40, MPI_COMM_WORLD);
        int no_segments = peer.second.size();
        MPI_Send(&no_segments, 1, MPI_INT, client_rank, 40, MPI_COMM_WORLD);
        for (auto segment : peer.second){
            MPI_Send(segment.c_str(), HASH_SIZE, MPI_CHAR, client_rank, 40, MPI_COMM_WORLD);
        }
    }
}

unordered_map<int, vector<string>> recv_data_from_tracker() {
    unordered_map<int, vector<string>> data;
    int no_peers;
    MPI_Recv(&no_peers, 1, MPI_INT, TRACKER_RANK, 40, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    for (int i = 0; i < no_peers; i++) {
        int peer_rank, no_segments;
        MPI_Recv(&peer_rank, 1, MPI_INT, TRACKER_RANK, 40, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&no_segments, 1, MPI_INT, TRACKER_RANK, 40, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        vector<string> segments;
        for (int j = 0; j < no_segments; j++) {
            char segment[HASH_SIZE + 1] = {0};
            MPI_Recv(&segment, HASH_SIZE, MPI_CHAR, TRACKER_RANK, 40, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            string seg(segment);
            segments.push_back(seg);
        }
        data[peer_rank] = segments;
    }
    return data;
}

void update_tracker(string filename, vector<string>chunks) {
    string messages = "UPD_TRAC";
    MPI_Send(messages.c_str(), messages.size(), MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
    MPI_Send(filename.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
    int no_chunks = chunks.size();
    MPI_Send(&no_chunks, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
    for (auto chunk : chunks) {
        MPI_Send(chunk.c_str(), HASH_SIZE, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
    }
}