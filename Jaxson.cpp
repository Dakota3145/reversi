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
#include <chrono>
#include <tuple>
#include <queue>
#include <algorithm>

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

double MAX_LEGAL_MOVES_POSSIBLE = 28.0;
int adjustedDepthLimit = 5;
int movesLeft = 31; // just to be safe

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
    // printf("sending %i, %i from %i\n", i, j, m);
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

void setAdjustedDepthBasedOnTimeLeftAndAvailableAmountOfMoves(int numValidMoves) {
    if (t2 < 5.0) { // safeguard
        adjustedDepthLimit = 6;
    } else if (t2 < 50.0) {
        adjustedDepthLimit = 7;
    } else if (t2 > 120.0 && numValidMoves <= 12) {
        adjustedDepthLimit = 8;
    } else if (numValidMoves <= 10) {
        adjustedDepthLimit = 10;
    } else {
        adjustedDepthLimit = 7;
    }
    cout << "and a depth of " << adjustedDepthLimit << ", it took ";
}

double normalizeScore(int score, int minValue, int maxValue) {
    double normalizedScore;
    if (score < minValue) return -1.0;
    if (score > maxValue) return 1.0;

    // Check if the range crosses zero
    if (minValue < 0 && maxValue > 0) {
        normalizedScore = static_cast<double>(score) / max(abs(minValue), maxValue);
    } else {
        normalizedScore = (2.0 * (score - minValue) / (maxValue - minValue)) - 1.0;
    }

    return normalizedScore;
}


double calculateCoinParity(int myState[8][8]){
    double myCoins = 0.0;
    double oppCoins = 0.0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (myState[i][j] == player) {
                myCoins += 1.0;
            }
            else if (myState[i][j] == opponent) {
                oppCoins += 1.0;
            }
        }
    }
    double totalCoins = myCoins + oppCoins;
    double score = myCoins - oppCoins;

    // Normalize the score between -1 and 1
    return normalizeScore(score, -totalCoins, totalCoins);
}

double calculateMobility(int myState[8][8], int depth = 1) {
    int myValidMoves[64];
    int myNumValidMoves;
    getValidMoves(round_to_play + depth, myState, myValidMoves, myNumValidMoves, player);
    int oppValidMoves[64];
    int oppNumValidMoves;
    getValidMoves(round_to_play + depth, myState, oppValidMoves, oppNumValidMoves, opponent);

    int score = myNumValidMoves - oppNumValidMoves;
    // decent equation for estimating the max number of legal moves available for one player based on round_to_play
    double maxLegalMoves = ((-27.0 / 1135.0) * pow((round_to_play - 30.5), 2)) + 26.0;
    return normalizeScore(score, -maxLegalMoves, maxLegalMoves);
}

double calculateCornerAdvantage(int myState[8][8]) {
  double myCorners = 0;
  double oppCorners = 0;
	if(myState[0][0] == player) myCorners += 1.0;
	else if(myState[0][0] == opponent) oppCorners += 1.0;
	if(myState[0][7] == player) myCorners += 1.0;
	else if(myState[0][7] == opponent) oppCorners += 1.0;
	if(myState[7][0] == player) myCorners += 1.0;
	else if(myState[7][0] == opponent) oppCorners += 1.0;
	if(myState[7][7] == player) myCorners += 1.0;
	else if(myState[7][7] == opponent) oppCorners += 1.0;

    return normalizeScore(myCorners - oppCorners, -4.0, 4.0);
}

// queue<tuple<int, int>> createTupleQueue(int array[8][8]) {
//     vector<pair<int, pair<int, int>>> valueCoordinates;

//     // Store values and corresponding coordinates in the vector
//     for (int i = 0; i < 8; ++i) {
//         for (int j = 0; j < 8; ++j) {
//             valueCoordinates.push_back(make_pair(array[i][j], make_pair(i, j)));
//         }
//     }

//     // Sort the vector based on values in ascending order
//     sort(valueCoordinates.begin(), valueCoordinates.end(),
//               [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

//     // Create a queue of tuples from the sorted vector
//     queue<tuple<int, int>> tupleQueue;
//     for (const auto& valueCoordinate : valueCoordinates) {
//         tupleQueue.push(make_tuple(valueCoordinate.second.first, valueCoordinate.second.second));
//     }

//     return tupleQueue;
// }


