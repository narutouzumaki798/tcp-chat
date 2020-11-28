#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <ncurses.h>
#include <sys/mman.h>
#include <semaphore.h>


#include "util.h"

#define debug 0


// network
char user_name[50];
char password[50];
unsigned char buffer[10000];
unsigned char img_buffer[1000000];
int sockfd, portno, n;
struct sockaddr_in serv_addr;
struct hostent *server;
char recipients_buffer[10000];

// curses
WINDOW* input_box;
WINDOW* output_box;
WINDOW* control_box;
int corner1x, corner1y, corner2x, corner2y, corner3x, corner3y;
int width, height1, height2; // dimensions of boxes
struct coord disp1;
struct coord disp2;
int* scroll_start;
int* hard;


// debug
FILE* err_fp;


// processes
sem_t* mutex1;
int pid;
char* output_buffer; // receiver
char* input_buffer; // sender
char** display1;
int* quit;
char* users_list;


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




int is_normal_character(char ch)
{
    int x = (int)ch;
    if(x >= 32 && x <= 126) return 1;
    return false;
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


void send_image(char* img) // send image marker
{
    FILE* img_fp = fopen(img, "rb");
    if(img_fp == NULL) return; // image does not exist

    int i = 0; while(i < 1000000) img_buffer[i++] = '\0'; // clear buffer

    i = 0; while(fread(img_buffer+i, 1, 1, img_fp)) i++; // read image
    int N = i;

    i = 0;
    if(debug) // printing image bytes
    {
	while(i < N)
	{
	    show_byte(err_fp, img_buffer[i]);
	    fprintf(err_fp, " ");
	    i++;
	    if(i%16 == 0) fprintf(err_fp, "\n"); // hexdump er moto 16 byte kore ek line e
	    fflush(err_fp);
	}
	fprintf(err_fp, "\n");
	fprintf(err_fp, "bytes: %d\n", N); fflush(err_fp);
    }

    // image header
    int img_size = N; fill_zero(buffer, 5000);
    if(recipients_buffer[0] == '\0')
    {
	buffer[0] = 'i'; i = 1;
    }
    else
    {
	buffer[0] = 's';
	append2(buffer, recipients_buffer);
	i = strlen(recipients_buffer) + 1;
	buffer[i] = (char)1; // delimeter
	i++;
    }
    buffer[i] = (img_size & 0xFF000000)>>24; i++;
    buffer[i] = (img_size & 0x00FF0000)>>16; i++;
    buffer[i] = (img_size & 0x0000FF00)>>8;  i++;
    buffer[i] = (img_size & 0x000000FF); i++;
    write(sockfd, buffer, i); // age size pathate hobe; final i = total length


    // image
    // buffer[0] = 'a';
    // buffer[1] = 'b';
    // buffer[2] = 0;
    // buffer[3] = 'c';
    // buffer[4] = 'd';
    // buffer[5] = 'e';
    // buffer[6] = 'f';
    // buffer[7] = '\0';

    int sent_bytes = 0;
    while(sent_bytes < img_size) // ek bare hoe jacche mone hoe
    {
	int x = write(sockfd, img_buffer+sent_bytes, img_size-sent_bytes);
	sent_bytes += x;
	if(debug) fprintf(err_fp, "debug: send_image sending %d/%d bytes\n", sent_bytes, img_size); fflush(err_fp);
    }

    // FILE* test_fp = fopen("test.png", "wb");
    // fwrite(img_buffer, 1, N, test_fp);
    // fclose(test_fp);
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
	if(debug) fprintf(err_fp, "debug img_sender: %3d (%c)\n", (int)ch, ch); fflush(err_fp);

	sem_wait(mutex1);
	switch((int)ch)
	{
	case 127: // backspace
	    if(idx > 7)
	    {
		idx--; 
		input_buffer[idx] = '\0';
	    }
	    break;

	case 10: // enter
	    if(idx == 0) break;
	    send_image(input_buffer+7); // send img-location string to send_image() function
	    idx = 0;
	    while(input_buffer[idx] != '\0') { input_buffer[idx] = '\0'; idx++; } // erase buffer
	    sem_post(mutex1);
	    return;

	case 16: // ctrl + p
	    if(debug) fprintf(err_fp, "debug sender: ctrl p --\n"); fflush(err_fp);
	    if((*scroll_start) > 1)
		(*scroll_start) = (*scroll_start) - 1;
	    break;

	case 14: // ctrl + n
	    if(debug) fprintf(err_fp, "debug sender: ctrl n ++\n"); fflush(err_fp);
	    if((*scroll_start) < 1000-1)
		(*scroll_start) = (*scroll_start) + 1;
	    break;
	    
	case 9: // ctrl + i
	    idx = 0;
	    while(input_buffer[idx] != '\0') { input_buffer[idx] = '\0'; idx++; } // erase buffer
	    sem_post(mutex1);
	    return;

	case 27: // ESC
	    idx = 0;
	    while(input_buffer[idx] != '\0') { input_buffer[idx] = '\0'; idx++; } // erase buffer
	    sem_post(mutex1);
	    return;

	default:
	   if(is_normal_character(ch))
		input_buffer[idx++] = ch;
	    break;
	}
	sem_post(mutex1);
    }

}

