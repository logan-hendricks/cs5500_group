#include <iostream>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>

#define MCW MPI_COMM_WORLD
#define DIM 10

using namespace std;

int landedAirplanes[DIM] {};
int currentLoser;
bool foundLoser;

int generateAirports(int (&airports)[DIM][DIM], int size) {
	int y, x;
	
	for (int y = 0; y < DIM; y++) {
		for (int x = 0; x < DIM; x++) {
			airports[y][x] = 99;
		}
	}

	for (int i = 0; i < size * 3; i++) {
		y = rand() % DIM;
		x = rand() % DIM;
		airports[y][x] = 100;
	}
}

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

void copy(int arrayToCopy[DIM][DIM], int (&destinationArray)[DIM][DIM]) {
	for (int i =0; i < DIM; ++i) {
		for (int j =0; j < DIM; ++j) {
			destinationArray[i][j] = arrayToCopy[i][j];
		}
	}
}

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

void receiveData(int(&airports)[DIM][DIM], int(&airplanes)[DIM][DIM], int size) {
	int flag, newY, response;
	MPI_Status mpiStatus;
	MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MCW, &flag, MPI_STATUS_IGNORE);
	while(flag) {
		MPI_Recv(&newY, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MCW, &mpiStatus);
		if (airplanes[newY][mpiStatus.MPI_TAG] == 100) {
			response = -1;
			landedAirplanes[mpiStatus.MPI_SOURCE] = 1;
			setLoserIfExists(size);
		} else {
			airplanes[newY][mpiStatus.MPI_TAG] = mpiStatus.MPI_SOURCE;
			response = 0;
		}
		
		if (mpiStatus.MPI_SOURCE != 0) {
			MPI_Send(&response, 1, MPI_INT, mpiStatus.MPI_SOURCE, 1, MCW);
		}
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MCW, &flag, MPI_STATUS_IGNORE);
	}
}

void printData(int data[DIM][DIM]) {
	cout << endl << endl << endl << endl;
	for (int y = 0; y < DIM; y++) {
		for (int x = 0; x < DIM; x++) {
			if (data[y][x] == 100) {
				cout << " #";
			} else if (data[y][x] == 99) {
				cout <<"  ";
			} else {
				cout << data[y][x];
			}
		}
		cout << endl;
	}
}

void endRound() {
	int losingFlag = -1;
	MPI_Send(&losingFlag, 1, MPI_INT, currentLoser, 1, MCW);
	cout << "Round Over, Loser of this round: " << currentLoser << endl;
	foundLoser = false;
	currentLoser = 0;
}

void newRound(int size) {
	int message = 1;
	int tag = 10;
	char input;
	
	while(input != 'y' && input != 'n') {
		cout << "Play another round? y/n" << endl;
		cin.clear();
		cin >> input;
		if (input == 'n') {
			tag = 5;
		}
	}

	for (int i = 0; i < size; i++) {
		MPI_Send(&message, 1, MPI_INT, i, tag, MCW);
		landedAirplanes[i] = 0;
	}
}


int main(int argc, char **argv) {
	int rank, size, killSwitch, completeMessageFlag, restartMessageFlag;
	int airports[DIM][DIM];
	int airplanes[DIM][DIM];
	int complete = 0;
	
	// Initialize MPI data
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MCW, &rank);
	MPI_Comm_size(MCW, &size);
	
	srand(time(NULL) / (rank + 1));
	int currentY = rand() % DIM;
	int currentX = rand() % DIM;
	
	if (!rank) {
		generateAirports(airports, size);
		foundLoser = false;
	}

	while(true) {
		MPI_Iprobe(0, 5, MCW, &completeMessageFlag, MPI_STATUS_IGNORE);
		if (completeMessageFlag) break;

		MPI_Iprobe(0, 10, MCW, &restartMessageFlag, MPI_STATUS_IGNORE);
		if (restartMessageFlag) {
			complete = 0;
			if (!rank) {
				generateAirports(airports, size);
				cout << "New Round!" << endl;
			}
		}

		if (!rank) {
			copy(airports, airplanes);
		}
		
		if (!complete) {
			MPI_Send(&currentY, 1, MPI_INT, 0, currentX, MCW);
		}

		MPI_Barrier(MCW);
		if (!rank) {
			receiveData(airports, airplanes, size);
			printData(airplanes);

			if (airports[currentY][currentX] == 100) {
				complete = 1;
			}

			if (foundLoser) {
				endRound();
				newRound(size);
			}
		} else {
			if (!complete) {
				MPI_Recv(&killSwitch, 1, MPI_INT, 0, 1, MCW, MPI_STATUS_IGNORE);
				
				if (killSwitch < 0) {
					complete = 1;
				} 
			}
		}
		
		if (!complete) {
			currentY = generateNewCoordinate(currentY);
			currentX = generateNewCoordinate(currentX);
		}

		sleep(1);
		MPI_Barrier(MCW);
	}

	if (!rank) {
		cout << "Thanks for playing!" << endl;
	}

	MPI_Finalize();
	return 0;
}

