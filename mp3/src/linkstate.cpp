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


typedef struct message{
    int src;
    int dst;
    string m;
    message(int src, int dst, string m) : src(src), dst(dst), m(m) {}
} message;

typedef map<int, int> edge; 
typedef map<int, edge> graph;//source, dest, cost
//typedef map <int, int> links;
typedef map<int, pair<int,int> > table;  //destination, link

typedef map <int, pair<int,int> > D_vertices;

void make_forwarding_table(table& forwarding_table);
int find_link_from_min_tree(int node_to_find,int source_link, D_vertices D_V);
void send_messages(table *forwarding_table);

graph neighbors;                // Src Node # -> {Dst Node #: Edge Cost}

set<int> nodes;                 // list of nodes
set<int> nodes_temp;
vector<message> message_list;   // list of messages
ofstream f_out;

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    ifstream f_topo(argv[1]);
    ifstream f_message(argv[2]);
    ifstream f_changes(argv[3]);
    // populate graph, messages from topofile
    f_out.open("output.txt");

    int src_id, dst_id, cost;
    string line, m;
    //build graph using f_topo
    while(f_topo >> src_id >> dst_id >> cost){
        neighbors[src_id][dst_id] = cost;
        neighbors[dst_id][src_id] = cost;
        //printf("%d + %d + %d \n",src_id, dst_id, cost);
        nodes.insert(src_id);
        nodes.insert(dst_id);      
    }
    nodes_temp=nodes;
    while(getline(f_message, line)){
         if(line != ""){
        stringstream ss(line);
        ss >> src_id >> dst_id;
        getline(ss, m);
      //  f_out<< src_id << " "<< dst_id << m;
        message mess(src_id, dst_id, m.substr(1));
        message_list.push_back(mess);
        }
    }   
    table forwarding_table[nodes.size()];
    table global_fwd;
    for(int j=0;j<nodes.size();j++)
    	make_forwarding_table(forwarding_table[j]);
    //messaging
    send_messages(forwarding_table);
    while(f_changes >> src_id >> dst_id >> cost)
    {

        if(cost == -999){ // remove edge
            neighbors[src_id].erase(dst_id);
            neighbors[dst_id].erase(src_id);

            if(neighbors[src_id].size() == 0){
                neighbors.erase(src_id);
                nodes.erase(src_id);
            }

            if(neighbors[dst_id].size() == 0){
                neighbors.erase(dst_id);
                nodes.erase(dst_id);
            }

        }else{ // add/update edge
            neighbors[src_id][dst_id] = cost;
            neighbors[dst_id][src_id] = cost;
            nodes.insert(src_id);
            nodes.insert(dst_id);  
        }

        // update/print forwarding table
        //f_out<<"CHANGE \n";
        table forwarding_table_changes[nodes.size()];
        nodes_temp=nodes;
        for(int j=0;j<nodes.size();j++)
    	make_forwarding_table(forwarding_table_changes[j]);
        send_messages(forwarding_table_changes);
    }

    

    return 0;
}
//from <x> to <y> cost <path_cost> hops <hop1> <hop2> <...> message <message>
void send_messages(table *forwarding_table)
{
    f_out<< endl << endl;
	for(int k=0;k<message_list.size();k++)
    {
    	int path[nodes.size()];
    	int curr_node= message_list[k].src;
    	int dest= message_list[k].dst;
    	int total_cost=0;
    	int i=0;
    	int found=0;
    	//CHECK if PATH EXISTS
    	//FORWARDING TABLE NEEDS TO BE A MAP?
    	//No,my fwding table has dest source nexthop. so each fwding table identified by source.
    	while(curr_node != dest)
    	{	
    		found=0;
	    	for(int j=0;j<nodes.size();j++)
	    	{
	    		for( map<int, pair<int,int> >::iterator it = forwarding_table[j].begin(); it != forwarding_table[j].end(); it++)
	    		{
	    			if(it->first== dest && it->second.first == curr_node)
	    			{
	    				if(it->second.second==9989)//marker for infinite hop
	    				{
	    					found =0;
	    					continue;
	    				}
	    				path[i]=curr_node;
	    				i++;
	    				total_cost= total_cost + neighbors[curr_node][it->second.second];
	    				curr_node= it->second.second;
	    				found=1;
	    			}
	    		}
	    	}
	    	if(!found)
	    		break;
	    }
	    if(!found)
	    {
	    	f_out << "from " << message_list[k].src << " to " << message_list[k].dst << " cost infinite hops unreachable message " << message_list[k].m << endl;
	    }
	    else
	    {
	    f_out<<"from "<<message_list[k].src<<" to "<<dest<<" cost "<<total_cost<< " hops ";
	    for(int m=0;m<i;m++)
        {
            if(path[m]==dest)
                continue;
	    	f_out<<path[m]<<" ";
        }
	    f_out<<"message "<<message_list[k].m;
	    f_out<< endl;
		}
    }
}
void make_forwarding_table(table& forwarding_table)
{
	//outer loop for to make tables for all nodes
	D_vertices D_V;
	set<int> N_array;

	int size= (nodes).size();				//each node only has forwards for others so n-1 pairs
	for(set<int>::iterator it = nodes.begin(); it != nodes.end(); it++) // src
	{
		int src = *it;
		D_V[src] = make_pair(9999,src); //set to infinity in D(V)
		//f_out<<D_V[src].first<< "-"<<D_V[src].second <<"\n";
	}
	set<int>::iterator it = nodes_temp.begin();
	int src_curr = *it;			

	D_V[src_curr].first= 0;
    D_V[src_curr].second= src_curr;
	while(size>0) 						//Last node's edges would have already been checked
		{
			int src=*it;
            N_array.insert(src);			//QUEUE in N_ARRAY to check for elements already gone through
			for(set<int>::iterator it2 = nodes.begin(); it2 != nodes.end(); it2++)
			{ // dest
            	int dst = *it2;
            	if (N_array.find(dst) != N_array.end())
            	{
            		continue;
            	}
            	//MIN of D_V or (edge + D_w)
            	//TIE SHOULD CHOOSE LOWEST NODE ID

            	else if(neighbors[src][dst]!=0 && neighbors[src][dst]+ D_V[src].first < D_V[dst].first)
            	{
            		D_V[dst].first= neighbors[src][dst] + D_V[src].first;
            		D_V[dst].second= src;
            		
            	}	
            	else if(neighbors[src][dst]!=0 && neighbors[src][dst]+ D_V[src].first == D_V[dst].first)
            	{
            		if(D_V[dst].second < src)
            		{	

            		}
            		else 
            		{
            			D_V[dst].first= neighbors[src][dst] + D_V[src].first;
            			D_V[dst].second= src;
            		}
            	}	
            }

            //Next node is one that has minimum value in D_V(also needs to not be in N_array))
            int min=9999; 
            int minind;
            for(map <int, pair<int,int> >::iterator it3 = D_V.begin(); it3 != D_V.end(); it3++)
            {
            	//f_out<<it3->first<<" "<<it3->second.first<<" "<<it3->second.second<<"\n";
            	if (it3->second.first < min && N_array.find(it3->first) == N_array.end())
            	{
            		min=it3->second.first;
            		minind= it3->first;
            	}
            	else if(it3->second.first == min && N_array.find(it3->first) == N_array.end())
            	{
            		if(it3->first < minind)			//TIE BREAK 
            		{
            			minind= it3->first;
            		}
            	}
            }

            it=(nodes).find(minind); //TIE SHOULD CHOOSE LOWEST NODE ID
            //f_out<<"MIN"<<minind<<"\n";
            size--;


		}
	//GO THROUGH N_ARRAY ASSIGN FORWARD ONE BY ONE, RECURSE TILL EDGE FROM SOURCE
		// destination nexthop pathcost 
		//my forwarding table has destination source nexthop and I modified output to match
	for(set<int>::iterator it = N_array.begin(); it != N_array.end(); it++)
	{
		int dest = *it;
		if(src_curr ==dest)
		{
			neighbors[dest][dest]=0;
			forwarding_table[dest].first=dest;
		}
		forwarding_table[dest].first= src_curr;
		if(D_V[dest].second == src_curr)
		{
			
			forwarding_table[dest].second= dest;

		}
		else
		{
			int find_link=  find_link_from_min_tree(D_V[dest].second, src_curr, D_V);
			forwarding_table[dest].second=find_link;
			//f_out<<find_link<<"\n";
		}

		//f_out<<D_V[dest].first<< "-"<<D_V[dest].second <<"\n";

	}
	for( map<int, pair<int,int> >::iterator it = forwarding_table.begin(); it != forwarding_table.end(); it++) 
	{
		//cout<<it->first<<" "<<it->second.first<<" "<<it->second.second<< endl;
		if(it->second.second==9989)
			continue;
		f_out<<it->first<<" "<<it->second.second<<" "<<D_V[it->first].first<< endl;
	}
	f_out<< endl;
	//erase node 
	//N-array needs to be initialised everytime, D_V too
	nodes_temp.erase(src_curr);
}

//recursive fn to find edge from source 
int find_link_from_min_tree(int node_to_find, int source_link,D_vertices D_V)
{
		if(D_V[node_to_find].second == source_link)  //if edge is from source, we've found it
		{
			return node_to_find;
		}
		else if(node_to_find == D_V[node_to_find].second )
			return 9989;

		else 
			return find_link_from_min_tree(D_V[node_to_find].second, source_link, D_V);


}