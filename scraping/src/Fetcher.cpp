#include "Fetcher.hpp"
#include <iostream>
#include <boost/asio/connect.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
// #include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

static void parse_url(const std::string& url, std::string& host, std::string& port, std::string& target)
{
    host.clear(); port.clear(); target = "/";
    std::string u = url;
    const std::string http_prefix = "http://";
    const std::string https_prefix = "https://";
    if (u.rfind(http_prefix, 0) == 0) u = u.substr(http_prefix.size());
    else if (u.rfind(https_prefix, 0) == 0) u = u.substr(https_prefix.size());
    if (const auto pos = u.find('/'); pos == std::string::npos) {
        host = u;
    } else {
        host = u.substr(0, pos);
        target = u.substr(pos);
    }
    // allow explicit port in host (host:port)
    if (const auto colon = host.find(':'); colon != std::string::npos) {
        port = host.substr(colon + 1);
        host = host.substr(0, colon);
    }
}

void Fetcher::async_fetch(net::io_context& ioc, const std::string& url, Callback cb)
{
    // determine scheme
    bool is_https = false;
    if (const std::string u = url; u.rfind("https://", 0) == 0) is_https = true;

    if (!is_https) {
        struct Session : std::enable_shared_from_this<Session> {
            tcp::resolver resolver;
            beast::tcp_stream stream;
            beast::flat_buffer buffer;
            http::request<http::empty_body> req;
            http::response<http::string_body> res;
            std::string host, port, target;
            std::string url;
            Callback cb;

            Session(net::io_context& ioc_, const std::string& u, Callback c)
            : resolver(ioc_), stream(ioc_), req(), url(u), cb(std::move(c)) {}

            void start() {
                parse_url(url, host, port, target);
                req.method(http::verb::get);
                req.target(target);
                req.version(11);
                req.set(http::field::host, host);
                req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

                auto self = shared_from_this();
                resolver.async_resolve(host, port, [self](const beast::error_code& ec, const tcp::resolver::results_type& results){
                    if (ec) {
                        std::cerr << "Resolver error: " << ec.message() << std::endl;
                        FetchResult fr; fr.body = ""; fr.status = 0;
                        fr.fetch_time = std::to_string(std::time(nullptr));
                        self->cb(fr);
                        return;
                    }
                    self->stream.async_connect(results, [self](const beast::error_code& error_code, const tcp::endpoint&){
                        if (error_code) {
                            std::cerr << "Connect error: " << error_code.message() << std::endl;
                            FetchResult fr; fr.body = ""; fr.status = 0; fr.fetch_time = std::to_string(std::time(nullptr));
                            self->cb(fr);
                            return;
                        }
                        http::async_write(self->stream, self->req, [self](const beast::error_code& error_code_two, std::size_t){
                            if (error_code_two) {
                                std::cerr << "Write error: " << error_code_two.message() << std::endl;
                                FetchResult fr; fr.body = ""; fr.status = 0; fr.fetch_time = std::to_string(std::time(nullptr));
                                self->cb(fr);
                                return;
                            }
                            http::async_read(self->stream, self->buffer, self->res, [self](beast::error_code error_code_three, std::size_t){
                                beast::error_code shut_ec;
                                self->stream.socket().shutdown(tcp::socket::shutdown_both, shut_ec);
                                if (error_code_three) {
                                    std::cerr << "Read error: " << error_code_three.message() << std::endl;
                                    FetchResult fr; fr.body = ""; fr.status = 0; fr.fetch_time = std::to_string(std::time(nullptr));
                                    self->cb(fr);
                                    return;
                                }
                                FetchResult fr;
                                fr.status = static_cast<int>(self->res.result_int());
                                fr.body = self->res.body();
                                fr.content_type = std::string(self->res[http::field::content_type]);
                                for (auto const& h : self->res.base()) fr.headers[std::string(h.name_string())] = std::string(h.value());
                                {
                                    std::time_t t = std::time(nullptr);
                                    std::tm tm = *std::gmtime(&t);
                                    std::ostringstream oss;
                                    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
                                    fr.fetch_time = oss.str();
                                }
                                self->cb(fr);
                            });
                        });
                    });
                });
            }
        };

        const auto s = std::make_shared<Session>(ioc, url, std::move(cb));
        s->start();
        return;
    }

    // HTTPS session
    struct SSLSession : std::enable_shared_from_this<SSLSession> {
        tcp::resolver resolver;
        boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23_client};
        boost::asio::ssl::stream<tcp::socket> stream;
        beast::flat_buffer buffer;
        http::request<http::empty_body> req;
        http::response<http::string_body> res;
        std::string host, port, target;
        std::string url;
        Callback cb;

        SSLSession(net::io_context& ioc_, const std::string& u, Callback c)
        : resolver(ioc_), ctx(boost::asio::ssl::context::sslv23_client), stream(ioc_, ctx), req(), url(u), cb(std::move(c)) {
            ctx.set_default_verify_paths();
            stream.set_verify_mode(boost::asio::ssl::verify_peer);
        }

        void start() {
            parse_url(url, host, port, target);
            // default https port
            if (port.empty() || port == "80") port = "443";
            req.method(http::verb::get);
            req.target(target);
            req.version(11);
            req.set(http::field::host, host);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

            auto self = shared_from_this();
            resolver.async_resolve(host, port, [self](const beast::error_code& ec, const tcp::resolver::results_type& results){
                if (ec) {
                    std::cerr << "SSL resolver error: " << ec.message() << std::endl;
                    FetchResult fr; fr.body = ""; fr.status = 0; fr.fetch_time = std::to_string(std::time(nullptr));
                    self->cb(fr);
                    return;
                }
                // perform blocking connect on the resolved endpoints (acceptable for prototype)
                beast::error_code conn_ec;
                net::connect(self->stream.next_layer(), results, conn_ec);
                if (conn_ec) { std::cerr << "SSL connect error: " << conn_ec.message() << std::endl; FetchResult fr; fr.body = ""; fr.status = 0; fr.fetch_time = std::to_string(std::time(nullptr)); self->cb(fr); return; }
                // set SNI (server name) for TLS via OpenSSL
                if (!SSL_set_tlsext_host_name(self->stream.native_handle(), self->host.c_str())) {
                    std::cerr << "Failed to set SNI for host: " << self->host << std::endl;
                }
                self->stream.async_handshake(boost::asio::ssl::stream_base::client, [self](const beast::error_code& ec){
                        if (ec) { std::cerr << "SSL handshake error: " << ec.message() << std::endl; FetchResult fr; fr.body = ""; fr.status = 0; fr.fetch_time = std::to_string(std::time(nullptr)); self->cb(fr); return; }
                    http::async_write(self->stream, self->req, [self](beast::error_code ec, std::size_t){
                            if (ec) { std::cerr << "SSL write error: " << ec.message() << std::endl; FetchResult fr; fr.body = ""; fr.status = 0; fr.fetch_time = std::to_string(std::time(nullptr)); self->cb(fr); return; }
                            http::async_read(self->stream, self->buffer, self->res, [self](beast::error_code ec, std::size_t){
                                beast::error_code shut_ec;
                                self->stream.next_layer().shutdown(tcp::socket::shutdown_both, shut_ec);
                                if (ec) { std::cerr << "SSL read error: " << ec.message() << std::endl; FetchResult fr; fr.body = ""; fr.status = 0; fr.fetch_time = std::to_string(std::time(nullptr)); self->cb(fr); return; }
                                FetchResult fr;
                                fr.status = static_cast<int>(self->res.result_int());
                                fr.body = self->res.body();
                                fr.content_type = std::string(self->res[http::field::content_type]);
                                for (auto const& h : self->res.base()) fr.headers[std::string(h.name_string())] = std::string(h.value());
                                {
                                    std::time_t t = std::time(nullptr);
                                    std::tm tm = *std::gmtime(&t);
                                    std::ostringstream oss;
                                    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
                                    fr.fetch_time = oss.str();
                                }
                                self->cb(fr);
                            });
                    });
                });
            });
        }
    };

    const auto s = std::make_shared<SSLSession>(ioc, url, std::move(cb));
    s->start();
}

