#include "Navigation.hpp"

#include <algorithm>

using std::placeholders::_1;

Navigation::Navigation() : Node("navigation") {
	std::vector<point_t>	path;
	_gain = 0.2;
	_sigma = 0.05;
	_buoyRange = 0;
	_exitBuoy = false;
	_pinger = create_subscription<ros_gz_interfaces::msg::ParamVec>("/navigation/pinger", 10,
				std::bind(&Navigation::pingerCallback, this, _1));
	_taskInfo = create_subscription<ros_gz_interfaces::msg::ParamVec>("/vrx/task/info", 10,
				std::bind(&Navigation::taskInfoCallback, this, _1));
	_buoyPinger = create_subscription<ros_gz_interfaces::msg::ParamVec>("/wamv/sensors/acoustics/receiver/range_bearing", 10,
				std::bind(&Navigation::buoyPingerCallback, this, _1));
	_publisherThrust = create_publisher<std_msgs::msg::Float64>("/wamv/thrusters/main/thrust", 5);
	_publisherPos = create_publisher<std_msgs::msg::Float64>("/wamv/thrusters/main/pos", 5);
}

Navigation::Navigation(double gain, double sigma) : Navigation() {
	_gain = gain;
	_sigma = sigma;
}

void Navigation::pingerCallback(ros_gz_interfaces::msg::ParamVec::SharedPtr msg) {
	bool scan = false;
	double scanValue;
	for (const auto &param: msg->params) {
		std::string name = param.name;
		if (name == "bearing") {
			_bearing = param.value.double_value;
		} else if (name == "range") {
			_range = param.value.double_value;
		} else if (name == "desiredRange") {
			_desiredRange = param.value.double_value;
		} else if (name == "state") {
			_state = param.value.integer_value;
		} else if (name == "scan") {
			scan = param.value.bool_value;
		} else if (name == "scanOrientation") {
			scanValue = param.value.integer_value;
		} else if (name == "isFollowingEnemy") {
			_isFollowingEnemy = param.value.bool_value;
		}
	}
	if (_state >= FOLLOW_STATE && !scan && std::abs(_buoyBearing - _bearing) > 0.2 && _buoyRange > BUOY_EXIT_RANGE) {
		setHeading();
		return;
	}
	if (_state >= FOLLOW_STATE && (_buoyRange < BUOY_EXIT_RANGE || _exitBuoy)) {
		_exitBuoy = true;
		goOutsideBuoy();
		return;
	}
	if (_state >= FOLLOW_STATE && _buoyRange < BUOY_FREE_RANGE && !scan) {
		orbitBuoy();
		return;
	}
	if (scan) {
		spin(scanValue);
		return;
	}
	setHeading();
}

void Navigation::buoyPingerCallback(ros_gz_interfaces::msg::ParamVec::SharedPtr msg) {
	for (const auto &param: msg->params) {
		std::string name = param.name;
		if (name == "range") {
			_buoyRange = param.value.double_value;
		}
		if (name == "bearing") {
			_buoyBearing = param.value.double_value;
		}
	}
}

void Navigation::taskInfoCallback(ros_gz_interfaces::msg::ParamVec::SharedPtr msg) {
	for (const auto & param: msg->params) {
		if (param.name == "state" && param.value.string_value == "finished") {
			rclcpp::shutdown();
			exit(0);
		}
	}
}

void Navigation::setHeading() {
	auto 	posMsg = std_msgs::msg::Float64();
	auto 	thrustMsg = std_msgs::msg::Float64();
	double	regulation;

	regulation = regulator(_bearing);
	posMsg.data = (POS_MIN + regulation * 2 * POS_MAX);
	thrustMsg.data = std::abs(std::abs(regulation - 0.5) - 0.5) * 2 * THRUST_MAX;
	thrustMsg.data = calculateThrust(regulation);
	if (thrustMsg.data < 0) {
		posMsg.data = -posMsg.data;
	}
	_publisherThrust->publish(thrustMsg);
	_publisherPos->publish(posMsg);
}

double Navigation::calculateThrust(double regulation) {
	const double amplitude = THRUST_MAX - NAV_THRUST_MIN;
	const double mean = 0.5;
	double thrust;

	if (_state >= FOLLOW_STATE && _isFollowingEnemy) {
		thrust = std::pow(3 * (_range - _desiredRange), 3) + 6000;
		thrust = std::min(thrust, 6000.);
		thrust = std::max(thrust, 0.);
		if (_range < 15) {
			thrust = -12000;
		}
	} else {
		if (_range < _desiredRange) {
			return (-12000.);
		}
		thrust = amplitude * std::exp(-std::pow(((regulation - mean)) / (_sigma * 2), 2));
		thrust += NAV_THRUST_MIN;
	}
	return (thrust);
}

double Navigation::regulator(double bearing) {
	const double	goal = 0;
	double			gap;
	double			regulation;

	gap = (goal - bearing) * _gain;
	regulation = (gap + M_PI) / (2 * M_PI);
	regulation = std::min(regulation, 1.);
	regulation = std::max(regulation, 0.);
	return (regulation);
}

void Navigation::goOutsideBuoy() {
	auto 	posMsg = std_msgs::msg::Float64();
	auto 	thrustMsg = std_msgs::msg::Float64();

	posMsg.data = 0;
	thrustMsg.data = THRUST_MIN;
	if (_buoyRange > (BUOY_FREE_RANGE + BUOY_EXIT_RANGE) / 2.) {
		_exitBuoy = false;
	}
	_publisherPos->publish(posMsg);
	_publisherThrust->publish(thrustMsg);
}


void Navigation::spin(double scanValue) {
	auto 	posMsg = std_msgs::msg::Float64();
	auto 	thrustMsg = std_msgs::msg::Float64();

	if (scanValue > 0) {
		posMsg.data = M_PI / 8;
	} else {
		posMsg.data = -(M_PI / 8);
	}
	thrustMsg.data = 500;
	_publisherPos->publish(posMsg);
	_publisherThrust->publish(thrustMsg);
}

void Navigation::orbitBuoy() {
	auto 	posMsg = std_msgs::msg::Float64();
	auto 	thrustMsg = std_msgs::msg::Float64();
	double	pos;

	thrustMsg.data = 2000;
	pos = -convertToMinusPiPi(_buoyBearing + M_PI_2);
	pos = std::min(pos, M_PI_4);
	pos = std::max(pos, -M_PI_4);
	posMsg.data = pos * 0.5;
	_publisherPos->publish(posMsg);
	_publisherThrust->publish(thrustMsg);
}

double Navigation::convertToMinusPiPi(double angleRadians) {
	while (angleRadians <= -M_PI) {
		angleRadians += 2.0 * M_PI;
	}
	while (angleRadians > M_PI) {
		angleRadians -= 2.0 * M_PI;
	}
	return (angleRadians);
}
