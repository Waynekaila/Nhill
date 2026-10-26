"""WSGI entry point for an external server, e.g.: gunicorn nhill_site.wsgi:application"""
from .app import App
from .config import Config

application = App(Config.from_env())
