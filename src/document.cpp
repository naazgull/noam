#include <filesystem>
#include <noam/document.h>
#include <zapata/http/retrieve.h>
#include <zapata/uri.h>

auto noam::load_document(std::string const& _location) -> std::string {
    static std::map<std::string, std::string> ___supported_blocks{
        { ".json", "json" }, { ".cpp", "c++" }, { ".c", "c++" },
        { ".h", "c++" },     { ".hpp", "c++" }, { ".lua", "lua" },
        { ".py", "python" }, { ".rs", "rust" }, { ".diff", "diff" }
    };

    auto _normalized = _location;
    if (_normalized.find("file:") != 0 && _normalized.find("http:") != 0 &&
        _normalized.find("https:") != 0) {
        if (_normalized[0] == '/' || _normalized[0] == '.') {
            _normalized = std::string{ "file:" } + _normalized;
        }
        else { _normalized = std::string{ "file:./" } + _normalized; }
    }

    auto _uri = zpt::uri::parse(_normalized);
    std::filesystem::path _path{ _uri("raw_path")->string() };

    std::string _content;
    if (_uri("scheme") == "file") {
        auto _size = std::filesystem::file_size(_path);
        _content.resize(_size);
        std::ifstream _ifs{ _path.string() };
        _ifs.read(&_content[0], _size);
    }
    else if (_uri("scheme") == "http" || _uri("scheme") == "https") {
        try {
            auto _request = zpt::allocate_message<zpt::http::basic_request>();
            _request //
              ->performative(zpt::Get)
              .uri(_normalized);

            auto _reply = zpt::http::retrieve(_request);
            if (_reply != nullptr) {
                if (_reply->status() == 200) {
                    _content = static_cast<std::string>(_reply->body());
                }
                else if (_reply->status() >= 300 && _reply->status() < 400) {
                    return std::format(
                      "\n**server replied with an HTTP status of `{}`. noam doesn't "
                      "follow any type of redirection.**",
                      _reply->status());
                }
                else if (_reply->status() >= 400) {
                    std::ostringstream _oss;
                    _oss << _request << "\n\n" << _reply << std::flush;
                    return std::format(
                      "\n**unable to fetch content from `{}`, server replied with an HTTP "
                      "status of `{}`:**\n```\n{}\n```\n",
                      _location,
                      _reply->status(),
                      _oss.str());
                }
            }
        }
        catch (std::exception const& _e) {
            return std::format("\n**unable to resolve `{}`**", _normalized);
        }
    }
    else { return "\nnoam only retrieves local or HTTP served files"; }

    std::map<std::string, std::string>::iterator _found;
    if (_path.extension() == "" || _path.extension() == ".md") { return _content; }
    else if ((_found = ___supported_blocks.find(_path.extension())) != ___supported_blocks.end()) {
        return std::format("```{}\n{}\n```", _found->second, _content);
    }
    else { return std::format("```\n{}\n```", _content); }
}
