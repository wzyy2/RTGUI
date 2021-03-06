/*
 * File      : window.c
 * This file is part of RTGUI in RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2009, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-10-04     Bernard      first version
 */
#include <rtgui/dc.h>
#include <rtgui/color.h>
#include <rtgui/image.h>
#include <rtgui/rtgui_system.h>
#include <rtgui/rtgui_server.h>
#include <rtgui/rtgui_app.h>

#include <rtgui/widgets/window.h>
#include <rtgui/widgets/button.h>
#include <rtgui/widgets/title.h>

static void _rtgui_win_constructor(rtgui_win_t *win)
{
    /* set toplevel to self */
    RTGUI_WIDGET(win)->toplevel = win;

    /* init win property */
    win->drawing = 0;

    /* hide win default */
    RTGUI_WIDGET_HIDE(win);

    RTGUI_WIDGET(win)->flag |= RTGUI_WIDGET_FLAG_FOCUSABLE;
    win->parent_window = RT_NULL;
    win->app = rtgui_app_self();
    /* init window attribute */
    win->on_activate   = RT_NULL;
    win->on_deactivate = RT_NULL;
    win->on_close      = RT_NULL;
    win->on_key        = RT_NULL;
    win->title         = RT_NULL;
    win->_title_wgt    = RT_NULL;
    win->modal_code    = RTGUI_MODAL_OK;

    /* initialize last mouse event handled widget */
    win->last_mevent_widget = RT_NULL;
    win->focused_widget = RT_NULL;

    /* set window hide */
    RTGUI_WIDGET_HIDE(win);

    /* set window style */
    win->style = RTGUI_WIN_STYLE_DEFAULT;

    win->flag  = RTGUI_WIN_FLAG_INIT;

    rtgui_object_set_event_handler(RTGUI_OBJECT(win), rtgui_win_event_handler);

    /* init user data */
    win->user_data = 0;
}

static void _rtgui_win_destructor(rtgui_win_t *win)
{
    struct rtgui_event_win_destroy edestroy;

    if (win->flag & RTGUI_WIN_FLAG_CONNECTED)
    {
        /* destroy in server */
        RTGUI_EVENT_WIN_DESTROY_INIT(&edestroy);
        edestroy.wid = win;
        if (rtgui_server_post_event_sync(RTGUI_EVENT(&edestroy),
                                         sizeof(struct rtgui_event_win_destroy)) != RT_EOK)
        {
            /* destroy in server failed */
            return;
        }
    }

    /* release field */
    if (win->_title_wgt)
        rtgui_widget_destroy(RTGUI_WIDGET(win->_title_wgt));
    if (win->title != RT_NULL)
        rt_free(win->title);
    /* release external clip info */
    win->drawing = 0;
}

static rt_bool_t _rtgui_win_create_in_server(struct rtgui_win *win)
{
    if (!(win->flag & RTGUI_WIN_FLAG_CONNECTED))
    {
        struct rtgui_event_win_create ecreate;
        RTGUI_EVENT_WIN_CREATE_INIT(&ecreate);

        /* send win create event to server */
        ecreate.parent_window = win->parent_window;
        ecreate.wid           = win;
        ecreate.parent.user   = win->style;

        if (rtgui_server_post_event_sync(RTGUI_EVENT(&ecreate),
                                         sizeof(struct rtgui_event_win_create)
                                        ) != RT_EOK)
        {
            rt_kprintf("create win: %s failed\n", win->title);
            return RT_FALSE;
        }

        win->flag |= RTGUI_WIN_FLAG_CONNECTED;
    }

    return RT_TRUE;
}

DEFINE_CLASS_TYPE(win, "win",
                  RTGUI_PARENT_TYPE(container),
                  _rtgui_win_constructor,
                  _rtgui_win_destructor,
                  sizeof(struct rtgui_win));

