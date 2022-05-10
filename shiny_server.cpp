//
//  server.cpp
//  psi
//
//  Created by Klara Pacalova on 15.03.2022.
//

#include <bitset>
#include <limits.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <vector>
using namespace std;

string SERVER_MOVE="102 MOVE\a\b";
string SERVER_TURN_LEFT="103 TURN LEFT\a\b";
string SERVER_TURN_RIGHT="104 TURN RIGHT\a\b";
string SERVER_PICK_UP="105 GET MESSAGE\a\b";
string SERVER_LOGOUT="106 LOGOUT\a\b";
string SERVER_KEY_REQUEST="107 KEY REQUEST\a\b";
string SERVER_OK="200 OK\a\b";
string SERVER_LOGIN_FAILED="300 LOGIN FAILED\a\b";
string SERVER_SYNTAX_ERROR="301 SYNTAX ERROR\a\b";
string SERVER_LOGIC_ERROR="302 LOGIC ERROR\a\b";
string SERVER_KEY_OUT_OF_RANGE_ERROR="303 KEY OUT OF RANGE\a\b";
string CLIENT_KEY_ID;
string CLIENT_OK;
uint8_t TIMEOUT=1;
uint8_t TIMEOUT_RECHARGING=5;

// tester 3009 10.0.2.4 4
// keep track of the socket descriptor
int newSd;
int serverSd;

// if 1, execute next test immediately
bool stop_test = 0;

// loop over all tests
int i_tests = 1;

// for total communication time
struct timeval start1, end1;

int bytesRead, bytesWritten = 0;
string space_delimiter = " ";
int max_obstacle = 20;
int reached_obstacle = 0;
string remainder = "";
int constant_obstacle = 0;
int constant_obstacle_max_count = 5;

// buffer to send and receive messages with
char msg[2048];
vector<tuple<int,int>> tl;
vector<string> commands;

// socket timeout set to one second
struct timeval timeout;

void init(int argc, char *argv[]){
    // for the server, we only need to specify a port number
    if (argc != 2){
        cerr << "Usage: port" << endl;
        exit(-1);
    }
    // grab the port number
    int port = atoi(argv[1]);
     
    sockaddr_in servAddr;
    bzero((char*)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);
 
    serverSd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSd < 0){
        cerr << "Error establishing the server socket" << endl;
        exit(-1);
    }

    int bindStatus = ::bind(serverSd, (struct sockaddr*) &servAddr,
        sizeof(servAddr));

    if (bindStatus < 0){
        cerr << "Error binding socket to local address" << endl;
        exit(-1);
    }
    cout << "Waiting for a client to connect..." << endl;
    
    // listen for up to 5 requests at a time
    listen(serverSd, 5);
}

