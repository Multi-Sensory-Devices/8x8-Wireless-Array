#include <GSPAT_SolverBEM.h>
#include <AsierInhoWireless.h>
#include <stdio.h>
#include <conio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <ReflectionSurface.h>
#include <BEMSolver.h>

void print(const char* str) {
	printf("%s\n", str);
}

void main() {
	//Create driver and connect to it
	AsierInhoWireless::RegisterPrintFuncs(print, print, print);
	AsierInhoWireless::AsierInhoWirelessBoard* driver = AsierInhoWireless::createAsierInho();
	//Setting the board
	int numBoards = 1;
	int numTransducers = 64 * numBoards;
	int boardIDs[] = { 10 };
	float matBoardToWorld[32] = { // only using the top board in this example
		/*top*/
		-1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0,-1, 0.0845f,
		0, 0, 0, 1,
	};
	// Connect to the board (it might be fail to connect but don't worry, you don't need actual boards)
	// The main purpose for this is to get the transducers' positions and normals
	if (!driver->connect(numBoards, boardIDs, matBoardToWorld))
		printf("Failed to connect to board.");
	float transducerPositions[512 * 3], transducerNormals[512 * 3], amplitudeAdjust[512];
	int mappings[512], phaseDelays[512], numDiscreteLevels;
	driver->readParameters(transducerPositions, transducerNormals, mappings, phaseDelays, amplitudeAdjust, &numDiscreteLevels);
	// create solver
	GSPAT_BEM::RegisterPrintFuncs(print, print, print);
	GSPAT::Solver* solver = GSPAT_BEM::createSolver(numTransducers);//Number of transducers used (two boards of 16x16)
	solver->setBoardConfig(transducerPositions, transducerNormals, mappings, phaseDelays, amplitudeAdjust, numDiscreteLevels);
	// Set your reflector
	ReflectionSurface reflector;
	glm::mat4 translate = glm::translate(glm::mat4(1.f), glm::vec3(0, 0, 0.0065f + 0.0045f));
	//reflector.addSTLObject("media/BEMFiles/STL/Flat-lam2.stl", translate);
	reflector.addSTLObject("media/BEMFiles/STL/santeri-lam6.stl", translate);
	//reflector.addSTLObject("media/BEMFiles/STL/extension-lam2.stl", translate);
	//reflector.addSTLObject("media/BEMFiles/STL/XYbigwavy-lam4.stl", translate);
	//reflector.addSTLObject("media/BEMFiles/STL/0.06-0.16extension-lam2.stl", translate);
	int numMeshes = reflector.getNumMeshes();
	float* meshPositions = reflector.getCentres();
	float* meshAreas = reflector.getAreas();
	float* meshNormals = reflector.getNormals();
	// Compute Matrices you need to compute the transducer-to-mesh matrix
	float* matrixA, * matrixB;
	GSPAT_BEM::computeMatrixAandB(solver, numMeshes, meshPositions, meshAreas, meshNormals, matrixA, matrixB);
	// We don't need the driver and sovler any more, so delete them
	driver->disconnect();
	delete driver;
	delete solver;
	// Create BEM solver and compute your transducer-to-mesh matrix
	BEMSolver* bemSolver = new BEMSolver();
	bemSolver->setConfiguration(numMeshes, numTransducers, matrixA, matrixB, false);
	bemSolver->decomposeMatrixA();
	float* transducer2MeshMatrix = bemSolver->solveMatrixT2M();
	// Export the necessary files
	BEMSolver::exportBEMFile("media/BEMFiles/Santeri.bin", numTransducers, numMeshes, meshPositions, meshAreas, meshNormals, transducer2MeshMatrix);
	// Delete the buffers
	delete[] matrixA, matrixB;
	delete bemSolver;
	delete[] meshPositions, meshAreas, meshNormals;
	delete[] transducer2MeshMatrix;
}