import BaseHTTPServer as bs, SimpleHTTPServer as ss, ssl
httpd = bs.HTTPServer(('localhost', 4443), ss.SimpleHTTPRequestHandler)
httpd.socket = ssl.wrap_socket(httpd.socket, certfile='server.crt', keyfile='server.key', server_side=True)
httpd.handle_request()
httpd.handle_request()
