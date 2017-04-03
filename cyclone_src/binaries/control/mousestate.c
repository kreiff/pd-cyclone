/* Copyright (c) 2002-2003 krzYszcz and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include <string.h>
#include "m_pd.h"
#include "hammer/gui.h"
#include "g_canvas.h"

/* 2017 - Derek Kwan
 * for a brief time, introduced iemguts/receivecanvas way of intercepting pd messages
 * for mouse pointer for mode 1, now basing off of given coords by the current 
 * canvas. also now rewrote gui.c to defer calculations to the c code here in interests
 * of multiple object independence, just use gui.c to send proper coords over
*/



typedef struct _mousestate
{
    t_object   x_ob;
    int        x_ispolling; 
    int        x_bang;
    int        x_hlast;
    int        x_vlast;
    int        x_hzero;
    int        x_vzero;
    int        x_mode; //0-screen, 1-object window, 2-active window
    int        x_zero; //if we are requesting to zero
    int         x_wx; 
    int         x_wy;
    int         x_ww;
    int         x_wh;
    t_outlet  *x_hposout;
    t_outlet  *x_vposout;
    t_outlet  *x_hdiffout;
    t_outlet  *x_vdiffout;

}  t_mousestate;

static t_class *mousestate_class;



static void mousestate_anything(t_mousestate *x,
				t_symbol *s, int ac, t_atom *av)
{
    /* dummy method, filtering out those messages from gui,
       which are not handled explicitly */
}

static void mousestate_doup(t_mousestate *x, t_floatarg f)
{
    outlet_float(((t_object *)x)->ob_outlet, ((int)f ? 0 : 1));
}


static void mousestate_dobang(t_mousestate *x, t_floatarg f1, t_floatarg f2)
{
    if (x->x_bang || x->x_ispolling)
    {
	int h = (int)f1, v = (int)f2;
	outlet_float(x->x_vdiffout, v - x->x_vlast);
	outlet_float(x->x_hdiffout, h - x->x_hlast);
	outlet_float(x->x_vposout, v - x->x_vzero);
	outlet_float(x->x_hposout, h - x->x_hzero);
	x->x_hlast = h;
	x->x_vlast = v;
	x->x_bang = 0;
    }
}



static void mousestate_dozero(t_mousestate *x, t_floatarg f1, t_floatarg f2)
{
    if (x->x_zero)
    {
	int h = (int)f1, v = (int)f2;
	x->x_hzero = h;
	x->x_vzero = v;
	x->x_zero = 0;
    };

}

static void mousestate__getscreen(t_mousestate *x, t_float screenx, t_float screeny)
{
  //callback from tcl for requesting screen coords
  t_float px, py;
    int mode = x->x_mode;
  //mode 0, no need to parse, just send directly to doer of things
  if(mode == 0)
    {
      px = screenx;
      py = screeny;
    }
  else if(mode == 1)
    {
      //screen coords relative to object's home canvas, we already have thsese
      //coords stored
      px = screenx - x->x_wx;
      py = screeny - x->x_wy;
      px = px >= x->x_ww ? (x->x_ww - 1) : (px  < 0 ? 0 : px);
      py = py >= x->x_wh ? (x->x_wh - 1) : (py < 0 ? 0 : py);
    };
  if(mode == 0 || mode == 1)
    {
      if(x->x_bang == 1 || x->x_ispolling == 1) mousestate_dobang(x, px, py);
      else if(x->x_zero == 1) mousestate_dozero(x, px, py);
    };
}

static void mousestate__getscreenfocused(t_mousestate *x, t_symbol *s, int argc, t_atom * argv)
{
  //callback from tcl for mode 2
  // screenx, screeny, focusx, focusy, focusw, focush
  //these are our pointer coords relative to focused window
  int i;
  t_float curf, screenx, screeny, focusx, focusy, focusw, focush;
  if(argc >= 6)
    {
      for(i=0;i<6;i++)
	{
	  if(argv[i].a_type == A_FLOAT)
	    {
	      curf = argv[i].a_w.w_float;
	      switch(i)
		{
		case 0:
		  screenx = curf;
		  break;
		case 1:
		  screeny = curf;
		  break;
		case 2:
		  focusx = curf;
		  break;
		case 3:
		  focusy = curf;
		  break;
		case 4:
		  focusw = curf;
		  break;
		case 5:
		  focush = curf;
		  break;
		default:
		  break;
		};
	    }
	  else return;
	};
    }
  else return;

  if(x->x_mode == 2)
    {
      t_float px, py;
      px = screenx - focusx;
      py = screeny - focusy;
      px = px >= focusw ? (focusw -1) : ( px < 0 ? 0 : px);
      py = py >= focush ? (focush - 1) : (py < 0 ? 0 : py);
      if(x->x_bang == 1 || x->x_ispolling == 1) mousestate_dobang(x, px, py);
      if(x->x_zero == 1) mousestate_dozero(x, px, py);
    };
}