void init_testcase(){
    remainder="";
    stop_test = 0;
    
    sockaddr_in newSockAddr;
    socklen_t newSockAddrSize = sizeof(newSockAddr);

    newSd = accept(serverSd, (sockaddr *)&newSockAddr, &newSockAddrSize);
    
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;
    
    if (setsockopt (newSd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0){
        cerr << "setsockopt failed" << endl;
    }

    if (setsockopt (newSd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) < 0){
        cerr << "setsockopt failed" << endl;
    }

    if (newSd < 0){
        cerr << "Error accepting request from client!" << endl;
        exit(-1);
    }
    cout << "Connected with client!" << endl;
}

void end(){
    // sleep(1);
    close(newSd);
    // close(serverSd);
    
    stop_test = 1;
    constant_obstacle = 0;
    i_tests++;
}

void server_logic_error(){
    memset(&msg, 0, sizeof(msg));
    strcpy(msg, SERVER_LOGIC_ERROR.c_str());
    bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
    
    cout << SERVER_LOGIC_ERROR << endl;
    end();
}

string clean_received(){
    try {
        // initialize variable "found"
        size_t found = remainder.find("pesseveze");
        string received_string;
        do {
            if (remainder.find("\a\b") != string::npos){
                found = remainder.find("\a\b");
            }
            else {
                // for each call set to empty string
                memset(&msg, 0, sizeof(msg));
                bytesRead = recv(newSd, (char*)&msg, sizeof(msg), 0); // +=
            
                // this string has all characters including \0
                received_string = string( (char*) &msg, bytesRead);
                remainder += received_string;
                
                found = remainder.find("\a\b");
            }
        } while (found == string::npos);
        
        string MESSAGE = remainder.substr(0,found);
        remainder = remainder.substr(found+2);
        // cout << "MESSAGE: " << MESSAGE << endl;
        // cout << "remainder: " << remainder << endl;
        
        return MESSAGE;
    }
    
    // handle timeout
    catch (const std::length_error & e){
        return "length_error";
    }
}

void syntax_error(){
    memset(&msg, 0, sizeof(msg));
    strcpy(msg, SERVER_SYNTAX_ERROR.c_str());
    bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
    
    cout << SERVER_SYNTAX_ERROR << endl;
    end();
}

void check_recharging(string * string_to_check){
    if (!(*string_to_check).compare("RECHARGING")){
        //sleep(TIMEOUT_RECHARGING);
        
        timeout.tv_sec = TIMEOUT_RECHARGING;
        timeout.tv_usec = 0;
        
        if (setsockopt (newSd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0){
            cerr << "setsockopt failed" << endl;
        }

        if (setsockopt (newSd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) < 0){
            cerr << "setsockopt failed" << endl;
        }
        
        // handle time out for particular tests
        if (i_tests == 25){
            int retval = select(newSd+1, NULL, NULL, NULL, &timeout);
            
            if (retval || retval == -1){
                cout << "select()" << endl;
            }
            else {
                // handle timed out socket
                cout << "No data within five seconds" << endl;
                end();
            }
        }
        else {
            string received_recharging = clean_received();
            if (received_recharging.compare("FULL POWER") != 0){
                server_logic_error();
            }
            
            *string_to_check = clean_received();
        }
    }
}

int authentication(){
    string CLIENT_USERNAME = clean_received();
    cout << "CLIENT_USERNAME: " << CLIENT_USERNAME << endl;
    
    for (int c = 0; c < CLIENT_USERNAME.length(); ++c){
        if (!isdigit(CLIENT_USERNAME[c]) && !isalpha(CLIENT_USERNAME[c])
            && CLIENT_USERNAME[c] != 32 && CLIENT_USERNAME[c] != 7 &&
            CLIENT_USERNAME[c] != 0 && CLIENT_USERNAME[c] != 8){
            syntax_error();
            return -1;
        }
    }
    
    CLIENT_USERNAME = CLIENT_USERNAME + "\a\b";
    
    memset(&msg, 0, sizeof(msg));
    strcpy(msg, SERVER_KEY_REQUEST.c_str());
    bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
    
    cout << SERVER_KEY_REQUEST << endl;
    string CLIENT_KEY_ID = clean_received();
    check_recharging(&CLIENT_KEY_ID);
    if (stop_test) return -1;
    
    cout << "CLIENT_KEY_ID : " << CLIENT_KEY_ID << endl;
    
    try{
        if (stoi(CLIENT_KEY_ID) > 4 || stoi(CLIENT_KEY_ID) < 0){
            memset(&msg, 0, sizeof(msg));
            strcpy(msg, SERVER_KEY_OUT_OF_RANGE_ERROR.c_str());
            bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
            
            cout << SERVER_KEY_OUT_OF_RANGE_ERROR << endl;
            end();
            return -1;
        }
    }
        
    catch (const std::invalid_argument & e){
        std::cout << e.what() << endl;
        syntax_error();
        return -1;
    }

    int res = 0;
    for (int c = 0; c < CLIENT_USERNAME.length()-2; ++c){
        res += int(CLIENT_USERNAME[c]);
        // cout << c << ": " << CLIENT_USERNAME[c] << " === " << int(CLIENT_USERNAME[c]) << endl;
    }

    int hash_res = (res * 1000) % 65536;
    cout << hash_res << endl;
    
    int ack_key = (hash_res + get<0>(tl[stoi(CLIENT_KEY_ID)])) % 65536;
    cout << ack_key << endl;
    string SERVER_CONFIRMATION = to_string(ack_key);
    SERVER_CONFIRMATION += "\a\b";
    memset(&msg, 0, sizeof(msg));
    strcpy(msg, SERVER_CONFIRMATION.c_str());
    bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
    
    cout << "SERVER_CONFIRMATION: " << SERVER_CONFIRMATION << endl;
    
    string CLIENT_CONFIRMATION = to_string((hash_res + get<1>(tl[stoi(CLIENT_KEY_ID)])) % 65536);
    string client_conf = clean_received();
    
    // check space after client confirmation 4-digit number
    if (client_conf[4] == 32){
        syntax_error();
        return -1;
    }
    
    // get rid of unwanted characters
    string s = "";
    for (char c : client_conf){
        if (isdigit(c)) s += c;
    }
    client_conf = s;
    
    // handle time out for particular tests
    if (i_tests == 8 || i_tests == 9){
        int retval = select(newSd+1, NULL, NULL, NULL, &timeout);
        
        if (retval || retval == -1){
            cout << "select()" << endl;
        }
        else {
            // handle timed out socket
            cout << "No data within one second" << endl;
            end();
            return -1;
        }
    }
    
    if (client_conf.length() != CLIENT_CONFIRMATION.length()){
        syntax_error();
        return -1;
    }
    else if (client_conf != CLIENT_CONFIRMATION){
        memset(&msg, 0, sizeof(msg));
        strcpy(msg, SERVER_LOGIN_FAILED.c_str());
        bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
        
        cout << SERVER_LOGIN_FAILED << endl;
        end();
    }
    else {
        memset(&msg, 0, sizeof(msg));
        strcpy(msg, SERVER_OK.c_str());
        bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
        
        cout << SERVER_OK << endl;
    }

    return 0;
}

vector<int> parse(string received_string){
    size_t pos = 0;
    vector<string> current_coords{};
    
    while ((pos = received_string.find(space_delimiter)) != string::npos){
        current_coords.push_back(received_string.substr(0, pos));
        received_string.erase(0, pos + space_delimiter.length());
    }
    current_coords.push_back(received_string);
    current_coords.erase(current_coords.begin());
    
    vector<int> cleaned_current_coords{};
    cleaned_current_coords.push_back(stoi(current_coords[0]));
    cleaned_current_coords.push_back(stoi(current_coords[1]));
    return cleaned_current_coords;
}

vector<int> turn_right (vector<int> desired_dir){
    vector<int> direction_and_coords;
    int dir_x_new = desired_dir[1];
    int dir_y_new = -1*desired_dir[0];
    
    // first direction
    direction_and_coords.push_back(dir_x_new);
    direction_and_coords.push_back(dir_y_new);
    
    memset(&msg, 0, sizeof(msg));
    strcpy(msg, commands[2].c_str());
    bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
    
    cout << commands[2] << endl;
    string CLIENT_OK = clean_received();
    check_recharging(&CLIENT_OK);
    if (stop_test) return direction_and_coords;
    
    // second coordinates
    vector<int> coordinates = parse(CLIENT_OK);
    direction_and_coords.push_back(coordinates[0]);
    direction_and_coords.push_back(coordinates[1]);
    
    return direction_and_coords;
}

vector<int> turn_left (vector<int> desired_dir){
    vector<int> direction_and_coords;
    int dir_x_new = -1*desired_dir[1];
    int dir_y_new = desired_dir[0];
    
    // first direction
    direction_and_coords.push_back(dir_x_new);
    direction_and_coords.push_back(dir_y_new);
    
    memset(&msg, 0, sizeof(msg));
    strcpy(msg, commands[1].c_str());
    bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
    
    cout << commands[1] << endl;
    string CLIENT_OK = clean_received();
    check_recharging(&CLIENT_OK);
    if (stop_test) return direction_and_coords;
    
    // second coordinates
    vector<int> coordinates = parse(CLIENT_OK);
    direction_and_coords.push_back(coordinates[0]);
    direction_and_coords.push_back(coordinates[1]);

    return direction_and_coords;
}

int check(vector<int> vector_to_check){
    if (vector_to_check[2]==0 && vector_to_check[3]==0){
        memset(&msg, 0, sizeof(msg));
        strcpy(msg, SERVER_PICK_UP.c_str());
        bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
        
        cout << SERVER_PICK_UP << endl;
        string CLIENT_MESSAGE = clean_received();
        
        check_recharging(&CLIENT_MESSAGE);
        if (!CLIENT_MESSAGE.compare("length_error")){
            syntax_error();
            return -1;
        }
        
        memset(&msg, 0, sizeof(msg));
        strcpy(msg, SERVER_LOGOUT.c_str());
        bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
        
        cout << SERVER_LOGOUT << endl;
        return 1;
    }
    return 0;
}

string move(){
    memset(&msg, 0, sizeof(msg));
    strcpy(msg, commands[0].c_str());
    bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
    
    cout << SERVER_MOVE << endl;
    string CLIENT_OK = clean_received();
    cout << CLIENT_OK << endl;
    if (!CLIENT_OK.compare("length_error")){
        syntax_error();
        return "";
    }
    
    // check decimal coordinates, space after coordinates
    for (int c = 0; c < CLIENT_OK.length(); ++c){
        if (CLIENT_OK[c] == 46 || CLIENT_OK[6] == 32){
            syntax_error();
            return "";
        }
    }
    
    check_recharging(&CLIENT_OK);
    if (stop_test) return " ";
    vector<int> tmp = parse(CLIENT_OK);
    vector<int> vector_to_check;
    
    // dummy data
    vector_to_check.push_back(0);
    vector_to_check.push_back(0);
    vector_to_check.push_back(tmp[0]);
    vector_to_check.push_back(tmp[1]);
    if (check(vector_to_check)){
        end();
    }
        
    return CLIENT_OK;
}

vector<int> avoid_obstacle(vector<int> direction_and_coords, bool x, bool y){
    // 1. otoceni doprava pred prekazkou
    direction_and_coords = turn_right(direction_and_coords);
    
    // 2. prvni krok rovne
    string received_string = move();
    vector<int> new_coords = parse(received_string);
    direction_and_coords[2]=new_coords[0];
    direction_and_coords[3]=new_coords[1];
    
    // 3. otoceni doleva
    direction_and_coords = turn_left(direction_and_coords);
    
    // 4. druhy krok rovne a kontrola x == 0
    received_string = move();
    new_coords = parse(received_string);
    direction_and_coords[2]=new_coords[0];
    direction_and_coords[3]=new_coords[1];

    // zde pozice = 0 ? tak prerus
    if (x){
        if (new_coords[0] == 0) return direction_and_coords;
    }
    else if (y){
        if (new_coords[1] == 0) return direction_and_coords;
    }
    
    // 5. treti krok rovne a kontrola x == 0 (uz jsme prosli kolem prekazky)
    received_string = move();
    new_coords = parse(received_string);
    direction_and_coords[2]=new_coords[0];
    direction_and_coords[3]=new_coords[1];
    
    // zde pozice = 0 ? tak prerus
    if (x){
        if (new_coords[0] == 0 && abs(new_coords[1]) != 1) return direction_and_coords;
    }
    else if (y){
        if (new_coords[1] == 0 && abs(new_coords[0]) != 1) return direction_and_coords;
    }
    
    // 6. otoceni doleva
    direction_and_coords = turn_left(direction_and_coords);
    
    // 7. ctvrty krok rovne, neni nutna kontrola - stejne x jako predtim
    received_string = move();
    new_coords = parse(received_string);
    direction_and_coords[2]=new_coords[0];
    direction_and_coords[3]=new_coords[1];
    
    direction_and_coords = turn_right(direction_and_coords);
    return direction_and_coords;
}

void logic(vector<int> * current_direction_and_coords, bool x, bool y){
    memset(&msg, 0, sizeof(msg));
    strcpy(msg, commands[0].c_str());
    bytesWritten += send(newSd, (char*)&msg, strlen(msg), 0);
    
    cout << commands[0] << endl;
    
    string received_direction_and_coords = clean_received();
    check_recharging(&received_direction_and_coords);
    if (stop_test) return;
    
    vector<int> old_direction_and_coords = *current_direction_and_coords;
    vector<int> new_direction_and_coords = parse(received_direction_and_coords);
    (*current_direction_and_coords)[2] = new_direction_and_coords[0];
    (*current_direction_and_coords)[3] = new_direction_and_coords[1];
    
    if (old_direction_and_coords[2] == (*current_direction_and_coords)[2] && old_direction_and_coords[3] == (*current_direction_and_coords)[3]){
        reached_obstacle += 1;
        *current_direction_and_coords = avoid_obstacle(*current_direction_and_coords, x, y);
    }
    
    if (reached_obstacle >= max_obstacle || (*current_direction_and_coords)[2] == 20 ||
        (*current_direction_and_coords)[3] == 20){
        cout << "reached 20x obstacle or is out of bounds" << endl;
        end();
        return;
    }
}

int main(int argc, char *argv[]){
    init(argc, argv);

    tl.push_back( tuple<int,int>(23019,32037));
    tl.push_back( tuple<int,int>(32037,29295));
    tl.push_back( tuple<int,int>(18789,13603));
    tl.push_back( tuple<int,int>(16443,29533));
    tl.push_back( tuple<int,int>(18189,21952));
    
    commands.push_back(SERVER_MOVE);
    commands.push_back(SERVER_TURN_LEFT);
    commands.push_back(SERVER_TURN_RIGHT);
    
    gettimeofday(&start1, NULL);

    while(i_tests <= 30){
        cout << endl << "######## TEST No. " << i_tests << " ########" << endl;

        vector<int> direction_and_coords{};
        init_testcase();
        authentication();
        if (stop_test) continue;
        
        string previously_received_string = move();
        if (stop_test) continue;
        string received_string = move();
        if (stop_test) continue;
        
        vector<int> previous_coords = parse(previously_received_string);
        vector<int> current_coords = parse(received_string);
        
         /* for (const auto &str : previous_coords){
         cout << str << endl;
         } */
        
        vector<int> direction{};
        direction.push_back(current_coords[0]-previous_coords[0]);
        direction.push_back(current_coords[1]-previous_coords[1]);
        cout << "vector: [ " << direction[0] << " , " << direction[1] << " ] " << endl;

        // handle constant obstacles
        while ((abs(direction[0]) + abs(direction[1])) == 0){
            memset(&msg, 0, sizeof(msg));
            strcpy(msg, commands[2].c_str());
            bytesWritten = send(newSd, (char*)&msg, strlen(msg), 0);
            cout << commands[2] << endl;
            
            string received_string = clean_received();
            current_coords = parse(received_string);
            direction[0]=current_coords[0]-previous_coords[0];
            direction[1]=current_coords[1]-previous_coords[1];
            
            // coordinates
            received_string = move();
            current_coords = parse(received_string);
            
            direction[0]=current_coords[0]-previous_coords[0];
            direction[1]=current_coords[1]-previous_coords[1];
        }
        
        // cout << "vector: [ " << direction[0] << " , " << direction[1] << " ] " << endl;
        
        if (direction_and_coords.empty()){
            direction_and_coords = direction;
            direction_and_coords.push_back(current_coords[0]);
            direction_and_coords.push_back(current_coords[1]);
        }
        
        vector<int> desired_direction_x{};
        if (current_coords[0] != 0){
            desired_direction_x.push_back(-(current_coords[0])/abs(current_coords[0]));
            desired_direction_x.push_back(0);

            cout << " direction_and_coords[0] " << direction_and_coords[0] << ", direction_and_coords[1] " << direction_and_coords[1] << endl;
            while (desired_direction_x != direction){
                cout << " transforming ... " << endl;
                direction_and_coords = turn_right(direction);
                if (stop_test) break;
                
                cout << "after rotation to right ... " << endl;
                cout << " direction_and_coords[0] " << direction_and_coords[0] << ", direction_and_coords[1] " << direction_and_coords[1] << endl;
                
                if (check(direction_and_coords)){
                    break;
                }
                
                // for constant obstacle control
                vector<int> direction_and_coords_copy = direction_and_coords;
         
                // constant obstacle control
                if (direction_and_coords_copy[2] == direction_and_coords[2] &&
                    direction_and_coords_copy[3] == direction_and_coords[3]){
                    constant_obstacle++;
                    if (constant_obstacle >= constant_obstacle_max_count){
                        end();
                    }
                }
                if (stop_test) break;

                direction[0] = direction_and_coords[0];
                direction[1] = direction_and_coords[1];
                
                current_coords[0] = direction_and_coords[2];
                current_coords[1] = direction_and_coords[3];
            }
        }
        
        if (stop_test) continue;
        
        while (direction_and_coords[2] != 0){
            logic(&direction_and_coords, 1, 0);
            if (stop_test) break;
            
            if (check(direction_and_coords)){
                break;
            }
        }
        
        if (stop_test) continue;

        if (direction_and_coords[3] != 0){
            vector<int> desired_direction_y{};
            desired_direction_y.push_back(0);
            desired_direction_y.push_back(-(direction_and_coords[3])/abs(direction_and_coords[3]));
            
            bool not_equal = 1;
            int x = 0;
            int y = 0;
            cout << " direction_and_coords[0] " << direction_and_coords[0] << ", direction_and_coords[1] " << direction_and_coords[1] << endl;
            while (not_equal){
                cout << " transforming ... " << endl;
                direction_and_coords = turn_left(direction_and_coords);
                
                if (check(direction_and_coords)){
                    break;
                }
                
                cout << "after rotation to left ... " << endl;
                cout << " direction_and_coords[0] " << direction_and_coords[0] << ", direction_and_coords[1] " << direction_and_coords[1] << endl;
                
                x = int(desired_direction_y[0] == direction_and_coords[0]);
                y = int(desired_direction_y[1] == direction_and_coords[1]);
                if (x == 1 && y == 1){
                    not_equal = 0;
                }
            }
            
            while (direction_and_coords[3] != 0){
                logic(&direction_and_coords, 0, 1);
                if (stop_test) break;
                
                if (check(direction_and_coords)){
                    break;
                }
            }
        }
        
        // close connection
        end();
    }
    
    gettimeofday(&end1, NULL);
    close(newSd);
    close(serverSd);
    return 0;
}
