# Lab 11 – Multiprocessing

**Name:** Jim Luzano-Belfield
**Course:** CPE-2600 Systems Programming, Section 111
**Assignment:** Lab 11 – Multiprocessing

## Overview

`mandelmovie` generates the frames of a movie that zooms into the Mandelbrot set. It does not compute any pixels itself. For every frame it forks a child process, and the child uses `execl` to replace itself with the provided `mandel` program, passing it the center point, scale, image size, iteration limit, and an output file name (`mandel0.jpg` … `mandel49.jpg`).

The number of children that may run at the same time is set with `-p`. The parent keeps a count of running children. Before forking a new one, if the count has reached the limit, the parent calls `wait()` to block until any child finishes, then starts the next frame in its place. After the last frame has been started, the parent waits for every remaining child before it exits. Each child is independent once forked, so no pipes or shared memory are needed.

The zoom is geometric: each frame's scale is the previous one multiplied by a constant factor, so the movie flies in at a steady visual speed from a scale of 4 (the whole set) down to 0.00005. The default target is Seahorse Valley, at (-0.7436438870371587, 0.13182590420531198).

### Changes to the provided `mandel` program

- `iteration_to_color()` now uses a smooth cosine color palette instead of gray, and points inside the set are drawn black.
- The `getopt` return value is stored in an `int` instead of a `char`. With a `char`, the `-1` end-of-options value can be misread on platforms where `char` is unsigned.
- The help text lists the correct default output file (`mandel.jpg`).

## Building and running

```bash
sudo apt install libjpeg-dev ffmpeg
make                       # builds mandel and mandelmovie
./mandelmovie -p 5         # 50 frames using up to 5 child processes
make movie                 # stitches mandel0.jpg..mandel49.jpg into mandel.mpg
```

### `mandelmovie` options

| Option | Meaning | Default |
|---|---|---|
| `-p <procs>` | Child processes running at once | 1 |
| `-n <frames>` | Number of frames | 50 |
| `-x <coord>` | X coordinate of zoom target | -0.7436438870 |
| `-y <coord>` | Y coordinate of zoom target | 0.1318259042 |
| `-s <scale>` | Starting scale | 4 |
| `-e <scale>` | Ending scale | 0.00005 |
| `-W <pixels>` | Width and height of each frame | 800 |
| `-m <max>` | Max iterations per point | 1000 |
| `-h` | Help | |

Any process count from 1 up to the number of frames works. A larger value is reduced to the number of frames.

The movie is played at 10 frames per second, so 50 frames make a 5-second movie (`mandel.mpg` in this repo).

## Runtime results

Runtimes were measured with bash's `time` command, using the `real` (wall-clock) time. `mandelmovie` was run once for each process count, generating all 50 frames each time.

| Child processes | Real time (s) | User time (s) | Speedup vs 1 |
|---|---|---|---|
| 1 | 78.9 | 78.6 | 1.0× |
| 2 | 36.8 | 72.9 | 2.1× |
| 5 | 17.1 | 83.9 | 4.6× |
| 10 | 9.1 | 86.3 | 8.7× |
| 20 | 8.0 | 99.4 | 9.9× |
| 50 | 8.5 | 104.4 | 9.3× |

![Runtime vs number of processes](runtime_graph.png)

## Discussion

Using more processes made the program much faster at first. One process took 78.9 s, and every step up to 10 processes cut the runtime nearly in proportion: 2 processes ran about 2.1× faster, 5 ran 4.6× faster, and 10 ran 8.7× faster, finishing in 9.1 s. Going from 10 to 20 processes, though, only saved about one more second (9.1 s to 8.0 s), and 50 processes (one per frame) was actually slightly *slower* than 20, at 8.5 s. The curve flattens out and then starts to tick back up.

Rendering a frame is purely CPU-bound work, with almost no time spent waiting on files or the network. Each running child keeps one CPU core busy, so as long as there are idle cores, each added process adds another core's worth of work, and the runtime drops in proportion. Once the number of processes passes the number of cores the machine can actually run at once, the extra processes have nowhere new to run. The operating system just takes turns running them on the cores it already has, so the runtime stops improving.

The `user` time shows the same thing from another angle. It is the total CPU time added up across all processes, and the total work (50 frames) never changes. From 1 to 10 processes, it stays fairly steady at roughly 73–86 s. At 20 processes it jumps to 99 s, and at 50 it reaches 104 s, even though the wall-clock time barely improved or got worse. That means each process was getting less work done per second of CPU time. This is what happens when processes are competing for the same physical hardware: with hyperthreading, two logical cores share one physical core and slow each other down, and extra context switching adds a little more overhead. The `sys` time also rose steadily, from 0.27 s with 1 process to 0.87 s with 20 and 1.38 s with 50, which fits the extra work the kernel does creating, scheduling, and switching between more processes.

So using more processes does **not** always speed things up. The speedup is close to linear until the process count reaches the number of available cores, and beyond that it levels off. Past that point, extra processes cost more CPU time and kernel overhead than they save, which is why 50 processes ran slower than 20.

One more detail affects how well the work spreads out. Frames are not equal in cost: the deeper frames near the end of the zoom contain more of the detailed boundary of the set, so they need more iterations per pixel and take longer. Because the parent hands out a new frame as soon as any child finishes, a core that finished a cheap frame immediately picks up the next one. This keeps all the cores busy, which is why the speedup stays so close to linear up to 10 processes.
