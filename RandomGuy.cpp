// the portion of the code that manages connections between the server and the client was provided by Ammon Shurtz and Lawry Sorenson

#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <cstring>
#include <cmath>

using namespace std;


// globals
int turn = -1;
int round_to_play;
int player;
double time_limit = -1;
double t1, t2; // time remaining
int state[8][8];

int validMoves[64];
int piecesCaptured[64];
int numValidMoves;

// variables for server buffer
int sfd;
const int BLOCK_SIZE = 1024;
char buf[2 * BLOCK_SIZE];
int bufstart = 0;
int bufend = 0;

class Node {
    public:
        int state[8][8];
        int depth;
        int heurScore;
        Node * children[];
        Node(int myState[8][8], int depth, int heurScore) { // Constructor with parameters
            for (int i = 0; i < 8; i++) {
                copy(begin(this->state[i]), end(this->state[i]), begin(myState[i]));
            }
            this->depth = depth;
            this->heurScore = heurScore;
        }
};

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

int checkDirection(int state[8][8], int row, int col, int incx, int incy) {
    int sequence[7] = {-1, -1, -1, -1, -1, -1, -1};
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
                    return count;
                break;
            }
        }
        else {
            if (sequence[i] == 1)
                count ++;
            else {
                if ((sequence[i] == 2) && (count > 0))
                    return count;
                break;
            }
        }
    }
    
    return 0;
}

int couldBe(int state[8][8], int row, int col) {
    int incx, incy;
    int capturedPieces = 0;
    for (incx = -1; incx < 2; incx++) {
        for (incy = -1; incy < 2; incy++) {
            if ((incx == 0) && (incy == 0))
                continue;
            capturedPieces += checkDirection(state, row, col, incx, incy);
        }
    }
    
    return capturedPieces;
}

// generates the set of valid moves for the player; returns a list of valid moves (validMoves)
void getValidMoves(int round_to_play, int state[8][8]) {
    int i, j;
    memset(piecesCaptured, 0, sizeof(piecesCaptured));
    memset(validMoves, 0, sizeof(validMoves));
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
                    int numOfPiecesCaptured = couldBe(state, i, j);
                    // cout << numOfPiecesCaptured << endl;
                    if (numOfPiecesCaptured > 0) {
                        validMoves[numValidMoves] = i*8 + j;
                        piecesCaptured[numValidMoves] = numOfPiecesCaptured;
                        cout << "FOUND A VALID MOVE, " << i*8 + j << ", which captures " << numOfPiecesCaptured << " pieces!!!\n";
                        numValidMoves ++;
                        printf("%i, %i\n", i, j);
                    }
                }
            }
        }
    }
}
void copyState(int ogState[8][8], int (&newState)[8][8]) {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            newState[i][j] = ogState[i][j];
        }
    }
    return;
}

void printState(int myState[8][8]) {
    printf("state is:\n");
    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            printf("%i ", myState[i][j]);
        }
        printf("\n");
    }
}

void changeColorOneDirection(int row, int col, int incx, int incy, int (&myState)[8][8]) {
    int sequence[] = {-1, -1, -1, -1, -1, -1, -1};
    int seqLen;
    int i, r, c;
    
    seqLen = 0;
    for (i = 1; i < 8; i++) {
        r = row+incy*i;
        c = col+incx*i;
    
        if ((r < 0) || (r > 7) || (c < 0) || (c > 7))
            break;
    
        sequence[seqLen] = myState[r][c];
        seqLen++;
    }
    
    int count = 0;
    for (i = 0; i < seqLen; i++) {
        if (player == 1) {
            if (sequence[i] == 2)
                count ++;
            else {
                if ((sequence[i] == 1) && (count > 0))
                    count = 20;
                break;
            }
        }
        else {
            if (sequence[i] == 1)
                count ++;
            else {
                if ((sequence[i] == 2) && (count > 0))
                    count = 20;
                break;
            }
        }
    }
    
    if (count > 10) {
        if (player == 1) {
            i = 1;
            r = row+incy*i;
            c = col+incx*i;
            while (myState[r][c] == 2) {
                myState[r][c] = 1;
                i++;
                r = row+incy*i;
                c = col+incx*i;
            }
        }
        else {
            i = 1;
            r = row+incy*i;
            c = col+incx*i;
            while (myState[r][c] == 1) {
                myState[r][c] = 2;
                i++;
                r = row+incy*i;
                c = col+incx*i;
            }
        }
    }
}