rtgui_win_t *rtgui_win_create(struct rtgui_win *parent_window,
                              const char *title,
                              rtgui_rect_t *rect,
                              rt_uint16_t style)
{
    struct rtgui_win *win;

    /* allocate win memory */
    win = RTGUI_WIN(rtgui_widget_create(RTGUI_WIN_TYPE));
    if (win == RT_NULL)
        return RT_NULL;

    /* set parent window */
    win->parent_window = parent_window;

    /* set title, rect and style */
    if (title != RT_NULL)
        win->title = rt_strdup(title);
    else
        win->title = RT_NULL;

    rtgui_widget_set_rect(RTGUI_WIDGET(win), rect);
    win->style = style;

    if (!((style & RTGUI_WIN_STYLE_NO_TITLE) && (style & RTGUI_WIN_STYLE_NO_BORDER)))
    {
        struct rtgui_rect trect = *rect;

        win->_title_wgt = rtgui_wintitle_create(win);
        if (!win->_title_wgt)
            goto __on_err;

        if (!(style & RTGUI_WIN_STYLE_NO_BORDER))
        {
            rtgui_rect_inflate(&trect, WINTITLE_BORDER_SIZE);
        }
        if (!(style & RTGUI_WIN_STYLE_NO_TITLE))
        {
            trect.y1 -= WINTITLE_HEIGHT;
        }
        rtgui_widget_set_rect(RTGUI_WIDGET(win->_title_wgt), &trect);
        /* Update the clip of the wintitle manually. */
        rtgui_region_subtract_rect(&(RTGUI_WIDGET(win->_title_wgt)->clip),
                                   &(RTGUI_WIDGET(win->_title_wgt)->clip),
                                   &(RTGUI_WIDGET(win)->extent));

        /* The window title is always un-hidden for simplicity. */
        rtgui_widget_show(RTGUI_WIDGET(win->_title_wgt));
    }

    if (_rtgui_win_create_in_server(win) == RT_FALSE)
    {
        goto __on_err;
    }
    return win;

__on_err:
    rtgui_widget_destroy(RTGUI_WIDGET(win));
    return RT_NULL;
}
RTM_EXPORT(rtgui_win_create);

rtgui_win_t *rtgui_mainwin_create(struct rtgui_win *parent_window, const char *title, rt_uint16_t style)
{
    struct rtgui_rect rect;

    /* get rect of main window */
    rtgui_get_mainwin_rect(&rect);

    return rtgui_win_create(parent_window, title, &rect, style);
}
RTM_EXPORT(rtgui_mainwin_create);

static rt_bool_t _rtgui_win_deal_close(struct rtgui_win *win,
                                       struct rtgui_event *event,
                                       rt_bool_t force_close)
{
    if (win->on_close != RT_NULL)
    {
        if ((win->on_close(RTGUI_OBJECT(win), event) == RT_FALSE) && !force_close)
            return RT_FALSE;
    }

    rtgui_win_hide(win);

    win->flag |= RTGUI_WIN_FLAG_CLOSED;

    if (win->flag & RTGUI_WIN_FLAG_MODAL)
    {
        rtgui_win_end_modal(win, RTGUI_MODAL_CANCEL);
    }
    else if (win->style & RTGUI_WIN_STYLE_DESTROY_ON_CLOSE)
    {
        rtgui_win_destroy(win);
    }

    return RT_TRUE;
}

void rtgui_win_destroy(struct rtgui_win *win)
{
    /* close the window first if it's not. */
    if (!(win->flag & RTGUI_WIN_FLAG_CLOSED))
    {
        struct rtgui_event_win_close eclose;

        RTGUI_EVENT_WIN_CLOSE_INIT(&eclose);
        eclose.wid = win;

        if (win->style & RTGUI_WIN_STYLE_DESTROY_ON_CLOSE)
        {
            _rtgui_win_deal_close(win,
                                  (struct rtgui_event *)&eclose,
                                  RT_TRUE);
            return;
        }
        else
            _rtgui_win_deal_close(win,
                                  (struct rtgui_event *)&eclose,
                                  RT_TRUE);
    }

    if (win->flag & RTGUI_WIN_FLAG_MODAL)
    {
        /* set the RTGUI_WIN_STYLE_DESTROY_ON_CLOSE flag so the window will be
         * destroyed after the event_loop */
        win->style |= RTGUI_WIN_STYLE_DESTROY_ON_CLOSE;
        rtgui_win_end_modal(win, RTGUI_MODAL_CANCEL);
    }
    else
    {
        rtgui_widget_destroy(RTGUI_WIDGET(win));
    }
}
RTM_EXPORT(rtgui_win_destroy);

