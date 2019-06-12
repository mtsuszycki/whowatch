Whowatch is a interactive, ncurses-based, process and users monitoring tool
which updates information in real time.
It can be described as a some kind of interactive mix of ps, pstree, lsof,
w, finger and kill.
You can watch information about users currently logged on to the machine:
login type - ie. "ssh" "telnet", tty, host, idle time and command line of
their current process. You can also naviagate in processes tree, watch process
details (memory, uid, open descriptors with TCP sockets mapped
on IP:port -> IP:port pair etc.).
Whowatch has support for plugins.

# Releases

Future whowatch releases will be signed by one of two OpenPGP keys:

* Paul Wise:     610B 28B5 5CFC FE45 EA1B  563B 3116 BA5E 9FFA 69A3
* Mike Suszycki: 3063 9B82 3094 E640 15F7  F40B 3E6C B16A 1D1A D8D5

# Related

* [namecheck](https://manpages.debian.org/namecheck): check project names are not already taken
* [Repology](https://repology.org/): FLOSS service that monitors package metadata in various repositories
* [pkgs.org](https://pkgs.org/): service for searching Linux/BSD package repositories
