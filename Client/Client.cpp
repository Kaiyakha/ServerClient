#include "Handler.h"

#define ARGC_REQUIRED 6
#define MAX_FILE_SIZE 1024 * 1024 * 10

int main(const int argc, const char* argv[]) {
	// Read argv
	if (argc != ARGC_REQUIRED) {
		std::cout << "Invalid argument count!" << std::endl;
		ExitProcess(EXIT_FAILURE);
	}
	const char* server_IP = argv[1];
	const USHORT TCP_server_port = static_cast<USHORT>(std::stoul(argv[2]));
	const USHORT UDP_server_port = static_cast<USHORT>(std::stoul(argv[3]));;
	const std::string filepath = argv[4];
	const int timeout = std::stoi(argv[5]);

	// Load ws2_32.dll
	const WORD WinSockDLLVersion = MAKEWORD(2, 2);
	WSADATA wsa;
	HANDLE_DLL_LOAD_ERROR(WSAStartup(WinSockDLLVersion, &wsa), "Failed to load ws2_32.dll.");

	// Define sockets, address structures and a buffer
	SOCKET tcp_client_socket, udp_client_socket;
	SOCKADDR_IN tcp_server, udp_server;
	char buffer[BUFSIZE];

	// Open a TCP socket
	tcp_client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	HANDLE_INVALID_SOCKET(tcp_client_socket, "Failed to create a TCP socket.");

	// Establish connection to a server
	tcp_server.sin_family = AF_INET;
	tcp_server.sin_addr.s_addr = inet_addr(server_IP);	
	tcp_server.sin_port = htons(TCP_server_port);
	HANDLE_SOCKET_ERROR(
		connect(tcp_client_socket, (const sockaddr*)&tcp_server, sizeof tcp_server),
		"Failed to connect to a server."
	);

	// Receive connection confirmation
	RESET(buffer);
	HANDLE_SOCKET_ERROR(
		recv(tcp_client_socket, buffer, BUFSIZE, 0),
		"Failed to receive a successful connection confirmation."
	);
	std::cout << '\n' << buffer << '\n';

	// Open a file to read, check its size and the number of packets
	std::ifstream file(filepath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		std::cout << "Failed to open a file." << std::endl;
		ExitProcess(EXIT_FAILURE);
	}
	const std::streampos filesize = file.tellg(); file.seekg(std::ios::beg);
	if (filesize > MAX_FILE_SIZE) {
		std::cout << "The size of the file to send exceeds the maximum file size allowed (10 MB)." << std::endl;
		ExitProcess(EXIT_FAILURE);
	}
	const int32_t packets_num = static_cast<decltype(packets_num)>(ceil(static_cast<double>(filesize) / sizeof packet_t));

	// Split the file into packets
	std::unique_ptr<packet_t[]> packets = std::make_unique<packet_t[]>(packets_num);
	RESET(packets[packets_num - 1]);
	for (int32_t id = 0; id < packets_num; id++) file.read(packets[id], sizeof packet_t);
	file.close();

	// Get the name of the file
	const size_t delim_pos = filepath.rfind('/');
	const std::string filename = filepath.substr(delim_pos + 1);

	// Send the parameters within one buffer
	RESET(buffer);
	sprintf_s(buffer, "%hu%c%s%c%d", UDP_server_port, SEP, filename.c_str(), SEP, packets_num);
	HANDLE_SOCKET_ERROR(
		send(tcp_client_socket, buffer, static_cast<int>(strlen(buffer)), 0),
		"Failed to send the parameters."
	);

	// Open a UDP socket
	udp_client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	HANDLE_INVALID_SOCKET(udp_client_socket, "Failed to open a UDP socket.");

	// Define UDP server info
	udp_server = tcp_server;
	udp_server.sin_port = htons(UDP_server_port);

	// Set a timeout
	HANDLE_SOCKET_ERROR(
		setsockopt(tcp_client_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, static_cast<int>(sizeof timeout)),
		"Failed to set a timeout."
	);

	// Loop sending file
	int recv_error;
	for (int32_t id = 0; id < packets_num; id++) {
		// Put an id and data within one buffer
		RESET(buffer);
		sprintf_s(buffer, "%d%c\0", id, SEP);
		memcpy(&buffer[strlen(buffer)], packets[id], sizeof packet_t);
		// Send a UDP Packet
		HANDLE_SOCKET_ERROR(
			sendto(udp_client_socket, buffer, BUFSIZE, 0, (const SOCKADDR*)&udp_server, sizeof udp_server),
			"Failed to send a packet."
		);
		// Receive confirmation from the server, process timeout
		RESET(buffer);
		if (recv(tcp_client_socket, buffer, BUFSIZE, 0) == SOCKET_ERROR) {
			recv_error = WSAGetLastError();
			if (recv_error == WSAETIMEDOUT) {
				id--;
				continue;
			}
			else {
				std::cout << "Failed to receive packet receipt confirmation. Error code: " << recv_error << std::endl;
				ExitProcess(EXIT_FAILURE);
			}
		}
		// Inform the server if there are more packets to receive
		RESET(buffer);
		strcpy_s(buffer, (id < packets_num - 1) ? "there are more packets to receive" : "no more packets to receive");
		HANDLE_SOCKET_ERROR(
			send(tcp_client_socket, buffer, static_cast<int>(strlen(buffer)), 0),
			"Failed to confirm if there are more packets to receive."
		);
		std::cout << '\r' << id + 1 << '/' << packets_num << " packets sent...          ";
	}

	std::cout << "\nThe file has been successfully uploaded.\n";

	closesocket(udp_client_socket);
	closesocket(tcp_client_socket);

	WSACleanup();
}