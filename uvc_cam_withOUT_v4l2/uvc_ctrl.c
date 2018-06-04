/*
 *      uvc_ctrl.c  --  USB Video Class driver - Controls
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

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/atomic.h>

#include "uvcvideo.h"

#define UVC_CTRL_DATA_CURRENT	0
#define UVC_CTRL_DATA_BACKUP	1
#define UVC_CTRL_DATA_MIN	2
#define UVC_CTRL_DATA_MAX	3
#define UVC_CTRL_DATA_RES	4
#define UVC_CTRL_DATA_DEF	5
#define UVC_CTRL_DATA_LAST	6

/* ------------------------------------------------------------------------
 * Controls
 */


/*
// demo struct
struct uvc_control_info {
struct list_head mappings;

__u8 entity[16];
__u8 index; // Bit index in bmControls 
__u8 selector;

__u16 size;
__u32 flags;
};
*/
static struct uvc_control_info uvc_ctrls[] = {
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_BRIGHTNESS_CONTROL,
        .index		= 0,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_CONTRAST_CONTROL,
        .index		= 1,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_HUE_CONTROL,
        .index		= 2,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_SATURATION_CONTROL,
        .index		= 3,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_SHARPNESS_CONTROL,
        .index		= 4,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_GAMMA_CONTROL,
        .index		= 5,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL,
        .index		= 6,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
        .index		= 7,
        .size		= 4,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_BACKLIGHT_COMPENSATION_CONTROL,
        .index		= 8,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_GAIN_CONTROL,
        .index		= 9,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_POWER_LINE_FREQUENCY_CONTROL,
        .index		= 10,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
            | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_HUE_AUTO_CONTROL,
        .index		= 11,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
            | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL,
        .index		= 12,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
            | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL,
        .index		= 13,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
            | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_DIGITAL_MULTIPLIER_CONTROL,
        .index		= 14,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL,
        .index		= 15,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL,
        .index		= 16,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_GET_CUR,
    },
    {
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_ANALOG_LOCK_STATUS_CONTROL,
        .index		= 17,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_GET_CUR,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_SCANNING_MODE_CONTROL,
        .index		= 0,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_AE_MODE_CONTROL,
        .index		= 1,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
            | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_GET_RES
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_AE_PRIORITY_CONTROL,
        .index		= 2,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
            | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL,
        .index		= 3,
        .size		= 4,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL,
        .index		= 4,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_FOCUS_ABSOLUTE_CONTROL,
        .index		= 5,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_FOCUS_RELATIVE_CONTROL,
        .index		= 6,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_MIN
            | UVC_CTRL_FLAG_GET_MAX | UVC_CTRL_FLAG_GET_RES
            | UVC_CTRL_FLAG_GET_DEF
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_IRIS_ABSOLUTE_CONTROL,
        .index		= 7,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_IRIS_RELATIVE_CONTROL,
        .index		= 8,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_ZOOM_ABSOLUTE_CONTROL,
        .index		= 9,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_ZOOM_RELATIVE_CONTROL,
        .index		= 10,
        .size		= 3,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_MIN
            | UVC_CTRL_FLAG_GET_MAX | UVC_CTRL_FLAG_GET_RES
            | UVC_CTRL_FLAG_GET_DEF
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_PANTILT_ABSOLUTE_CONTROL,
        .index		= 11,
        .size		= 8,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_PANTILT_RELATIVE_CONTROL,
        .index		= 12,
        .size		= 4,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_ROLL_ABSOLUTE_CONTROL,
        .index		= 13,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR
            | UVC_CTRL_FLAG_GET_RANGE
            | UVC_CTRL_FLAG_RESTORE
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_ROLL_RELATIVE_CONTROL,
        .index		= 14,
        .size		= 2,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_MIN
            | UVC_CTRL_FLAG_GET_MAX | UVC_CTRL_FLAG_GET_RES
            | UVC_CTRL_FLAG_GET_DEF
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_FOCUS_AUTO_CONTROL,
        .index		= 17,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
            | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_PRIVACY_CONTROL,
        .index		= 18,
        .size		= 1,
        .flags		= UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
            | UVC_CTRL_FLAG_RESTORE
            | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
};

