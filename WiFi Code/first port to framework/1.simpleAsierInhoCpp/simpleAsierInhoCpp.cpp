//#include <AsierInho.h>
//#include <AsierInho_V2.h>
#include <AsierInhoWireless.h>
#include <Helper\HelperMethods.h>
#include <Helper\microTimer.h>
#include <string.h>
#include <conio.h>
#include <windows.h>
void print(const char* str) {
	printf("%s\n", str);
}

void createTrap(int numTransducers, float position[3], float *t_positions, float* amplitudes, float* phases) {
	//1. Traverse each transducer:
	for (int buffer_index=0; buffer_index< numTransducers; buffer_index++) {
		//a. Get its position:
		float* t_pos=&(t_positions[3*buffer_index]);
		//b. Compute phase delay and amplitudes from transducer to target point: 
		float distance, amplitude;
		computeAmplitudeAndDistance(t_pos, position, &amplitude, &distance);
		float phase = fmodf(-K()*distance, (float)(2 * M_PI));
		printf("%d: %f -> %f\n", buffer_index, distance, phase);

		//c. Store it in the output buffers.
		phases[buffer_index] = phase;
		amplitudes[buffer_index] = 1.f;// amplitude;
	}
}

//void main(void) {
//	//Create handler and connect to it
//	AsierInho::RegisterPrintFuncs(print, print, print);
//	AsierInho::AsierInhoBoard* driver = AsierInho::createAsierInho();
//	if(!driver->connect(AsierInho::BensDesign, 15))
//		printf("Failed to connect to board.");
//
//	float t_positions[512 * 3], amplitudeAdjust[512];
//	int phaseAdjust[512], transducerIDs[512], numDiscreteLevels;
//	driver->readParameters(t_positions, transducerIDs, phaseAdjust, amplitudeAdjust, &numDiscreteLevels);
//	//Program: Create a trap and move it with the keyboard
//	float curPos[] = { 0,0,0.1f };
//	float phases[512], amplitudes[512];
//	unsigned char phases_disc[512], amplitudes_disc[512];
//	//a. create a first trap and send it to the board: 
//	createTrap(curPos, t_positions,amplitudes, phases);
//	driver->discretizePhases(phases, phases_disc);
//	driver->updateDiscretePhases(phases_disc);
//	printf("\n Place a bead at (%f,%f,%f)\n ", curPos[0], curPos[1], curPos[2]);
//	printf("Use keys A-D , W-S and Q-E to move the bead\n");
//	printf("Press 'X' to finish the application.\n");
//
//	//b. Main loop (finished when space bar is pressed):
//	bool finished = false;
//	while (!finished) {
//		printf("\n Point at (%f,%f,%f)\n ", curPos[0], curPos[1], curPos[2]);
//		//Update 3D position from keyboard presses
//		switch (getch()) {
//			case 'a':curPos[0] += 0.0005f; break;
//			case 'd':curPos[0] -= 0.0005f; break;
//			case 'w':curPos[1] += 0.0005f; break;
//			case 's':curPos[1] -= 0.0005f; break;
//			case 'q':curPos[2] += 0.00025f; break;
//			case 'e':curPos[2] -= 0.00025f; break;
//			case 'X':
//			case 'x': 
//				finished = true;
//
//		}
//		//Create the trap and send to the board:
//		createTrap(curPos, t_positions,amplitudes, phases);
//		driver->discretizePhases( phases, phases_disc);
//		for (int s = 0; s < 16;s++)//Make sure we fill the buffers and the update is processed without delay
//			driver->updateDiscretePhases(phases_disc);
//	}
//	//Deallocate the AsierInho controller: 
//	for (int s = 0; s < 16;s++)
//		driver->turnTransducersOff();
//	driver->disconnect();
//	delete driver;	
//}


void updatePhasesAndAmplitudes(AsierInhoWireless::AsierInhoWirelessBoard* driver, float* phases, float* amplitudes) {
	unsigned char *message = driver->discretize(phases, amplitudes);
	driver->updateMessage(message);
}

