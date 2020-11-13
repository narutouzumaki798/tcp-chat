#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <ncurses.h>
#include <sys/mman.h>
#include <semaphore.h>


#include "util.h"


// network
char user_name[50];
char buffer[1024];
int sockfd, portno, n;
struct sockaddr_in serv_addr;
struct hostent *server;

// curses
WINDOW* input_box;
WINDOW* output_box;
int corner1x, corner1y, corner2x, corner2y;
int width, height1, height2; // dimensions of boxes
struct coord disp1;
struct coord disp2;
int* scroll_start;


// debug
FILE* err_fp;


// processes
sem_t* mutex1;
int pid;
char* output_buffer; // receiver
char* input_buffer; // sender
char** display1;


void* create_shared_memory(size_t size) {
  // Our memory buffer will be readable and writable:
  int protection = PROT_READ | PROT_WRITE;

  // The buffer will be shared (meaning other processes can access it), but
  // anonymous (meaning third-party processes cannot obtain an address for it),
  // so only this process and its children will be able to use it:
  int visibility = MAP_SHARED | MAP_ANONYMOUS;

  // The remaining parameters to `mmap()` are not important for this use case,
  // but the manpage for `mmap` explains their purpose.
  return mmap(NULL, size, protection, visibility, -1, 0);
}




void generate_message(int n, char** arr)
{
    buffer[0] = 'a';
    buffer[1] = 'b';
    buffer[2] = 'c';
    buffer[3] = '\0';

    /* int idx = 0; */
    /* for(int i=3; i<n; i++) */
    /* { */
    /* 	int j = 0; */
    /* 	while(arr[i][j] != '\0') */
    /* 	{ */
    /* 	    buffer[idx++] = arr[i][j]; */
    /* 	    j++; */
    /* 	} */
    /* 	buffer[idx++] = ' '; */
    /* } */
    /* buffer[idx] = '\0'; */
}



void setup_connection(int argc, char* argv[])
{
    const char* target_serv_addr = "192.168.0.104";
    const char* target_portno = "8001";

    if (argc >= 2)
	target_serv_addr = argv[1];
    if (argc >= 3)
	target_portno = argv[2];

    portno = atoi(target_portno);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) 
        error("ERROR opening socket");

    server = gethostbyname(target_serv_addr);
    if (server == NULL) {
        error("ERROR, no such host\n");
        exit(0);
    }

    fill_zero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
}


void send_image(char* img)
{
    FILE* img_fp = fopen(img, "rb");
    unsigned char img_buffer[1000000];
    int i = 0;
    while(fread(img_buffer+i, 1, 1, img_fp)) i++;
    int N = i; i = 0;
    while(i < N)
    {
	show_byte(err_fp, img_buffer[i]);
	fprintf(err_fp, " ");
	i++;
	if(i%16 == 0) fprintf(err_fp, "\n");
	fflush(err_fp);
    }
    fprintf(err_fp, "\n");

    FILE* test_fp = fopen("test.png", "wb");
    fwrite(img_buffer, 1, N, test_fp);
    fclose(test_fp);
    fclose(img_fp);
}