void recipient() // recipient marker
{
    // fprintf(err_fp, "sending: %s\n", user_name); fflush(err_fp);
    // n = write(sockfd, user_name, strlen(user_name));
    int idx = 0, r_idx = 0;
    while(input_buffer[idx] != '\0') { input_buffer[idx++] = '\0'; } // erase buffer
    copy2(input_buffer, "recipients: "); idx = 12; // prompt
    while(recipients_buffer[r_idx] != '\0') input_buffer[idx++] = recipients_buffer[r_idx++]; // copy recipient list
    while(1) 
    {
	char ch = getch();
	fprintf(err_fp, "debug sender: %3d (%c)\n", (int)ch, ch); fflush(err_fp);

	sem_wait(mutex1);
	switch((int)ch)
	{
	case 127: // backspace
	    if(idx > 12) // cannot delete promt
	    {
		idx--; 
		input_buffer[idx] = '\0';
		r_idx--;
		recipients_buffer[r_idx] = '\0';
	    }
	    break;

	case 10: // enter
	    if(idx == 0) break;
	    idx = 0;
	    while(input_buffer[idx] != '\0') { input_buffer[idx] = '\0'; idx++; } // erase buffer
	    sem_post(mutex1);
	    return;

	case 16: // ctrl + p
	    fprintf(err_fp, "debug sender: ctrl p --\n"); fflush(err_fp);
	    if((*scroll_start) > 1)
		(*scroll_start) = (*scroll_start) - 1;
	    *hard = 1;
	    break;

	case 14: // ctrl + n
	    fprintf(err_fp, "debug sender: ctrl n ++\n"); fflush(err_fp);
	    if((*scroll_start) < 1000-1)
		(*scroll_start) = (*scroll_start) + 1;
	    *hard = 1;
	    break;

	case 5: // ctrl + e
	    fprintf(err_fp, "debug sender: ctrl e img\n"); fflush(err_fp);
	    system("feh -d -x -g 960x720 test.png");
	    *hard = 1;
	    break;

	case 18: // ctrl + r
	    idx = 0;
	    while(input_buffer[idx] != '\0') { input_buffer[idx] = '\0'; idx++; } // erase buffer
	    sem_post(mutex1);
	    return;

	case 27: // ESC
	    idx = 0;
	    while(input_buffer[idx] != '\0') { input_buffer[idx] = '\0'; idx++; } // erase buffer
	    sem_post(mutex1);
	    return;

	default:
	    if(is_normal_character(ch))
	    {
		input_buffer[idx++] = ch;
		recipients_buffer[r_idx++] = ch;
	    }
	    break;
	}
	sem_post(mutex1);
	// fprintf(err_fp, "debug sender: exit semaphore\n"); fflush(err_fp);
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
	switch((int)ch)
	{
	case 127: // backspace
	    if(idx > 0)
	    {
		idx--; 
		input_buffer[idx] = '\0';
	    }
	    break;

	case 10: // enter
	    if(idx == 0) break;
	    if(recipients_buffer[0] == '\0') // no recipient list
	    {
		appendfrontchar(input_buffer, 't');
		n = write(sockfd, input_buffer, strlen(input_buffer)); // send input_buffer
	    }
	    else
	    {
		char* tmp_buffer = (char*)malloc(5000*sizeof(char));
		fill_zero(tmp_buffer, 5000); tmp_buffer[0] = 'r'; // decision byte
		append2(tmp_buffer, recipients_buffer); // recipients list
		appendfrontchar(input_buffer, (char)1); // delimeter
		append2(tmp_buffer, input_buffer); // message
		n = write(sockfd, tmp_buffer, strlen(tmp_buffer));
	    }
	    idx = 0;
	    while(input_buffer[idx] != '\0') { input_buffer[idx] = '\0'; idx++; } // erase buffer
	    idx = 0;
	    break;

	case 12: // ctrl + l
		input_buffer[idx++] = '\n';

	case 16: // ctrl + p
	    if(debug) fprintf(err_fp, "debug sender: ctrl p --\n"); fflush(err_fp);
	    if((*scroll_start) > 1)
		(*scroll_start) = (*scroll_start) - 1;
	    *hard = 1;
	    break;

	case 14: // ctrl + n
	    if(debug) fprintf(err_fp, "debug sender: ctrl n ++\n"); fflush(err_fp);
	    if((*scroll_start) < 1000-1)
		(*scroll_start) = (*scroll_start) + 1;
	    *hard = 1;
	    break;

	case 5: // ctrl + e
	    fprintf(err_fp, "debug sender: ctrl e img\n"); fflush(err_fp);
	    system("feh -d -x -g 960x720 test.png");
	    *hard = 1;
	    break;
	    
	case 9: // ctrl + i 
	    sem_post(mutex1);
	    img_sender(); 
	    idx = 0;
	    sem_wait(mutex1);
	    break;

	case 18: // ctrl + r
	    sem_post(mutex1);
	    recipient(); 
	    idx = 0;
	    sem_wait(mutex1);
	    break;

	case 27: // ESC
	    *quit = 1;
	    sem_post(mutex1);
	    return;

	default:
	    if(is_normal_character(ch))
		input_buffer[idx++] = ch;
	    break;
	} // switch
	sem_post(mutex1);
    }
}