queue<tuple<int, int>> createTraversalQueue(int myState[8][8]) {
    queue<tuple<int, int>> traversalQueue;

    for (int i = 0; i < 8; i++) {
        traversalQueue.push(make_tuple(i, i));
        for (int j = i + 1; j < 8; j++) {
            traversalQueue.push(make_tuple(i, j));
            traversalQueue.push(make_tuple(j, i));
        }
    }


    // int grid[8][8];
    // int counter = 0;
    // // Print the queue elements
    // while (!traversalQueue.empty()) {
    //     tuple<int, int> position = traversalQueue.front();
    //     traversalQueue.pop();
    //     int x = get<0>(position);
    //     int y = get<1>(position);
    //     grid[x][y] = counter;
    //     counter++;
    // }
    // cout << endl << "Traversal order is: " << endl;
    // printState(grid);
    return traversalQueue;
}

int STABLE = 1;
int ROCK_SOLID = 2;
// int PENALTY_FOR_BEING_AT_RISK_OF_CONCEDING_CORNER = 10;
// int EDGE_NEXT_TO_CORNER_BONUS = 3;

bool isCorner(int x, int y) {
    return (x == 0 && y == 0)
    || (x == 0 && y == 7)
    || (x == 7 && y == 0)
    || (x == 7 && y == 7);
}

bool isEdgePiece(int x, int y) {
    return x == 0 || x == 7 || y == 0 || y == 7;
}

bool isOutOfBoundsOrEqualToSafeNumber(int x, int y, int safeNumber, int stabilityGrid[8][8]) {
    return x < 0 || y < 0 || x > 7 || y > 7 || stabilityGrid[x][y] == safeNumber;
}

bool isThisDirectionSafe(int x, int y, int direction, int stabilityGrid[8][8], int safeNumber) {
    switch (direction) {
        case 0: // LEFT
            return isOutOfBoundsOrEqualToSafeNumber(x - 1, y, safeNumber, stabilityGrid);
            break;
        case 1: // LEFT-UP
            return isOutOfBoundsOrEqualToSafeNumber(x - 1, y + 1, safeNumber, stabilityGrid);
            break;
        case 2: // UP
            return isOutOfBoundsOrEqualToSafeNumber(x , y + 1, safeNumber, stabilityGrid);
            break;
        case 3: // UP-RIGHT
            return isOutOfBoundsOrEqualToSafeNumber(x + 1, y + 1, safeNumber, stabilityGrid);
            break;
        case 4: // RIGHT
            return isOutOfBoundsOrEqualToSafeNumber(x + 1, y, safeNumber, stabilityGrid);
            break;
        case 5: // DOWN-RIGHT
            return isOutOfBoundsOrEqualToSafeNumber(x + 1, y - 1, safeNumber, stabilityGrid);
            break;
        case 6: // DOWN
            return isOutOfBoundsOrEqualToSafeNumber(x, y - 1, safeNumber, stabilityGrid);
            break;
        case 7: // DOWN-LEFT
            return isOutOfBoundsOrEqualToSafeNumber(x - 1, y - 1, safeNumber, stabilityGrid);
            break;
    }
    return false;

}

int numberOfConsecutiveSafeDirectionsThatConstitutesSafe = 4;

void rotateArray(int original[8][8], int rotated[8][8]) {
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            rotated[j][7 - i] = original[i][j];
        }
    }
}

int getStabilityScoreForThisCell(int x, int y, int stabilityGrid[8][8], int playerMultiplier) {
    if (isCorner(x, y)) {
        return playerMultiplier * ROCK_SOLID;
    }

    int numberOfConsecutiveSafeDirections = 0;
    for (int i = 0; i < 8; i++) {
        if (isThisDirectionSafe(x, y, i, stabilityGrid, playerMultiplier * ROCK_SOLID)) {
            numberOfConsecutiveSafeDirections++;
        } else {
            numberOfConsecutiveSafeDirections = 0;
        }

        if (numberOfConsecutiveSafeDirections >= numberOfConsecutiveSafeDirectionsThatConstitutesSafe || (i > 5 && numberOfConsecutiveSafeDirections == 0)) {
            break;
        }
    }

    if (numberOfConsecutiveSafeDirections >= numberOfConsecutiveSafeDirectionsThatConstitutesSafe) {
        return playerMultiplier * ROCK_SOLID;
    } else if (isEdgePiece(x, y)) {
        return playerMultiplier * STABLE;
    }

    return 0;
}

void traverseQueueUntilWeStopSeeingStableCoins (queue<tuple<int, int>> queue, int stabilityGrid[8][8], int state[8][8], int& numPieces, int& totalScore) {
    int x, y;
    tuple<int, int> position;

    int tempScore = 0;
    int cellsWeHaventUpdated = 0;
    while (!queue.empty() && cellsWeHaventUpdated < 15) {
        position = queue.front();
        queue.pop();
        x = get<0>(position);
        y = get<1>(position);

        if (state[x][y] == 0) {
            stabilityGrid[x][y] = 0;
            cellsWeHaventUpdated += 1;
            continue;
        } else if (stabilityGrid[x][y] == ROCK_SOLID) { 
            numPieces += 1;
            cellsWeHaventUpdated += 1;
            continue;
        }
        else {
            numPieces += 1;
            tempScore = getStabilityScoreForThisCell(x, y, stabilityGrid, state[x][y] == player ? 1 : -1);
            stabilityGrid[x][y] = tempScore;
            totalScore += tempScore;
            cellsWeHaventUpdated = 0;
        }
    }
}

