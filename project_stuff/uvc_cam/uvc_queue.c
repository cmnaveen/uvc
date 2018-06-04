/*
 *      uvc_queue.c  --  USB Video Class driver - Buffers management
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 */

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include "uvcvideo.h"


// complete
static void uvc_queue_return_buffers(struct uvc_video_queue *queue,
			       enum uvc_buffer_state state)
{
	while (!list_empty(&queue->irqqueue)) {
		struct uvc_buffer *buf = list_first_entry(&queue->irqqueue,
							  struct uvc_buffer,
							  queue);
		list_del(&buf->queue);
		buf->state = state;
	}
}



// complete // from uvc_drive 
int uvc_queue_init(struct uvc_video_queue *queue, enum video_buf_type type,
		    int drop_corrupted)
{

	mutex_init(&queue->mutex);
	spin_lock_init(&queue->irqlock);
	INIT_LIST_HEAD(&queue->irqqueue);
	queue->flags = drop_corrupted ? UVC_QUEUE_DROP_CORRUPTED : 0;

	return 0;
}


//complete // from uvc_video 
void uvc_queue_cancel(struct uvc_video_queue *queue, int disconnect)
{
	unsigned long flags;

	spin_lock_irqsave(&queue->irqlock, flags);
	uvc_queue_return_buffers(queue, UVC_BUF_STATE_ERROR);
	
   if (disconnect)
		queue->flags |= UVC_QUEUE_DISCONNECTED;
	spin_unlock_irqrestore(&queue->irqlock, flags);
}
// complete // from uvc_video
struct uvc_buffer *uvc_queue_next_buffer(struct uvc_video_queue *queue,
		struct uvc_buffer *buf)
{
	struct uvc_buffer *nextbuf;
	unsigned long flags;

	if ((queue->flags & UVC_QUEUE_DROP_CORRUPTED) && buf->error) {
		buf->error = 0;
		buf->state = UVC_BUF_STATE_QUEUED;
		buf->bytesused = 0;
		return buf;
	}

	spin_lock_irqsave(&queue->irqlock, flags);
	list_del(&buf->queue);
	if (!list_empty(&queue->irqqueue))
		nextbuf = list_first_entry(&queue->irqqueue, struct uvc_buffer,
					   queue);
	else
		nextbuf = NULL;
	spin_unlock_irqrestore(&queue->irqlock, flags);

	buf->state = buf->error ? UVC_BUF_STATE_ERROR : UVC_BUF_STATE_DONE;

	return nextbuf;
}
