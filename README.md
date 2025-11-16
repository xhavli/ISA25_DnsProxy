# ISA25_DnsProxy

## Tests

```terminal
- dig @127.0.0.1 -p 5300 example.com A +short +tries=1 +retry=0
```

```terminal
- dig @[::1] -p 5300 vut.cz A +short +tries=1 +retry=0
```

## TODO

- Add compression of QNAME
- Receive from ipv4 and also ipv6
- -getaddrinfo() for -s upstream resolver is valid
- ID replacement?
- QCOUNT > 1 ? return not implemented
