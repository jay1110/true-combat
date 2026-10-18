"""Native NavStudio window backed by the existing local-only Studio service.

WebView2 renders the same tested editor without opening an external browser.
The HTTP service belongs to the desktop process and closes with its window.
Offline worker invocations continue to bypass the GUI entirely.
"""
import argparse
import json
import sys
import threading
from pathlib import Path
from urllib.request import Request, urlopen


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace')
    parser.add_argument('--settings-file')
    parser.add_argument('--port', type=int, default=8766)
    parser.add_argument('--rcon-port', type=int, default=27986)
    parser.add_argument('--maps-dir', type=Path, action='append')
    parser.add_argument('--no-browser', action='store_true', help='Run only the local service for automation')
    return parser.parse_args(argv)


def existing_service(url, workspace):
    from server import APP_ID
    try:
        with urlopen(url + 'api/state', timeout=4) as response:
            state = json.load(response)
        if state.get('app') == APP_ID and Path(state.get('workspace', '')).resolve() == workspace:
            return state
    except (OSError, ValueError):
        pass
    return None


class DesktopLifecycle:
    """Save/return the active project before the native window is destroyed."""
    def __init__(self, url):
        self.url = url
        self.window = None
        self.closing = False
        self.may_close = False

    def close(self):
        if self.may_close:
            return True
        if not self.closing:
            self.closing = True
            threading.Thread(target=self._save_close, daemon=True).start()
        return False

    def _save_close(self):
        try:
            # Project saving cannot serialize text still inside the companion
            # editor. Keep the window open until that text is saved/discarded.
            if self.window.evaluate_js('window.navstudioBundle ? window.navstudioBundle.canClose() : true') is False:
                self.closing = False
                return
            with urlopen(self.url + 'api/state', timeout=8) as response:
                state = json.load(response)
            with urlopen(self.url + 'api/projects', timeout=8) as response:
                projects = json.load(response)
            if projects.get('active'):
                request = Request(self.url + 'api/project',
                                  data=json.dumps({'op': 'save_close'}).encode('utf-8'),
                                  headers={'Content-Type': 'application/json', 'X-Navstudio-Token': state['token']},
                                  method='POST')
                with urlopen(request, timeout=45) as response:
                    json.load(response)
            self.may_close = True
            self.window.destroy()
        except Exception as exc:
            self.closing = False
            message = 'Projekt konnte nicht geschlossen werden: ' + str(exc)
            self.window.evaluate_js('window.navstudioProjects && window.navstudioProjects.showError(' + json.dumps(message) + ')')


def main(argv=None):
    args = arguments(argv)
    if args.no_browser:
        from server import main as serve
        return serve(sys.argv[1:] if argv is None else argv)
    if not 1024 <= args.port <= 65535 or not 1 <= args.rcon_port <= 65535:
        raise ValueError('UngÃ¼ltiger Port.')
    # Fail before starting a hidden service if the desktop dependency is absent.
    try:
        import webview
    except ImportError as exc:
        raise RuntimeError('NavStudio Desktop benÃ¶tigt pywebview. Das vollstÃ¤ndige Desktop-Paket verwenden oder requirements-desktop.txt installieren.') from exc
    from server import Studio, StudioHTTPServer, Handler, discover_workspace
    workspace = discover_workspace(args.workspace)
    url = 'http://127.0.0.1:' + str(args.port) + '/'
    server = studio = thread = None
    if not existing_service(url, workspace):
        studio = Studio(args)
        try:
            server = StudioHTTPServer(('127.0.0.1', args.port), Handler)
        except OSError:
            studio.executor.shutdown(wait=False)
            raise
        server.studio = studio
        server.daemon_threads = True
        thread = threading.Thread(target=server.serve_forever, kwargs={'poll_interval': .25}, daemon=True)
        thread.start()
    lifecycle = DesktopLifecycle(url)
    try:
        # WebView2 runtime already installed on Windows; no Chromium copy.
        webview.settings['OPEN_EXTERNAL_LINKS_IN_BROWSER'] = False
        webview.settings['ALLOW_DOWNLOADS'] = True  # Local bundle export opens a native Save dialog.
        window = webview.create_window('TCE2 NavStudio · Projekte', url + '?desktop=1',
                                       width=1440, height=960, min_size=(1080, 720),
                                       background_color='#182126', text_select=True, hidden=False)
        lifecycle.window = window
        window.events.closing += lifecycle.close
        # A launcher may hide its console through STARTUPINFO. Show the editor
        # again after the first page load so that flag cannot hide the GUI too.
        window.events.loaded += window.show
        webview.start(gui='edgechromium' if sys.platform == 'win32' else None,
                      private_mode=False, storage_path=str(workspace / 'build' / 'navstudio-webview'))
    finally:
        if server:
            server.shutdown()
            server.server_close()
            thread.join(timeout=4)
            studio.executor.shutdown(wait=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
