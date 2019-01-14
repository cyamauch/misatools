static void set_fontsize()
{
    int win, i;
    int ev_win, ev_type, ev_btn;	/* for event handling */
    double ev_x, ev_y;
    win = gopen(36 * 4, 24 * 2 + 2);
    gsetbgcolor(win,"#606060");
    gclr(win);
    drawstr(win, 0,24-1,24, 0, " Font Size?");
    for ( i = 0 ; i < 4 ; i++ ) {
	newrgbcolor(win,0x40,0x40,0x40);
	drawrect(win, 36*i,24, 35,24);
	newrgbcolor(win,0x80,0x80,0x80);
	drawline(win, 36*i,24, 36*i+34,24);
	drawline(win, 36*i,24, 36*i,24+24);
    }
    newrgbcolor(win,0xff,0xff,0xff);
    drawstr(win, 6+ 36*0,48-1,24, 0, "14");
    drawstr(win, 6+ 36*1,48-1,24, 0, "16");
    drawstr(win, 6+ 36*2,48-1,24, 0, "20");
    drawstr(win, 6+ 36*3,48-1,24, 0, "24");
    ev_win = ggetxpress(&ev_type,&ev_btn,&ev_x,&ev_y);
    if ( ev_win == win && ev_type == ButtonPress ) {
	if ( 0 <= ev_x && ev_x < 36*1 ) Fontsize = 14;
	else if ( 36*1 <= ev_x && ev_x < 36*2 ) Fontsize = 16;
	else if ( 36*2 <= ev_x && ev_x < 36*3 ) Fontsize = 20;
	else if ( 36*3 <= ev_x && ev_x < 364 ) Fontsize = 24;
    }
    gclose(win);

    return;
}
