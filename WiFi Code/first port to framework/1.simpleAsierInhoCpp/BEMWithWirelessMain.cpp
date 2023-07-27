//#include <AsierInho.h>
//#include <AsierInho_V2.h>
#include <AsierInhoWireless.h>
#include <GSPAT_SolverBEM.h>
#include <Helper\HelperMethods.h>
#include <Helper\microTimer.h>
#include <string.h>
#include <conio.h>
#include <windows.h>
void print(const char* str) {
	printf("%s\n", str);
}

void createTrap(int numTransducers, float position[3], float *t_positions, float* amplitudes, float* phases, float p_amplitude = 1.f, bool twinTrap = false) {
	//1. Traverse each transducer:
	for (int buffer_index=0; buffer_index< numTransducers; buffer_index++) {
		//a. Get its position:
		float* t_pos=&(t_positions[3*buffer_index]);
		//b. Compute phase delay and amplitudes from transducer to target point: 
		float distance, amplitude;
		computeAmplitudeAndDistance(t_pos, position, &amplitude, &distance);
		float signature = (float)(twinTrap) * (buffer_index < numTransducers / 2 ? 0 : M_PI);
		float phase = fmodf(-K()*distance + signature, (float)(2 * M_PI));
		//printf("%d: %f -> %f\n", buffer_index, distance, phase);

		//c. Store it in the output buffers.
		phases[buffer_index] = phase;
		amplitudes[buffer_index] = p_amplitude;// amplitude;
	}
}

void updatePhasesAndAmplitudes(AsierInhoWireless::AsierInhoWirelessBoard* driver, float* phases, float* amplitudes) {
	unsigned char *message = driver->discretize(phases, amplitudes);
	driver->updateMessage(message);
}

void amplitudeModulation(AsierInhoWireless::AsierInhoWirelessBoard* driver, float *phases, float *amplitudes, float *amplitudesOff, int initialFrequency = 50) {
	int targetFrequency = initialFrequency;
	bool finished = false;
	bool amplitudeOn = true;
	DWORD lastUpdate = microTimer::uGetTime();
	DWORD updateInterval = 1000000 / targetFrequency / 2;
	int updateCount = 0;
	int numUpdates = targetFrequency * 2;
	DWORD lastReport = microTimer::uGetTime();
	while (!finished) {
		if (_kbhit()) {
			//Update 3D position from keyboard presses
			switch (getch()) {
			case '1': targetFrequency += 10; break;
			case '2': targetFrequency -= 10; if(targetFrequency < 10) targetFrequency = 10; break;
			case 'X':
			case 'x':
				finished = true;
			}
			printf("\n Current target mudulation frequency is %d \n ", targetFrequency);
		}
		updateInterval = 1000000 / targetFrequency / 2;
		numUpdates = targetFrequency * 2;

		DWORD currentTime = microTimer::uGetTime();
		if (currentTime - lastUpdate > updateInterval) {
			lastUpdate = currentTime;
			if (amplitudeOn) updatePhasesAndAmplitudes(driver, phases, amplitudesOff);
			else updatePhasesAndAmplitudes(driver, phases, amplitudes);
			amplitudeOn = !amplitudeOn;

			if (amplitudeOn) {
				if (updateCount == numUpdates - 1) {
					DWORD currentReport = microTimer::uGetTime();
					float timeInSec = (currentReport - lastReport) * 0.000001f;
					printf("It took %f sec to update %d frames => %f Hz\n", timeInSec, numUpdates, numUpdates / timeInSec);
					updateCount = 0;
					lastReport = currentReport;
				}
				else updateCount++;
			}
		}
	}
}

