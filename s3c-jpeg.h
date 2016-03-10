/* linux/drivers/media/video/samsung/jpeg_v2/s3c-jpeg.h
  *
  * Copyright (c) 2010 Samsung Electronics Co., Ltd.
  * http://www.samsung.com/
  *
  * Header file for Samsung Jpeg Interface driver
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
 */


#ifndef __JPEG_DRIVER_H__
#define __JPEG_DRIVER_H__


#define MAX_INSTANCE_NUM	1
#define MAX_PROCESSING_THRESHOLD 1000	/* 1Sec */

#define JPEG_IOCTL_MAGIC 'J'

#define IOCTL_JPG_DECODE			_IO(JPEG_IOCTL_MAGIC, 1)
#define IOCTL_JPG_ENCODE			_IO(JPEG_IOCTL_MAGIC, 2)
#define IOCTL_JPG_GET_STRBUF			_IO(JPEG_IOCTL_MAGIC, 3)
#define IOCTL_JPG_GET_FRMBUF			_IO(JPEG_IOCTL_MAGIC, 4)
#define IOCTL_JPG_GET_THUMB_STRBUF		_IO(JPEG_IOCTL_MAGIC, 5)
#define IOCTL_JPG_GET_THUMB_FRMBUF		_IO(JPEG_IOCTL_MAGIC, 6)
#define IOCTL_JPG_GET_PHY_FRMBUF		_IO(JPEG_IOCTL_MAGIC, 7)
#define IOCTL_JPG_GET_PHY_THUMB_FRMBUF		_IO(JPEG_IOCTL_MAGIC, 8)
#define JPG_CLOCK_DIVIDER_RATIO_QUARTER	4

/* Driver Helper function */
#define to_jpeg_plat(d)		(to_platform_device(d)->dev.platform_data)


enum jpg_return_status {
	JPG_FAIL,
	JPG_SUCCESS,
	OK_HD_PARSING,
	ERR_HD_PARSING,
	OK_ENC_OR_DEC,
	ERR_ENC_OR_DEC,
	ERR_UNKNOWN
};

enum image_type {
	JPG_RGB16,
	JPG_YCBYCR,
	JPG_TYPE_UNKNOWN
};

enum sample_mode {
	JPG_444,
	JPG_422,
	JPG_420,
	JPG_400,
	RESERVED1,
	RESERVED2,
	JPG_411,
	JPG_SAMPLE_UNKNOWN
};

enum out_mode {
	YCBCR_422,
	YCBCR_420,
	YCBCR_SAMPLE_UNKNOWN
};

enum in_mode {
	JPG_MODESEL_YCBCR = 1,
	JPG_MODESEL_RGB,
	JPG_MODESEL_UNKNOWN
};

enum encode_type {
    JPG_MAIN,//分辨率大于320*240
    JPG_THUMBNAIL //分辨率小于320*240
};

enum image_quality_type {
	JPG_QUALITY_LEVEL_1 = 0, /*high quality*/
	JPG_QUALITY_LEVEL_2,
	JPG_QUALITY_LEVEL_3,
	JPG_QUALITY_LEVEL_4     /*low quality*/
};

struct jpg_dec_proc_param {
	enum sample_mode	sample_mode;
	enum encode_type	dec_type;
	enum out_mode		out_format;
	unsigned int		width;
	unsigned int		height;
	unsigned int		data_size;
	unsigned int		file_size;
};

struct jpg_enc_proc_param {
	enum sample_mode	sample_mode;
	enum encode_type	enc_type;
	enum in_mode		in_format;
	enum image_quality_type	quality;
	unsigned int		width;
	unsigned int		height;
	unsigned int		data_size;
	unsigned int		file_size;
};

struct jpg_args {
	char			*in_buf;
	char			*phy_in_buf;
	int			in_buf_size;
	char			*out_buf;
	char			*phy_out_buf;
	int			out_buf_size;
	char			*in_thumb_buf;
	char			*phy_in_thumb_buf;
	int			in_thumb_buf_size;
	char			*out_thumb_buf;
	char			*phy_out_thumb_buf;
	int			out_thumb_buf_size;
	char			*mapped_addr;
	struct jpg_dec_proc_param	*dec_param;
	struct jpg_enc_proc_param	*enc_param;
	struct jpg_enc_proc_param	*thumb_enc_param;
};

#endif /*__JPEG_DRIVER_H__*/