void changeColorsAllDirections(int row, int col, int (&myState)[8][8]) {
    int incx, incy;

    for (incx = -1; incx < 2; incx++) {
        for (incy = -1; incy < 2; incy++) {
            if ((incx == 0) && (incy == 0))
                continue;
        
            changeColorOneDirection(row, col, incx, incy, myState);
        }
    }
}



int heurParity(){
    int opponent = 3 - player;
    int cpuCoins = 0;
    int oppCoins = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (state[i][j] == player) {
                cpuCoins += 1;
            }
            else if (state[i][j] == opponent) {
                oppCoins += 1;
            }
        }
    }
    int parity = cpuCoins - oppCoins;
    return parity;
}

int heurEval() {
    int evalScore = 0;
    //calc heuristic for coin parity
    int parityWeight = 1;
    int parityScore = parityWeight * heurParity();
    evalScore += parityScore;
    //calc heuristic for mobility
    //calc heuristic for corners
    //calc heuristic for stability
    return evalScore;
}


int pickAlphaBetaMove() {
    int alphaBetaMove = validMoves[0];
    if (sizeof(validMoves) > 1) {
        int heurScore = heurEval();
        Node root(state, 1, heurScore);

    }
    return alphaBetaMove;
}


// int getCoinParity()

// TODO: You should modify this function
// validMoves is a list of valid locations that you could place your "stone" on this turn
// Note that "state" is a global variable 2D list that shows the state of the game
int move() {
    // just move randomly for now
    //int myMove = rand() % numValidMoves;


    // for each valid move:
        // calculate coin parity (how many coins I have vs how many you have)
        // mobility
        // corners
        // stability
    int highestCaptured = -1;
    int highestCapturedMove = -1;
    for (int i = 0; i < numValidMoves; i++) {
        cout << "Valid move is: " << validMoves[i] << "\n";
        cout << "This move would capture: " << piecesCaptured[i] << " pieces\n";
        if (piecesCaptured[i] > highestCaptured) {
            highestCaptured = piecesCaptured[i];
            highestCapturedMove = validMoves[i];
        }
    }
    int heurScore = heurEval();
    cout << "heuristic score of board before moving: " << heurScore << "\n";
    cout << "The move to " << highestCapturedMove << " captures the most pieces at " << 
        highestCaptured << " pieces\n";
    int myMove = highestCapturedMove;
    int highestRow = floor(highestCapturedMove / 8);
    int highestColumn = highestCapturedMove % 8;
    cout << "move will be at row " << highestRow << " and column " << highestColumn << "\n";
    int tempState[8][8];
    copyState(state, tempState);
    tempState[highestRow][highestColumn] = player;
    changeColorsAllDirections(highestRow, highestColumn, tempState);
    cout << "Temp State\n";
    printState(tempState);
    return myMove;
}


// compile on your machine: g++ RandomGuy.cpp -o RandomGuy
// call: ./RandomGuy [ipaddress] [player_number]
//   ipaddress is the ipaddress on the computer the server was launched on.  Enter "localhost" if it is on the same computer
//   player_number is 1 (for the black player) and 2 (for the white player)
int main(int argc, char *argv[]) {
    argc = 3;
    argv[0] = "./RandomGuy";
    argv[1] = "localhost";
    argv[2] = "2";
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
            printState(state);
            // printf("state is:\n");
            // for (int i = 7; i >= 0; i--) {
            //     for (int j = 0; j < 8; j++) {
            //         printf("%i ", state[i][j]);
            //     }
            //     printf("\n");
            // }
            getValidMoves(round_to_play, state);
            
            int myMove = move();

            // printf("Selection: %i, %i\n", (int)(validMoves[myMove] / 8), validMoves[myMove] % 8);
            
            sendMessage(myMove);
        }
        else {
            printf("their move");
        }
    }

}
