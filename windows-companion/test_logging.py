"""Regression test: OAuth/HTTP library debug output must never reach the log file."""

import io
import logging

import main


def test_oauth_libraries_are_silenced():
    main.setup_logging(verbose=True)
    stream = io.StringIO()
    handler = logging.StreamHandler(stream)
    handler.setLevel(logging.DEBUG)
    logging.getLogger().addHandler(handler)
    try:
        for name in ("requests_oauthlib.oauth2_session", "google_auth_oauthlib.flow", "oauthlib.oauth2",
                     "urllib3.connectionpool", "requests"):
            logging.getLogger(name).debug("refresh_token=SECRET-%s", name)
            logging.getLogger(name).info("access_token=SECRET-%s", name)
        assert "SECRET" not in stream.getvalue()
    finally:
        logging.getLogger().removeHandler(handler)


if __name__ == "__main__":
    test_oauth_libraries_are_silenced()
    print("ok  test_oauth_libraries_are_silenced")
