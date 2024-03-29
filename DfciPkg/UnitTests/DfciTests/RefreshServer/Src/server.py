# Web Server for testing the Refresh from Network function in DFCI.


import cherrypy
from main import dfci_refresh_server

from OpenSSL import SSL

if __name__ == '__main__':
    # Mount the application
    cherrypy.tree.graft(dfci_refresh_server, "/")

    # Unsubscribe the default server
    cherrypy.server.unsubscribe()
    cherrypy.engine.signals.subscribe()

    cherrypy.config.update({
        'environment': 'embedded',
        'log.screen': True,
        })

    # Instantiate a new server object
    server_http = cherrypy._cpserver.Server()

    # Configure the server object
    server_http.socket_host = "0.0.0.0"
    server_http.socket_port = 80
    server_http.thread_pool = 30

    server_https = cherrypy._cpserver.Server()
    server_https.ssl_module = 'PyOpenSSL'

    ctx = SSL.Context(SSL.TLS_SERVER_METHOD)
    ctx.use_privatekey_file('ssl/DFCI_HTTPS.key')
    ctx.use_certificate_file('ssl/DFCI_HTTPS.pem')

    # Set the Maximum Protocol Version supported by Intune
    ctx.set_max_proto_version(SSL.TLS1_2_VERSION)

    # Set the Minimum Protocol Version supported by Intune
    ctx.set_min_proto_version(SSL.TLS1_2_VERSION)

    # Set the Cipher List supported by Intune
    # You may verify the cipher list with the following command:
    # nmap --script ssl-enum-ciphers -p 443 127.0.0.1
    
    cipher_list = [b'TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384',
                   b'TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256',
                   b'TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384', 
                   b'TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256']
    
    ctx.set_cipher_list(b':'.join(cipher_list))
    
    server_https.ssl_context = ctx
    server_https.socket_host = "0.0.0.0"
    server_https.socket_port = 443
    server_https.thread_pool = 30

    # Subscribe this server
    server_http.subscribe()
    server_https.subscribe()

    # Start the server engine (Option 1 *and* 2)
    cherrypy.engine.start()
    cherrypy.engine.block()
