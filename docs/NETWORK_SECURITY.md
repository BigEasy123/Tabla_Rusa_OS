# Network And Security Subsystems

Tabla Rusa OS networking is currently loopback/socket-like infrastructure. It does not provide real internet or hardware NIC networking yet.

## Network API

Core calls:

```c
net_socket_create(pid, "tcp");
net_bind(socket_id, 10001);
net_listen(socket_id);
net_connect(socket_id, 10001);
net_send(socket_id, "hello");
net_recv(socket_id, &message);
net_socket_close(socket_id);
net_connection_list(out, max);
net_port_list(out, max);
```

Connection states:

```text
CLOSED LISTENING CONNECTING CONNECTED CLOSING ERROR
```

Port states:

```text
FREE BOUND LISTENING BLOCKED RESERVED
```

The stack tracks owner PID, peer socket, local/remote ports, TX/RX bytes, flood score, receive buffers, and last-activity ticks. `net_shutdown_idle()` closes stale sockets.

## Security API

Core calls:

```c
security_register_app("network", "loopback tools");
security_register_capability("network", "network.loopback", "local socket tests");
security_set_permission("network", "network.client", SECURITY_PERMISSION_ALLOW);
security_check_permission("network", "network.client");
security_log_event(...);
security_get_events(out, max);
security_scan_app("/home/projects/demo.rusa", out, max);
```

Permission decisions:

```text
allow deny ask inherited default-deny
```

The scanner is intentionally lightweight. It detects simple suspicious patterns and does not claim comprehensive malware detection.

## Policy API

Policy checks live in `policy.c`:

```c
policy_firewall_add(app, port, allow, explanation);
policy_firewall_remove(rule_id);
policy_firewall_set_enabled(rule_id, enabled);
policy_firewall_list(out, max);
policy_firewall_test(app, port, server);
policy_firewall_explain(rule_id, out, max);
policy_check_network_access(app, port, server);
policy_check_file_access(app, path, write);
policy_check_device_access(app, device);
policy_check_process_access(app, process, action);
```

These functions combine privacy state, firewall rules, app permissions, reserved-port rules, and protected-resource checks. Firewall rules can match a specific app or `*`, and port `0` acts as an all-port wildcard.

## Terminal Commands

```text
netstat
ports
listen 10001
connect SOCKET PORT
send SOCKET MESSAGE
recv SOCKET
services
security events
security scan /path/file.rusa
permissions APP
allow APP RESOURCE
deny APP RESOURCE
firewall list
firewall add deny demo 10001 block demo loopback
firewall add allow * 0 allow ordinary loopback
firewall disable 1
firewall enable 1
firewall remove 1
firewall test demo 10001
firewall explain 1
```

## GUI Integration

Security Center panels:

- Overview
- Connections
- Permissions
- Services
- Scanner
- Devices
- Events

Network app shows live sockets, shield state, IP masking, packet counts, and loopback actions. Task Manager rows include per-process connection counts.

Settings privacy controls show and toggle master privacy, network visibility, cookie policy, device access, and telemetry through the same reusable privacy APIs used by terminal commands.

Security Center actions now call reusable APIs:

- Connections: disconnect socket `0` and open firewall rules in Terminal.
- Permissions: allow or deny the demo app's `network.client` permission.
- Services: list services or request a network service stop through Terminal.