static struct uvc_menu_info power_line_frequency_controls[] = {
    { 0, "Disabled" },
    { 1, "50 Hz" },
    { 2, "60 Hz" },
};




// demo struct
/*
   struct uvc_control_mapping {
   struct list_head list;
   struct list_head ev_subs;

   __u32 id;
   __u8 name[32];
   __u8 entity[16];
   __u8 selector;

   __u8 size;
   __u8 offset;
   enum v4l2_ctrl_type video_type;
   __u32 data_type;

   struct uvc_menu_info *menu_info;
   __u32 menu_count;

   __u32 master_id;
   __s32 master_manual;
   __u32 slave_ids[2];

   __s32 (*get) (struct uvc_control_mapping *mapping, __u8 query,
   const __u8 *data);
   void (*set) (struct uvc_control_mapping *mapping, __s32 value,
   __u8 *data);
   };
   */


static struct uvc_control_mapping uvc_ctrl_mappings[] = {
    {
        .id		= VIDEO_CID_BRIGHTNESS, 
        .name		= "Brightness",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_BRIGHTNESS_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
    },
    {
        .id		= VIDEO_CID_CONTRAST,
        .name		= "Contrast",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_CONTRAST_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id		= VIDEO_CID_HUE,
        .name		= "Hue",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_HUE_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
        .master_id	= VIDEO_CID_HUE_AUTO,
        .master_manual	= 0,
    },
    {
        .id		= VIDEO_CID_SATURATION,
        .name		= "Saturation",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_SATURATION_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id		= VIDEO_CID_SHARPNESS,
        .name		= "Sharpness",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_SHARPNESS_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id		= VIDEO_CID_GAMMA,
        .name		= "Gamma",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_GAMMA_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id		= VIDEO_CID_BACKLIGHT_COMPENSATION,
        .name		= "Backlight Compensation",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_BACKLIGHT_COMPENSATION_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id		= VIDEO_CID_GAIN,
        .name		= "Gain",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_GAIN_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id		= VIDEO_CID_POWER_LINE_FREQUENCY,
        .name		= "Power Line Frequency",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_POWER_LINE_FREQUENCY_CONTROL,
        .size		= 2,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_MENU,
        .data_type	= UVC_CTRL_DATA_TYPE_ENUM,
        .menu_info	= power_line_frequency_controls,
        .menu_count	= ARRAY_SIZE(power_line_frequency_controls),
    },
    {
        .id		= VIDEO_CID_HUE_AUTO,
        .name		= "Hue, Auto",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_HUE_AUTO_CONTROL,
        .size		= 1,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_BOOLEAN,
        .data_type	= UVC_CTRL_DATA_TYPE_BOOLEAN,
        .slave_ids	= { VIDEO_CID_HUE, },
    },
    {
        .id		= VIDEO_CID_EXPOSURE_AUTO,
        .name		= "Exposure, Auto",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_AE_MODE_CONTROL,
        .size		= 4,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_MENU,
        .data_type	= UVC_CTRL_DATA_TYPE_BITMASK,
       // .menu_info	= exposure_auto_controls,
       // .menu_count	= ARRAY_SIZE(exposure_auto_controls),
        .slave_ids	= { VIDEO_CID_EXPOSURE_ABSOLUTE, },
    },
    {
        .id		= VIDEO_CID_EXPOSURE_AUTO_PRIORITY,
        .name		= "Exposure, Auto Priority",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_AE_PRIORITY_CONTROL,
        .size		= 1,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_BOOLEAN,
        .data_type	= UVC_CTRL_DATA_TYPE_BOOLEAN,
    },
    {
        .id		= VIDEO_CID_EXPOSURE_ABSOLUTE,
        .name		= "Exposure (Absolute)",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL,
        .size		= 32,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
        .master_id	= VIDEO_CID_EXPOSURE_AUTO,
        .master_manual	= VIDEO_EXPOSURE_MANUAL,
    },
    {
        .id		= VIDEO_CID_AUTO_WHITE_BALANCE,
        .name		= "White Balance Temperature, Auto",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL,
        .size		= 1,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_BOOLEAN,
        .data_type	= UVC_CTRL_DATA_TYPE_BOOLEAN,
        .slave_ids	= { VIDEO_CID_WHITE_BALANCE_TEMPERATURE, },
    },
    {
        .id		= VIDEO_CID_WHITE_BALANCE_TEMPERATURE,
        .name		= "White Balance Temperature",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
        .master_id	= VIDEO_CID_AUTO_WHITE_BALANCE,
        .master_manual	= 0,
    },
    {
        .id		= VIDEO_CID_AUTO_WHITE_BALANCE,
        .name		= "White Balance Component, Auto",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL,
        .size		= 1,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_BOOLEAN,
        .data_type	= UVC_CTRL_DATA_TYPE_BOOLEAN,
        .slave_ids	= { VIDEO_CID_BLUE_BALANCE,
            VIDEO_CID_RED_BALANCE },
    },
    {
        .id		= VIDEO_CID_BLUE_BALANCE,
        .name		= "White Balance Blue Component",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
        .master_id	= VIDEO_CID_AUTO_WHITE_BALANCE,
        .master_manual	= 0,
    },
    {
        .id		= VIDEO_CID_RED_BALANCE,
        .name		= "White Balance Red Component",
        .entity		= UVC_GUID_UVC_PROCESSING,
        .selector	= UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
        .size		= 16,
        .offset		= 16,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
        .master_id	= VIDEO_CID_AUTO_WHITE_BALANCE,
        .master_manual	= 0,
    },
    {
        .id		= VIDEO_CID_FOCUS_ABSOLUTE,
        .name		= "Focus (absolute)",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_FOCUS_ABSOLUTE_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
        .master_id	= VIDEO_CID_FOCUS_AUTO,
        .master_manual	= 0,
    },
    {
        .id		= VIDEO_CID_FOCUS_AUTO,
        .name		= "Focus, Auto",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_FOCUS_AUTO_CONTROL,
        .size		= 1,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_BOOLEAN,
        .data_type	= UVC_CTRL_DATA_TYPE_BOOLEAN,
        .slave_ids	= { VIDEO_CID_FOCUS_ABSOLUTE, },
    },
    {
        .id		= VIDEO_CID_IRIS_ABSOLUTE,
        .name		= "Iris, Absolute",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_IRIS_ABSOLUTE_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id		= VIDEO_CID_IRIS_RELATIVE,
        .name		= "Iris, Relative",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_IRIS_RELATIVE_CONTROL,
        .size		= 8,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
    },
    {
        .id		= VIDEO_CID_ZOOM_ABSOLUTE,
        .name		= "Zoom, Absolute",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_ZOOM_ABSOLUTE_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id		= VIDEO_CID_ZOOM_CONTINUOUS,
        .name		= "Zoom, Continuous",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_ZOOM_RELATIVE_CONTROL,
        .size		= 0,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
    //    .get		= uvc_ctrl_get_zoom,
      //  .set		= uvc_ctrl_set_zoom,
    },
    {
        .id		= VIDEO_CID_PAN_ABSOLUTE,
        .name		= "Pan (Absolute)",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_PANTILT_ABSOLUTE_CONTROL,
        .size		= 32,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
    },
    {
        .id		= VIDEO_CID_TILT_ABSOLUTE,
        .name		= "Tilt (Absolute)",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_PANTILT_ABSOLUTE_CONTROL,
        .size		= 32,
        .offset		= 32,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
    },
    {
        .id		= VIDEO_CID_PAN_SPEED,//V4L2_CID_CAMERA_CLASS_BASE+32
        .name		= "Pan (Speed)",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_PANTILT_RELATIVE_CONTROL,
        .size		= 16,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
        //.get		= uvc_ctrl_get_rel_speed,
        //.set		= uvc_ctrl_set_rel_speed,
    },
    {
        .id		= VIDEO_CID_TILT_SPEED,  //V4L2_CID_CAMERA_CLASS_BASE+33
        .name		= "Tilt (Speed)",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_PANTILT_RELATIVE_CONTROL,
        .size		= 16,
        .offset		= 16,
        .video_type	= VIDEO_CTRL_TYPE_INTEGER,
        .data_type	= UVC_CTRL_DATA_TYPE_SIGNED,
       // .get		= uvc_ctrl_get_rel_speed,
       // .set		= uvc_ctrl_set_rel_speed,
    },
    {
        .id		= VIDEO_CID_PRIVACY, //V4L2_CID_CAMERA_CLASS_BASE+16
        .name		= "Privacy",
        .entity		= UVC_GUID_UVC_CAMERA,
        .selector	= UVC_CT_PRIVACY_CONTROL,
        .size		= 1,
        .offset		= 0,
        .video_type	= VIDEO_CTRL_TYPE_BOOLEAN,
        .data_type	= UVC_CTRL_DATA_TYPE_BOOLEAN,
    },
};



