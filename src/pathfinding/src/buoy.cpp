#include "Pathfinding.hpp"

#include <cmath>

void Pathfinding::addBuoy(point_t buoyPos) {
	_buoyPos.x = buoyPos.x;
	_buoyPos.y = buoyPos.y;
	_buoyGraphIndex = _obstaclesGraph.addVertex();
	generateNodeAdjList(_buoyPos, _buoyGraphIndex, _obstaclesGraph);
}

void Pathfinding::calculateBuoyPos() {
	double buoyOrientation = convertToMinusPiPi(_orientation + _buoyBearing);
	_buoyPos.x = _buoyRange * std::cos(buoyOrientation) + _boatPos.x;
	_buoyPos.y = _buoyRange * std::sin(buoyOrientation) + _boatPos.y;
}

double Pathfinding::convertToMinusPiPi(double angleRadians) {
	while (angleRadians <= -M_PI) {
		angleRadians += 2.0 * M_PI;
	}
	while (angleRadians > M_PI) {
		angleRadians -= 2.0 * M_PI;
	}
	return (angleRadians);
}