int getbyte(int start, int idx) // faltu
{
    int i = start + idx*2; 
    int u = (buffer[i]>=0 && buffer[i]<=9)? (buffer[i]-'0'):(buffer[i] - 'a' + 10);
    i++;
    int l = (buffer[i]>=0 && buffer[i]<=9)? (buffer[i]-'0'):(buffer[i] - 'a' + 10);
    int ans = (u<<4)|l;
    return ans;
}

void receive_image(int first_read) // receive image marker
{
    if(debug) fprintf(err_fp, "debug receive image: first_read: %d\n", first_read); fflush(err_fp);
    int i, img_idx = 0, read_bytes = 0;
    i = 0; while(i < 1000000) img_buffer[i++] = '\0'; // clear buffer
    int img_size = (((int)buffer[1]) << 24) | (((int)buffer[2]) << 16) | (((int)buffer[3]) << 8) | (((int)buffer[4]));
    i = 5; img_idx = 0;
    while(i < first_read) img_buffer[img_idx++] = buffer[i++]; // prothom bare jotota eseche
    read_bytes = first_read - 5;
    if(debug) fprintf(err_fp, "read_bytes=%d  img_size=%d\n", read_bytes, img_size); fflush(err_fp);
    while(read_bytes < img_size)
    {
	if(debug) fprintf(err_fp, "receiving image: %d / %d bytes\n", read_bytes, img_size); fflush(err_fp);
	fill_zero(buffer, 1024);
	int x = read(sockfd, buffer, min(img_size-read_bytes, 1024)); // dorkari jinis
	i = 0; while(i < x) img_buffer[img_idx++] = buffer[i++];
	read_bytes += x;
    }
    if(debug) fprintf(err_fp, "finished receiving image: %d / %d bytes\n", read_bytes, img_size); fflush(err_fp);
    FILE* test_fp = fopen("test.png", "wb");
    fwrite(img_buffer, 1, img_size, test_fp);
    fclose(test_fp);
}

