#include <Windows.h>
#include <iostream>
#include <chrono>
#include <cstdint>
#include "FlightData.h"

using namespace std::chrono;

#pragma comment(lib, "Ws2_32.lib")

FlightData* data;
FlightData2* data2;

#define RAD_TO_DEG(rad) (rad*180/3.1415926535)
#define DEG_TO_RAD(deg) (deg/180*3.1415926535)

#define FPS_TO_KT(fps) (fps*0.592484)
#define FT_TO_M(ft) (ft/0.3048)


float calculateAirDensity(float altitude)
{
	//Constants for the barometric formula
		const float seaLevelPressure = 1013.25; //Sea level pressure in hPa
		const float temperatureLapseRate = 0.0065; //Temperature lapse rate in K / m
		const float gravitationalConstant = 9.80665; //Gravitational constant in m / s ^ 2
		const float molarGasConstant = 8.31446261815324; //Molar gas constant in J / (mol K)
		const float molarMassOfDryAir = 0.0289644; //Molar mass of dry air in kg / mol

	// Calculate temperature at the given altitude
		float temperature = 288.15 - temperatureLapseRate * altitude;

	// Calculate pressure at the given altitude
		float pressure = seaLevelPressure * pow(1 - (temperatureLapseRate * altitude) / 288.15, gravitationalConstant * molarMassOfDryAir / (temperatureLapseRate * molarGasConstant));

	// Calculate air density based on the ideal gas law
		float airDensity = pressure * molarMassOfDryAir / (temperature * molarGasConstant);

		return airDensity * 100;
}