void main(void) {
	//Create handler and connect to it
	AsierInhoWireless::RegisterPrintFuncs(print, print, print);
	AsierInhoWireless::AsierInhoWirelessBoard* driver = AsierInhoWireless::createAsierInho();
	int COMPortNumber = 10;
	if (!driver->connect(COMPortNumber))
		printf("Failed to connect to board.");

	float t_positions[512 * 3], t_normals[512 * 3], amplitudeAdjust[512];
	int phaseAdjust[512], transducerIDs[512], numDiscreteLevels;
	driver->readParameters(t_positions, t_normals, transducerIDs, phaseAdjust, amplitudeAdjust, &numDiscreteLevels);
	int numTransducers = driver->totalTransducers();


	//// Turn one of the transducers on from 0 to 63
	//float* phases = new float[numTransducers];		// phase can range from 0 to 2 pi
	//float* amplitudes = new float[numTransducers];  // amplitude can range from 0 (off) to 1 (maximum) 

	////////////////////////////////////////////////
	//// feed your buffers manually			    //
	//// this example turns every transducers off //
	////////////////////////////////////////////////
	//for (int i = 0; i < numTransducers; i++) {
	//	phases[i] = 0;
	//	amplitudes[i] = 0;
	//}

	//updatePhasesAndAmplitudes(driver, phases, amplitudes);

	////next, let's turn on a transducer one by one
	//for (int i = 0; i < numTransducers; i++) {
	//	printf("Transducer %d is on\n", i);
	//	amplitudes[i] = 1;
	//	updatePhasesAndAmplitudes(driver, phases, amplitudes);
	//	amplitudes[i] = 0;
	//	_getch();
	//}

	//driver->disconnect();
	//delete driver;
	//delete[] phases;
	//delete[] amplitudes;



	//Program: Create a trap and move it with the keyboard
	float curPos[] = { 0, 0, 0.08f };
	float phases[512], amplitudes[512], amplitudesOff[512];
	//a. create a first trap and send it to the board: 
	createTrap(numTransducers, curPos, t_positions, amplitudes, phases);
	memset(amplitudesOff, 0, numTransducers * sizeof(float));
	updatePhasesAndAmplitudes(driver, phases, amplitudes);
	printf("\n Place a bead at (%f,%f,%f)\n ", curPos[0], curPos[1], curPos[2]);
	printf("Use keys A-D , W-S and Q-E to move the bead\n");
	printf("Press 'X' to finish the application.\n");

	//b. Main loop (finished when space bar is pressed):
	bool finished = false;
	while (!finished) {
		printf("\n Point at (%f,%f,%f)\n ", curPos[0], curPos[1], curPos[2]);
		//Update 3D position from keyboard presses
		switch (getch()) {
		case 'a':curPos[0] += 0.0005f; break;
		case 'd':curPos[0] -= 0.0005f; break;
		case 'w':curPos[1] += 0.0005f; break;
		case 's':curPos[1] -= 0.0005f; break;
		case 'q':curPos[2] += 0.00025f; break;
		case 'e':curPos[2] -= 0.00025f; break;
		case 'X':
		case 'x':
			finished = true;

		}
		//Create the trap and send to the board:
		createTrap(numTransducers, curPos, t_positions, amplitudes, phases);
		updatePhasesAndAmplitudes(driver, phases, amplitudes);

		int targetHz = 50;
		bool amplitudeOn = true;
		DWORD lastUpdate = microTimer::uGetTime();
		DWORD updateInterval = 1000000 / targetHz / 2;
		int updateCount = 0;
		int numUpdates = targetHz * 2;
		DWORD lastReport = microTimer::uGetTime();
		while (1) {
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
	//Deallocate the AsierInho controller: 
	for (int s = 0; s < 16; s++)
		driver->turnTransducersOff();
	driver->disconnect();
	delete driver;
}
