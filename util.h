void fill_zero(char* a, int n)
{
    for(int i=0; i<n; i++)
	a[i] = 0;
}


void error(const char *msg)
{
    printf("ERROR: %s\n\n", msg);
    exit(1);
}


void ip_split(unsigned long x)
{
    int i = 64;  
    int w = 1;
    int ans = 0;
    while(i--)
    {
	ans += w*((int)(x&(unsigned long)1));
	w = w*2;
	x = x>>1;
	if(i%8 == 0)
	{
	    printf("byte %d : %d\n", (i/8)+1, ans);
	    w = 1;
	    ans = 0;
	}
    }
}


int is(char* a, char* b)
{
    if(a == NULL && b == NULL) return 1;
    if(a == NULL) return 0;
    if(b == NULL) return 0;

    int i = 0, j = 0;
    while(1)
    {
	if(a[i] != b[i]) return 0;

	// after this point a[i] = b[i]
	if(a[i] == '\0') return 1; // both words end

	i++;
    }
    // should not reach here
    return 0;
}

void copy1(char** a, char* b) // reallocates a
{
    // printf("copy1: b = %s\n\n", b);
    int n = 0;
    while(b[n] != '\0')
	n++;
    (*a) = (char*)malloc((n+1)*sizeof(char));
    for(int i=0; i<=n; i++)
    {
	(*a)[i] = b[i];
    }
}

void copy2(char* a, char* b) // does not reallocate a
{
    int i = 0;
    while(b[i] != '\0')
    {
	a[i] = b[i]; i++;
    }
    a[i] = '\0';
}

void append1(char** a, char* b) // reallocates a 
{
    char* ta = (*a);
    int n = 0;
    while(ta[n] != '\0')
	n++;
    int m = 0;
    while(b[m] != '\0')
	m++;
    
    char* tmp = (char*)malloc((m+n+1)*sizeof(char));

    for(int i=0; i<n; i++)
	tmp[i] = ta[i];

    // free(ta);

    for(int i=0; i<m; i++)
	tmp[n+i] = b[i];

    tmp[n+m] = '\0';
    (*a) = tmp;
}
void append2(char* a, char* b) // does not reallocate a
{
    if(a == NULL) return;
    if(b == NULL) return;

    int i = 0;
    while(a[i] != '\0') i++;

    int j = 0;
    while(b[j] != '\0') { a[i] = b[j]; i++; j++; }
    a[i] = '\0';
}

char* to_string1(int x)
{
    char* tmp = (char*)malloc(10*sizeof(char));
    char* ans = (char*)malloc(10*sizeof(char));
    ans[0] = '0';
    for(int i=1; i<10; i++) ans[i] = '\0';

    int i = 0;
    while(x)
    {
	tmp[i] = x%10 + '0';
	i++; x = x/10;
    }
    i--;

    int j = 0;
    while(i >= 0)
    {
	ans[j++] = tmp[i];
	i--;
    }
    free(tmp);
    return ans;
}

int to_int1(char* a)
{
    int ans = 0;
    if(a == NULL) return 0;
    int n = 0;
    while(a[n] != '\0') n++;
    int i = 0;
    while(i < n)
    {
	ans = ans*10 + (a[i]-'0');
	i++;
    }
    return ans;
}

int valid(char* a) // valid password
{
    if(a == NULL) return 0;
    int n = 0;
    while(a[n] != '\0') n++;
    if(n < 3) return 0;
    return 1;
}

struct coord{
    int x;
    int y;
};

void show_byte(FILE* fp, unsigned char a)
{
    int x = (int)a&15;
    int y = a>>4;

    if(y < 10) fprintf(fp, "%d", y);
    else fprintf(fp, "%c", 'A'+(y-10));

    if(x < 10) fprintf(fp, "%d", x);
    else fprintf(fp, "%c", 'A'+(x-10));
}



void appendfrontchar(char* a, char x)
{
    int n = 0; 
    while(a[n] != '\0') n++;

    for(int i = n; i >= 1; i--)
	a[i] = a[i-1];

    a[0] = (char)x;
}

int min(int a, int b)
{
    if(a < b) return a;
    return b;
}

char* get_word(char* a)
{
    int i = 0, n = 0;
    while(a[n] != ' ') n++;
    char* ans = (char*)malloc((n+1)*sizeof(char));
    for(int i=0; i<n; i++) ans[i] = a[i];
    ans[n] = '\0';
    return ans;
}

int is_user(char* a, int n, char* b, int m)
{
    if(n != m) return 0;
    for(int i=0; i<n; i++)
	if(a[i] != b[i]) return 0;
    return 1;
}

void remove_user(char* a, char* b)
{
    int n = 0;
    while(a[n] != '\0') n++;
    int m = 0;
    while(b[m] != '\0') m++; 

    int start = 0; int from = -1, to = -1;
    for(int i=0; i<n; i++)
    {
	if(a[i] == '\n')
	{
	    if(is_user(a+start, i-start, b, m))
	    {
		from = i+1;
		to = start;
		break;
	    }
	    start = i+1;
	}
    }
    if(from == -1) return; // user name not found

    // removing
    while(a[from] != '\0')
    {
	a[to] = a[from];
	to++; from++;
    }
    while(a[to] != '\0')
    {
	a[to] = '\0';
	to++;
    }
}

void add_user(char* a, char* b)
{
    int i = 0;
    while(a[i] != '\0') i++;
    int j = 0;
    while(b[j] != '\0') { a[i] = b[j]; i++; j++; }
    a[i] = '\n';
}





