#include <iostream>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>

#define MCW MPI_COMM_WORLD
#define DIM 10

using namespace std;

int landedAirplanes[DIM] {};

int generateAirports(int (&airports)[DIM][DIM], int size) {
	int y, x;
	
	for (int y = 0; y < DIM; y++) {
		for (int x = 0; x < DIM; x++) {
			airports[y][x] = 0;
		}
	}

	for (int i = 0; i < size * 3; i++) {

		y = rand() % DIM;
		x = rand() % DIM;
		airports[y][x] = 1;
	}
}

int generateNewCoordinate(int coordinate) {
	if (rand() % 2) {
		return coordinate + 1 == DIM ? 0 : coordinate + 1;
	} else {
		return coordinate - 1 < 0 ? DIM - 1 : coordinate - 1;
	}
}

void copy(int arrayToCopy[DIM][DIM], int (&destinationArray)[DIM][DIM]) {
	for (int i =0; i < DIM; ++i) {
		for (int j =0; j < DIM; ++j) {
			destinationArray[i][j] = arrayToCopy[i][j];
		}
	}
}

void receiveData(int(&airports)[DIM][DIM], int(&airplanes)[DIM][DIM]) {
	int flag, newY, response;
	MPI_Status mpiStatus;
	MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MCW, &flag, MPI_STATUS_IGNORE);
	while(flag) {
		MPI_Recv(&newY, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MCW, &mpiStatus);
		if (airplanes[newY][mpiStatus.MPI_TAG] == 1) {
			airports[newY][mpiStatus.MPI_TAG] = 3;
			airplanes[newY][mpiStatus.MPI_TAG] = 3;
			response = -1;
			landedAirplanes[mpiStatus.MPI_SOURCE] = 1;
		} else {
			airplanes[newY][mpiStatus.MPI_TAG] = 2;
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
			if (data[y][x] == 1) {
				cout << " #";
			} else if (data[y][x] == 2) {
				cout <<" x";
			} else if (data[y][x] == 3) {
				cout << " *";
			} else {
				cout << " .";
			}
		}
		cout << endl;
	}
}

int allAirplanesLanded(int size) {
	for (int i = 0; i < size; i++) {
		if (!landedAirplanes[i]) {
			return 0;
		}
	}
	return 1;
}

int main(int argc, char **argv) {
	int rank, size, killSwitch, completeMessageFlag;
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
		printData(airports);
	}

	while(true) {
		MPI_Iprobe(0, 5, MCW, &completeMessageFlag, MPI_STATUS_IGNORE);
		if (completeMessageFlag) {
			MPI_Recv(&completeMessageFlag, 1, MPI_INT, 0, 5, MCW, MPI_STATUS_IGNORE);
			if (completeMessageFlag) break;
		}

		if (!rank) {
			copy(airports, airplanes);
		}
		
		if (!complete) {
			MPI_Send(&currentY, 1, MPI_INT, 0, currentX, MCW);
		}

		MPI_Barrier(MCW);
		if (!rank) {
			if (airports[currentY][currentX] == 1) {
				complete = 1;
			}

			receiveData(airports, airplanes);
			printData(airplanes);
			
			if (complete) {
				killSwitch = allAirplanesLanded(size); 
				if (killSwitch) {
					for (int i = 0; i < size; i++) {
						MPI_Send(&killSwitch, 1, MPI_INT, i, 5, MCW);
					}
				}
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
		cout << "All airplanes landed" << endl;
	}

	MPI_Finalize();
	return 0;
}

