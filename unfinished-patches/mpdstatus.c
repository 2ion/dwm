
timer_t mpdcmd_us_timerid;
struct sigevent mpdcmd_us_se;
struct sigaction mpdcmd_us_sa;
struct itimerspec mpdcmd_us_its = {
    .it_value       = { .tv_sec = 1, .tv_nsec = 0 },
    .it_interval    = { .tv_sec = 1, .tv_nsec = 0 }};

void
run(void) {
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while(running && !XNextEvent(dpy, &ev)) {
		if(handler[ev.type])
			handler[ev.type](&ev);
    }
        /*
        if((time(NULL)-us_clock0) >= 1) {
            updatestatus();
            us_clock0 = time(NULL);
        }
        */
//    }
    /*
    while(running) {
        FD_ZERO(&ums_fds);
        FD_SET(ums_x11_fd, &ums_fds);
        ums_interval.tv_usec = 500;
        ums_interval.tv_sec = 0;
        if(select(ums_x11_fd+1, &ums_fds, NULL, NULL, &ums_interval))
            while(!XNextEvent(dpy, &ev))
                if(handler[ev.type])
                    handler[ev.type](&ev);
        else
            updatestatus();
    }
    */
}


void
updatempdstatus(void) {
    char statustext[STEXTSIZE];
    struct mpd_status *s = NULL;
    struct mpd_song *so = NULL;
    int s_volume,
        s_flagmask,
        s_queuelength,
        s_songpos,
        s_state,
        s_songlen,
        s_songtime;
    int s_time_r, s_time_min, s_time_sec;
    const char *artist, *title;

    if(mpdcmd_connect() != 0)
        return;

    s = mpd_run_status(mpdc);
    if(s == NULL)
        return;

    // retrieve status information

    if((s_state = mpd_status_get_state(s)) == MPD_STATE_STOP ||
        (so = mpd_run_current_song(mpdc)) == NULL)
        goto EXIT;

    s_volume = mpd_status_get_volume(s);
    s_queuelength = mpd_status_get_queue_length(s);
    s_songpos = mpd_status_get_song_pos(s);
    s_songlen = mpd_status_get_total_time(s);
    s_songtime = mpd_status_get_elapsed_time(s);
    s_time_r = s_songlen - s_songtime;
    s_time_sec = s_time_r % 60;
    s_time_min = (s_time_r - s_time_sec) / 60;
    if(mpd_status_get_consume(s) == 1)
        s_flagmask |= MpdFlag_Consume;
    if(mpd_status_get_single(s) == 1)
        s_flagmask |= MpdFlag_Single;
    if(mpd_status_get_random(s) == 1)
        s_flagmask |= MpdFlag_Random;
    if(mpd_status_get_repeat(s) == 1)
        s_flagmask |= MpdFlag_Repeat;
    artist = mpd_song_get_tag(so, MPD_TAG_ARTIST, 0);
    title = mpd_song_get_tag(so, MPD_TAG_TITLE, 0);


    //%artist - %title (-%total-%elapsed) [$flags] [#%pos/%queuelength]

    snprintf(statustext, STEXTSIZE,
            "%s - %s  (-%d:%02d)",
            artist != NULL ? artist : "名無し",
            title != NULL ? title : "曲名無し",
            s_time_min,
            s_time_sec);
    strncpy(stext, statustext, STEXTSIZE);

EXIT:;
    mpd_song_free(so);
    mpd_status_free(s);
    return;
}


void
mpdcmd_sigarlm_handler(int sig) {
    if(sig == SIGUSR1) {
        updatempdstatus();
    }
}


void
mpdcmd_install_timer(void) {
    // create the signal handler
    mpdcmd_us_sa.sa_flags = 0;
    mpdcmd_us_sa.sa_handler = mpdcmd_sigarlm_handler;
    sigemptyset(&mpdcmd_us_sa.sa_mask);
    if(sigaction(SIGUSR1, &mpdcmd_us_sa, NULL) == -1) {
        perror("dwm:error:mpdcmd_install_timer:sigaction()");
        return;
    }

    // create the timer
    mpdcmd_us_se.sigev_notify = SIGEV_SIGNAL;
    mpdcmd_us_se.sigev_signo = SIGUSR1;
    mpdcmd_us_se.sigev_value.sival_ptr = mpdcmd_us_timerid;
    if(timer_create(CLOCK_REALTIME, &mpdcmd_us_se, &mpdcmd_us_timerid) == -1) {
        perror("dwm:error:mpdcmd_install_timer:timer_create()");
        return;
    }

    // start the timer
    if(timer_settime(mpdcmd_us_timerid, 0, &mpdcmd_us_its, NULL) == -1) {
        perror("dwm:error:mpdcmd_install_timer:timer_settime()");
        return;
    }
}