/* ------------------------------------------------------------------------
 * Utility functions
 */
//complete
static inline __u8 *uvc_ctrl_data(struct uvc_control *ctrl, int id)
{
    return ctrl->uvc_data + id * ctrl->info.size;
}

//complete
static inline int uvc_test_bit(const __u8 *data, int bit)
{
    return (data[bit >> 3] >> (bit & 7)) & 1;
}

//complete
static inline void uvc_clear_bit(__u8 *data, int bit)
{
    data[bit >> 3] &= ~(1 << (bit & 7));
}

/* Extract the bit string specified by mapping->offset and mapping->size
 * from the little-endian data stored at 'data' and return the result as
 * a signed 32bit integer. Sign extension will be performed if the mapping
 * references a signed data type.
 */

//complete
static __s32 uvc_get_le_value(struct uvc_control_mapping *mapping,
        __u8 query, const __u8 *data)
{
    int bits = mapping->size;
    int offset = mapping->offset;
    __s32 value = 0;
    __u8 mask;

    data += offset / 8;
    offset &= 7;
    mask = ((1LL << bits) - 1) << offset;

    for (; bits > 0; data++) {
        __u8 byte = *data & mask;
        value |= offset > 0 ? (byte >> offset) : (byte << (-offset));
        bits -= 8 - (offset > 0 ? offset : 0);
        offset -= 8;
        mask = (1 << bits) - 1;
    }

    /* Sign-extend the value if needed. */
    if (mapping->data_type == UVC_CTRL_DATA_TYPE_SIGNED)
        value |= -(value & (1 << (mapping->size - 1)));

    return value;
}

