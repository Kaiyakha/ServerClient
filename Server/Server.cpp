#include "Handler.h"
#include "Rnd.h"
#include <map>
#include <vector>

#define ARGC_REQUIRED 4

static const std::map<const std::string, std::string>
parse_str(const std::string& str, const std::vector<std::string>& keys, const std::string& delim);

int main(const int argc, const char* argv[]) {
	// Read argv
	if (argc != ARGC_REQUIRED) {
		std::cout << "Invalid argument count!" << std::endl;
		ExitProcess(EXIT_FAILURE);
	}
	const char* server_IP = argv[1];
	const USHORT TCP_port = static_cast<USHORT>(std::stoul(argv[2]));
	std::string filedir = argv[3]; if (filedir.back() != '\\') filedir += '\\';
		
	// Load ws2_32.dll
	const WORD WinSockDLLVersion = MAKEWORD(2, 2);
	WSADATA wsa;
	HANDLE_DLL_LOAD_ERROR(WSAStartup(WinSockDLLVersion, &wsa), "Failed to load ws2_32.dll.");

	// Define sockets, address structures and a buffer
	SOCKET tcp_server_socket, tcp_client_socket, udp_server_socket;
	SOCKADDR_IN tcp_server = {0}, tcp_client = {0}, udp_server = {0}, udp_client = {0};
	int tcp_client_size = sizeof tcp_client, udp_client_size = sizeof udp_client;
	char buffer[BUFSIZE];

	// Open a TCP socket
	tcp_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	HANDLE_INVALID_SOCKET(tcp_server_socket, "Failed to open a TCP socket.");

	// Bind the TCP socket to the server
	tcp_server.sin_family = AF_INET;
	tcp_server.sin_addr.s_addr = inet_addr(server_IP);
	tcp_server.sin_port = htons(TCP_port);
	HANDLE_SOCKET_ERROR(bind(tcp_server_socket, (const sockaddr*)(&tcp_server), sizeof tcp_server), "Failed to bind a socket.");

	// Create a directory
	RESET(buffer);
	sprintf_s(buffer, "mkdir %s", filedir.c_str());
	system(buffer);

	// Optional: get a RNG for delay simulation
	// Rnd<DWORD> rng(1000);

	// Start listening via TCP
	HANDLE_SOCKET_ERROR(listen(tcp_server_socket, 1), "Failed to listen.");

	bool listening = true;
	while (listening) {
		std::cout << "\nWaiting for incoming connections...\n\n";

		// Accept a client socket
		tcp_client_socket = accept(tcp_server_socket, (SOCKADDR*)&tcp_client, &tcp_client_size);
		HANDLE_INVALID_SOCKET(tcp_client_socket, "Failed to accept a valid client socket.");
		std::cout << "IP " << inet_ntoa(tcp_client.sin_addr) << " has connected to the server.\n";

		// Confirm connection
		RESET(buffer);
		strcpy_s(buffer, "Connected successfully!");
		HANDLE_SOCKET_ERROR(
			send(tcp_client_socket, buffer, static_cast<int>(strlen(buffer)), 0),
			"Failed to send a connection confirmation."
		);

		// Receive a UDP port number, a filename and the number of packets within one buffer
		RESET(buffer);
		HANDLE_SOCKET_ERROR(recv(tcp_client_socket, buffer, BUFSIZE, 0), "Failed to receive a parameter list.");
		
		// Parse all the parameters
		std::vector<std::string> keys = { "UDP_port", "filename", "num_packets" };
		std::map<const std::string, std::string> params = parse_str(buffer, keys, std::string(1, SEP));
		std::cout << "UDP_port to receive datagrams: " << params["UDP_port"];
		std::cout << "\nThe name of a file to receive: " << params["filename"] << "\n\n";
		
		// Open a UDP socket
		udp_server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		HANDLE_INVALID_SOCKET(udp_server_socket, "Failed to open a UDP socket.");

		// Bind the UDP socket to the server
		udp_server = tcp_server;
		udp_server.sin_port = htons(static_cast<USHORT>(std::stoul(params["UDP_port"])));
		HANDLE_SOCKET_ERROR(bind(udp_server_socket, (const sockaddr*)&udp_server, sizeof udp_server), "Failed to bind a socket.");

		// Get the number of packets to receive
		int32_t packets_num = std::stoi(params["num_packets"]);
		std::unique_ptr<packet_t[]> packets = std::make_unique<packet_t[]>(packets_num);
		
		// Loop receiving UDP data packets from the client
		std::map<const std::string, std::string> packet_map;
		keys = { "id" };
		int32_t cur_id = -1, next_id;
		do {
			// Receive a UDP packet from the client
			RESET(buffer);
			HANDLE_SOCKET_ERROR(
				recvfrom(udp_server_socket, buffer, BUFSIZE, 0, (SOCKADDR*)&udp_client, &udp_client_size), \
				"Failed to receive a UDP packet."
			);
			// Optional: simulate delay
			// Sleep(rng());
			// Extract id from the packet, receive the packet again if the id has not increased
			packet_map = parse_str(buffer, keys, std::string(1, SEP));
			next_id = std::stoi(packet_map["id"]);
			if (cur_id == next_id) continue;
			else cur_id = next_id;
			// Copy packet's data from the buffer
			memcpy(packets[cur_id], &buffer[packet_map["id"].size() + sizeof SEP], sizeof packet_t);
			// Confirm packet receipt
			RESET(buffer);
			strcpy_s(buffer, "packet received");
			HANDLE_SOCKET_ERROR(send(tcp_client_socket, buffer, static_cast<int>(strlen(buffer)), 0), "Failed to confirm packet receipt.");
			// Ask whether there are more packets to receive
			RESET(buffer);
			HANDLE_SOCKET_ERROR(recv(tcp_client_socket, buffer, BUFSIZE, 0), "Failed to receive confirmation for more packets to receive.");
			std::cout << '\r' << cur_id + 1 << '/' << packets_num << " packets received...          ";
		} while (strcmp(buffer, "no more packets to receive"));

		// Write data to a file
		const std::string filepath = filedir + params["filename"];
		std::ofstream file(filepath, std::ios::binary);
		if (!file.is_open()) {
			std::cout << "Failed to open a file." << std::endl;
			ExitProcess(EXIT_FAILURE);
		}
		for (int32_t id = 0; id < packets_num; id++) file.write(packets[id], sizeof packet_t);

		file.close();

		std::cout << "\nA new file has been received.\n";

		closesocket(tcp_client_socket);
		closesocket(udp_server_socket);

		// if ... listening = false;
	}

	closesocket(tcp_server_socket);
	WSACleanup();
}


static const std::map<const std::string, std::string>
parse_str(const std::string& str, const std::vector<std::string>& keys, const std::string& delim) {
	std::map<const std::string, std::string> parsed;
	std::string::size_type substr_front = 0;
	std::string::size_type substr_count = str.find(delim);
	size_t key_id = 0;

	std::string key, value;
	while ((substr_count + substr_front <= str.size()) && key_id < keys.size() - 1) {
		if (substr_count) {
			key = keys[key_id]; key_id++;
			value = str.substr(substr_front, substr_count);
			parsed[key] = value;
		}
		substr_front += substr_count + delim.size();
		substr_count = str.find(delim, substr_front) - substr_front;
	}

	key = keys[key_id];
	if (substr_count == std::string::npos) substr_count = str.size() - substr_front;
	value = str.substr(substr_front, substr_count);
	parsed[key] = value;

	return parsed;
}