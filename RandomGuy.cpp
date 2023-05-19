// the portion of the code that manages connections between the server and the client was provided by Ammon Shurtz and Lawry Sorenson

#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <cstring>


// globals
int turn = -1;
int round_to_play;
int player;
double time_limit = -1;
double t1, t2; // time remaining
int state[8][8];

int validMoves[64];
int numValidMoves;

// variables for server buffer
int sfd;
const int BLOCK_SIZE = 1024;
char buf[2 * BLOCK_SIZE];
int bufstart = 0;
int bufend = 0;


char* readline() {
    int i = bufstart;
    
    while (i < bufend && buf[i] != '\n') ++i;

    if (i == bufend && bufstart >= BLOCK_SIZE) { // shift to start of buffer
        for (int i=bufstart; i< bufend; ++i) {
            buf[i-bufstart] = buf[i];
        }
        i = bufend -= bufstart;
        bufstart = 0;
    }

    while (i == bufend) {
        int temp = read(sfd, buf+bufend, 2*BLOCK_SIZE-bufend);
        if (temp > 0) bufend += temp;
        else {
            printf("ERROR: FAILED TO READ FROM SERVER\n");
            exit(1);
        }

        while (i < bufend && buf[i] != '\n') ++i;
    }
    
    if (buf[i] != '\n') {
        printf("ERROR: FILLED BUFFER BUT NO NEW LINE\n");
        exit(1);
    }

    buf[i] = 0;
    int ret = bufstart;
    bufstart = i+1;
    return buf+ret;
}


// function provided by Ammon Shurtz and Lawry Sorenson (modified slightly by Jacob Crandall)
void initconn(char* host) {
    struct sockddr *addr;

    char port[5] = "3333";
    port[3]+=player;

    // printf("PORT: %s\n", port);

	struct addrinfo hints;
	struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;  /* Any protocol */

    int s = getaddrinfo(host, port, &hints, &result);
    if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

    // // Explicitly assigning port number by
    // // binding client with that port
    // struct sockaddr_in my_addr1;
    // my_addr1.sin_family = AF_INET;
    // my_addr1.sin_addr.s_addr = INADDR_ANY;
    // my_addr1.sin_port = htons(3335 + player);

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd == -1)
			continue;

        // cout << ntohs(my_addr1.sin_port) << endl;
        // if (bind(sfd, (struct sockaddr*) &my_addr1, sizeof(struct sockaddr_in)) == 0)
        //     printf("Binded Correctly\n");
        // else
        //     printf("Unable to bind\n");

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;  /* Success */

		close(sfd);
	}

	if (rp == NULL) {   /* No address succeeded */
        perror("Could not connect");
		exit(EXIT_FAILURE);
	}

    // cout << sfd << endl;

	freeaddrinfo(result);   /* No longer needed */

    printf("Server info: %s\n", readline());
}

inline int read_int() {
    return atoi(readline());
}

inline double read_double() {
    return atof(readline());
}

void readMessage() {
    turn = read_int();

    if (turn == -999) { // end the game
        // TODO: clean up memory

        sleep(1);
        exit(0);
    }
    
    round_to_play = read_int();

    t1 = read_double();
    t2 = read_double();
    if (time_limit == -1)
        time_limit = t2 - 10;

    // store state as x,y-coordinate
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            state[i][j] = read_int();   // stored in row, col format -> state[i][j] is for row i, col j
        }
    }

    readline(); // why?


    // printf("Turn: %d\n", turn);
    // printf("Round: %d\n", round_to_play);
    // printf("t1: %f\n", t1);
    // printf("t2: %f\n", t2);

    // print_board(state);
}

void sendMessage(int m) {
    int i = m/8;
    int j = m%8;
    printf("sending %i, %i from %i\n", i, j, m);
    dprintf(sfd, "%d\n%d\n", i, j);
}

bool checkDirection(int state[8][8], int row, int col, int incx, int incy) {
    int sequence[7];
    int seqLen;
    int i, r, c;
    
    seqLen = 0;
    for (i = 1; i < 8; i++) {
        r = row+incy*i;
        c = col+incx*i;
    
        if ((r < 0) || (r > 7) || (c < 0) || (c > 7))
            break;
    
        sequence[seqLen] = state[r][c];
        seqLen++;
    }
    
    int count = 0;
    for (i = 0; i < seqLen; i++) {
        if (player == 1) {
            if (sequence[i] == 2)
                count ++;
            else {
                if ((sequence[i] == 1) && (count > 0))
                    return true;
                break;
            }
        }
        else {
            if (sequence[i] == 1)
                count ++;
            else {
                if ((sequence[i] == 2) && (count > 0))
                    return true;
                break;
            }
        }
    }
    
    return false;
}

bool couldBe(int state[8][8], int row, int col) {
    int incx, incy;
    
    for (incx = -1; incx < 2; incx++) {
        for (incy = -1; incy < 2; incy++) {
            if ((incx == 0) && (incy == 0))
                continue;
        
            if (checkDirection(state, row, col, incx, incy))
                return true;
        }
    }
    
    return false;
}

// generates the set of valid moves for the player; returns a list of valid moves (validMoves)
void getValidMoves(int round_to_play, int state[8][8]) {
    int i, j;
    
    numValidMoves = 0;
    if (round_to_play < 4) {
        if (state[3][3] == 0) {
            validMoves[numValidMoves] = 3*8 + 3;
            numValidMoves ++;
        }
        if (state[3][4] == 0) {
            validMoves[numValidMoves] = 3*8 + 4;
            numValidMoves ++;
        }
        if (state[4][3] == 0) {
            validMoves[numValidMoves] = 4*8 + 3;
            numValidMoves ++;
        }
        if (state[4][4] == 0) {
            validMoves[numValidMoves] = 4*8 + 4;
            numValidMoves ++;
        }
        printf("Valid Moves:\n");
        for (i = 0; i < numValidMoves; i++) {
            printf("%i, %i\n", (int)(validMoves[i] / 8), validMoves[i] % 8);
        }
        printf("\n");
    }
    else {
        printf("Valid Moves:\n");
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                if (state[i][j] == 0) {
                    if (couldBe(state, i, j)) {
                        validMoves[numValidMoves] = i*8 + j;
                        numValidMoves ++;
                        printf("%i, %i\n", i, j);
                    }
                }
            }
        }
    }
}

// TODO: You should modify this function
// validMoves is a list of valid locations that you could place your "stone" on this turn
// Note that "state" is a global variable 2D list that shows the state of the game
int move() {
    // just move randomly for now
    int myMove = rand() % numValidMoves;
    
    return myMove;
}


// compile on your machine: g++ RandomGuy.cpp -o RandomGuy
// call: ./RandomGuy [ipaddress] [player_number]
//   ipaddress is the ipaddress on the computer the server was launched on.  Enter "localhost" if it is on the same computer
//   player_number is 1 (for the black player) and 2 (for the white player)
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Not enough parameters\n");
    }

    srand(time(NULL));

    player = atoi(argv[2]);

    initconn(argv[1]);
    
    while (true) {
        printf("Read\n");
        readMessage();
        
        if (turn == player) {
            printf("my move in round_to_play %i\n", round_to_play);
            printf("state is:\n");
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    printf("%i ", state[i][j]);
                }
                printf("\n");
            }
            getValidMoves(round_to_play, state);
            
            int myMove = move();

            // printf("Selection: %i, %i\n", (int)(validMoves[myMove] / 8), validMoves[myMove] % 8);
            
            sendMessage(validMoves[myMove]);
        }
        else {
            printf("their move");
        }
    }

}