/* Set the bit string specified by mapping->offset and mapping->size
 * in the little-endian data stored at 'data' to the value 'value'.
 */
//complete
static void uvc_set_le_value(struct uvc_control_mapping *mapping,
        __s32 value, __u8 *data)
{
    int bits = mapping->size;
    int offset = mapping->offset;
    __u8 mask;

    data += offset / 8;
    offset &= 7;

    for (; bits > 0; data++) {
        mask = ((1LL << bits) - 1) << offset;
        *data = (*data & ~mask) | ((value << offset) & mask);
        value >>= offset ? offset : 8;
        bits -= 8 - offset;
        offset = 0;
    }
}

/* ------------------------------------------------------------------------
 * Terminal and unit management
 */

static const __u8 uvc_processing_guid[16] = UVC_GUID_UVC_PROCESSING;
static const __u8 uvc_camera_guid[16] = UVC_GUID_UVC_CAMERA;
static const __u8 uvc_media_transport_input_guid[16] =
UVC_GUID_UVC_MEDIA_TRANSPORT_INPUT;

// complete 
static int uvc_entity_match_guid(const struct uvc_entity *entity,
        const __u8 guid[16])
{
    switch (UVC_ENTITY_TYPE(entity)) {
        case UVC_ITT_CAMERA:
            return memcmp(uvc_camera_guid, guid, 16) == 0;

        case UVC_ITT_MEDIA_TRANSPORT_INPUT:
            return memcmp(uvc_media_transport_input_guid, guid, 16) == 0;

        case UVC_VC_PROCESSING_UNIT:
            return memcmp(uvc_processing_guid, guid, 16) == 0;

        case UVC_VC_EXTENSION_UNIT:
            return memcmp(entity->extension.guidExtensionCode,
                    guid, 16) == 0;

        default:
            return 0;
    }
}