double calculateStability(int myState[8][8]) {
    if (round_to_play < 6) return 0.0;
    int stabilityGrid[8][8]; 

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            stabilityGrid[i][j] = 0;
        }
    }

    int traversalGrid[8][8];
    int counter = 0;
    for (int i = 0; i < 8; i++) {
        traversalGrid[i][i] = counter++;
        for (int j = i + 1; j < 8; j++) {
            traversalGrid[i][j] = counter++;
            traversalGrid[j][i] = counter++;
        }
    }

    queue<tuple<int, int>> queueOne = createTraversalQueue(traversalGrid);
    int rotated[8][8];
    rotateArray(traversalGrid, rotated);
    queue<tuple<int, int>> queueTwo = createTraversalQueue(rotated);
    int rotatedAgain[8][8];
    rotateArray(rotated, rotatedAgain);
    queue<tuple<int, int>> queueThree = createTraversalQueue(rotatedAgain);
    int rotatedAgainAgain[8][8];
    rotateArray(rotatedAgain, rotatedAgainAgain);
    queue<tuple<int, int>> queueFour = createTraversalQueue(rotatedAgainAgain);

    int numPieces = 0;
    int totalScore = 0;

    traverseQueueUntilWeStopSeeingStableCoins(queueOne, stabilityGrid, myState, numPieces, totalScore);
    traverseQueueUntilWeStopSeeingStableCoins(queueTwo, stabilityGrid, myState, numPieces, totalScore);
    traverseQueueUntilWeStopSeeingStableCoins(queueThree, stabilityGrid, myState, numPieces, totalScore);
    traverseQueueUntilWeStopSeeingStableCoins(queueFour, stabilityGrid, myState, numPieces, totalScore);

    // cout << endl << "Total Score is: " << totalScore << ", Stability Grid: " << endl;
    // printState(stabilityGrid);

    return normalizeScore(totalScore, -(numPieces * ROCK_SOLID), numPieces * ROCK_SOLID);
}


double evaluationForThisState(int myState[8][8], int depth) {
    double coinParity = calculateCoinParity(myState);
    double mobilityScore = calculateMobility(myState, depth);
    double cornerScore = calculateCornerAdvantage(myState);
    // cout << endl << "Getting stability score for the following state: " << endl;
    // printState(myState);
    double stabilityScore = calculateStability(myState);
    // cout << "Stability score is: " << stabilityScore << endl;

    double score;
    if (round_to_play > 54) {
        score = 10.0 * coinParity + 1.0 * mobilityScore + 1.0 * stabilityScore + 1.0 * cornerScore;
    } else {
        score = 1.0 * coinParity + 1.5 * mobilityScore + 5 * stabilityScore + 10 * cornerScore;
    }
    // cout << "Current score for the following state is: " << score << endl;
    // printState(myState);
    // cout << endl << endl;
    // sleep(10);
    return score;
}

// /Users/jaxsonruff/reversi-470/reversi/Jaxson.cpp

int minimax(int currentState[8][8], int depth, int alpha, int beta, int maximizingPlayer, int (&scoresByIndex)[15]) {
    if (depth >= adjustedDepthLimit) { // Hit end of depth. Returning
        return evaluationForThisState(currentState, depth);
    }
    int myValidMoves[64];
    int myNumValidMoves = 0;
    getValidMoves(round_to_play + depth, currentState, myValidMoves, myNumValidMoves, maximizingPlayer ? player : opponent);
    if (myNumValidMoves == 0) { // No moves available. Returning
        return evaluationForThisState(currentState, depth);
    }
    if (depth == 0) {
        cout << "For " << myNumValidMoves << " moves, ";
      setAdjustedDepthBasedOnTimeLeftAndAvailableAmountOfMoves(myNumValidMoves);
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
    movesLeft -= 1;
    int scoresByIndex[15];
    adjustedDepthLimit = 1;
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
            auto start = chrono::high_resolution_clock::now();
            int myMove = move();
            auto end = chrono::high_resolution_clock::now();
            chrono::duration<double> duration = end - start;
            cout << duration.count() << " seconds." << endl;
            sendMessage(myMove);
        }
        else {
            // printf("their move");
            cout << endl;
        }
    }

}
