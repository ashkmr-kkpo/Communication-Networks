#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <climits>

using namespace std;

typedef struct path{
    int next_hop;
    int cost;
    path(int next_hop, int cost) : next_hop(next_hop), cost(cost) {}
} path;

typedef struct message{
    int src;
    int dst;
    string m;
    message(int src, int dst, string m) : src(src), dst(dst), m(m) {}
} message;

typedef map<int, int> edge;
typedef map <int, path*> hop;
typedef map<int, edge> graph;
typedef map<int, hop> table;

void populate_forwarding_table();
void update_forwarding_table();
void print_topology();
void send_messages();

graph neighbors;                // Src Node # -> {Dst Node #: Edge Cost}
table forwarding_table;         // Src Node # -> {Dst Node # -> (Next Hop Node #, Path Cost)}
set<int> nodes;                 // list of nodes
vector<message> message_list;   // list of messages

ofstream f_out;

int main(int argc, char** argv){
    ////printf("Number of arguments: %d", argc);
    if (argc != 4) {
        //printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }

    ifstream f_topo(argv[1]);
    ifstream f_message(argv[2]);
    ifstream f_changes(argv[3]);
    
    f_out.open("output.txt");

    // populate graph, messages from topofile

    int src_id, dst_id, cost;
    string line, m;

    while(f_topo >> src_id >> dst_id >> cost){
        neighbors[src_id][dst_id] = cost;
        neighbors[dst_id][src_id] = cost;
        nodes.insert(src_id);
        nodes.insert(dst_id);        
    }

    while(getline(f_message, line)){
        if(line != ""){
            stringstream ss(line);
            ss >> src_id >> dst_id;
            getline(ss, m);
            message mess(src_id, dst_id, m.substr(1));
            message_list.push_back(mess);            
        }
    }

    // check for duplicates in message_list

    // table forwarding_table;

    populate_forwarding_table();
    print_topology();
    f_out << endl << endl;
    send_messages();

    // loop through changes file

    while(f_changes >> src_id >> dst_id >> cost){

        if(cost == -999){ // remove edge
            neighbors[src_id].erase(dst_id);
            neighbors[dst_id].erase(src_id);

            forwarding_table[src_id].erase(dst_id);
            forwarding_table[dst_id].erase(src_id);
        }else{ // add/update edge
            neighbors[src_id][dst_id] = cost;
            neighbors[dst_id][src_id] = cost;
            nodes.insert(src_id);
            path *p = new path(src_id, 0);
            forwarding_table[src_id][src_id] = p;
            nodes.insert(dst_id);
            p = new path(dst_id, 0);
            forwarding_table[dst_id][dst_id] = p;  
            p = new path(dst_id, neighbors[src_id][dst_id]);
            forwarding_table[src_id][dst_id] = p;
            p = new path(src_id, neighbors[dst_id][src_id]);
            forwarding_table[dst_id][src_id] = p;
        }

        update_forwarding_table();
        print_topology();
        f_out << endl << endl;
        send_messages();
    }

    // close files
    return 0;
}

void populate_forwarding_table(){

    // add neighbor costs to forwarding table
    for(set<int>::iterator it = nodes.begin(); it != nodes.end(); it++){ // src
        int src = *it;

        for(set<int>::iterator it2 = nodes.begin(); it2 != nodes.end(); it2++){ // dest
            int dst = *it2;

            if(src == dst){
                path *p = new path(dst, 0);
                forwarding_table[src][dst] = p;
            }else if(neighbors[src].count(dst) > 0){ // dst is src's neighbor
                path *p = new path(dst, neighbors[src][dst]);
                forwarding_table[src][dst] = p;
            }else{
                // don't add entry in forwarding_table
            }
        }
    }

    update_forwarding_table();
}

void update_forwarding_table(){

    // loop till forwarding_table converges

    bool mod = true;
    while(mod){
        mod = false; // if forwarding table is modified, mod = true

        for(set<int>::iterator it = nodes.begin(); it != nodes.end(); it++){ // src
            int src = *it;

            // loop through neighbors
            for(edge::iterator it2 = neighbors[src].begin(); it2 != neighbors[src].end(); it2++){
                int v = it2->first;
                int cost = it2->second;

                // loop through neighbor's DV
                hop h = forwarding_table[v];
                for(hop::iterator it2 = h.begin(); it2 != h.end(); it2++){ // dst
                    int dst = it2->first;
                    path *p = it2->second;

                    // check entire path for src (path loops back)

                    bool found = false;

                    int next = p->next_hop;

                    while(next != dst){
                        if(next == src) found = true;
                        if(forwarding_table[next].count(dst) <= 0){
                            found = true;
                            break;
                        }
                        next = forwarding_table[next][dst]->next_hop;
                    }

                    if(!found && ((forwarding_table[src].count(dst) <= 0) || (cost + p->cost < forwarding_table[src][dst]->cost))){ // lower cost
                        path *p_new = new path(v, cost + p->cost);
                        forwarding_table[src][dst] = p_new;

                        //printf("here4\n");

                        mod = true;
                    }
                }
            }

            hop h = forwarding_table[src];
            for(hop::iterator it2 = h.begin(); it2 != h.end(); it2++){ // dst
                int dst = it2->first;
                path *p = it2->second;

                if(src != dst){
                    if(neighbors[src].count(p->next_hop) <= 0){ // next_hop is no longer neighbor
                        forwarding_table[src].erase(dst);
                        //printf("here1\n");
                        mod = true;
                    }else{
                        if(forwarding_table[p->next_hop].count(dst) <= 0){ // neighbor no longer has path to dst
                            forwarding_table[src].erase(dst);
                            //printf("here2\n");
                            mod = true;
                        }else{
                            // check entire path for src (path loops back)

                            bool found = false;

                            int next = p->next_hop;

                            while(next != dst){
                                if(next == src) found = true;
                                if(forwarding_table[next].count(dst) <= 0){
                                    found = true;
                                    break;
                                }
                                next = forwarding_table[next][dst]->next_hop;
                            }

                            if(found){
                                forwarding_table[src].erase(dst);
                                //printf("here6\n");
                                mod = true;
                            }

                            if(p->cost != neighbors[src][p->next_hop] + forwarding_table[p->next_hop][dst]->cost){ // cost incorrect
                                p->cost = neighbors[src][p->next_hop] + forwarding_table[p->next_hop][dst]->cost;
                                //printf("here3 %d %d %d\n", src, p->next_hop, dst);
                                mod = true;
                            }
                        }
                    }
                }
            }     
        }
    }
}

void print_topology(){
    for(table::iterator it = forwarding_table.begin(); it != forwarding_table.end(); it++){ // src
        // int src = it->first;
        hop h = it->second;

        for(hop::iterator it2 = h.begin(); it2 != h.end(); it2++){ // dst
            int dst = it2->first;
            path *p = it2->second;

            f_out << dst << " " << p->next_hop << " " << p->cost << endl;
        }   

        f_out << endl;
    }   
}

// same message printing twice

void send_messages(){
    for(vector<message>::iterator it = message_list.begin(); it != message_list.end(); it++){ // messages
        message mess = *it;

        if(forwarding_table[mess.src].count(mess.dst) <= 0){ // path doesn't exist
            f_out << "from " << mess.src << " to " << mess.dst << " cost infinite hops unreachable message " << mess.m << endl;
            continue;
        }

        vector<int> path;

        int next = mess.src;

        while(next != mess.dst){
            path.push_back(next);
            next = forwarding_table[next][mess.dst]->next_hop;
        }

        f_out << "from " << mess.src << " to " << mess.dst << " cost " << forwarding_table[mess.src][mess.dst]->cost << " hops ";

        for(vector<int>::iterator it2 = path.begin(); it2 != path.end(); it2++){
            f_out << *it2 << " ";
        }

        f_out << "message " << mess.m << endl;
    }
}