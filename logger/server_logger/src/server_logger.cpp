#include "../include/server_logger.h"
#include <sys/stat.h>
#include <fstream>
#include <chrono>
#include <cstring>  // Для работы с strncpy

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

server_logger::server_logger(
        const std::string& dest,
        const std::unordered_map<logger::severity, std::pair<std::string, bool>>& streams)
        : _destination(dest), _streams(streams)
{
#ifndef _WIN32
    struct stat buffer;
    if (stat(dest.c_str(), &buffer) != 0) {
        throw std::runtime_error("Server socket not available");
    }
#endif
}

server_logger::~server_logger() noexcept = default;

logger& server_logger::log(const std::string& message, logger::severity severity) &
{
    auto it = _streams.find(severity);
    if (it == _streams.end() || !it->second.second) {
        return *this;
    }

    std::ostringstream log_stream;
    log_stream << "[" << current_datetime_to_string() << "] "
               << "[" << severity_to_string(severity) << "] "
               << "[PID:" << inner_getpid() << "] "
               << message << "\n";

    const std::string log_message = log_stream.str();

#ifdef _WIN32
    HANDLE pipe = CreateFileA(
            _destination.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

    if (pipe == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to connect to named pipe");
    }

    DWORD bytes_written;
    BOOL write_result = WriteFile(
            pipe,
            log_message.c_str(),
            static_cast<DWORD>(log_message.size()),
            &bytes_written,
            nullptr);

    if (!write_result) {
        CloseHandle(pipe);
        throw std::runtime_error("Failed to write to named pipe");
    }

    CloseHandle(pipe);
#else
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Socket creation failed");
    }

    struct sockaddr_un server_addr{};
    server_addr.sun_family = AF_UNIX;
    // Используем безопасное копирование пути
    strncpy(server_addr.sun_path, _destination.c_str(), sizeof(server_addr.sun_path) - 1);

    // Пробуем подключиться к серверу
    if (connect(sockfd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        close(sockfd);
        throw std::runtime_error("Connection to server failed");
    }

    // Отправляем лог-сообщение
    ssize_t bytes_written = write(sockfd, log_message.c_str(), log_message.size());
    if (bytes_written < 0) {
        close(sockfd);
        throw std::runtime_error("Failed to write to socket");
    }

    // Закрываем сокет
    close(sockfd);
#endif

    return *this;
}

server_logger::server_logger(server_logger&& other) noexcept
        : _destination(std::move(other._destination)),
          _streams(std::move(other._streams))
{
}

server_logger& server_logger::operator=(server_logger&& other) noexcept
{
    if (this != &other) {
        _destination = std::move(other._destination);
        _streams = std::move(other._streams);
    }
    return *this;
}

int server_logger::inner_getpid()
{
#ifdef _WIN32
    return ::_getpid();
#else
    return ::getpid();
#endif
}
