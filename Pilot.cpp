#include <iostream>
#include <vector>
#include <conio.h>

#include <WinSock2.h>
#include <ws2tcpip.h>

#include <QuickDraw.h>
#include <Timer.h>
#include "Room.h"
#include "Ship.h"
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
const int gamePort = 33303;
enum MESSAGECODES {PLAYERUPDATE, WORLDUPDATE};


class PlayerStuff
{
public:
	double xpos;
	double ypos;
	double direction;

	PlayerStuff(double x, double y, double d) : xpos(x), ypos(y), direction(d){}

};

//MOST OF DIS COPIED FROM BATTLE
void server()
{
	// Set up the game environment.
	QuickDraw window;
	View & view = (View &)window;
	Controller & controller = (Controller &)window;
	Room model(-400, 400, 100, -500);
	// Create a timer to measure the real time since the previous game cycle.
	Timer timer;
	timer.mark(); // zero the timer.
	double lasttime = timer.interval();
	double avgdeltat = 0.0;

	Ship * opponent;
	opponent = new Ship(controller, Ship::CONNECTED, "Bob");
	model.addActor(opponent);
	opponent = new Ship(controller, Ship::AUTO, "Fred");
	model.addActor(opponent);
	opponent = new Ship(controller, Ship::AUTO, "Joe");
	model.addActor(opponent);
	opponent = new Ship(controller, Ship::CONNECTED, "Nick");
	model.addActor(opponent);

	double scale = 1.0;

	// Set up the socket.
	// Argument is an IP address to send packets to. 
	int socket_d;
	socket_d = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in my_addr;	// my address information

	my_addr.sin_family = AF_INET;		 // host byte order
	my_addr.sin_port = htons(gamePort); // short, network byte order
	my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

	if (bind(socket_d, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1)
	{
		cerr << "Bind error: " << WSAGetLastError() << "\n";
		return;
	}

	u_long iMode = 1;
	ioctlsocket(socket_d, FIONBIO, &iMode); // put the socket into non-blocking mode.

	// A list of the IP addresses of all clients. Clients must all be on separate machines because a single port is
	// used. This could be fixed (how?)
	struct ClientAddr
	{
		int ip;
		int port;

		ClientAddr(int IP, int PORT) : ip(IP), port(PORT) {}
	};
	vector <ClientAddr> clients;

	while (true)
	{
		int n;

		// Calculate the time since the last iteration.
		double currtime = timer.interval();
		double deltat = currtime - lasttime;

		// Run a smoothing step on the time change, to overcome some of the
		// limitations with timer accuracy.
		avgdeltat = 0.2 * deltat + 0.8 * avgdeltat;
		deltat = avgdeltat;
		lasttime = lasttime + deltat;

		// Allow the environment to update.
		model.update(deltat);

		view.clearScreen();
		double offsetx = 0.0;
		double offsety = 0.0;
		//(*ship).getPosition(offsetx, offsety);
		model.display(view, offsetx, offsety, scale);
		std::ostringstream score;
		//score << "Score: " << ship->getScore();
		view.drawText(20, 20, score.str());
		view.swapBuffer();


		struct sockaddr_in their_addr; // connector's address information
		int addr_len = sizeof(struct sockaddr);
		char buf[50000];
		int numbytes = 50000;

		// Receive requests from clients.
		if ((n = recvfrom(socket_d, buf, numbytes, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK) // A real problem - not just avoiding blocking.
			{
				cerr << "Recv error: " << WSAGetLastError() << "\n";
			}
		}
		else
		{
			//		cout << "Received: " << n << "\n";
			// Add any new clients provided they have not been added previously.
			bool found = false;
			for (unsigned int i = 0; i < clients.size(); i++)
			{
				if ((clients[i].ip == their_addr.sin_addr.s_addr) && (clients[i].port == their_addr.sin_port))
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				clients.push_back(ClientAddr(their_addr.sin_addr.s_addr, their_addr.sin_port));
				char their_source_addr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(their_addr.sin_addr), their_source_addr, sizeof(their_source_addr));
				cout << "Adding client: " << their_source_addr << " " << ntohs(their_addr.sin_port) << "\n";
			}

			// Process any player to server messages.
			switch (*(int *)buf) // First int of each packet is the message code.
			{
			case PLAYERUPDATE:
			{
				PlayerStuff * ps = (PlayerStuff *)buf;
				model.updatePlayerStuff(ps->direction, ps->ypos, ps->direction);
				cout << "Doing something: " << endl;
			}
				break;
			default:
			{
				//				cout << "Unknown message code: " << * (int *) buf << "\n";
			}
				break;
			}
		}
		// Server to player - send environment updates. Very clunky - try to do better.
		//int modelsize;
		//char * modeldata = model.serialize(WORLDUPDATE, modelsize);

		for (unsigned int i = 0; i < clients.size(); i++)
		{
			// Send environment updates to clients.
			int addr = clients[i].ip;
			int port = clients[i].port;
			int modelsize;
			char * modeldata = model.serialize(WORLDUPDATE, modelsize);
			struct sockaddr_in dest_addr;
			dest_addr.sin_family = AF_INET;
			dest_addr.sin_addr.s_addr = addr;
			dest_addr.sin_port = port;
			do
			{
				n = sendto(socket_d, modeldata, modelsize, 0, (const sockaddr *)&(dest_addr), sizeof(dest_addr));
				if (n <= 0)
				{
					cout << "Send failed: " << n << "\n";
				}
			} while (n <= 0); // retransmit if send fails. This server is capable of producing data faster than the machine can send it.
			//			cout << "Sending world: " << n << " " << modelsize << " to " << hex << addr << dec << "\n";
			delete[] modeldata;
		}

		cout << "Server running: " << deltat << "             \r";

	}
}

//ALSO COPIED FROM BATTLE
void client(char * serverIP, char * clientID)
{
	QuickDraw window;
	View & view = (View &)window;
	Controller & controller = (Controller &)window;

	Room model(-400, 400, 100, -500);

	Ship * ship = new Ship (controller, Ship::INPLAY, "You");
	model.addActor (ship);

	// Add some opponents. These are computer controlled - for the moment...
	Ship * opponent;
	opponent = new Ship(controller, Ship::AUTO, "Bob");
	model.addActor(opponent);
	opponent = new Ship(controller, Ship::AUTO, "Fred");
	model.addActor(opponent);
	opponent = new Ship(controller, Ship::AUTO, "Joe");
	model.addActor(opponent);
	//opponent = new Ship(controller, Ship::AUTO, "Nick");
	//model.addActor(opponent);

	double scale = 1.0;

	int player = atoi(clientID);

	// Set up the socket.
	// Argument is an IP address to send packets to. Multicast allowed.
	int socket_d;
	socket_d = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in my_addr;	// my address information

	my_addr.sin_family = AF_INET;		 // host byte order
	my_addr.sin_port = htons(gamePort + 1 + player); // short, network byte order
	my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

	if (bind(socket_d, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1)
	{
		cerr << "Bind error: " << WSAGetLastError() << "\n";
		return;
	}

	u_long iMode = 1;
	ioctlsocket(socket_d, FIONBIO, &iMode); // put the socket into non-blocking mode.

	struct sockaddr_in dest_addr;
	dest_addr.sin_family = AF_INET;
	inet_pton(AF_INET, serverIP, &dest_addr.sin_addr.s_addr);
	dest_addr.sin_port = htons(gamePort);

	while (true)
	{
		//Check for events from the player.
		//STUFF LIKE BUTTON INPUTSSSSS
		/* FOR EXAMPLE
		if ((mb == WM_RBUTTONDOWN) && (startset))
		{
			model.addLink(player, startx, starty, x, y);
			AddLink al(player, startx, starty, x, y);
			sendto(socket_d, (const char *)&al, sizeof(al), 0, (const sockaddr *)&(dest_addr), sizeof(dest_addr));

			startset = false;
		}
		*/
		
		//NEED TO SEND SHIT TO THE SERVER

		double x = 300.0;
		double y = 40.0;
		double d = 1.0;
		PlayerStuff ps(x, y, d);
		sendto(socket_d, (const char *) &ps, sizeof(ps), 0,(const sockaddr *)&(dest_addr), sizeof(dest_addr));

		//Check for updates from the server.
		struct sockaddr_in their_addr; // connector's address information
		int addr_len = sizeof(struct sockaddr);
		char buf[50000];
		int numbytes = 50000;
		int n;

		// Receive requests from server.
		if ((n = recvfrom(socket_d, buf, numbytes, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK) // A real problem - not just avoiding blocking.
			{
				cerr << "Recv error: " << WSAGetLastError() << "\n";
			}
		}
		else
		{
			cout << "Received world dump: " << n << "\n";
			if (*(int *)buf == WORLDUPDATE)
			{
				model.deserialize(buf, n);
			}
		}

		
		view.clearScreen();
		double offsetx = 0.0;
		double offsety = 0.0;
		//(*ship).getPosition(offsetx, offsety);
		model.display(view, offsetx, offsety, scale);
		std::ostringstream score;
		//score << "Score: " << ship->getScore();
		view.drawText(20, 20, score.str());
		view.swapBuffer();
	}
}

char * getipv4(int, char **)
{
	char ac[80];
	if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
		cerr << "Error " << WSAGetLastError() <<
			" getting local host name." << endl;
	}

	struct hostent *phe = gethostbyname(ac);
	if (phe == 0) {
		cerr << "Bad host lookup." << endl;
	}

	char * address;

	for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
		struct in_addr addr;
		memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
		address = inet_ntoa(addr);
	}

	return address;
}

// i kinda made dis
int main(int argc, char * argv [])
{
	cout << "Start:\n1 - Server\n2 - Client\n";
	string input;
	cin >> input;
	// Messy process with windows networking - "start" the networking API.
	WSADATA wsaData;
	//int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
		return 255;
	}

	char * lipaddr = getipv4(argc, argv);

	if (input == "2")
	{
		// client. First argumentL IP address of server, second argument: player number (0, 1, 2, ...)
		client(lipaddr, "101");
	}
	else if (input == "1")
	{
		// server.
		server();
	}

	WSACleanup();
	return 0;
}