void receiver() // receiver marker
{
    // fprintf(err_fp, "debug reciver 1\n"); fflush(err_fp);

    int idx = 0, disconnect_counter = 0, i;
    while(1)
    {
	fill_zero(buffer, 1024); // reset buffer
	int read_bytes = read(sockfd, buffer, 1024);

	if(read_bytes == 0)
	{
	    disconnect_counter++;
	    if(disconnect_counter > 10) exit(1);
	}
	if(buffer[0] == 0) continue;

	if(debug)
	{
	    fprintf(err_fp, "received [%d]: ", read_bytes);
	    for(int i=0; i<20; i++)
		fprintf(err_fp, "%d(%c) ", (int)buffer[i], (char)buffer[i]);
	    fprintf(err_fp, "\n"); fflush(err_fp);
	}

	switch(buffer[0])
	{
	case 'i': // image eseche
	    receive_image(read_bytes);   
	    break;

	case 't': // text eseche
	    i = 1;
	    sem_wait(mutex1);
	    while(buffer[i] != '\0') output_buffer[idx++] = buffer[i++];
	    int tmp = width-2; while(tmp--) output_buffer[idx++] = '-'; // output_buffer[idx++] = '\n';
	    sem_post(mutex1);
	    break;

	} // switch
	if(*quit) { sem_post(mutex1); return; }

    }
}



