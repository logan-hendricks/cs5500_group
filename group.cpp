#include <iostream>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>

#define MCW MPI_COMM_WORLD
#define DIM 10

using namespace std;

// Global variables for convenience between helper methods
int landedAirplanes[DIM] {};
int currentLoser;
bool foundLoser;

// Helper Method to generate the airports for each round
int generateAirports(int (&airports)[DIM][DIM], int size) {
	int y, x;
	
	// Blank Space is designated as a int 99, populate the entire 2-D array with blank space
	for (int y = 0; y < DIM; y++) {
		for (int x = 0; x < DIM; x++) {
			airports[y][x] = 99;
		}
	}

	// Generate 3 times as many airports as there are nodes, mark them with an 100
	for (int i = 0; i < size * 3; i++) {
		y = rand() % DIM;
		x = rand() % DIM;
		airports[y][x] = 100;
	}
}

// Helper method to generate an x/y for an individual node
int generateNewCoordinate(int coordinate) {
	int direction = rand() % 3;

	switch(direction) {
		case 0: return coordinate + 1 == DIM ? 0 : coordinate + 1;
		        break;	
		case 1: return coordinate - 1 < 0 ? DIM - 1 : coordinate - 1;
			break;
		case 2: return coordinate;
			break;
	}
}

// Copy a 2-D array into an another 2-D array
void copy(int arrayToCopy[DIM][DIM], int (&destinationArray)[DIM][DIM]) {
	for (int i =0; i < DIM; ++i) {
		for (int j =0; j < DIM; ++j) {
			destinationArray[i][j] = arrayToCopy[i][j];
		}
	}
}

// Helper method to check if there is a loser yet in the round, and set the loser flag and rank
void setLoserIfExists(int size) {
	bool loserFound = false;
	int loser;
	for(int i = 0; i < size; i++) {
		if (!landedAirplanes[i]) {
			if (loserFound) {
				return;
			}
			loser = i;
			loserFound = true;
		}
	}
	
	currentLoser = loser;
	foundLoser = true;
}

// Helper method for the master node to receive data from the other nodes
void receiveData(int(&airports)[DIM][DIM], int(&airplanes)[DIM][DIM], int size) {
	int flag, newY, response;
	MPI_Status mpiStatus;

	// Probe for new data from the ranks
	MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MCW, &flag, MPI_STATUS_IGNORE);
	while(flag) {
		MPI_Recv(&newY, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MCW, &mpiStatus);
		if (airplanes[newY][mpiStatus.MPI_TAG] == 100) {
			// Check if the x,y for a node matches up with an airport, if so notify the rank that it has landed and cache it as such
			response = -1;
			landedAirplanes[mpiStatus.MPI_SOURCE] = 1;
			setLoserIfExists(size);
		} else {
			// if the airplane has not landed, output its rank to the display
			airplanes[newY][mpiStatus.MPI_TAG] = mpiStatus.MPI_SOURCE;
			response = 0;
		}
		
		if (mpiStatus.MPI_SOURCE != 0) {
			// Send the response to each of the ranks as they come in as to whether the airplane has landed
			MPI_Send(&response, 1, MPI_INT, mpiStatus.MPI_SOURCE, 1, MCW);
		}

		// Probe for new data from the ranks
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MCW, &flag, MPI_STATUS_IGNORE);
	}
}

// Print out the airport/airplane data in the 2-D array
void printData(int data[DIM][DIM]) {
	cout << endl << endl << endl << endl;
	for (int y = 0; y < DIM; y++) {
		for (int x = 0; x < DIM; x++) {
			if (data[y][x] == 100) {
				// Output airports (100) as a # symbol
				cout << " #";
			} else if (data[y][x] == 99) {
				// Output Empty Space (99) as a space
				cout <<"  ";
			} else {
				// Output the rank data
				cout << data[y][x];
			}
		}
		cout << endl;
	}
}

// Helper method for master node to end a round
void endRound() {
	int losingFlag = -1;
	MPI_Send(&losingFlag, 1, MPI_INT, currentLoser, 1, MCW);
	cout << "Round Over, Loser of this round: " << currentLoser << endl;
	foundLoser = false;
	currentLoser = 0;
}

// Helper method for the master node to start a new round
void newRound(int size) {
	// Tag 10 is to restart the gamea
	int tag = 10;
	int message = 1;
	char input;
	
	// Receive input from the user
	while(input != 'y' && input != 'n') {
		cout << "Play another round? y/n" << endl;
		cin.clear();
		cin >> input;
		if (input == 'n') {
			// Tag 5 is to end the game
			tag = 5;
		}
	}

	// Send a start/stop command to all of the nodes based on the tag set
	for (int i = 0; i < size; i++) {
		MPI_Send(&message, 1, MPI_INT, i, tag, MCW);
		landedAirplanes[i] = 0;
	}
}


int main(int argc, char **argv) {
	// Node Flags and round data for each round
	int rank, size, killSwitch, completeMessageFlag, restartMessageFlag;
	int airports[DIM][DIM];
	int airplanes[DIM][DIM];
	int complete = 0;
	
	// Initialize MPI data
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MCW, &rank);
	MPI_Comm_size(MCW, &size);
	
	// Current location of each node for each round of the game
	srand(time(NULL) / (rank + 1));
	int currentY = rand() % DIM;
	int currentX = rand() % DIM;
	
	if (!rank) {
		// Generate the initial data for the entire application
		generateAirports(airports, size);
		foundLoser = false;
	}

	while(true) {
		// Tag and probe to finish the game entirely
		MPI_Iprobe(0, 5, MCW, &completeMessageFlag, MPI_STATUS_IGNORE);
		if (completeMessageFlag) break;

		// Tag and probe to restart a round of the game
		MPI_Iprobe(0, 10, MCW, &restartMessageFlag, MPI_STATUS_IGNORE);
		if (restartMessageFlag) {
			complete = 0;
			if (!rank) {
				generateAirports(airports, size);
				cout << "New Round!" << endl;
			}
		}

		if (!rank) {
			// Copy the airport data to airplanes in for the newest round
			copy(airports, airplanes);
		}
		
		if (!complete) {
			// Only send the nodes x,y if the node has not been marked as complete
			MPI_Send(&currentY, 1, MPI_INT, 0, currentX, MCW);
		}

		MPI_Barrier(MCW);
		if (!rank) {
			// For master process, receive the data from the other nodes, and print it out
			receiveData(airports, airplanes, size);
			printData(airplanes);

			if (airports[currentY][currentX] == 100) {
				// In order to prevent a cyclical loop, the master node calculates whether or not it is complet
				complete = 1;
			}

			if (foundLoser) {
				// Once a loser has been found, end the round and prompt the user to see if a new round should be started
				endRound();
				newRound(size);
			}
		} else {
			if (!complete) {
				// Await answer from the master node as to whether this node is complete
				MPI_Recv(&killSwitch, 1, MPI_INT, 0, 1, MCW, MPI_STATUS_IGNORE);
				
				if (killSwitch < 0) {
					complete = 1;
				} 
			}
		}
		
		// Calculate a new x,y for this round
		if (!complete) {
			currentY = generateNewCoordinate(currentY);
			currentX = generateNewCoordinate(currentX);
		}

		sleep(1);
		MPI_Barrier(MCW);
	}

	// Output to the user the game is complete
	if (!rank) {
		cout << "Thanks for playing!" << endl;
	}

	MPI_Finalize();
	return 0;
}

