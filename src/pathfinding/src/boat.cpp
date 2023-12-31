#include "Pathfinding.hpp"

#include <cmath>

#include "rcl_interfaces/msg/parameter.hpp"

point_t	Pathfinding::calculateMapPos(double latitude, double longitude) {
	point_t	pos;

	pos.x = (longitude - LONGITUDE_0) / (LONGITUDE_1 - LONGITUDE_0);
	pos.x = pos.x * 600 - 300;
	pos.y = (latitude - LATITUDE_0) / (LATITUDE_1 - LATITUDE_0);
	pos.y = pos.y * 600 - 300;
	return (pos);
}

double Pathfinding::calculateYaw(const geometry_msgs::msg::Quaternion& orientation) {
	double siny_cosp = 2 * (orientation.w * orientation.z + orientation.x * orientation.y);
	double cosy_cosp = 1 - 2 * (orientation.y * orientation.y + orientation.z * orientation.z);
	return (std::atan2(siny_cosp, cosy_cosp));
}

std::pair<double, double> Pathfinding::calculateRangeBearing() {
	std::pair<double, double>	rangeBearing;
	point_t						checkpointVec;
	point_t						boatVec;

	rangeBearing.first = calculateDist(_boatPos, _path[0]);
	checkpointVec.x = _path[0].x - _boatPos.x;
	checkpointVec.y = _path[0].y - _boatPos.y;
	boatVec.x = std::cos(_orientation);
	boatVec.y = std::sin(_orientation);

	double dotProduct = boatVec.x * checkpointVec.x + boatVec.y * checkpointVec.y;

	double crossProduct = boatVec.x * checkpointVec.y - boatVec.y * checkpointVec.x;

	rangeBearing.second = std::atan2(crossProduct, dotProduct);
	return (rangeBearing);
}

void Pathfinding::publishRangeBearing(const std::pair<double, double>& rangeBearing, double desiredRange) {
	auto	paramVecMsg = ros_gz_interfaces::msg::ParamVec();
	rcl_interfaces::msg::Parameter	rangeMsg;
	rcl_interfaces::msg::Parameter	bearingMsg;
	rcl_interfaces::msg::Parameter	desiredRangeMsg;
	rcl_interfaces::msg::Parameter	stateMsg;
	rcl_interfaces::msg::Parameter	isFollowingEnemyMsg;

	rangeMsg.name = "range";
	rangeMsg.value.double_value = rangeBearing.first;
	paramVecMsg.params.push_back(rangeMsg);

	bearingMsg.name = "bearing";
	bearingMsg.value.double_value = rangeBearing.second;
	paramVecMsg.params.push_back(bearingMsg);

	desiredRangeMsg.name = "desiredRange";
	desiredRangeMsg.value.double_value = desiredRange;
	paramVecMsg.params.push_back(desiredRangeMsg);

	stateMsg.name = "state";
	stateMsg.value.integer_value = _state;
	paramVecMsg.params.push_back(stateMsg);

	isFollowingEnemyMsg.name = "isFollowingEnemy";
	isFollowingEnemyMsg.value.bool_value = (_path.size() == 1);
	paramVecMsg.params.push_back(isFollowingEnemyMsg);


	if (rangeBearing.first < desiredRange) {
		_path.erase(_path.begin());
		for (auto node : _path)
			std::cout << "[" << node.x << "," << node.y << "], ";
		std::cout << std::endl;
	}
	_publisherRangeBearing->publish(paramVecMsg);
}

void Pathfinding::checkEnemyCollision(ros_gz_interfaces::msg::ParamVec::SharedPtr msg) {
	double	range = 130.0;

	for (const auto &param: msg->params) {
		std::string name = param.name;

		if (name == "range") {
			range = param.value.double_value;
		} else if (name == "x") {
			_enemyPos.x = param.value.double_value;
		} else if (name == "y") {
			_enemyPos.y = param.value.double_value;
		}
	}
	if (range >= 129.0)
		return;
	std::cout << "enemy detected: " << _enemyPos.x << ", " << _enemyPos.y << std::endl;
	_enemyPing = true;
	if (!_buoyPing)
		return;
	_path = calculatePath(_boatPos);
	for (auto node : _path)
		std::cout << "[" << node.x << "," << node.y << "], ";
	std::cout << std::endl;
}