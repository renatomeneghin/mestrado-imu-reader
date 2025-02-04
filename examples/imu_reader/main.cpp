#include <iostream>
#include <fstream>
#include <cmath>
#include <csignal>
#include <string.h>

// Include this header file to get access to VectorNav sensors.
#include "vn/sensors.h"

// We need this file for our sleep function.
#include "vn/thread.h"
#include "vn/conversions.h"
#include "motionVector.h"

using namespace std;
using namespace vn::math;
using namespace vn::sensors;
using namespace vn::protocol::uart;
using namespace vn::xplat;

// Now let's create a VnSensor object and use it to connect to our sensor.
VnSensor vs;

MotionVector currentMotionVector = MotionVector();

// Method declarations for future use.
void signalHandler(int signum);
void asciiAsyncMessageReceived(void* userData, Packet& p, size_t index);

int main(int argc, char *argv[])
{
	// VNISL = state of the sensors
	// VNINS = with internal kalman filter

	if (strcmp(argv[1], "VNISL") == 0 || strcmp(argv[1], "VNINS") == 0) {

		const string SensorPort = "/dev/ttyUSB0"; // Linux format for virtual (USB) serial port.
		const uint32_t SensorBaudrate = 115200;

		vs.connect(SensorPort, SensorBaudrate);

		// VNISL or VNINS
		if (strcmp(argv[1], "VNISL") == 0) {
			vs.writeAsyncDataOutputType(VNISL);
		} else {
			vs.writeAsyncDataOutputType(VNINS);
		}
		
		AsciiAsync asyncType = vs.readAsyncDataOutputType();
		cout << "ASCII Async Type: " << asyncType << endl;

		vs.registerAsyncPacketReceivedHandler(NULL, asciiAsyncMessageReceived);

		while(true)
		{
			// Now sleep so that our asynchronous callback method can
			// receive and display receive yaw, pitch, roll packets.
			Thread::sleepSec(1);
		}

	} else {
		cout << "Wrong parameter value, please select VNISL or VNINS" << endl;

		return 1;
	}

	return 0;
}

// Function to be executed when (Ctrl+C) command 
void signalHandler(int signum) {
    cout << "Interruption signal (Ctrl+C) received." << endl;

    // Unregister our callback method.
	vs.unregisterAsyncPacketReceivedHandler();

	vs.disconnect();

    // Agora, você pode encerrar o programa
    exit(signum);
}

void asciiAsyncMessageReceived(void* userData, Packet& p, size_t index)
{
	// Make sure we have an ASCII packet and not a binary packet.
	if(p.type() != Packet::TYPE_ASCII)
		return;

	// Make sure we have a VNISL data packet.
	if (p.determineAsciiAsyncType() == VNISL) {

		vec3f ypr;
		vec3d lla;
		vec3f velocity;
		vec3f acceleration;
		vec3f angularRate;

		p.parseVNISL(&ypr, &lla, &velocity, &acceleration, &angularRate);

		currentMotionVector.setLLA(lla);
		currentMotionVector.setSpeed(velocity.mag());
		currentMotionVector.setHeading(acos(velocity.norm().x));
		currentMotionVector.setYawRate(angularRate.z);
		currentMotionVector.setAcceleration(acceleration.mag());
		currentMotionVector.setOrientation(deg2rad(ypr.x));

		cout << currentMotionVector.toString() << endl;

	} else if (p.determineAsciiAsyncType() == VNINS) {

		double time;
		uint16_t week;
		uint16_t status;
		vec3f ypr;
		vec3d lla;
		vec3f velocity_NED;
		float attUncertainty;
		float posUncertainty;
		float velUncertainty;

		p.parseVNINS(&time, &week, &status, &ypr, &lla, &velocity_NED, &attUncertainty, &posUncertainty, &velUncertainty);

		vec2f NED_vel_New = vec2f(velocity_NED.x,velocity_NED.y);
		vec2f NED_vel_Old = vec2f(currentMotionVector.getSpeed()*cos(currentMotionVector.getOrientation()),
								currentMotionVector.getSpeed()*sin(currentMotionVector.getOrientation()));
		vec2f delta_velocity = NED_vel_New-NED_vel_Old;
		float dyaw = deg2rad(ypr.x)-currentMotionVector.getOrientation();

		currentMotionVector.setLLA(lla);
		currentMotionVector.setSpeed(velocity_NED.mag());
		currentMotionVector.setHeading(acos(velocity_NED.norm().x));
		currentMotionVector.setYawRate(dyaw/time);
		currentMotionVector.setAcceleration(delta_velocity.mag()/time);
		currentMotionVector.setOrientation(deg2rad(ypr.x));

		cout << currentMotionVector.toString() << endl;

	} else {
		cout << "ASCII Async: Type(" << p.determineAsciiAsyncType() << ")" << endl;
	}

}

//   if (p.type() == Packet::TYPE_BINARY)
//     {
//         // First make sure we have a binary packet type we expect since there
//         // are many types of binary output types that can be configured.
//         if (!p.isCompatible(
//             (1<<0)|(1<<3)|(1<<4)|(1<<5)|(1<<6)|(1<<7)|(1<<8), // Offsets : funções -> 0:timeStartup, 3:YPR, 4:Quaternion , 5:AngularRate , 6:LLA , 7:NED_vel , 8:Accel
//             TIMEGROUP_NONE,
//             IMUGROUP_NONE,
//             GPSGROUP_NONE,
//             ATTITUDEGROUP_NONE,
//             INSGROUP_NONE))
//             // Not the type of binary packet we are expecting.
//             return;// Offsets : funções -> 0:timeStartup, 3:YPR, 4:Quaternion , 5:AngularRate , 6:LLA , 7:NED_vel , 8:Accel
//         uint64_t timeStartup = p.extractUint64();
//         vec3f ypr = deg2rad(p.extractVec3f());
//         vec4f Quat = p.extractVec4f();
//         vec3f angularRate = p.extractVec3f();
//         vec3d lla = p.extractVec3d();
//         vec3f NED_Vel = p.extractVec3f();
//         vec3f Accel = p.extractVec3f();

//         currentMotionVector.setLLA(lla);
//         currentMotionVector.setSpeed(NED_Vel.mag());
//         currentMotionVector.setHeading(acos(NED_Vel.norm().x)_;
//         currentMotionVector.setYawRate(angularRate.z);
//         currentMotionVector.setAcceleration(Accel.mag());
//         currentMotionVector.setOrientation(ypr.x);


//         cout << "Binary Async TimeStartup: " << timeStartup << endl;
//         cout << "Binary Async YPR: " << ypr << endl;
//     }
// }