/* send a close event to myself to get a consistent behavior */
rt_bool_t rtgui_win_close(struct rtgui_win *win)
{
    struct rtgui_event_win_close eclose;

    RTGUI_EVENT_WIN_CLOSE_INIT(&eclose);
    eclose.wid = win;
    return _rtgui_win_deal_close(win,
                                 (struct rtgui_event *)&eclose,
                                 RT_FALSE);
}
RTM_EXPORT(rtgui_win_close);

rt_base_t rtgui_win_show(struct rtgui_win *win, rt_bool_t is_modal)
{
    rt_base_t exit_code = -1;
    struct rtgui_app *app;
    struct rtgui_event_win_show eshow;

    RTGUI_EVENT_WIN_SHOW_INIT(&eshow);
    eshow.wid = win;

    if (win == RT_NULL)
        return exit_code;

    win->flag &= ~RTGUI_WIN_FLAG_CLOSED;
    win->flag &= ~RTGUI_WIN_FLAG_CB_PRESSED;

    /* if it does not register into server, create it in server */
    if (!(win->flag & RTGUI_WIN_FLAG_CONNECTED))
    {
        if (_rtgui_win_create_in_server(win) == RT_FALSE)
            return exit_code;
    }

    /* set window unhidden before notify the server */
    rtgui_widget_show(RTGUI_WIDGET(win));

    if (rtgui_server_post_event_sync(RTGUI_EVENT(&eshow),
                                     sizeof(struct rtgui_event_win_show)) != RT_EOK)
    {
        /* It could not be shown if a parent window is hidden. */
        rtgui_widget_hide(RTGUI_WIDGET(win));
        return exit_code;
    }

    if (win->focused_widget == RT_NULL)
        rtgui_widget_focus(RTGUI_WIDGET(win));

    app = win->app;
    RT_ASSERT(app != RT_NULL);

    /* set main window */
    if (app->main_object == RT_NULL)
        rtgui_app_set_main_win(app, win);

    if (is_modal == RT_TRUE)
    {
        struct rtgui_event_win_modal_enter emodal;

        RTGUI_EVENT_WIN_MODAL_ENTER_INIT(&emodal);
        emodal.wid = win;

        win->flag |= RTGUI_WIN_FLAG_MODAL;

        if (rtgui_server_post_event_sync((struct rtgui_event *)&emodal,
                                         sizeof(emodal)) != RT_EOK)
            return exit_code;

        exit_code = rtgui_app_run(app);
        win->flag &= ~RTGUI_WIN_FLAG_MODAL;

        rtgui_win_hide(win);

        if (win->style & RTGUI_WIN_STYLE_DESTROY_ON_CLOSE)
        {
            rtgui_win_destroy(win);
        }
    }

    return exit_code;
}
RTM_EXPORT(rtgui_win_show);

void rtgui_win_end_modal(struct rtgui_win *win, rtgui_modal_code_t modal_code)
{
    if (win == RT_NULL || !(win->flag & RTGUI_WIN_FLAG_MODAL))
        return;

    rtgui_app_exit(win->app, modal_code);

    /* remove modal mode */
    win->flag &= ~RTGUI_WIN_FLAG_MODAL;
}
RTM_EXPORT(rtgui_win_end_modal);

void rtgui_win_hide(struct rtgui_win *win)
{
    RT_ASSERT(win != RT_NULL);

    if (!RTGUI_WIDGET_IS_HIDE(win) &&
            win->flag & RTGUI_WIN_FLAG_CONNECTED)
    {
        /* send hidden message to server */
        struct rtgui_event_win_hide ehide;
        RTGUI_EVENT_WIN_HIDE_INIT(&ehide);
        ehide.wid = win;

        if (rtgui_server_post_event_sync(RTGUI_EVENT(&ehide),
                                         sizeof(struct rtgui_event_win_hide)) != RT_EOK)
        {
            rt_kprintf("hide win: %s failed\n", win->title);
            return;
        }

        rtgui_widget_hide(RTGUI_WIDGET(win));
        win->flag &= ~RTGUI_WIN_FLAG_ACTIVATE;
    }
}
RTM_EXPORT(rtgui_win_hide);

