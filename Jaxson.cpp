// the portion of the code that manages connections between the server and the client was provided by Ammon Shurtz and Lawry Sorenson

#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <cstring>
#include <cmath>
#include <map>

using namespace std;


// globals
int turn = -1;
int round_to_play;
int player;
int opponent;
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
int bufend, eval = 0;
int minEval, maxEval, childMove;
int CORNER_BONUS = 50;
int PENALTY_FOR_BEING_AT_RISK_OF_CONCEDING_CORNER = 50;
int NORMAL_EDGE_BONUS = 20;
int EDGE_NEXT_TO_CORNER_BONUS = 35;

// Node bestMoveSoFar;
// map<int, int> scoreThatWillMostLikelyBeChosenForAGivenDepth;

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

int checkDirection(int state[8][8], int row, int col, int incx, int incy, int playerNum) {
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
        if (playerNum == 1) {
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

int couldBe(int state[8][8], int row, int col, int playerNum) {
    int incx, incy;
    int capturedPieces = 0;
    for (incx = -1; incx < 2; incx++) {
        for (incy = -1; incy < 2; incy++) {
            if ((incx == 0) && (incy == 0))
                continue;
            capturedPieces += checkDirection(state, row, col, incx, incy, playerNum);
        }
    }
    
    return capturedPieces;
}

// generates the set of valid moves for the player; returns a list of valid moves (validMoves)
void getValidMoves(int round_to_play, int state[8][8], int (&myValidMoves)[64], int &myNumValidMoves, int playerNum) {
    int i, j;
    memset(piecesCaptured, 0, sizeof(piecesCaptured));
    memset(myValidMoves, 0, sizeof(myValidMoves));
    myNumValidMoves = 0;
    if (round_to_play < 4) {
        if (state[3][3] == 0) {
            myValidMoves[myNumValidMoves] = 3*8 + 3;
            myNumValidMoves ++;
        }
        if (state[3][4] == 0) {
            myValidMoves[myNumValidMoves] = 3*8 + 4;
            myNumValidMoves ++;
        }
        if (state[4][3] == 0) {
            myValidMoves[myNumValidMoves] = 4*8 + 3;
            myNumValidMoves ++;
        }
        if (state[4][4] == 0) {
            myValidMoves[myNumValidMoves] = 4*8 + 4;
            myNumValidMoves ++;
        }
        // printf("Valid Moves:\n");
        // for (i = 0; i < myNumValidMoves; i++) {
        //     printf("%i, %i\n", (int)(myValidMoves[i] / 8), myValidMoves[i] % 8);
        // }
        // printf("\n");
    }
    else {
        // printf("Valid Moves:\n");
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                if (state[i][j] == 0) {
                    int numOfPiecesCaptured = couldBe(state, i, j, playerNum);
                    // cout << numOfPiecesCaptured << endl;
                    if (numOfPiecesCaptured > 0) {
                        myValidMoves[myNumValidMoves] = i*8 + j;
                        piecesCaptured[myNumValidMoves] = numOfPiecesCaptured;
                        // cout << "FOUND A VALID MOVE, " << i*8 + j << ", which captures " << numOfPiecesCaptured << " pieces!!!\n";
                        myNumValidMoves ++;
                        // printf("%i, %i\n", i, j);
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


int calculateCoinParity(int myState[8][8]){
    int myCoins = 0;
    int oppCoins = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (myState[i][j] == player) {
                myCoins += 1;
            }
            else if (myState[i][j] == opponent) {
                oppCoins += 1;
            }
        }
    }
    return 100 * (myCoins - oppCoins) / (myCoins + oppCoins);
}

int calculateMobility(int myState[8][8], int depth = 1) {
    int myValidMoves[64];
    int myNumValidMoves;
    getValidMoves(round_to_play + depth, myState, myValidMoves, myNumValidMoves, player);
    int oppValidMoves[64];
    int oppNumValidMoves;
    getValidMoves(round_to_play + depth, myState, oppValidMoves, oppNumValidMoves, opponent);

    if ( myNumValidMoves + oppNumValidMoves != 0) {
      return 100 * (myNumValidMoves - oppNumValidMoves) / (myNumValidMoves + oppNumValidMoves);
    } else {
      return 0;
    }
}

int calculateCornerAdvantage(int myState[8][8]) {
  int myCorners, oppCorners = 0;
	if(myState[0][0] == player) myCorners++;
	else if(myState[0][0] == opponent) oppCorners++;
	if(myState[0][7] == player) myCorners++;
	else if(myState[0][7] == opponent) oppCorners++;
	if(myState[7][0] == player) myCorners++;
	else if(myState[7][0] == opponent) oppCorners++;
	if(myState[7][7] == player) myCorners++;
	else if(myState[7][7] == opponent) oppCorners++;

    if ( myCorners + oppCorners != 0) {
      return 100 * (myCorners - oppCorners) / (myCorners + oppCorners);
    } else {
      return 0;
    }
}

int calculateStability(int myState[8][8]) {
    int stabilityScore = 0;
    
    // Check corners
    if (myState[0][0] == player)
        stabilityScore += CORNER_BONUS;
    if (myState[0][7] == player)
        stabilityScore += CORNER_BONUS;
    if (myState[7][0] == player)
        stabilityScore += CORNER_BONUS;
    if (myState[7][7] == player)
        stabilityScore += CORNER_BONUS;
    
    // Check edges
    for (int i = 1; i < 7; i++) {
        if (myState[0][i] == player) { // check bottom row
            if (i == 1) { // next to bottom left corner
              if (myState[0][0] == player) {
                stabilityScore += EDGE_NEXT_TO_CORNER_BONUS;
              } else {
                stabilityScore -= PENALTY_FOR_BEING_AT_RISK_OF_CONCEDING_CORNER;
              }
            } else if (i == 6) { // next to bottom right corner
              if (myState[0][7] == player) {
                stabilityScore += EDGE_NEXT_TO_CORNER_BONUS;
              } else {
                stabilityScore -= PENALTY_FOR_BEING_AT_RISK_OF_CONCEDING_CORNER;
              }
            } else {
              stabilityScore += NORMAL_EDGE_BONUS;
            }
        }
        if (myState[7][i] == player) { // check top row
            if (i == 1) { // next to top left corner
              if (myState[7][0] == player) {
                stabilityScore += EDGE_NEXT_TO_CORNER_BONUS;
              } else {
                stabilityScore -= PENALTY_FOR_BEING_AT_RISK_OF_CONCEDING_CORNER;
              }
            } else if (i == 6) { // next to top right corner
              if (myState[7][7] == player) {
                stabilityScore += EDGE_NEXT_TO_CORNER_BONUS;
              } else {
                stabilityScore -= PENALTY_FOR_BEING_AT_RISK_OF_CONCEDING_CORNER;
              }
            } else {
              stabilityScore += NORMAL_EDGE_BONUS;
            }
        }
        if (myState[i][0] == player) { // check left column
            if (i == 1) { // on top of bottom left corner
              if (myState[0][0] == player) {
                stabilityScore += EDGE_NEXT_TO_CORNER_BONUS;
              } else {
                stabilityScore -= PENALTY_FOR_BEING_AT_RISK_OF_CONCEDING_CORNER;
              }
            } else if (i == 6) { // next to top left corner
              if (myState[7][0] == player) {
                stabilityScore += EDGE_NEXT_TO_CORNER_BONUS;
              } else {
                stabilityScore -= PENALTY_FOR_BEING_AT_RISK_OF_CONCEDING_CORNER;
              }
            } else {
              stabilityScore += NORMAL_EDGE_BONUS;
            }
        }
        if (myState[i][7] == player) { // check right column
            if (i == 1) { // on top of bottom right corner
              if (myState[0][7] == player) {
                stabilityScore += EDGE_NEXT_TO_CORNER_BONUS;
              } else {
                stabilityScore -= PENALTY_FOR_BEING_AT_RISK_OF_CONCEDING_CORNER;
              }
            } else if (i == 6) { // under top right corner
              if (myState[7][7] == player) {
                stabilityScore += EDGE_NEXT_TO_CORNER_BONUS;
              } else {
                stabilityScore -= PENALTY_FOR_BEING_AT_RISK_OF_CONCEDING_CORNER;
              }
            } else {
              stabilityScore += NORMAL_EDGE_BONUS;
            }
        }
        
    }
    
    return stabilityScore;
}


int heurEval(int myState[8][8], int depth) {
    int coinParity = calculateCoinParity(myState);
    int mobilityScore = calculateMobility(myState, depth);
    int cornerScore = calculateCornerAdvantage(myState);
    int stabilityScore = calculateStability(myState);

    int score = coinParity + mobilityScore + cornerScore + stabilityScore;
    cout << "Current score for the following state is: " << score << endl;
    printState(myState);
    cout << endl << endl;
    // sleep(10);

    return score;
}

int validCornerMove(int myValidMoves[64], int myNumValidMoves) {
    int validCornerMove = -1;
    for (int i = 0; i < myNumValidMoves; i++) {
        if (myValidMoves[i] == 0 || 
            myValidMoves[i] == 7 || 
            myValidMoves[i] == 56 || 
            myValidMoves[i] == 63) {
                validCornerMove = myValidMoves[i];
                return validCornerMove;
            }
    }
    return validCornerMove;
}

int minimax(int currentState[8][8], int depth, int alpha, int beta, int maximizingPlayer, int (&scoresByIndex)[15]) {
    if (depth >= 5) { // Hit end of depth. Returning
        return heurEval(currentState, depth);
    }
    int myValidMoves[64];
    int myNumValidMoves = 0;
    getValidMoves(round_to_play + depth, currentState, myValidMoves, myNumValidMoves, maximizingPlayer ? player : opponent);
    if (myNumValidMoves == 0) { // No moves available. Returning
        return heurEval(currentState, depth);
    }

    if (maximizingPlayer) {
        maxEval = (int)-INFINITY;
        for (int i = 0; i < myNumValidMoves; i++) {
            childMove = myValidMoves[i];
            int newState[8][8];


            copyState(currentState, newState);
            newState[(int)(childMove / 8)][childMove % 8] = player;
            changeColorsAllDirections((int)(childMove / 8), childMove % 8, newState);
            eval = minimax(newState, depth + 1, alpha, beta, false, scoresByIndex);
            maxEval = max(maxEval, eval);
            if (depth == 0) { // save final eval for comparison and selection
                scoresByIndex[i] = eval;
            }
            alpha = max(alpha, eval);
            if (beta <= alpha) { // prune
                break;
            }
        }
        if (depth == 0) {
            // cout << "FINDING OPTIMAL MOVES from the lot:" << endl;
            // for (int i = 0; i < myNumValidMoves; i++) {
            //     cout << myValidMoves[i] << endl;
            // }
            // cout << endl;
            // for (int i = 0; i < myNumValidMoves; i++) {
            //     cout <<  i << " --> " << scoresByIndex[i] << endl;
            // }
            int maxKey = 0;    // Initialize the variable to store the key of the maximum value
            int maxValue = -(int)INFINITY;  // Initialize the variable to store the maximum value
            for (int i = 0; i < myNumValidMoves; i++) {
                if (scoresByIndex[i] > maxValue) {
                    maxKey = i;
                    maxValue = scoresByIndex[i];
                }
            }
            
            return myValidMoves[maxKey];
        } else {
            return maxEval;
        }
    } else {
        minEval = (int)INFINITY;
        for (int i = 0; i < myNumValidMoves; i++) {
            childMove = myValidMoves[i];
            int newState[8][8];

            copyState(currentState, newState);
            newState[(int)(childMove / 8)][childMove % 8] = opponent;
            changeColorsAllDirections((int)(childMove / 8), childMove % 8, newState);
            eval = minimax(newState, depth + 1, alpha, beta, true, scoresByIndex);
            minEval = min(minEval, eval);
            beta = min(beta, eval);
            if (beta <= alpha) { // prune
                break;
            }
        }
        return minEval;
    }
}


int move() {
    
    int scoresByIndex[15];
    return minimax(state, 0, (int)-INFINITY, (int)INFINITY, true, scoresByIndex);
}


// compile on your machine: g++ RandomGuy.cpp -o RandomGuy
// call: ./RandomGuy [ipaddress] [player_number]
//   ipaddress is the ipaddress on the computer the server was launched on.  Enter "localhost" if it is on the same computer
//   player_number is 1 (for the black player) and 2 (for the white player)
int main(int argc, char *argv[]) {
    // argc = 3;
    // argv[0] = "./RandomGuy";
    // argv[1] = "localhost";
    // argv[2] = "2";
    if (argc < 3) {
        printf("Not enough parameters\n");
    }

    srand(time(NULL));

    player = atoi(argv[2]);
    opponent = 3 - player;

    initconn(argv[1]);
    
    while (true) {
        printf("Read\n");
        readMessage();
        
        if (turn == player) {
            int myMove = move();
            sendMessage(myMove);
        }
        else {
            printf("their move");
        }
    }

}