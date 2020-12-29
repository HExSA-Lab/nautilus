

#define NONCANONICAL_ADDR (void*)0xdeafbeef10000000ULL;

void *stdin=NONCANONICAL_ADDR;
void *stdout=NONCANONICAL_ADDR;
void *stderr=NONCANONICAL_ADDR;


int isdigit(int c)
{
    return c>='0' && c<='9';
}

int toupper(int c)
{
    return (c>='a' && c<='z') ? c-'a'+'A' : c;
}