rt_err_t rtgui_win_activate(struct rtgui_win *win)
{
    struct rtgui_event_win_activate eact;
    RTGUI_EVENT_WIN_ACTIVATE_INIT(&eact);
    eact.wid = win;

    return rtgui_server_post_event_sync(RTGUI_EVENT(&eact),
                                        sizeof(eact));
}
RTM_EXPORT(rtgui_win_activate);

rt_bool_t rtgui_win_is_activated(struct rtgui_win *win)
{
    RT_ASSERT(win != RT_NULL);

    if (win->flag & RTGUI_WIN_FLAG_ACTIVATE) return RT_TRUE;

    return RT_FALSE;
}
RTM_EXPORT(rtgui_win_is_activated);

void rtgui_win_move(struct rtgui_win *win, int x, int y)
{
    struct rtgui_widget *wgt;
    struct rtgui_event_win_move emove;
    int dx, dy;
    RTGUI_EVENT_WIN_MOVE_INIT(&emove);

    if (win == RT_NULL)
        return;

    if (win->_title_wgt)
    {
        wgt = RTGUI_WIDGET(win->_title_wgt);
        dx = x - wgt->extent.x1;
        dy = y - wgt->extent.y1;
        rtgui_widget_move_to_logic(wgt, dx, dy);

        wgt = RTGUI_WIDGET(win);
        rtgui_widget_move_to_logic(wgt, dx, dy);
    }
    else
    {
        wgt = RTGUI_WIDGET(win);
        dx = x - wgt->extent.x1;
        dy = y - wgt->extent.y1;
        rtgui_widget_move_to_logic(wgt, dx, dy);
    }

    if (win->flag & RTGUI_WIN_FLAG_CONNECTED)
    {
        rtgui_widget_hide(RTGUI_WIDGET(win));

        emove.wid   = win;
        emove.x     = x;
        emove.y     = y;
        if (rtgui_server_post_event_sync(RTGUI_EVENT(&emove),
                                         sizeof(struct rtgui_event_win_move)) != RT_EOK)
        {
            return;
        }
    }

    rtgui_widget_show(RTGUI_WIDGET(win));
    return;
}
RTM_EXPORT(rtgui_win_move);

static rt_bool_t rtgui_win_ondraw(struct rtgui_win *win)
{
    struct rtgui_dc *dc;
    struct rtgui_rect rect;
    struct rtgui_event_paint event;

    /* begin drawing */
    dc = rtgui_dc_begin_drawing(RTGUI_WIDGET(win));
    if (dc == RT_NULL)
        return RT_FALSE;

    /* get window rect */
    rtgui_widget_get_rect(RTGUI_WIDGET(win), &rect);
    /* fill area */
    rtgui_dc_fill_rect(dc, &rect);

    /* paint each widget */
    RTGUI_EVENT_PAINT_INIT(&event);
    event.wid = RT_NULL;
    rtgui_container_dispatch_event(RTGUI_CONTAINER(win),
                                   (rtgui_event_t *)&event);

    rtgui_dc_end_drawing(dc);

    return RT_FALSE;
}

void rtgui_win_update_clip(struct rtgui_win *win)
{
    struct rtgui_container *cnt;
    struct rtgui_list_node *node;

    if (win == RT_NULL)
        return;

    /* update the clip info of each child */
    cnt = RTGUI_CONTAINER(win);
    rtgui_list_foreach(node, &(cnt->children))
    {
        rtgui_widget_t *child = rtgui_list_entry(node, rtgui_widget_t, sibling);

        rtgui_widget_update_clip(child);
    }
}

