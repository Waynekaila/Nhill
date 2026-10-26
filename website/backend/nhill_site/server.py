"""Runs the form backend with the standard library WSGI server (threaded).

  python3 -m nhill_site.server        (configuration: environment variables, see .env.example)

Meant to listen on 127.0.0.1 behind the TLS reverse proxy (Caddy) that also serves the
static site. For heavier traffic, any WSGI server can host nhill_site.wsgi:application.
"""
import logging
import socketserver
from wsgiref.simple_server import WSGIRequestHandler, WSGIServer, make_server

from .app import App
from .config import Config


class ThreadingWSGIServer(socketserver.ThreadingMixIn, WSGIServer):
    daemon_threads = True


class QuietHandler(WSGIRequestHandler):
    def log_message(self, fmt, *args):  # no client IP or path with personal data in logs
        logging.getLogger("nhill_site.http").info("%s %s", self.command, self.path.split("?")[0])


def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    cfg = Config.from_env()
    app = App(cfg)
    if cfg.engine == "sqlite":
        app.storage.init_schema()
    with make_server(cfg.listen_host, cfg.listen_port, app, server_class=ThreadingWSGIServer, handler_class=QuietHandler) as srv:
        logging.info("Nhill site backend on http://%s:%d (%s)", cfg.listen_host, cfg.listen_port, cfg.engine)
        srv.serve_forever()


if __name__ == "__main__":
    main()
