#pragma once
#include <string>
namespace Redirection {
    // Starts an HTTP server that redirects all requests to the HTTPS version.
    void startRedirectServer(const std::string& host = "0.0.0.0", int port = 80);
}
