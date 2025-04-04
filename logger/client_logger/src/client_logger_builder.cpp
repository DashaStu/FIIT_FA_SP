#include <filesystem>
#include <utility>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include "../include/client_logger_builder.h"

using namespace nlohmann;

logger_builder& client_logger_builder::add_file_stream(
        std::string const &stream_file_path,
        logger::severity severity) &
{
    const std::filesystem::path absolute_path = std::filesystem::absolute(stream_file_path);
    auto it = _output_streams.find(severity);
    if(it != _output_streams.end()){
        bool flag = true;
        for (auto &ref_stream: it->second.first){
            if(ref_stream._stream.first == absolute_path.string()){
                flag = false;
                break;
            }
        }
        if(flag){
            it->second.first.emplace_front(absolute_path.string());
        }
    }
    else{
        _output_streams[severity] = std::make_pair(std::forward_list<client_logger::refcounted_stream>(), false);
        _output_streams[severity].first.emplace_front(absolute_path.string());
    }
    return *this;
}

logger_builder& client_logger_builder::add_console_stream(
        logger::severity severity) &
{
    auto it = _output_streams.find(severity);
    if(it != _output_streams.end()){
        it->second.second = true;
    }
    else{
        _output_streams[severity] = std::make_pair(std::forward_list<client_logger::refcounted_stream>(), true);
    }
    return *this;
}

logger_builder& client_logger_builder::transform_with_configuration(
        std::string const &configuration_file_path,
        std::string const &configuration_path) &
{
    json js;
    std::ifstream stream(configuration_file_path);
    if (stream.is_open())
    {
        json::parser_callback_t callback = [&configuration_path](int depth,
                                                                 json::parse_event_t event, json &parsed) {
            if (event == json::parse_event_t::key && depth == 1 && parsed != json(configuration_path))
            {
                return false;
            }
            return true;
        };

        js = json::parse(stream, callback);
        stream.close();
        if (js.contains(configuration_path) == false)
        {
            return *this;
        }

        js = js[configuration_path];
        if (js.contains("format"))
        {
            set_format(js["format"]);
        }
        if (js.contains("console_stream"))
        {
            for (auto &[key, value]: js["console_stream"].items())
            {
                try
                {
                    add_console_stream(string_to_severity(value.get<std::string>()));
                }
                catch (std::out_of_range const &e) {}
            }
        }
        if (js.find("file_streams") != js.end())
        {
            for (auto &[path, sev]: js["file_streams"].items())
            {
                try
                {
                    add_file_stream(path, string_to_severity(sev.get<std::string>()));
                }
                catch (std::out_of_range const &e) {}
            }
        }
    }
    return *this;
}

logger_builder& client_logger_builder::clear() &
{
    _output_streams.clear();
    _format = "%m";
    return *this;
}

logger *client_logger_builder::build() const
{
    return new client_logger(_output_streams, _format);
}

logger_builder& client_logger_builder::set_format(const std::string &format) &
{
    _format = format;
    return *this;
}

void client_logger_builder::parse_severity(logger::severity sev, nlohmann::json& j){}

logger_builder& client_logger_builder::set_destination(const std::string &format) &
{
    return set_format(format);
}
