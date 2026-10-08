#include <filesystem>
#include <noam/document.h>
#include <zapata/http/retrieve.h>
#include <zapata/uri.h>

auto noam::load_document(std::string const& _location) -> std::string {
    auto _normalized = _location;
    if (_normalized.find("file:") != 0 && _normalized.find("http:") != 0 &&
        _normalized.find("https:") != 0) {
        if (_normalized[0] == '/' || _normalized[0] == '.') {
            _normalized = std::string{ "file:" } + _normalized;
        }
        else { _normalized = std::string{ "file:./" } + _normalized; }
    }

    auto _uri = zpt::uri::parse(_normalized);
    if (_uri("scheme") == "file") {
        std::filesystem::path _path{ _uri("raw_path")->string() };
        auto _size = std::filesystem::file_size(_path);
        std::string _content(_size, '\0');
        std::ifstream _ifs{ _path.string() };
        _ifs.read(&_content[0], _size);
        return _content;
    }
    else if (_uri("scheme") == "http" || _uri("scheme") == "https") {
        try {
            auto _request = zpt::allocate_message<zpt::http::basic_request>();
            _request //
              ->performative(zpt::Get)
              .uri(_normalized);

            auto _reply = zpt::http::retrieve(_request);
            if (_reply != nullptr) {
                if (_reply->status() == 200) { return static_cast<std::string>(_reply->body()); }
                else if (_reply->status() >= 300 && _reply->status() < 400) {
                    return std::format(
                      "\n**server replied with an HTTP status of `{}`. noam doesn't "
                      "follow any type of redirection.**",
                      _reply->status());
                }
                else if (_reply->status() >= 400) {
                    return std::format(
                      "\n**unable to fetch content, server replied with an HTTP status of `{}`.**",
                      _reply->status());
                }
            }
        }
        catch (std::exception const& _e) {
        }
        return std::format("\n**unable to resolve `{}`**", _normalized);
    }
    return "\nnoam only retrieves local or HTTP served files";
}