static rt_bool_t _win_handle_mouse_btn(struct rtgui_win *win, struct rtgui_event *eve)
{
    /* check whether has widget which handled mouse event before.
     *
     * Note #1: that the widget should have already received mouse down
     * event and we should only feed the mouse up event to it here.
     *
     * Note #2: the widget is responsible to clean up
     * last_mevent_widget on mouse up event(but not overwrite other
     * widgets). If not, it will receive two mouse up events.
     */
    if (((struct rtgui_event_mouse *)eve)->button & RTGUI_MOUSE_BUTTON_UP
        && win->last_mevent_widget != RT_NULL)
    {
        if (RTGUI_OBJECT(win->last_mevent_widget)->event_handler(
                                                                 RTGUI_OBJECT(win->last_mevent_widget),
                                                                 eve) == RT_TRUE)
        {
            /* clean last mouse event handled widget */
            win->last_mevent_widget = RT_NULL;

            return RT_TRUE;
        }
    }

    /** if a widget will destroy the window in the event_handler(or in
     * on_* callbacks), it should return RT_TRUE. Otherwise, it will
     * crash the application.
     *
     * TODO: add it in the doc
     */
    return rtgui_container_dispatch_mouse_event(RTGUI_CONTAINER(win),
                                               (struct rtgui_event_mouse *)eve);
}

rt_bool_t rtgui_win_event_handler(struct rtgui_object *object, struct rtgui_event *event)
{
    struct rtgui_win *win;

    RT_ASSERT(object != RT_NULL);
    RT_ASSERT(event != RT_NULL);

    win = RTGUI_WIN(object);

    switch (event->type)
    {
    case RTGUI_EVENT_WIN_SHOW:
        rtgui_win_show(win, RT_FALSE);
        break;

    case RTGUI_EVENT_WIN_HIDE:
        rtgui_win_hide(win);
        break;

    case RTGUI_EVENT_WIN_CLOSE:
        _rtgui_win_deal_close(win, event, RT_FALSE);
        /* don't broadcast WIN_CLOSE event to others */
        return RT_TRUE;

    case RTGUI_EVENT_WIN_MOVE:
    {
        struct rtgui_event_win_move *emove = (struct rtgui_event_win_move *)event;

        /* move window */
        rtgui_win_move(win, emove->x, emove->y);
    }
    break;

    case RTGUI_EVENT_WIN_ACTIVATE:
        if (RTGUI_WIDGET_IS_HIDE(win))
        {
            /* activate a hide window */
            return RT_TRUE;
        }

        win->flag |= RTGUI_WIN_FLAG_ACTIVATE;
        /* A paint event will follow the active event and the title will be
         * paint there. */

        if (win->on_activate != RT_NULL)
        {
            win->on_activate(RTGUI_OBJECT(object), event);
        }
        break;

    case RTGUI_EVENT_WIN_DEACTIVATE:
        win->flag &= ~RTGUI_WIN_FLAG_ACTIVATE;
        /* No paint event follow the deactive event. So we have to update
         * the title manually to reflect the change. */
        if (win->_title_wgt)
            rtgui_widget_update(RTGUI_WIDGET(win->_title_wgt));

        if (win->on_deactivate != RT_NULL)
            win->on_deactivate(RTGUI_OBJECT(object), event);

        break;

    case RTGUI_EVENT_CLIP_INFO:
        /* update win clip */
        rtgui_win_update_clip(win);
        break;

    case RTGUI_EVENT_PAINT:
        if (win->_title_wgt)
            rtgui_widget_update(RTGUI_WIDGET(win->_title_wgt));
        rtgui_win_ondraw(win);
        break;

	case RTGUI_EVENT_VPAINT_REQ:
	{
		struct rtgui_dc *dc;
		struct rtgui_event_vpaint_ack vpaint_ack;

		/* get drawing dc */
		dc = rtgui_win_get_drawing(win);

		/* send ack for vpaint request */
		RTGUI_EVENT_VPAINT_ACK_INIT((&vpaint_ack), win);
		vpaint_ack.buffer = dc;

		rtgui_send(event->sender, RTGUI_EVENT(&vpaint_ack),
			sizeof(struct rtgui_event_vpaint_ack));
		break;
	}

    case RTGUI_EVENT_MOUSE_BUTTON: {
        struct rtgui_event_mouse *emouse = (struct rtgui_event_mouse*)event;

        if (rtgui_rect_contains_point(&RTGUI_WIDGET(win)->extent,
                                      emouse->x, emouse->y) == RT_EOK)
            return _win_handle_mouse_btn(win, event);

        if (win->_title_wgt)
        {
            struct rtgui_object *tobj = RTGUI_OBJECT(win->_title_wgt);
            return tobj->event_handler(tobj, event);
        }
    }
    break;

    case RTGUI_EVENT_MOUSE_MOTION:
#if 0
        if (rtgui_widget_dispatch_mouse_event(widget,
                                              (struct rtgui_event_mouse *)event) == RT_FALSE)
        {
#ifndef RTGUI_USING_SMALL_SIZE
            /* handle event in current widget */
            if (widget->on_mousemotion != RT_NULL)
            {
                return widget->on_mousemotion(widget, event);
            }
#endif
        }
        else return RT_TRUE;
#endif
        break;

    case RTGUI_EVENT_KBD:
        /* we should dispatch key event firstly */
        if (!(win->flag & RTGUI_WIN_FLAG_HANDLE_KEY))
        {
            struct rtgui_widget *widget;
            rt_bool_t res = RT_FALSE;
            /* we should dispatch the key event just once. Once entered the
             * dispatch mode, we should swtich to key handling mode. */
            win->flag |= RTGUI_WIN_FLAG_HANDLE_KEY;

            /* dispatch the key event */
            for (widget = win->focused_widget;
                    widget && !res;
                    widget = widget->parent)
            {
                if (RTGUI_OBJECT(widget)->event_handler != RT_NULL)
                    res = RTGUI_OBJECT(widget)->event_handler(
                              RTGUI_OBJECT(widget), event);
            }

            win->flag &= ~RTGUI_WIN_FLAG_HANDLE_KEY;
            return res;
        }
        else
        {
            /* in key handling mode(it may reach here in
             * win->focused_widget->event_handler call) */
            if (win->on_key != RT_NULL)
                return win->on_key(RTGUI_OBJECT(win), event);
        }
        break;

    case RTGUI_EVENT_COMMAND:
        if (rtgui_container_dispatch_event(RTGUI_CONTAINER(object), event) != RT_TRUE)
        {
#ifndef RTGUI_USING_SMALL_SIZE
            if (RTGUI_WIDGET(object)->on_command != RT_NULL)
            {
                RTGUI_WIDGET(object)->on_command(object, event);
            }
#endif
        }
        else
            return RT_TRUE;
        break;

    default:
        return rtgui_container_event_handler(object, event);
    }

    return RT_FALSE;
}
RTM_EXPORT(rtgui_win_event_handler);

