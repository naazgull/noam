#include <filesystem>
#include <noam/document.h>
#include <zapata/http/retrieve.h>
#include <zapata/uri.h>

auto noam::load_document(std::string const& _location) -> std::string {
    auto _normalized = _location;
    if (_normalized.find("file:") != 0 && _normalized.find("http:") != 00 &&
        _normalized.find("https:") != 0 && _normalized.find("/") != 0 &&
        _normalized.find("./") != 0) {
        _normalized = std::string{ "file:./" } + _normalized;
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
        auto _request = zpt::allocate_message<zpt::http::basic_request>();
        _request //
          ->performative(zpt::Get)
          .uri(_normalized);

        std::cout << _request << std::endl;

        auto _reply = zpt::http::retrieve(_request);
        if (_reply != nullptr) {
            std::cout << _reply << std::endl;
            return static_cast<std::string>(_reply->body());
        }
        return std::format("**UNABLE TO FETCH `{}`**", _normalized);
    }
    return "noam only retrieves local or HTTP served files";
}
