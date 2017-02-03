# jautolock

## Description

jautolock will fire a program after specified time of user inactivity.
It has basically the same functionality as xautolock,
but is designed to be more flexible.
You can specify any number of tasks, instead of only three.
Smallest time unit is nanosecond, instead of second or minute.

## Build

jautolock has the following dependencies:
  * confuse
  * libxdg-basedir
  * libxss
  * libx11 (should be implied by libxss)

To build, use the usual make command:
```bash
make
```

## Usage

Unlike xautolock, jautolock has no default locker.
You must tell jautolock what program to fire.

Put your config at ~/.config/jautolock/config

Sample config:
```
task notify {
    time = 50s
    command = "notify-send jautolock \"10 seconds before locking\""
}
task lock {
    time = 60s
    command = "i3lock -n"
}
task screenoff {
    time = 70s
    command = "xset dpms force off"
}
```
Supported time units are d (days), h (hours), m (minutes),
s (seconds), ms (milliseconds) and ns (nanoseconds).

Once you have your configuration, run:
```bash
jautolock
```

Like xautolock, jautolock can communicate with an already running instance.
Use `jautolock <message>` to send message.
Currently these messages are understood:

+ `exit`: Exit.
+ `now <taskname>`: Fire task with the specified name.
+ `busy`: Assume the user is always active.
+ `unbusy`: No longer assume the user is always active.

## Timing

*Need help with this section.*

Take the sample config above as an example.
When task lock is running,
task screenoff will be fired after 10 seconds of inactivity instead of 70,
and task notify will not be automatically fired.