void controls()
{
    control_box = newwin(12, 30, corner3y, corner3x);
    box(control_box, 0, 0); int i = 1;
    mvwaddstr(control_box, i, 1, "Controls:"); i++;
    mvwaddstr(control_box, i, 1, "---------"); i++;
    mvwaddstr(control_box, i, 1, "Enter: send message"); i++;
    mvwaddstr(control_box, i, 1, "Ctrl+L: insert new line"); i++;
    mvwaddstr(control_box, i, 1, "Ctrl+P: scroll up"); i++;
    mvwaddstr(control_box, i, 1, "Ctrl+N: scroll down"); i++;
    mvwaddstr(control_box, i, 1, "Ctrl+I: select image to send"); i++;
    mvwaddstr(control_box, i, 1, "Ctrl+E: view received image"); i++;
    mvwaddstr(control_box, i, 1, "Ctrl+R: recipients list"); i++;
    mvwaddstr(control_box, i, 1, "ESC: exit"); i++;

    wrefresh(control_box);
}
void reset_win(WINDOW* win)
{
    werase(win);
    box(win, 0, 0);
}
void hard_reset()
{
    clear(); refresh();
    // werase(input_box);  wrefresh(input_box);
    // werase(output_box); wrefresh(output_box);
    delwin(input_box);  delwin(output_box); 
    delwin(control_box);
    
    output_box = newwin(height1, width, corner1y, corner1x);
    input_box = newwin(height2, width, corner2y, corner2x);
    box(output_box, 0, 0);
    box(input_box, 0, 0);
    controls();

    *hard = 0;
}
void display() // display marker
{
    output_box = newwin(height1, width, corner1y, corner1x);
    input_box = newwin(height2, width, corner2y, corner2x);
    controls();

    box(output_box, 0, 0);
    box(input_box, 0, 0);

    int idx1 = 0, idx2 = 0; 
    disp1.x = 1; disp1.y = 1;
    disp2.x = 1; disp2.y = 1;

    int itr = 0;
    while(1)
    {
	reset_win(input_box);

	sem_wait(mutex1); // critical start
	if(*hard) hard_reset();

	// display 2 maker (intput box)
	idx2 = 0; disp2.x = 1; disp2.y = 1;
	while(input_buffer[idx2] != '\0')
	{
	    if(input_buffer[idx2] == '\n')
	    {
		disp2.x = 1; disp2.y++; idx2++;
		continue;
	    }
	    mvwaddch(input_box, disp2.y, disp2.x, input_buffer[idx2]);
	    idx2++;
	    disp2.x++;
	    if(disp2.x == width-1) { disp2.x = 1; disp2.y++; }
	}
	wattron(input_box, A_REVERSE);
	mvwaddch(input_box, disp2.y, disp2.x, ' ');
	wattroff(input_box, A_REVERSE);		      // cursor

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
	if(*quit) { sem_post(mutex1); return; }
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
  corner3x = 75; corner3y = 10;
  height1 = 32; height2 = 8;
  width = 70;
}


void login()
{
    printf("user name:\n");
    while(1)
    {
	printf(">> ");
	fill_zero(user_name, 50); scanf("%s", user_name);
	if(is(user_name, "you"))
	{
	    printf("reseved word\n");
	    continue;
	}
	else if(!valid(user_name))
	{
	    printf("invalid username\n");
	    continue;
	}
	break;
    }
    write(sockfd, user_name, strlen(user_name)); // send to server [write 1]
    fill_zero(buffer, 1024); read(sockfd, buffer, 2); // [read 1]
    if(is(buffer, "no")) // old user name
    {
	printf("Enter Password: ");
	fill_zero(password, 50); scanf("%s", password);
	write(sockfd, password, strlen(password)); // [write 2]
	fill_zero(buffer, 1024); read(sockfd, buffer, 2); // [read 2]
	if(!is(buffer, "ok"))
	{
	    printf("Wrong Password\n\n");
	    exit(1);
	}
    }
    else // new user name
    {
	fill_zero(password, 50);
	while(!valid(password))
	{
	    printf("Create Password: ");
	    fill_zero(password, 50); scanf("%s", password);
	}
	write(sockfd, password, strlen(password)); // [write 2]
	fill_zero(buffer, 1024); read(sockfd, buffer, 2); // faltu [read 2]
    }
    if(debug) fprintf(err_fp, "asking for users list\n"); fflush(err_fp);
    write(sockfd, "u", 1); // asking for users list
    read(sockfd, users_list, 10000);
    write(sockfd, "thik ache", 9); // faltu
    if(debug) fprintf(err_fp, "debug login users_list: %s\n", users_list); fflush(err_fp);
}


void precomp()
{
    for(int i=0; i<10000; i++) 
    {
	output_buffer[i] = '\0';
	input_buffer[i] = '\0';
	recipients_buffer[i] = '\0';
	users_list[i] = '\0';
    }

    display1 = (char**)malloc(1000*sizeof(char*));
    for(int i=0; i<1000; i++)
    {
	display1[i] = (char*)malloc(80*sizeof(char)); // allocate 2d display array
	for(int j=0; j<80; j++)
	    display1[i][j] = ' ';
    }
    *scroll_start = 1;
    *hard = 0;
}


int main(int argc, char *argv[])
{
    err_fp = fopen("debug.txt", "w");
    setup_connection(argc, argv);

    users_list = (char*)create_shared_memory(10000*sizeof(char)); // eta age lagbe
    fill_zero(users_list, 10000);
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
    hard = (int*)create_shared_memory(sizeof(int));
    quit = (int*)create_shared_memory(sizeof(int));
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

    wait(NULL);
    endwin();
    return 0;
}





