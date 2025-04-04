#include <string>
#include <sstream>
#include <algorithm>
#include <utility>
#include "../include/client_logger.h"
#include <not_implemented.h>

std::unordered_map<std::string, std::pair<size_t, std::ofstream>> client_logger::refcounted_stream::_global_streams;


logger& client_logger::log(
        const std::string &text,
        logger::severity severity) &
{
    auto it = _output_streams.find(severity);
    if( it != _output_streams.end()){
        const std::string form_msg(make_format(text, severity));
        const auto &streams = it->second.first;
        for (const auto &stream : streams){
            if(stream._stream.second && stream._stream.second->is_open()){
                *stream._stream.second << form_msg << std::endl;
            }
        }
        if (it->second.second){
            std::cout << form_msg << std::endl;
        }
    }
    return *this;
}

std::string client_logger::make_format(const std::string &message, severity sev) const
{
    std::stringstream formatted_message;

    for (size_t i = 0; i < _format.length(); ++i)
    {
        if (_format[i] == '%' && i + 1 < _format.length())
        {
            switch (char_to_flag(_format[i + 1]))
            {
                case flag::DATE:
                    formatted_message << current_date_to_string();
                    break;
                case flag::TIME:
                    formatted_message << current_time_to_string();
                    break;
                case flag::SEVERITY:
                    formatted_message << severity_to_string(sev);
                    break;
                case flag::MESSAGE:
                    formatted_message << message;
                    break;
                default:
                    formatted_message << '%' << _format[i + 1];
            }
            ++i;
        }
        else
        {
            formatted_message << _format[i];
        }
    }
    return formatted_message.str();
}

client_logger::client_logger(
        const std::unordered_map<logger::severity, std::pair<std::forward_list<refcounted_stream>, bool>> &streams,
        std::string format) : _output_streams(streams), _format(format) {
}

client_logger::flag client_logger::char_to_flag(char c) noexcept
{
    switch (c)
    {
        case 'd': return flag::DATE;
        case 't': return flag::TIME;
        case 's': return flag::SEVERITY;
        case 'm': return flag::MESSAGE;
        default: return flag::NO_FLAG;
    }
}

client_logger::client_logger(const client_logger &other): _output_streams(other._output_streams), _format(other._format) {
}

client_logger &client_logger::operator=(const client_logger &other)
{
    if(&other != this){
        _output_streams = other._output_streams;
        _format = other._format;
    }
    return *this;
}

client_logger::client_logger(client_logger &&other) noexcept : _output_streams(std::move(other._output_streams)), _format(std::move(other._format))
{
    other._output_streams.clear();
    other._format.clear();
}

client_logger &client_logger::operator=(client_logger &&other) noexcept
{
    if(&other != this){
        _output_streams = std::move(other._output_streams);
        _format = std::move(other._format);
        other._output_streams.clear();
        other._format.clear();
    }
    return *this;

}

client_logger::~client_logger() noexcept {
}

client_logger::refcounted_stream::refcounted_stream(const std::string &path) : _stream(path, nullptr)
{
    auto it = _global_streams.find(path);
    if (it != _global_streams.end()){
        it->second.first++;
        _stream.second = &it->second.second;
    }
    else{
        std::ofstream stream(path);
        _global_streams[path] = std::make_pair(1, std::move(stream));
        _stream.second = &_global_streams[path].second;
    }
}

client_logger::refcounted_stream::refcounted_stream(const client_logger::refcounted_stream &oth): _stream(oth._stream)
{
    auto it = _global_streams.find(_stream.first);
    if (it != _global_streams.end())
    {
        it->second.first++;
    }
    else
    {
        std::ofstream stream(_stream.first);
        _global_streams[_stream.first] = std::make_pair(1, std::move(stream));
        _stream.second = &_global_streams[_stream.first].second;
    }
}

void client_logger::refcounted_stream::open()
{
    if (_stream.second == nullptr)
    {
        auto it = _global_streams.find(_stream.first);
        if (it != _global_streams.end())
        {
            it->second.first++;
            _stream.second = &it->second.second;
        }
        else
        {
            std::ofstream stream(_stream.first);
            _global_streams[_stream.first] = std::make_pair(1, std::move(stream));
            _stream.second = &_global_streams[_stream.first].second;
        }
    }
}


client_logger::refcounted_stream &
client_logger::refcounted_stream::operator=(const client_logger::refcounted_stream &oth)
{
    if (&oth != this)
    {
        auto old_it = _global_streams.find(_stream.first);
        if (old_it != _global_streams.end())
        {
            old_it->second.first--;
            if (old_it->second.first == 0)
            {
                _global_streams.erase(old_it);
            }
        }

        _stream = oth._stream;

        auto it = _global_streams.find(_stream.first);
        if (it != _global_streams.end())
        {
            it->second.first++;
        }
        else
        {
            std::ofstream stream(_stream.first);
            _global_streams[_stream.first] = std::make_pair(1, std::move(stream));
            _stream.second = &_global_streams[_stream.first].second;
        }
    }
    return *this;
}

client_logger::refcounted_stream::refcounted_stream(client_logger::refcounted_stream &&oth) noexcept: _stream(
        std::move(oth._stream)) {}

client_logger::refcounted_stream &client_logger::refcounted_stream::operator=(
        client_logger::refcounted_stream &&oth) noexcept
{
    if (&oth != this)
    {
        _stream = std::move(oth._stream);
    }
    return *this;
}

client_logger::refcounted_stream::~refcounted_stream()
{
    auto it = _global_streams.find(_stream.first);
    if (it != _global_streams.end())
    {
        it->second.first--;
        if (it->second.first == 0)
        {
            _global_streams.erase(it);
        }
    }
}