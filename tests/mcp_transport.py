"""Exercise the shipped stdio relay against the editor's native MCP server."""
import json
import queue
import socket
import subprocess
import sys
import threading

backend, editor = sys.argv[1:]
server = subprocess.Popen([backend, '--serve'], stdout=subprocess.PIPE, text=True, encoding='utf-8')
relay = None
try:
    config = json.loads(server.stdout.readline())
    # Reject connections with no current editor token.
    with socket.create_connection(('127.0.0.1', config['port']), timeout=3) as connection:
        connection.sendall(b'wrong-token\n')
        assert connection.recv(16) == b''
    relay = subprocess.Popen([editor, '--mcp-connect', str(config['port']), config['token']],
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             text=True, encoding='utf-8')
    messages = queue.Queue()
    def read():
        for line in relay.stdout:
            messages.put(json.loads(line))
    threading.Thread(target=read, daemon=True).start()
    def send(message):
        relay.stdin.write(json.dumps(message) + '\n')
        relay.stdin.flush()
    def rpc(identifier, method, params=None):
        send({'jsonrpc': '2.0', 'id': identifier, 'method': method, 'params': params or {}})
        result = messages.get(timeout=8)
        assert result['id'] == identifier, result
        assert 'error' not in result, result
        return result['result']
    result = rpc(1, 'initialize', {'protocolVersion': '2025-11-25', 'capabilities': {}, 'clientInfo': {'name': 'transport-test', 'version': '1'}})
    assert result['serverInfo']['name'] == 'tilezeitjobb'
    send({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
    assert len(rpc(2, 'tools/list')['tools']) >= 10
    def tool(identifier, name, arguments=None):
        return rpc(identifier, 'tools/call', {'name': name, 'arguments': arguments or {}})
    result = tool(3, 'create_layer', {'name': 'Through stdio'})
    assert not result['isError'] and result['structuredContent']['layer'] == 1
    result = tool(4, 'draw_pixels', {'layer': 1, 'pixels': [[2, 3, 1], [3, 3, 2]]})
    assert result['structuredContent']['changed']
    assert tool(5, 'undo')['structuredContent']['changed']
    result = tool(6, 'render_preview')
    assert result['content'][0]['data'].startswith('iVBORw0KGgo')
    result = tool(7, 'get_project')
    assert result['structuredContent']['layers'][1]['name'] == 'Through stdio'
    assert tool(8, 'draw_pixels', {'layer': 1, 'pixels': [[9999, 3, 1]]})['isError']
    relay.stdin.close()
    relay.wait(timeout=4)
    assert relay.returncode == 0
    print('MCP stdio handshake, authentication, tools, live edit queue, undo and PNG preview passed')
finally:
    for process in (relay, server):
        if process and process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
