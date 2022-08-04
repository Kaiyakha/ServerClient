#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <iostream>
#include <fstream>
#include <string>

template <typename T>
static void handle_error(T returned_val, T err_val, const std::string& message) {
	if (returned_val == err_val) {
		std::cout << message << " Error code: " << WSAGetLastError() << std::endl;
		ExitProcess(EXIT_FAILURE);
	}
}
#define HANDLE_DLL_LOAD_ERROR(returned_val, message) handle_error<decltype(returned_val)>(returned_val, !0, message)
#define HANDLE_SOCKET_ERROR(returned_val, message) handle_error<decltype(SOCKET_ERROR)>(returned_val, SOCKET_ERROR, message)
#define HANDLE_INVALID_SOCKET(returned_val, message) handle_error<decltype(INVALID_SOCKET)>(returned_val, INVALID_SOCKET, message)

#define BUFSIZE 1024
#define SEP '|'
#define RESET(buffer) memset(buffer, '\0', sizeof buffer);

typedef char packet_t[BUFSIZE - sizeof int32_t - sizeof SEP];