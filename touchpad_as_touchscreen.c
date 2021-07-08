#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <linux/input.h>

// ---
#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"

#define AX_X 0
#define AX_Y 1

/* Disable to control the area on the screen where the cursor is moved around (using geometry_x,y,w,h)*/
#define MAP_TO_ENTIRE_SCREEN 1

/* Width/Height around touchpad where nothing happens when touched*/
#define TP_MARGIN_X 150
#define TP_MARGIN_Y 100

enum absval{
   ABSVAL_VALUE,
   ABSVAL_MIN,
   ABSVAL_MAX,
   ABSVAL_FUZZ,
   ABSVAL_FLAT,
   ABSVAL_RESOLUTION
};

struct dev_absdata{
   int min_value_abs_x;
   int max_value_abs_x;
   int min_value_abs_y;
   int max_value_abs_y;
} tp_device_absdata, vts_device_absdata;

struct event_block_data{
   int ev_key_btn_touch;
   int abs_x;
   int abs_y;
   int abs_mt_slot;
   int abs_mt_position_x;
   int abs_mt_position_y;
   int abs_mt_tracking_id;
} event_block;

int fd, vts_fd;
char* tp_device_path;
char* vts_device_path;
// --- touch variables
/*TODO convert to struct*/
int touch_x = -1;
int touch_y = -1;
int initial_x = -1;
int initial_y = -1;
// geometry of area to map to on display
int geometry_x = 100;
int geometry_y = 100;
int geometry_w = 780;
int geometry_h = 320;

volatile sig_atomic_t stop = 0;

void interrupt_handler(int sig)
{
	stop = 1;
}

int is_event_device(const struct dirent *dir) {
	return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}
/**
 * Scans all /dev/input/event*, display them and ask the user which one to
 * open, 2 files should be chosen, either by the user or automatically detected
 * (touchpad event file and virtual touchscreen event file)
 *
 * @param tp_device_path string variable to write result file path to
 */
int scan_devices(char ** tp_device_path, char ** vts_device_path)
{
	struct dirent **namelist;
	int  i,ndev, devnum;
   bool tp_found = false;
   bool vts_found = false;
	char *filename;

	ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, alphasort);
	if (ndev <= 0)
		return 1;

	fprintf(stderr, "Available devices:\n");

	for (i = 0; i < ndev; i++)
	{
		char fname[64];
		int fd = -1;
		char name[256] = "???";

		snprintf(fname, sizeof(fname),
			 "%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);
		fd = open(fname, O_RDONLY);
		if (fd < 0)
			continue;
		ioctl(fd, EVIOCGNAME(sizeof(name)), name);

		fprintf(stderr, "%s:	%s\n", fname, name);
		close(fd);

		sscanf(namelist[i]->d_name, "event%d", &devnum);

		free(namelist[i]);

      if (strstr(name, "TouchPad") != NULL){
         *tp_device_path = (char *) malloc(64);
         strcpy(*tp_device_path, fname);
         tp_found = 1;
      }
      if(strstr(name, "Virtual touchscreen") != NULL){
         *vts_device_path = (char *) malloc(64);
         strcpy(*vts_device_path, fname);
         vts_found = 1;
      }

      if(tp_found && vts_found){
		   free(namelist);
         return 0;
      }
	}

   if(!tp_found){
      fprintf(stderr, "select the device event number for your touchpad: ");
      scanf("%d", &devnum);

      *tp_device_path = (char *) malloc(64);
      sprintf(*tp_device_path, "%s/%s%d",
          DEV_INPUT_EVENT, EVENT_DEV_NAME,
          devnum);
   }

   if(!vts_found){
      fprintf(stderr, "select the device event number for your virtual touchscreen: ");
      scanf("%d", &devnum);

      *vts_device_path = (char *) malloc(64);
      sprintf(*vts_device_path, "%s/%s%d",
          DEV_INPUT_EVENT, EVENT_DEV_NAME,
          devnum);
   }
	return 0;
}

/**
 * record additional information for absolute axes (min/max value).
 *
 * @param fd the file descriptor to the device.
 * @param axis the axis identifier (e.g. abs_x).
 */