static void mousestate_objwin(t_mousestate *x, int argc, t_atom * argv){
    t_float objx, objy;
    if(argc >=2 && x->x_mode == 1){
        objx = atom_getfloatarg(0, argc, argv);
        objy = atom_getfloatarg(1, argc, argv);
        mousestate_dobang(x, objx, objy); 
    };
}

static void mousestate_bang(t_mousestate *x)
{
  int mode = x->x_mode;
  if(mode == 0 || mode == 1) hammergui_getscreen();
  else if (mode == 2) hammergui_getscreenfocused();
  x->x_bang = 1;
}

static void mousestate_poll(t_mousestate *x)
{
    int mode = x->x_mode;
    //pollmode: mode + 1 : mode0 -> 1, mode1 -> 2, mode2-> 3
    int pollmode = mode + 1;
    hammergui_startpolling((t_pd *)x, pollmode);
    x->x_ispolling = 1;
}


static void mousestate_nopoll(t_mousestate *x)
{
    hammergui_stoppolling((t_pd *)x);
    x->x_ispolling = 0;
}

static void mousestate_zero(t_mousestate *x)
{
  int mode = x->x_mode;
  if(mode == 0 || mode == 1) hammergui_getscreen();
  else if (mode == 2) hammergui_getscreenfocused();
  x->x_zero = 1;
}

static void mousestate_reset(t_mousestate *x)
{
    x->x_hzero = x->x_vzero = 0;
}

static void mousestate_free(t_mousestate *x)
{
  if(x->x_ispolling == 1) mousestate_nopoll(x);
  hammergui_unbindmouse((t_pd *)x);
}

static void mousestate_mode(t_mousestate *x, t_floatarg f){
    int mode = (int) f;
    int polling = x->x_ispolling;
    if(mode < 0){
        mode = 0;
    }
    else if(mode > 2){
        mode = 2;
    };
    if(polling)
    {
        mousestate_nopoll(x);
        x->x_mode = mode;
        mousestate_poll(x);
    }
    else x->x_mode = mode;
}

static void *mousestate_new(void)
{
    int x1, x2, y1, y2;
    t_mousestate *x = (t_mousestate *)pd_new(mousestate_class);
    x->x_ispolling = x->x_bang = x->x_zero = 0;
    outlet_new((t_object *)x, &s_float);
    x->x_hposout = outlet_new((t_object *)x, &s_float);
    x->x_vposout = outlet_new((t_object *)x, &s_float);
    x->x_hdiffout = outlet_new((t_object *)x, &s_float);
    x->x_vdiffout = outlet_new((t_object *)x, &s_float);
    x->x_mode = 0;
    x->x_zero = 0;
    hammergui_bindmouse((t_pd *)x);
    hammergui_willpoll();
    mousestate_reset(x);
    t_glist *g_list = (t_glist *)canvas_getcurrent();

    x1 =  g_list->gl_screenx1;
    y1 = g_list->gl_screeny1;
    x2 = g_list->gl_screenx2;
    y2 = g_list->gl_screeny2;

    x->x_wx = x1;
    x->x_wy = y1;
    x->x_ww = x2 - x1;
    x->x_wh = y2 - y1;
//     post("%d %d %d %d", x->x_x1, x->x_y1, x->x_x2, x->x_y2);

    return (x);
}

void mousestate_setup(void)
{
    mousestate_class = class_new(gensym("mousestate"),
				 (t_newmethod)mousestate_new,
				 (t_method)mousestate_free,
				 sizeof(t_mousestate), 0, 0);

    class_addcreator((t_newmethod)mousestate_new, gensym("MouseState"), 0, 0);
    class_addcreator((t_newmethod)mousestate_new, gensym("cyclone/MouseState"), 0, 0);
    class_addanything(mousestate_class, mousestate_anything);
    class_addmethod(mousestate_class, (t_method)mousestate_doup,
		    gensym("_up"), A_FLOAT, 0);
    class_addmethod(mousestate_class, (t_method)mousestate__getscreen,
		    gensym("_getscreen"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(mousestate_class, (t_method)mousestate__getscreenfocused,
		    gensym("_getscreenfocused"), A_GIMME, 0);
    class_addmethod(mousestate_class, (t_method)mousestate_dobang,
		    gensym("_bang"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(mousestate_class, (t_method)mousestate_dozero,
		    gensym("_zero"), A_FLOAT, A_FLOAT, 0);
    class_addbang(mousestate_class, mousestate_bang);
    class_addmethod(mousestate_class, (t_method)mousestate_poll,
		    gensym("poll"), 0);
    class_addmethod(mousestate_class, (t_method)mousestate_nopoll,
		    gensym("nopoll"), 0);
    class_addmethod(mousestate_class, (t_method)mousestate_zero,
		    gensym("zero"), 0);
    class_addmethod(mousestate_class, (t_method)mousestate_reset,
		    gensym("reset"), 0);
    class_addmethod(mousestate_class, (t_method)mousestate_mode,
		    gensym("mode"), A_FLOAT, 0);

}

void MouseState_setup(void)
{
    mousestate_setup();
}
