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
	int COMPortNumber = 10;
	if (!driver->connect(COMPortNumber))
		printf("Failed to connect to board.");

	float t_positions[512 * 3], t_normals[512 * 3], amplitudeAdjust[512];
	int phaseAdjust[512], transducerIDs[512], numDiscreteLevels;
	driver->readParameters(t_positions, t_normals, transducerIDs, phaseAdjust, amplitudeAdjust, &numDiscreteLevels);
	int numTransducers = driver->totalTransducers();

	bool twinTrap = false;
	float p_amplitude = 1.f;
	float phases[64], amplitudes[64];

	//for (int i = 0; i < numTransducers; i++) {
	//	phases[i] = 0;
	//	amplitudes[i] = 0;
	//}
	//updatePhasesAndAmplitudes(driver, phases, amplitudes);

	//for (int i = 0; i < numTransducers * 4; i++) {
	//	_getch();
	//	amplitudes[i] = 1;
	//	updatePhasesAndAmplitudes(driver, phases, amplitudes);
	//	printf("Transducer %d: amplitude = %f, phase = %f\n", i, amplitudes[0], phases[0]);
	//	amplitudes[i] = 0;
	//}


	//Program: Create a trap and move it with the keyboard
	float curPos[] = { 0, 0, 0.08f };
	float amplitudesOff[512];
	//a. create a first trap and send it to the board: 
	createTrap(numTransducers, curPos, t_positions, amplitudes, phases, p_amplitude, twinTrap);
	memset(amplitudesOff, 0, numTransducers * sizeof(float));
	updatePhasesAndAmplitudes(driver, phases, amplitudes);
	printf("\n Place a bead at (%f,%f,%f)\n ", curPos[0], curPos[1], curPos[2]);
	printf("Use keys A-D , W-S and Q-E to move the bead\n");
	printf("Press 'X' to finish the application.\n");

	//b. Main loop (finished when space bar is pressed):
	float moveStep = 0.0002f;
	bool finished = false;
	while (!finished) {
		if (_kbhit()) {
			printf("\n Point at (%f,%f,%f) with amplitude of %f\n ", curPos[0], curPos[1], curPos[2], p_amplitude);
			//Update 3D position from keyboard presses
			switch (getch()) {
			case 'a':curPos[0] += moveStep; break;
			case 'd':curPos[0] -= moveStep; break;
			case 'w':curPos[1] += moveStep; break;
			case 's':curPos[1] -= moveStep; break;
			case 'q':curPos[2] += moveStep; break;
			case 'e':curPos[2] -= moveStep; break;
			case '0':amplitudeModulation(driver, phases, amplitudes, amplitudesOff, 50); break;
			case '1':p_amplitude += 0.1f; if (p_amplitude > 1.f) p_amplitude = 1.f; break;
			case '2':p_amplitude -= 0.1f; if (p_amplitude < 0.f) p_amplitude = 0.f; break;
			case 'X':
			case 'x':
				finished = true;
			}
		}
		//Create the trap and send to the board:
		createTrap(numTransducers, curPos, t_positions, amplitudes, phases, p_amplitude, twinTrap);
		updatePhasesAndAmplitudes(driver, phases, amplitudes);
		Sleep(100);
	}
	//Deallocate the AsierInho controller: 
	for (int s = 0; s < 16; s++)
		driver->turnTransducersOff();
	driver->disconnect();
	Sleep(50);
	delete driver;
}