void main(void) {
	//Create handler and connect to it
	AsierInhoWireless::RegisterPrintFuncs(print, print, print);
	AsierInhoWireless::AsierInhoWirelessBoard* driver = AsierInhoWireless::createAsierInho();
	int numBoards = 1;
	int boardIDs[] = { 10 };
	float matBoardToWorld[32] = {
		/*top*/
		-1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0,-1, 0.0845f,
		0, 0, 0, 1,
	};
	if (!driver->connect(numBoards, boardIDs, matBoardToWorld))
		printf("Failed to connect to board.");

	float t_positions[64 * 3], t_normals[64 * 3], amplitudeAdjust[64];
	int phaseAdjust[64], transducerIDs[64], numDiscreteLevels;
	driver->readParameters(t_positions, t_normals, transducerIDs, phaseAdjust, amplitudeAdjust, &numDiscreteLevels);
	int numTransducers = driver->totalTransducers();

	//Create solver:
	GSPAT_BEM::RegisterPrintFuncs(print, print, print);
	GSPAT::Solver* solver = GSPAT_BEM::createSolver(numTransducers);//Number of transducers used (two boards of 16x16)
	solver->setBoardConfig(t_positions, t_normals, transducerIDs, phaseAdjust, amplitudeAdjust, numDiscreteLevels);

	//Tune the algorithm
	int numIterations = 200;			// Number of iterations in the solver (more iteration, it gets better but slower)
	int numPointsPerTrap = 2;			// 1: Create focal points using WGS, 2 or 4: Create traps by minimizing Gor'kov potential
	float targetPotential = -4e-7;		// Target Gor'kov potential at each point
	GSPAT::Solver::ConfigParameter algorithmConfig[3];
	algorithmConfig[0] = GSPAT::Solver::ConfigParameter{ GSPAT_BEM::NUM_ITERATIONS, (void*)numIterations };
	algorithmConfig[1] = GSPAT::Solver::ConfigParameter{ GSPAT_BEM::NUM_POINTS_PER_TRAP, (void*)numPointsPerTrap };
	algorithmConfig[2] = GSPAT::Solver::ConfigParameter{ GSPAT_BEM::TARGET_POTENTIAL, (void*)(&targetPotential) };
	solver->setConfigParameters(3, algorithmConfig);
	//Set up your reflector
	char reflectorName[] = "media/BEMFiles/FlatMini.bin";
	GSPAT::Solver::ConfigParameter reflectorConfig[1];
	reflectorConfig[0] = GSPAT::Solver::ConfigParameter{ GSPAT_BEM::REFLECTOR_FILE, (void*)reflectorName };
	solver->setConfigParameters(1, reflectorConfig);


	//Fill your position and amplitude buffers
	const int numPoints = 1;			// Number of points (trap/focus) 
	const int numGeometries = 1;		// It's okay to fix this to 1 in this main file
	float curPos[4 * numGeometries * numPoints];
	float amplitude[numGeometries * numPoints];
	for (int g = 0; g < numGeometries; g++) {
		for (int p = 0; p < numPoints; p++) {
			curPos[4 * g * numPoints + 4 * p + 0] = 0;
			curPos[4 * g * numPoints + 4 * p + 1] = 0;
			curPos[4 * g * numPoints + 4 * p + 2] = 0.011f + 0.00865f / 4.f;
			curPos[4 * g * numPoints + 4 * p + 3] = 1;
			amplitude[g * numPoints + p] = 10000;
		}
	}
	//set up your start and end matrices (if you use an identity matrix, it does not do anything)
	float matI[] = { 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 };
	float m1[numPoints * 16];
	for (int i = 0; i < numPoints; i++)
		memcpy(&(m1[i * 16]), matI, 16 * sizeof(float));
	//a. create a solution
	GSPAT::Solution* solution = solver->createSolution(numPoints, numGeometries, true, curPos, amplitude, m1, m1);
	solver->compute(solution);
	unsigned char* msg;
	solution->finalMessages(&msg);
	driver->updateMessage(msg);
	solver->releaseSolution(solution);

	//b. Main loop (finished when space bar is pressed):
	printf("\n Place a bead at (%f,%f,%f)\n ", curPos[0], curPos[1], curPos[2]);
	printf("Use keys A-D , W-S and Q-E to move the bead\n");
	printf("Press 'X' to destroy the solver.\n");

	bool finished = false;
	while (!finished) {
		//Update 3D position from keyboard presses
		switch (getch()) {
		case 'a':
			for (int g = 0; g < numGeometries; g++)
				for (int p = 0; p < numPoints; p++)
					curPos[4 * g * numPoints + 4 * p + 0] += 0.00025f; break;
		case 'd':
			for (int g = 0; g < numGeometries; g++)
				for (int p = 0; p < numPoints; p++)
					curPos[4 * g * numPoints + 4 * p + 0] -= 0.00025f; break;
		case 'w':
			for (int g = 0; g < numGeometries; g++)
				for (int p = 0; p < numPoints; p++)
					curPos[4 * g * numPoints + 4 * p + 1] += 0.00025f; break;
		case 's':
			for (int g = 0; g < numGeometries; g++)
				for (int p = 0; p < numPoints; p++)
					curPos[4 * g * numPoints + 4 * p + 1] -= 0.00025f; break;
		case 'q':
			for (int g = 0; g < numGeometries; g++)
				for (int p = 0; p < numPoints; p++)
					curPos[4 * g * numPoints + 4 * p + 2] += 0.00025f; break;
		case 'e':
			for (int g = 0; g < numGeometries; g++)
				for (int p = 0; p < numPoints; p++)
					curPos[4 * g * numPoints + 4 * p + 2] -= 0.00025f; break;
		case 'x':
		case 'X':
			finished = true;

		}
		//Create the trap and send to the board:
		GSPAT::Solution* solution = solver->createSolution(numPoints, numGeometries, true, curPos, amplitude, m1, m1);
		solver->compute(solution);
		solution->finalMessages(&msg);
		driver->updateMessage(msg);
		solver->releaseSolution(solution);
		printf("\n Bead location (%f,%f,%f)\n ", curPos[0], curPos[1], curPos[2]);

	}
	//Deallocate the AsierInho controller: 
	for (int s = 0; s < 16; s++)
		driver->turnTransducersOff();
	driver->disconnect();
	Sleep(50);
	delete driver;
	delete solver;
}