void rtgui_win_set_rect(rtgui_win_t *win, rtgui_rect_t *rect)
{
    struct rtgui_event_win_resize event;

    if (win == RT_NULL || rect == RT_NULL) return;

    RTGUI_WIDGET(win)->extent = *rect;

    if (win->flag & RTGUI_WIN_FLAG_CONNECTED)
    {
        /* set window resize event to server */
        RTGUI_EVENT_WIN_RESIZE_INIT(&event);
        event.wid = win;
        event.rect = *rect;

        rtgui_server_post_event(&(event.parent), sizeof(struct rtgui_event_win_resize));
    }
}
RTM_EXPORT(rtgui_win_set_rect);

void rtgui_win_set_onactivate(rtgui_win_t *win, rtgui_event_handler_ptr handler)
{
    if (win != RT_NULL)
    {
        win->on_activate = handler;
    }
}
RTM_EXPORT(rtgui_win_set_onactivate);

void rtgui_win_set_ondeactivate(rtgui_win_t *win, rtgui_event_handler_ptr handler)
{
    if (win != RT_NULL)
    {
        win->on_deactivate = handler;
    }
}
RTM_EXPORT(rtgui_win_set_ondeactivate);

void rtgui_win_set_onclose(rtgui_win_t *win, rtgui_event_handler_ptr handler)
{
    if (win != RT_NULL)
    {
        win->on_close = handler;
    }
}
RTM_EXPORT(rtgui_win_set_onclose);