/* --------------------------------------------------------------------------
 * Control transactions
 *
 * To make extended set operations as atomic as the hardware allows, controls
 * are handled using begin/commit/rollback operations.
 *
 * At the beginning of a set request, uvc_ctrl_begin should be called to
 * initialize the request. This function acquires the control lock.
 *
 * When setting a control, the new value is stored in the control data field
 * at position UVC_CTRL_DATA_CURRENT. The control is then marked as dirty for
 * later processing. If the UVC and V4L2 control sizes differ, the current
 * value is loaded from the hardware before storing the new value in the data
 * field.
 *
 * After processing all controls in the transaction, uvc_ctrl_commit or
 * uvc_ctrl_rollback must be called to apply the pending changes to the
 * hardware or revert them. When applying changes, all controls marked as
 * dirty will be modified in the UVC device, and the dirty flag will be
 * cleared. When reverting controls, the control data field
 * UVC_CTRL_DATA_CURRENT is reverted to its previous value
 * (UVC_CTRL_DATA_BACKUP) for all dirty controls. Both functions release the
 * control lock.
 */




// complete
static int uvc_ctrl_commit_entity(struct uvc_device *dev,
        struct uvc_entity *entity, int rollback)
{
    struct uvc_control *ctrl;
    unsigned int i;
    int ret;

    if (entity == NULL)
        return 0;

    for (i = 0; i < entity->ncontrols; ++i) {
        ctrl = &entity->controls[i];
        if (!ctrl->initialized)
            continue;

        /* Reset the loaded flag for auto-update controls that were
         * marked as loaded in uvc_ctrl_get/uvc_ctrl_set to prevent
         * uvc_ctrl_get from using the cached value, and for write-only
         * controls to prevent uvc_ctrl_set from setting bits not
         * explicitly set by the user.
         */
        if (ctrl->info.flags & UVC_CTRL_FLAG_AUTO_UPDATE ||
                !(ctrl->info.flags & UVC_CTRL_FLAG_GET_CUR))
            ctrl->loaded = 0;

        if (!ctrl->dirty)
            continue;


        if (rollback || ret < 0)
            memcpy(uvc_ctrl_data(ctrl, UVC_CTRL_DATA_CURRENT),
                    uvc_ctrl_data(ctrl, UVC_CTRL_DATA_BACKUP),
                    ctrl->info.size);

        ctrl->dirty = 0;

        if (ret < 0)
            return ret;
    }

    return 0;
}

/* --------------------------------------------------------------------------
 * Suspend/resume
 */

/*
 * Restore control values after resume, skipping controls that haven't been
 * changed.
 *
 * TODO
 * - Don't restore modified controls that are back to their default value.
 * - Handle restore order (Auto-Exposure Mode should be restored before
 *   Exposure Time).
 */

// complete    // from uvc_driver resume
int uvc_ctrl_restore_values(struct uvc_device *dev)
{
    struct uvc_control *ctrl;
    struct uvc_entity *entity;
    unsigned int i;
    int ret;

    /* Walk the entities list and restore controls when possible. */
    list_for_each_entry(entity, &dev->entities, list) {

        for (i = 0; i < entity->ncontrols; ++i) {
            ctrl = &entity->controls[i];

            if (!ctrl->initialized || !ctrl->modified ||
                    (ctrl->info.flags & UVC_CTRL_FLAG_RESTORE) == 0)
                continue;

            printk(KERN_INFO "restoring control %pUl/%u/%u\n",
                    ctrl->info.entity, ctrl->info.index,
                    ctrl->info.selector);
            ctrl->dirty = 1;
        }

        ret = uvc_ctrl_commit_entity(dev, entity, 0);
        if (ret < 0)
            return ret;
    }

    return 0;
}

/* --------------------------------------------------------------------------
 * Control and mapping handling
 */

/*
 * Add control information to a given control.
 */

// complete
static int uvc_ctrl_add_info(struct uvc_device *dev, struct uvc_control *ctrl,
        const struct uvc_control_info *info)
{
    int ret = 0;

    ctrl->info = *info;
    INIT_LIST_HEAD(&ctrl->info.mappings);

    /* Allocate an array to save control values (cur, def, max, etc.) */
    ctrl->uvc_data = kzalloc(ctrl->info.size * UVC_CTRL_DATA_LAST + 1,
            GFP_KERNEL);
    if (ctrl->uvc_data == NULL) {
        ret = -ENOMEM;
        goto done;
    }

    ctrl->initialized = 1;

    uvc_trace(UVC_TRACE_CONTROL, "Added control %pUl/%u to device %s "
            "entity %u\n", ctrl->info.entity, ctrl->info.selector,
            dev->udev->devpath, ctrl->entity->id);

done:
    if (ret < 0)
        kfree(ctrl->uvc_data);
    return ret;
}

