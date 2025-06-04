#include "../include/httplib.h"
#include "../include/log.h"
#include <string>
#include "RedirectServer.h"

namespace Redirection {

    void startRedirectServer(const std::string& host, int port) {
        httplib::Server redirectServer;

        // Catch-all handler to redirect HTTP to HTTPS on port 443
        redirectServer.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) -> httplib::Server::HandlerResponse {
            std::string hostHeader = req.get_header_value("Host", "");

            // Remove existing port if present
            auto colonPos = hostHeader.find(':');
            std::string hostname = (colonPos != std::string::npos) ? hostHeader.substr(0, colonPos) : hostHeader;

            // Construct the HTTPS URL explicitly targeting port 443
            std::string httpsUrl = "https://" + hostname + ":443" + req.target;

            res.set_redirect(httpsUrl, 302);
            return httplib::Server::HandlerResponse::Handled;
            });

        Logger::getInstance().logInfo("RedirectServer", "Starting HTTP redirection server on " + host + ":" + std::to_string(port));
        if (!redirectServer.listen(host.c_str(), port)) {
            Logger::getInstance().logError("RedirectServer", "Failed to start HTTP redirection server on " + host + ":" + std::to_string(port));
        }
    }

}