void rtgui_win_set_onkey(rtgui_win_t *win, rtgui_event_handler_ptr handler)
{
    if (win != RT_NULL)
    {
        win->on_key = handler;
    }
}
RTM_EXPORT(rtgui_win_set_onkey);

void rtgui_win_set_title(rtgui_win_t *win, const char *title)
{
    /* send title to server */
    if (win->flag & RTGUI_WIN_FLAG_CONNECTED)
    {
    }

    /* modify in local side */
    if (win->title != RT_NULL)
    {
        rtgui_free(win->title);
        win->title = RT_NULL;
    }

    if (title != RT_NULL)
    {
        win->title = rt_strdup(title);
    }
}
RTM_EXPORT(rtgui_win_set_title);

char *rtgui_win_get_title(rtgui_win_t *win)
{
    RT_ASSERT(win != RT_NULL);

    return win->title;
}
RTM_EXPORT(rtgui_win_get_title);

#ifdef RTGUI_USING_VFRAMEBUFFER
#include <rtgui/driver.h>
struct rtgui_dc *rtgui_win_get_drawing(rtgui_win_t * win)
{
	struct rtgui_dc *dc;
	struct rtgui_rect rect;

	if (rtgui_app_self() == RT_NULL) return RT_NULL;
	if (win == RT_NULL || !(win->flag & RTGUI_WIN_FLAG_CONNECTED)) return RT_NULL;

	if (win->app == rtgui_app_self())
	{
		/* under the same application context */
		rtgui_region_t region;
		rtgui_region_t clip_region;

		rtgui_region_init(&clip_region);
		rtgui_region_copy(&clip_region, &RTGUI_WIDGET(win)->clip);

		rtgui_graphic_driver_vmode_enter();

		rtgui_graphic_driver_get_rect(RT_NULL, &rect);
	    region.data = RT_NULL;
	    region.extents.x1 = rect.x1;
	    region.extents.y1 = rect.y1;
	    region.extents.x2 = rect.x2;
	    region.extents.y2 = rect.y2;
		
		/* remove clip */
	    rtgui_region_reset(&RTGUI_WIDGET(win)->clip,
	                       &RTGUI_WIDGET(win)->extent);
		rtgui_region_intersect(&(RTGUI_WIDGET(win)->clip),
							   &(RTGUI_WIDGET(win)->clip),
							   &region);
		rtgui_win_update_clip(win);
		/* use virtual framebuffer */
		rtgui_widget_update(RTGUI_WIDGET(win));

		rtgui_graphic_driver_vmode_exit();

		/* get the extent of widget */
		rtgui_widget_get_extent(RTGUI_WIDGET(win), &rect);

		dc = rtgui_graphic_driver_get_rect_buffer(RT_NULL, &rect);

		/* restore the clip information of window */
	    rtgui_region_reset(&RTGUI_WIDGET(win)->clip,
	                       &RTGUI_WIDGET(win)->extent);
		rtgui_region_intersect(&(RTGUI_WIDGET(win)->clip),
							   &(RTGUI_WIDGET(win)->clip),
							   &clip_region);
		rtgui_region_fini(&region);
		rtgui_region_fini(&clip_region);

		rtgui_win_update_clip(win);
	}
	else
	{
		/* send vpaint_req to the window and wait for response */
		struct rtgui_event_vpaint_req req;
		struct rtgui_event_vpaint_ack ack;
		int freeze;

		/* make sure the screen is not locked. */
		freeze = rtgui_screen_lock_freeze();

		RTGUI_EVENT_VPAINT_REQ_INIT(&req, win);
		rtgui_send(win->app, &(req.parent), sizeof(struct rtgui_event_vpaint_req));

		/* wait for vpaint_ack event */
		rtgui_recv_filter(RTGUI_EVENT_VPAINT_ACK, &(ack.parent), sizeof(ack));
		dc = ack.buffer;

		rtgui_screen_lock_thaw(freeze);
	}

	return dc;
}
RTM_EXPORT(rtgui_win_get_drawing);
#endif