void img_sender() // img sender marker
{
    int idx = 0;
    while(input_buffer[idx] != '\0') { input_buffer[idx++] = '\0'; } // erase buffer
    copy2(input_buffer, "image: "); idx = 7;
    while(1)
    {
	char ch = getch();
	fprintf(err_fp, "debug img_sender: %3d (%c)\n", (int)ch, ch); fflush(err_fp);

	sem_wait(mutex1);
	if((int)ch == 127) // backspace (not allowed to remove image: prompt)
	{
	    if(idx > 7)
	    {
		idx--; 
		input_buffer[idx] = '\0';
	    }
	}
	else if((int)ch == 10 && idx != 0) // enter
	{
	    send_image(input_buffer+7); // send img-location string to send_image() function
	    idx = 0;
	    while(input_buffer[idx] != '\0') { input_buffer[idx] = '\0'; idx++; } // erase buffer
	    sem_post(mutex1);
	    return;
	}
	else if((int)ch == 16) // ctrl + p
	{
	    fprintf(err_fp, "debug sender: ctrl p --\n"); fflush(err_fp);
	    if((*scroll_start) > 1)
		(*scroll_start) = (*scroll_start) - 1;
	}
	else if((int)ch == 14) // ctrl + n
	{
	    fprintf(err_fp, "debug sender: ctrl n ++\n"); fflush(err_fp);
	    if((*scroll_start) < 1000-1)
		(*scroll_start) = (*scroll_start) + 1;
	}
	else if((int)ch ==  9) // ctrl + i
	{
	    idx = 0;
	    while(input_buffer[idx] != '\0') { input_buffer[idx] = '\0'; idx++; } // erase buffer
	    sem_post(mutex1);
	    return;
	}
	else
	input_buffer[idx++] = ch;
	sem_post(mutex1);
    }

}
void sender() // sender marker
{
    // fprintf(err_fp, "sending: %s\n", user_name); fflush(err_fp);
    // n = write(sockfd, user_name, strlen(user_name));
    int idx = 0;
    while(1) 
    {
	// sleep(5);
	// string tmp = "abcd";
	// fprintf(err_fp, "sending: %s\n", tmp.c_str()); fflush(err_fp);
	// n = write(sockfd, tmp.c_str(), (int)tmp.size());
	char ch = getch();
	fprintf(err_fp, "debug sender: %3d (%c)\n", (int)ch, ch); fflush(err_fp);

	sem_wait(mutex1);
	if((int)ch == 127) // backspace
	{
	    if(idx > 0)
	    {
		idx--; 
		input_buffer[idx] = '\0';
	    }
	}
	else if((int)ch == 10 && idx != 0) // enter
	{
	    n = write(sockfd, input_buffer, strlen(input_buffer)); // send data

	    idx = 0;
	    while(input_buffer[idx] != '\0') { input_buffer[idx] = '\0'; idx++; } // erase buffer
	    idx = 0;
	}
	else if((int)ch == 16) // ctrl + p
	{
	    fprintf(err_fp, "debug sender: ctrl p --\n"); fflush(err_fp);
	    if((*scroll_start) > 1)
		(*scroll_start) = (*scroll_start) - 1;
	}
	else if((int)ch == 14) // ctrl + n
	{
	    fprintf(err_fp, "debug sender: ctrl n ++\n"); fflush(err_fp);
	    if((*scroll_start) < 1000-1)
		(*scroll_start) = (*scroll_start) + 1;
	}
	else if((int)ch == 5) // ctrl + e
	{
	    fprintf(err_fp, "debug sender: ctrl e img"); fflush(err_fp);
	    system("feh img1.png");
	}
	else if((int)ch ==  9) // ctrl + i
	{
	    sem_post(mutex1);
	    img_sender(); 
	    sem_wait(mutex1);
	}
	else
	input_buffer[idx++] = ch;
	sem_post(mutex1);

	// fprintf(err_fp, "debug sender: exit semaphore\n"); fflush(err_fp);
    }
}

void receiver() // receiver marker
{
    // fprintf(err_fp, "debug reciver 1\n"); fflush(err_fp);

    int idx = 0;
    while(1)
    {
	fill_zero(buffer, 1024); // reset buffer
	n = read(sockfd, buffer, 1024);
	fprintf(err_fp, "received: %s\n", buffer); fflush(err_fp);  fflush(err_fp);
	int i = 0;

	sem_wait(mutex1);
	while(buffer[i] != '\0') output_buffer[idx++] = buffer[i++];
	sem_post(mutex1);

    }
}




void reset_win(WINDOW* win)
{
    werase(win);
    box(win, 0, 0);
}