/*
 * Add a control mapping to a given control.
 */

//complete
static int __uvc_ctrl_add_mapping(struct uvc_device *dev,
        struct uvc_control *ctrl, const struct uvc_control_mapping *mapping)
{
    struct uvc_control_mapping *map;
    unsigned int size;

    /* Most mappings come from static kernel data and need to be duplicated.
     * Mappings that come from userspace will be unnecessarily duplicated,
     * this could be optimized.
     */
    map = kmemdup(mapping, sizeof(*mapping), GFP_KERNEL);
    if (map == NULL)
        return -ENOMEM;

    INIT_LIST_HEAD(&map->ev_subs);

    size = sizeof(*mapping->menu_info) * mapping->menu_count;
    map->menu_info = kmemdup(mapping->menu_info, size, GFP_KERNEL);
    if (map->menu_info == NULL) {
        kfree(map);
        return -ENOMEM;
    }

    if (map->get == NULL)
        map->get = uvc_get_le_value;
    if (map->set == NULL)
        map->set = uvc_set_le_value;

    list_add_tail(&map->list, &ctrl->info.mappings);
    uvc_trace(UVC_TRACE_CONTROL,
            "Adding mapping '%s' to control %pUl/%u.\n",
            map->name, ctrl->info.entity, ctrl->info.selector);

    return 0;
}

/*
 * Prune an entity of its bogus controls using a blacklist. Bogus controls
 * are currently the ones that crash the camera or unconditionally return an
 * error when queried.
 */
// complete 
static void uvc_ctrl_prune_entity(struct uvc_device *dev,
        struct uvc_entity *entity)
{
    struct uvc_ctrl_blacklist {
        struct usb_device_id id;
        u8 index;
    };

    static const struct uvc_ctrl_blacklist processing_blacklist[] = {
        { { USB_DEVICE(0x13d3, 0x509b) }, 9 }, /* Gain */
        { { USB_DEVICE(0x1c4f, 0x3000) }, 6 }, /* WB Temperature */
        { { USB_DEVICE(0x5986, 0x0241) }, 2 }, /* Hue */
    };
    static const struct uvc_ctrl_blacklist camera_blacklist[] = {
        { { USB_DEVICE(0x06f8, 0x3005) }, 9 }, /* Zoom, Absolute */
    };

    const struct uvc_ctrl_blacklist *blacklist;
    unsigned int size;
    unsigned int count;
    unsigned int i;
    u8 *controls;

    switch (UVC_ENTITY_TYPE(entity)) {
        case UVC_VC_PROCESSING_UNIT:
            blacklist = processing_blacklist;
            count = ARRAY_SIZE(processing_blacklist);
            controls = entity->processing.bmControls;
            size = entity->processing.bControlSize;
            break;

        case UVC_ITT_CAMERA:
            blacklist = camera_blacklist;
            count = ARRAY_SIZE(camera_blacklist);
            controls = entity->camera.bmControls;
            size = entity->camera.bControlSize;
            break;

        default:
            return;
    }

    for (i = 0; i < count; ++i) {
        if (!usb_match_one_id(dev->intf, &blacklist[i].id))
            continue;

        if (blacklist[i].index >= 8 * size ||
                !uvc_test_bit(controls, blacklist[i].index))
            continue;

        uvc_trace(UVC_TRACE_CONTROL, "%u/%u control is black listed, "
                "removing it.\n", entity->id, blacklist[i].index);

        uvc_clear_bit(controls, blacklist[i].index);
    }
}

/*
 * Add control information and hardcoded stock control mappings to the given
 * device.
 */