void record_absdata(int fd, struct dev_absdata * device_absdata)
{
	int abs_x[6], abs_y[6] = {0};
	int k;

	ioctl(fd, EVIOCGABS(AX_X), abs_x);
	ioctl(fd, EVIOCGABS(AX_Y), abs_y);
	for (k = 0; k < 6; k++){
		if ((k < 3) || abs_x[k] || abs_y[k]){
			//printf("      %s %6d\n", absval[k], abs_x[k]);
         if (k == ABSVAL_MIN){
            device_absdata -> min_value_abs_x = abs_x[k];
            device_absdata -> min_value_abs_y = abs_y[k];
         }
         else if (k == ABSVAL_MAX){
            device_absdata -> max_value_abs_x = abs_x[k];
            device_absdata -> max_value_abs_y = abs_y[k];
         }
      }
   }

   fprintf(stderr, "min x = %d\n", device_absdata -> min_value_abs_x);
   fprintf(stderr, "max x = %d\n", device_absdata -> max_value_abs_x);
   fprintf(stderr, "min y = %d\n", device_absdata -> min_value_abs_y);
   fprintf(stderr, "max y = %d\n", device_absdata -> max_value_abs_y);
}

/**
 * open device, grabs it for exclusive access,
 * then record some data required later
 */
int init_dev_event_reader(){
   char name[256] = "???";
	if ((getuid ()) != 0) {
        fprintf(stderr, "you are not root! this may not work...\n");
        return 1;
    }

    /* open touchpad device */
    fd = open(tp_device_path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "%s is not a vaild device\n", tp_device_path);
        return 1;
    }

    /* print device name */
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    printf("\nreading from:\n");
    printf("device file = %s\n", tp_device_path);
    printf("device name = %s\n", name);

   ioctl(fd, EVIOCGRAB, (void*)1);

   record_absdata(fd, &tp_device_absdata);

    /* open virtual touchscreen device */
   vts_fd = open(vts_device_path, O_WRONLY);
   if (fd == -1) {
     fprintf(stderr, "%s is not a vaild device\n", vts_device_path);
     return 1;
   }
   /* print device name */
   ioctl(vts_fd, EVIOCGNAME(sizeof(name)), name);
   printf("\nwriting to:\n");
   printf("device file = %s\n", vts_device_path);
   printf("device name = %s\n", name);

   record_absdata(vts_fd, &vts_device_absdata);

   return 0;
}

/**
 * translate x or y coordinates on touchpad to coordinates on display														|
 *
 * @param point x or y coordinate from ev[i].value
 * @param type 0 for x, 1 for y
 */
int translate_pt(
	int point,	// x/y coordinate from ev[i].value
	bool type		// 0 for x, 1 for y
	)
{
   /* display offset is usually 0 and display_size is usually vts_device_absdata.max_value_? if geometry_* variables are not touched*/
	int display_offset, display_size, touchpad_min, touchpad_size;
	if(!type){ //x
		display_offset = geometry_x;
		display_size = geometry_w;
		touchpad_min = tp_device_absdata.min_value_abs_x + TP_MARGIN_X;
		touchpad_size = tp_device_absdata.max_value_abs_x - tp_device_absdata.min_value_abs_x - (2*TP_MARGIN_X);
	}
	else{ //y
		display_offset = geometry_y;
		display_size = geometry_h;
		touchpad_min = tp_device_absdata.min_value_abs_y + TP_MARGIN_Y;
		touchpad_size = tp_device_absdata.max_value_abs_y - tp_device_absdata.min_value_abs_y - (2*TP_MARGIN_Y);
	}

	point = point - touchpad_min;
   if (point < 0)
      point = 0;
   else if (point > touchpad_size)
      point = touchpad_size;
	return display_offset + (int) floor(point*display_size/touchpad_size);
}

/**
 * change geometry_x,y values
 *
 * @param rel_x distance to move geometry in the x axis direction
 * @param rel_y distance to move geometry in the y axis direction
 */