void display() // display marker
{
    output_box = newwin(height1, width, corner1y, corner1x);
    input_box = newwin(height2, width, corner2y, corner2x);
    box(output_box, 0, 0);
    box(input_box, 0, 0);

    int idx1 = 0, idx2 = 0; 
    disp1.x = 1; disp1.y = 1;
    disp2.x = 1; disp2.y = 1;

    int itr = 0;
    while(1)
    {
	sem_wait(mutex1); // critical start

	// display 2 maker (intput box)
	idx2 = 0; disp2.x = 1; disp2.y = 1;
	reset_win(input_box);
	while(input_buffer[idx2] != '\0')
	{
	    mvwaddch(input_box, disp2.y, disp2.x, input_buffer[idx2]);
	    idx2++;
	    disp2.x++;
	    if(disp2.x == width-1) { disp2.x = 1; disp2.y++; }
	}
	wattron(input_box, A_REVERSE);
	mvwaddch(input_box, disp2.y, disp2.x, ' ');
	wattroff(input_box, A_REVERSE);               // cursor

	// display 1 marker (output box)
	while(output_buffer[idx1] != '\0')
	{
	    if(output_buffer[idx1] == '\n')
	    {
		disp1.x = 1; disp1.y++;
		idx1++;
		continue;
	    }
	    // mvwaddch(output_box, disp1.y, disp1.x, output_buffer[idx1]); 
	    display1[disp1.y][disp1.x] = output_buffer[idx1];
	    idx1++;
	    disp1.x++;
	    if(disp1.x == width - 1) { disp1.x = 1; disp1.y++; }
	}

	sem_post(mutex1); // critical end

	reset_win(output_box);
	int i = (*scroll_start);
	while(i < (*scroll_start) + height1 - 2 && i < 1000)
	{
	    for(int j = 1; j < width-1; j++)
	    {
		mvwaddch(output_box, i - (*scroll_start) + 1, j, display1[i][j]);
	    }
	    i++;
	}
	
	// mvwprintw(output_box, 10, 10, "display: itr=%3d", (itr%1000)); itr++;

	wrefresh(input_box);
	wrefresh(output_box);
	usleep(16000); // refresh display around 60 fps
    }
}

void curses_init()
{
  initscr();
  noecho();
  curs_set(0);
  erase();
  refresh();

  // window parameters
  corner1x = 3; corner1y = 2;
  corner2x = 3; corner2y = 35;
  height1 = 32; height2 = 8;
  width = 70;
}


void login()
{
    printf("user name:\n");
    while(1)
    {
	printf(">> ");
	scanf("%s", user_name);
	write(sockfd, user_name, strlen(user_name));
	read(sockfd, buffer, 1024);
	if(is(user_name, "you"))
	{
	    printf("reseved word\n");
	}
	else if(is(buffer, "no"))
	{
	    printf("user name not available\n");
	}
        else
        {
	    break;
	}
    }

}

void precomp()
{
    for(int i=0; i<10000; i++) 
	output_buffer[i] = '\0';

    for(int i=0; i<10000; i++) 
	input_buffer[i] = '\0';

    display1 = (char**)malloc(1000*sizeof(char*));
    for(int i=0; i<1000; i++)
    {
	display1[i] = (char*)malloc(80*sizeof(char)); // allocate 2d display array
	for(int j=0; j<80; j++)
	    display1[i][j] = ' ';
    }
    *scroll_start = 1;
}


int main(int argc, char *argv[])
{
    err_fp = fopen("debug.txt", "w");
    setup_connection(argc, argv);
    login();

    // while(1)
    // {
    // copy2(buffer, user_name);
    // write(sockfd, buffer, strlen(buffer));
    // sleep(5);
    // }
    // return 0;

    curses_init();

    mutex1 = (sem_t*)create_shared_memory(sizeof(sem_t));
    output_buffer = (char*)create_shared_memory(10000*sizeof(char));
    input_buffer = (char*)create_shared_memory(10000*sizeof(char));
    scroll_start = (int*)create_shared_memory(sizeof(int));
    precomp();

    // sem_t* , whether share b/w process or thread, value
    sem_init(mutex1, 1, 1);

    pid = fork();
    if(pid == 0) sender();
    if(pid == 0) return 0; // creates sender process

    pid = fork();
    if(pid == 0) receiver();
    if(pid == 0) return 0; // creates receiver process

    display(); // parent process used to display

    endwin();
    return 0;
}





