///
//  mandelmovie.c
//  Name: Jim Luzano-Belfield
//  Assignment: Lab 11 - Multiprocessing
//  Section: CPE-2600-151
//
//  Generates the frames of a Mandelbrot "fly-in" movie by running the
//  provided mandel program once per frame. Each frame is rendered by its
//  own child process (fork + execl). The -p option sets how many children
//  may run at the same time, so the frames can be computed in parallel.
//
//  Build:  make
//  Run:    ./mandelmovie -p 4
//  Movie:  ffmpeg -i mandel%d.jpg mandel.mpg
///
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>

// Default zoom target: "Seahorse Valley", between the main cardioid
// and the large circular bulb on the left of the set.
#define DEFAULT_X        -0.7436438870371587
#define DEFAULT_Y         0.13182590420531198
#define DEFAULT_START     4.0        // starting x-scale (whole set visible)
#define DEFAULT_END       0.00005    // ending x-scale (deep zoom)
#define DEFAULT_FRAMES    50
#define DEFAULT_PROCS     1
#define DEFAULT_SIZE      800        // image width and height in pixels
#define DEFAULT_MAX       1000       // max iterations per point
#define MANDEL_PATH       "./mandel" // program each child runs

static void show_help(void);
static void render_frame(int frame, double x, double y, double scale,
                         int width, int height, int max);

int main(int argc, char *argv[])
{
	int c;

	// configuration, overridable from the command line
	int    nprocs      = DEFAULT_PROCS;
	int    nframes     = DEFAULT_FRAMES;
	double xcenter     = DEFAULT_X;
	double ycenter     = DEFAULT_Y;
	double start_scale = DEFAULT_START;
	double end_scale   = DEFAULT_END;
	int    size        = DEFAULT_SIZE;
	int    max         = DEFAULT_MAX;

	while((c = getopt(argc, argv, "p:n:x:y:s:e:W:m:h")) != -1) {
		switch(c)
		{
			case 'p':
				nprocs = atoi(optarg);
				break;
			case 'n':
				nframes = atoi(optarg);
				break;
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				start_scale = atof(optarg);
				break;
			case 'e':
				end_scale = atof(optarg);
				break;
			case 'W':
				size = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			case 'h':
				show_help();
				exit(0);
			default:   // getopt already printed an "invalid option" message
				show_help();
				exit(1);
		}
	}

	// sanity-check the arguments
	if(nframes < 1) {
		fprintf(stderr, "mandelmovie: number of frames must be at least 1\n");
		exit(1);
	}
	if(nprocs < 1) {
		fprintf(stderr, "mandelmovie: number of processes must be at least 1\n");
		exit(1);
	}
	if(nprocs > nframes) {
		// more children than frames would just leave some idle
		printf("mandelmovie: only %d frames, using %d processes\n", nframes, nframes);
		nprocs = nframes;
	}
	if(start_scale <= 0 || end_scale <= 0 || size < 1 || max < 1) {
		fprintf(stderr, "mandelmovie: scale, size, and max must be positive\n");
		exit(1);
	}

	// Zoom geometrically so every frame shrinks the view by the same
	// factor; a linear zoom would race through the start and crawl at the end.
	double zoom = (nframes > 1) ? pow(end_scale / start_scale, 1.0 / (nframes - 1)) : 1.0;

	printf("mandelmovie: %d frames, %d processes, center (%g, %g), scale %g -> %g\n",
	       nframes, nprocs, xcenter, ycenter, start_scale, end_scale);
	fflush(stdout);   // flush before fork so buffered text is not duplicated in children

	int running = 0;   // number of children currently alive
	double scale = start_scale;

	for(int frame = 0; frame < nframes; frame++) {

		// If we are at the limit, block until any one child finishes
		// before starting the next one.
		if(running >= nprocs) {
			if(wait(NULL) > 0) {
				running--;
			}
		}

		pid_t pid = fork();

		if(pid < 0) {
			perror("mandelmovie: fork");
			break;   // stop making new frames, but still wait for the ones running
		}
		else if(pid == 0) {
			// child: becomes mandel for this one frame (does not return on success)
			render_frame(frame, xcenter, ycenter, scale, size, size, max);
		}

		// parent: count the new child and move to the next frame
		running++;
		scale *= zoom;
	}

	// wait for every remaining child before exiting
	while(running > 0) {
		if(wait(NULL) > 0) {
			running--;
		} else {
			break;   // no children left to wait for
		}
	}

	printf("mandelmovie: done\n");
	return 0;
}

/*
Runs in the child process. Builds the mandel command line for one frame
and replaces this process with mandel via execl. Only returns if execl
fails, in which case the child exits with an error.
*/
static void render_frame(int frame, double x, double y, double scale,
                         int width, int height, int max)
{
	char xs[32], ys[32], ss[32], ws[16], hs[16], ms[16], outfile[64];

	// %.17g keeps full double precision, which matters at deep zoom
	snprintf(xs, sizeof(xs), "%.17g", x);
	snprintf(ys, sizeof(ys), "%.17g", y);
	snprintf(ss, sizeof(ss), "%.17g", scale);
	snprintf(ws, sizeof(ws), "%d", width);
	snprintf(hs, sizeof(hs), "%d", height);
	snprintf(ms, sizeof(ms), "%d", max);
	snprintf(outfile, sizeof(outfile), "mandel%d.jpg", frame);

	execl(MANDEL_PATH, "mandel",
	      "-x", xs, "-y", ys, "-s", ss,
	      "-W", ws, "-H", hs, "-m", ms,
	      "-o", outfile, (char *)NULL);

	// only reached if execl failed
	perror("mandelmovie: execl " MANDEL_PATH);
	_exit(1);
}

// Show help message
static void show_help(void)
{
	printf("Use: mandelmovie [options]\n");
	printf("Where options are:\n");
	printf("-p <procs>  Number of child processes to run at once. (default=%d)\n", DEFAULT_PROCS);
	printf("-n <frames> Number of frames to generate. (default=%d)\n", DEFAULT_FRAMES);
	printf("-x <coord>  X coordinate of the zoom target. (default=%.10g)\n", DEFAULT_X);
	printf("-y <coord>  Y coordinate of the zoom target. (default=%.10g)\n", DEFAULT_Y);
	printf("-s <scale>  Starting scale (X-axis width). (default=%g)\n", DEFAULT_START);
	printf("-e <scale>  Ending scale (X-axis width). (default=%g)\n", DEFAULT_END);
	printf("-W <pixels> Width and height of each frame. (default=%d)\n", DEFAULT_SIZE);
	printf("-m <max>    Maximum iterations per point. (default=%d)\n", DEFAULT_MAX);
	printf("-h          Show this help text.\n");
	printf("\nFrames are written as mandel0.jpg ... mandel<N-1>.jpg\n");
	printf("Example: mandelmovie -p 5\n");
}
