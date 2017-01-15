# jautolock

## Description

jautolock will fire a program after specified time of user inactivity.
It is intended to replace xautolock.

## Development

jautolock has the following dependencies:
  * confuse
  * libxss

## Upstream

jautolock is developed at https://github.com/johnchen902/jautolock

## Compilation

Compiling is done with the usual make-line
```bash
make && sudo make install
```

## Configuration

Put your config at ~/.config/jautolock/config

Sample config:
```
action notify {
    time = 50
    command = "notify-send jautolock \"10 seconds before locking\""
}
action lock {
    time = 60
    command = "i3lock -nc 000000"
}
```