int move_geometry(int rel_x, int rel_y){

	geometry_x += rel_x;
	geometry_y += rel_y;
	if (geometry_x < 0) geometry_x=0;
	if (geometry_y < 0) geometry_y=0;
	if (geometry_x > vts_device_absdata.max_value_abs_x-geometry_w) geometry_x=vts_device_absdata.max_value_abs_x-geometry_w;
	if (geometry_y > vts_device_absdata.max_value_abs_y-geometry_h) geometry_y=vts_device_absdata.max_value_abs_y-geometry_h;

	printf("geometry changed to: %d, %d \n", geometry_x, geometry_y);
	return 0;
}

int event_listener_loop(){
   struct input_event ev[64];
	const size_t ev_size = sizeof(struct input_event);
	ssize_t size;
   int i;
	fd_set rdfs;

	FD_ZERO(&rdfs);
	FD_SET(fd, &rdfs);

   while (!stop){
      select(fd + 1, &rdfs, NULL, NULL, NULL);
      if (stop)
         break;
      size = read(fd, ev, sizeof(ev));

      if (size < ev_size) {
        fprintf(stderr, "error size when reading\n");
        return 1;
      }

      for (i = 0; i < size / ev_size; i++){
         if (ev[i].type == EV_ABS){
            switch(ev[i].code)
            {  // A list of event codes that virtual_touchscreen accept
               case ABS_X:
                  ev[i].value = translate_pt(ev[i].value, 0);
                  break;

               case ABS_Y:
                  ev[i].value = translate_pt(ev[i].value, 1);
                  break;

               //case ABS_MT_SLOT:
               //   break;

               case ABS_MT_POSITION_X:
                  ev[i].value = translate_pt(ev[i].value, 0);
                  break;

               case ABS_MT_POSITION_Y:
                  ev[i].value = translate_pt(ev[i].value, 1);
                  break;

               //case ABS_MT_TRACKING_ID:
               //   break;
            }
         }

         //else if (ev[i].type == EV_KEY){
         //   if (ev[i].code == BTN_TOUCH)
         //      ;
         //}
      }

      // Write the events read from touchpad, then write all of it to virtual touchscreen
      // virtual touchscreen will ignore any events it cannot process
      write(vts_fd, ev, size);
   }
	return 0;
}

void print_usage(){
   const char* cmd_name = "tpats";
   printf("Usage:\n    %s [touchpad_number virtual_touchscreen_number]\n", cmd_name);
   printf("\n'touchpad_number' and 'virtual_touchscreen_number' is the number of the event file (/dev/input/eventX)\n");
   printf("Leave blank if unsure, program will try to detect the files, if they are not found, you will be prompted to choose from the given list\n");
}

int main(int argc, char** argv)
{
   if (argc > 1){
      if (argc != 3){
         printf("please give both event file numbers as arguement (touchpad then virtual touchscreen), or none at all!");
      }
      tp_device_path = (char *) malloc(64);
      sprintf(tp_device_path, "%s/%s%s", DEV_INPUT_EVENT, EVENT_DEV_NAME, argv[1]);
      vts_device_path = (char *)malloc(64);
      sprintf(vts_device_path, "%s/%s%s", DEV_INPUT_EVENT, EVENT_DEV_NAME, argv[2]);
   }

   else{
      if (scan_devices(&tp_device_path, &vts_device_path))
         return EXIT_FAILURE;
   }

	int ret = init_dev_event_reader();
	if (ret == 1)
		return EXIT_FAILURE;

   if (MAP_TO_ENTIRE_SCREEN){
      geometry_x = vts_device_absdata.min_value_abs_x;
      geometry_y = vts_device_absdata.min_value_abs_y;
      geometry_w = vts_device_absdata.max_value_abs_x - vts_device_absdata.min_value_abs_x;
      geometry_h = vts_device_absdata.max_value_abs_y - vts_device_absdata.min_value_abs_y;
   }

   printf("geo: %d %d %d %d",
   geometry_x,geometry_y,geometry_w, geometry_h);


	signal(SIGINT, interrupt_handler);
	signal(SIGTERM, interrupt_handler);

   event_listener_loop();
   close(fd);
   close(vts_fd);
	ioctl(fd, EVIOCGRAB, (void*)0);

   return EXIT_SUCCESS;
}
