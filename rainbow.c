#define RAINBOW_FILEMODE (S_IRUSR | S_IWUSR)
#define RAINBOW_OPENMODE (O_RDWR | O_CREAT | O_TRUNC)

enum {
  SHM_NORMBORDERCOLOR = 0,
  SHM_SELBORDERCOLOR  = 1,
  SHM_NORMBGCOLOR     = 2,
  SHM_NORMFGCOLOR     = 3,
  SHM_SELBGCOLOR      = 4,
  SHM_SELFGCOLOR      = 5
};

static int rainbowfd[6];
static const char *rainbownames[] = {
  "/dwm-normbordercolor",
  "/dwm-selbordercolor",
  "/dwm-normbgcolor",
  "/dwm-normfgcolor",
  "/dwm-selbgcolor",
  "/dwm-selfgcolor",
  NULL
};

void
exportrainbow(const char *p, const char *v) {
  int fd = shm_open(p, O_WRONLY | O_CREAT | O_TRUNC, RAINBOW_FILEMODE);
  write(fd, v, strlen(v));
  close(fd);
}

#define RAINBOWFD(idx, var) exportrainbow(rainbownames[(idx)], (var))
void
initrainbow(void) {
  RAINBOWFD(SHM_NORMBORDERCOLOR,  normbordercolor);
  RAINBOWFD(SHM_SELBORDERCOLOR,   selbordercolor);
  RAINBOWFD(SHM_NORMBGCOLOR,      normbgcolor);
  RAINBOWFD(SHM_NORMFGCOLOR,      normfgcolor);
  RAINBOWFD(SHM_SELBGCOLOR,       selbgcolor);
  RAINBOWFD(SHM_SELFGCOLOR,       selfgcolor);
}
#undef RAINBOWFD

void
sigusr1(int unused) {
  runrainbow();
}

char*
maprainbow(int i) {
  switch(i) {
    case SHM_NORMBORDERCOLOR:
      return normbordercolor;
    case SHM_SELBORDERCOLOR:
      return selbordercolor;
    case SHM_NORMBGCOLOR:
      return normbgcolor;
    case SHM_NORMFGCOLOR:
      return normfgcolor;
    case SHM_SELBGCOLOR:
      return selbgcolor;
    case SHM_SELFGCOLOR:
      return selfgcolor;
  }
  return NULL;
}

void
runrainbow(void) {
  LERROR(0,0, "runrainbow()");
  char buf[8];
  char *cur;
  int fd;
  for(int i = 0; i < 6; i++) {
    fd = shm_open(rainbownames[i], O_RDONLY, RAINBOW_FILEMODE); 
    if(fd == -1)
      continue;
    read(fd, buf, 7); /* read: #abcdef, excl. NULL */
    close(fd);

    buf[7] = '\0'; /* NULL-terminate colour string */
    cur = maprainbow(i);
    if(strcmp(buf, cur) == 0)
      continue; /* no change */
    LERROR(0,0, "runrainbow: %s: %s => %s\n", rainbownames[i], cur, buf);
  }
  return;
}

void
uninitrainbow(void) {
  for(int i = 0; i < 6; i++)
    shm_unlink(rainbownames[i]);
  return;
}
