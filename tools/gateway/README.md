# Gateway helpers (gw85224-01)

`tcp_echo6.py`: TCP echo on [::]:7777 for ztoit's Thread TCP tests, run as a user unit (no sudo on the gateway;
lingering is on). Deploy:

    scp tcp_echo6.py gw85224-01:.local/bin/ && scp tcp-echo6.service gw85224-01:.config/systemd/user/
    ssh gw85224-01 'systemctl --user daemon-reload && systemctl --user enable --now tcp-echo6'

The UDP twin (`udp-echo6`, :7777) is rete's, a system unit; leave it alone.