// complete 
static void uvc_ctrl_init_ctrl(struct uvc_device *dev, struct uvc_control *ctrl)
{
    const struct uvc_control_info *info = uvc_ctrls;
    const struct uvc_control_info *iend = info + ARRAY_SIZE(uvc_ctrls);
    const struct uvc_control_mapping *mapping = uvc_ctrl_mappings;
    const struct uvc_control_mapping *mend =
        mapping + ARRAY_SIZE(uvc_ctrl_mappings);

    /* XU controls initialization requires querying the device for control
     * information. As some buggy UVC devices will crash when queried
     * repeatedly in a tight loop, delay XU controls initialization until
     * first use.
     */
    if (UVC_ENTITY_TYPE(ctrl->entity) == UVC_VC_EXTENSION_UNIT)
        return;

    for (; info < iend; ++info) {
        if (uvc_entity_match_guid(ctrl->entity, info->entity) &&
                ctrl->index == info->index) {
            uvc_ctrl_add_info(dev, ctrl, info);
            break;
        }
    }

    if (!ctrl->initialized)
        return;

    for (; mapping < mend; ++mapping) {
        if (uvc_entity_match_guid(ctrl->entity, mapping->entity) &&
                ctrl->info.selector == mapping->selector)
            __uvc_ctrl_add_mapping(dev, ctrl, mapping);
    }
}

/*
 * Initialize device controls.
 */


// complete   // from uvc_driver

int uvc_ctrl_init_device(struct uvc_device *dev)
{
    struct uvc_entity *entity;
    unsigned int i;

    /* Walk the entities list and instantiate controls */
    list_for_each_entry(entity, &dev->entities, list) {
        struct uvc_control *ctrl;
        unsigned int bControlSize = 0, ncontrols;
        __u8 *bmControls = NULL;

        if (UVC_ENTITY_TYPE(entity) == UVC_VC_EXTENSION_UNIT) { //Video Class-Specific VC Interface Descriptor Subtypes 0x05
            bmControls = entity->extension.bmControls;
            bControlSize = entity->extension.bControlSize;
        } else if (UVC_ENTITY_TYPE(entity) == UVC_VC_PROCESSING_UNIT) { //Video Class-Specific VC Interface Descriptor Subtypes 0x06
            bmControls = entity->processing.bmControls;
            bControlSize = entity->processing.bControlSize;
        } else if (UVC_ENTITY_TYPE(entity) == UVC_ITT_CAMERA) {  // ITT input terminal type 
            bmControls = entity->camera.bmControls;
            bControlSize = entity->camera.bControlSize;
        }

        /* Remove bogus/blacklisted controls */
        uvc_ctrl_prune_entity(dev, entity);

        /* Count supported controls and allocate the controls array */
        ncontrols = memweight(bmControls, bControlSize);
        if (ncontrols == 0)
            continue;

        entity->controls = kcalloc(ncontrols, sizeof(*ctrl),
                GFP_KERNEL);
        if (entity->controls == NULL)
            return -ENOMEM;
        entity->ncontrols = ncontrols;

        /* Initialize all supported controls */
        ctrl = entity->controls;
        for (i = 0; i < bControlSize * 8; ++i) {
            if (uvc_test_bit(bmControls, i) == 0)
                continue;

            ctrl->entity = entity;
            ctrl->index = i;

            uvc_ctrl_init_ctrl(dev, ctrl);
            ctrl++;
        }
    }

    return 0;
}

/*
 * Cleanup device controls.
 */
// complete 

static void uvc_ctrl_cleanup_mappings(struct uvc_device *dev,
        struct uvc_control *ctrl)
{
    struct uvc_control_mapping *mapping, *nm;

    list_for_each_entry_safe(mapping, nm, &ctrl->info.mappings, list) {
        list_del(&mapping->list);
        kfree(mapping->menu_info);
        kfree(mapping);
    }
}

// complete   // from uvc_driver 
void uvc_ctrl_cleanup_device(struct uvc_device *dev)
{
    struct uvc_entity *entity;
    unsigned int i;

    /* Free controls and control mappings for all entities. */
    list_for_each_entry(entity, &dev->entities, list) {
        for (i = 0; i < entity->ncontrols; ++i) {
            struct uvc_control *ctrl = &entity->controls[i];

            if (!ctrl->initialized)
                continue;

            uvc_ctrl_cleanup_mappings(dev, ctrl);
            kfree(ctrl->uvc_data);
        }

        kfree(entity->controls);
    }
}