int main()
{
	char display[100];
	int iResult;

	//Startup Winsock
		WSADATA wsaData;
		iResult = WSAStartup(MAKEWORD(1, 1), &wsaData);
		if (iResult != NO_ERROR)
		{
			std::cout << "Error starting Winsock\n\n";
			return 0;
		}
	//Create the xmit socket for sending to TelemFFB
		SOCKET sock_send = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (sock_send == INVALID_SOCKET)
		{
			sprintf_s(display, "Error creating transmit socket\nError: %d\n\n", WSAGetLastError());
			std::cout << display;
			return 0;
		}

		sockaddr_in destination;
		destination.sin_family = AF_INET;
		destination.sin_port = htons(34380);
		destination.sin_addr.s_addr = inet_addr("127.255.255.255");

	//Create the recv socket.  Not sure if we really need to do this
		SOCKET sock_recv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (sock_recv == INVALID_SOCKET)
		{
			sprintf_s(display, "Error creating recv socket\nError: %d\n\n", WSAGetLastError());
			std::cout << display;
			return 0;
		}
		
		sockaddr_in source;
		source.sin_family = AF_INET;
		source.sin_port = htons(34381);
		source.sin_addr.s_addr = INADDR_ANY;

		//Bind the socket for receiving
			if (bind(sock_recv, (sockaddr*)&source, sizeof(source)) < 0)
			{
				sprintf_s(display, "Error binding recv socket\nError: %d\n\n", WSAGetLastError());
				std::cout << display;
				return 0;
			}

		//Set the recv timeout
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 100000; //100ms

	//Get handles to the shared memory area
		HANDLE SharedMemHandle = OpenFileMapping(FILE_MAP_READ, FALSE, L"FalconSharedMemoryArea");
		HANDLE SharedMemHandle2 = OpenFileMapping(FILE_MAP_READ, FALSE, L"FalconSharedMemoryArea2");

	//Check if we got them
		if (!SharedMemHandle || !SharedMemHandle2)
		{
			std::cout << "Falcon BMS not detected!\n\n";
			return 0;
		}

	//Map it to our address space
		data = (FlightData*)MapViewOfFile(SharedMemHandle, FILE_MAP_READ, 0, 0, 0);
		data2 = (FlightData2*)MapViewOfFile(SharedMemHandle2, FILE_MAP_READ, 0, 0, 0);

	//Check if we got them
		if (!SharedMemHandle || !SharedMemHandle2)
		{
			std::cout << "Error accessing shared memory!\n\n";
			return 0;
		}

	int n_bytes = ::sendto(sock_send, "Ev = Start", 11, 0, reinterpret_cast<sockaddr*>(&destination), sizeof(destination));

	while (1)
	{
		char string[5000];
		char sAppended[1000];

		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), COORD{ 0,0 });

		//Sometimes TelemFFB sends commands back.  I haven't looked into this yet.  May not be necessary.
			fd_set stReadFDS;
			FD_ZERO(&stReadFDS);
			FD_SET(sock_recv, &stReadFDS);
			int t = select(-1, &stReadFDS, 0, 0, &tv);
			if (t == SOCKET_ERROR)
			{ 
				std::cout << "Select Error!\n\n";
				return 0;
			}
			else if (t > 0)
			{
				n_bytes = recv(sock_recv, string, 5000, 0);
			}


		//Get current system time in seconds (with decimals)
			double time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()/1000.0;
			
		//Send sample data for testing.  This is sitting on the runway in DCS			
			//sprintf_s(string, "T=%0.03f;N=F-16C_50;src=DCS;SelfData=135.44~0.37~0.07;EngRPM=65.834~0.000;HydSys=n/a;HydPress=0.000~0.000;ACCs=0.01~1.00~-0.00;Gun=510;Wind=8.30~-0.16~-7.72;VlctVectors=-0.00~-0.00~-0.00;VerticalSpeed=-2.4344564735657e-05;altASL=466.32;altAgl=1.82;AoA=-0.52;IAS=16.11;IAS_kt=31.31;TAS_raw=0.00;TAS=11.33;TAS_incidence=11.33;TAS_raw_kt=0.00;TAS_incidence_kt=22.03;WeightOnWheels=0.62~0.73~0.63;Flares=60;Chaff=60;PayloadInfo=AIM-120C-4.4.7.106*1~AIM-9X-4.4.7.136*1~Mk-82-4.5.9.31*2~fuel_tank_370gal-1.3.43.11*1~ALQ-184-4.15.45.142*1~fuel_tank_370gal-1.3.43.11*1~Mk-82-4.5.9.31*2~AIM-9X-4.4.7.136*1~AIM-120C-4.4.7.106*1~UNKNOWN_0,0,0,0-0.0.0.0*0~AN/AAQ-28 LITENING-4.15.44.101*1~UNKNOWN_0,0,0,0-0.0.0.0*0;Mach=0.0487;MechInfo={\"canopy\":{\"status\":0,\"value\":0},\"controlsurfaces\":{\"eleron\":{\"left\":0.99949878454208,\"right\":-0.9994752407074},\"elevator\":{\"left\":5.7555735111237e-06,\"right\":1.8253922462463e-05},\"rudder\":{\"left\":0,\"right\":-0.00036318475031294}},\"flaps\":{\"status\":0,\"value\":0.99949878454208},\"gear\":{\"main\":{\"left\":{\"rod\":0.61779183149338},\"right\":{\"rod\":0.62874746322632}},\"nose\":{\"rod\":0.72942852973938},\"status\":0,\"value\":1},\"parachute\":{\"status\":0,\"value\":0},\"refuelingboom\":{\"status\":0,\"value\":0},\"speedbrakes\":{\"status\":0,\"value\":0},\"wheelbrakes\":{\"status\":0,\"value\":0}};Afterburner=0.00~0.00;DynamicPressure=0.000;AirDensity=1.171;Incidence=11.334~0.089~0.323;CAlpha=-0.448;CBeta=1.631;RelWind=-11.334~-0.089~-0.323;Damage=not enabled;Wind_Kts=22.04;Wind_direction=317", time);
			//n_bytes = ::sendto(sock_send, string, strlen(string), 0, reinterpret_cast<sockaddr*>(&destination), sizeof(destination));

		//Send data
			string[0] = 0;

			sprintf_s(sAppended, "T=%0.03f;N=F-16C_50;src=DCS;", time); //TODO: StringData has AcNCTR for a/c type.  Once we no longer need to fool TelemFFB
			strcat_s(string, sAppended);

			sprintf_s(sAppended, "SelfData=%0.02f~%0.02f~%0.02f;", RAD_TO_DEG(data->yaw), RAD_TO_DEG(data->pitch), RAD_TO_DEG(data->roll));
			strcat_s(string, sAppended);

			sprintf_s(sAppended, "EngRPM=%0.02f~%0.02f;", data->rpm, data2->rpm2);
			strcat_s(string, sAppended);

			sprintf_s(sAppended, "HydSys=n/a;HydPress=0.000~0.000;");
			strcat_s(string, sAppended);

			sprintf_s(sAppended, "ACCs=%0.2f~%0.2f~%0.2f;", 0.0f, data->gs, 0.0f); //TODO: X and Z accel are missing? Maybe can derive from other values (X/Y/Z, roll/pitch/yaw), but vector math is above my paygrade
			strcat_s(string, sAppended);

			sprintf_s(sAppended, "Chaff=%02.0f;Flares=%02.0f;", data->ChaffCount, data->FlareCount);
			strcat_s(string, sAppended);

			sprintf_s(sAppended, "IAS=%0.02f;IAS_kt=%0.02f;", data->kias/1.944, data->kias);
			strcat_s(string, sAppended);

			sprintf_s(sAppended, "Incidence=%0.03f~%0.03f~%0.03f;", DEG_TO_RAD(data->beta), DEG_TO_RAD(data->AOA), data->gamma);
			strcat_s(string, sAppended);

			sprintf_s(sAppended, "AOA=%0.03f;", data->alpha);
			strcat_s(string, sAppended);

			sprintf_s(sAppended, "TAS_raw=%0.03f;", FT_TO_M(data->vt));
			strcat_s(string, sAppended);

			sprintf_s(sAppended, "TAS_raw_kt=%0.03f;", FPS_TO_KT(data->vt));
			strcat_s(string, sAppended);


			//I didn't calculate these, like is done in DCS.  I just want to get something working
			sprintf_s(sAppended, "TAS=%0.03f;", FT_TO_M(data->vt));
			strcat_s(string, sAppended);
			sprintf_s(sAppended, "TAS_incidence=%0.03f;", FT_TO_M(data->vt));
			strcat_s(string, sAppended);
			sprintf_s(sAppended, "TAS_incidence_kt=%0.03f;", FPS_TO_KT(data->vt));
			strcat_s(string, sAppended);


			sprintf_s(sAppended, "altASL=%0.03f;", FT_TO_M(data->z * -1));
			strcat_s(string, sAppended);
			sprintf_s(sAppended, "altAgl=%0.03f;", FT_TO_M(data->z * -1)); //We don't have ground elev data
			strcat_s(string, sAppended);

			float AirDensity = calculateAirDensity(FT_TO_M(data->z*-1));
			float DynamicPressure = 0.5 * AirDensity * pow(FT_TO_M(data->vt),2); //kg / ms ^ 2
			
			sprintf_s(sAppended, "AirDensity=%0.03f;DynamicPressure=%0.03f;", AirDensity, DynamicPressure);
			strcat_s(string, sAppended);

			strcat_s(string, "Damage=not enabled;"); //TODO: Investigate the damage model. I know there are some variables available for it

			float fWOW = (data->lightBits3 & 0x1000) ? (1.0f) : (0.0f);
			float fMLGWow = (data->lightBits3&0x4000000) ? (1.0f) : (0.0f);
			float fNLGWow = (data->lightBits3 & 0x8000000) ? (1.0f) : (0.0f);
			sprintf_s(sAppended, "WeightOnWheels=%0.02f~%0.02f~%0.02f;", fWOW, fWOW, fWOW); //TODO: fMLGWow & fNLGWow depend on a/c power. Could compare with fWOW and give more accurate info if needed, but this should be fine
			strcat_s(string, sAppended);

			sprintf_s(sAppended, "Mach=%0.03f;", data->mach);
			strcat_s(string, sAppended);
			
			
			//TODO: Lots of this is hard coded
			sprintf_s(sAppended, "MechInfo={\"canopy\":{\"status\":0,\"value\":0},\"controlsurfaces\":{\"eleron\":{\"left\":1.0,\"right\":-1},\"elevator\":{\"left\":0.0,\"right\":0.0},\"rudder\":{\"left\":0,\"right\":0.0}},\"flaps\":{\"status\":0,\"value\":%0.01f},\"gear\":{\"main\":{\"left\":{\"rod\":1.0},\"right\":{\"rod\":1.0}},\"nose\":{\"rod\":1.0},\"status\":0,\"value\":%0.01f},\"parachute\":{\"status\":0,\"value\":0},\"refuelingboom\":{\"status\":0,\"value\":0},\"speedbrakes\":{\"status\":0,\"value\":%0.01f},\"wheelbrakes\":{\"status\":0,\"value\":0}};", data2->tefPos, data->gearPos, data->speedBrake);
			strcat_s(string, sAppended);

			//TODO: This is all hard coded for now
			sprintf_s(sAppended, "PayloadInfo=AIM - 120C - 4.4.7.106 * 1~AIM - 9X - 4.4.7.136 * 1~Mk - 82 - 4.5.9.31 * 2~fuel_tank_370gal - 1.3.43.11 * 1~ALQ - 184 - 4.15.45.142 * 1~fuel_tank_370gal - 1.3.43.11 * 1~Mk - 82 - 4.5.9.31 * 2~AIM - 9X - 4.4.7.136 * 1~AIM - 120C - 4.4.7.106 * 1~UNKNOWN_0, 0, 0, 0 - 0.0.0.0 * 0~AN / AAQ - 28 LITENING - 4.15.44.101 * 1~UNKNOWN_0, 0, 0, 0 - 0.0.0.0 * 0;");
			strcat_s(string, sAppended);
			sprintf_s(sAppended, "Afterburner=0.00~0.00;"); //How tf do they not give us afterburner data?  Will have to figure something out based on nozzle pos, fuel flow, etc
			strcat_s(string, sAppended);
		
			n_bytes = ::sendto(sock_send, string, strlen(string), 0, reinterpret_cast<sockaddr*>(&destination), sizeof(destination));


		std::cout << string << "\n\n";
		
		sprintf_s(display, "%d Bytes Sent", n_bytes);
		std::cout << display;

		Sleep(33); //30 fps
	}

	return 0;
}
