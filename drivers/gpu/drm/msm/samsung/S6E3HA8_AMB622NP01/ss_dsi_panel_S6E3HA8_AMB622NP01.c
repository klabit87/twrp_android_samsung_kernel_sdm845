/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Author: jb09.kim
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2017, Samsung Electronics. All rights reserved.

*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
#include "ss_dsi_panel_S6E3HA8_AMB622NP01.h"
#include "ss_dsi_mdnie_S6E3HA8_AMB622NP01.h"

/* AOD Mode status on AOD Service */

enum {
	ALPM_CTRL_2NIT,
	ALPM_CTRL_10NIT,
	ALPM_CTRL_30NIT,
	ALPM_CTRL_60NIT,
	HLPM_CTRL_2NIT,
	HLPM_CTRL_10NIT,
	HLPM_CTRL_30NIT,
	HLPM_CTRL_60NIT,
	MAX_LPM_CTRL,
};

enum {
	HIGH_TEMP = 0,
	MID_TEMP,
	LOW_TEMP,
	MAX_TEMP
};

/* Register to control brightness level */
#define ALPM_REG	0x53
/* Register to cnotrol ALPM/HLPM mode */
#define ALPM_CTRL_REG	0xBB
#define LUMINANCE_MAX 74

#define IRC_MODERATO_MODE_VAL	0x69
#define IRC_FLAT_GAMMA_MODE_VAL	0x29

u8 vint_array_revA[2] = {
	0x28, /* 700~15nit*/
	0x23, /* 14~2nit*/
};

u8 vint_array_revG[5] = {
	0x26, /*  HBM(700nit) */
	0x25, /*  675nit */
	0x25, /*  650nit */
	0x24, /*  625nit */
	0x23, /*  600~2nit */
};

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	int on_reg_list[1][2] = { {POC_CTRL_REG, -EINVAL} };
	struct dsi_panel_cmd_set *on_cmd_list[1];
	char poc_buffer[4] = {0,};
	static unsigned int i_poc_buffer[4] = {0,};
	int MAX_POC = 4;
	int loop;

	on_cmd_list[0] = ss_get_cmds(vdd, DSI_CMD_SET_ON);

	LCD_INFO("+: ndx=%d\n", vdd->ndx);
	ss_panel_attach_set(vdd, true);

	if (!vdd->poc_driver.is_support) {
		LCD_DEBUG("Not Support POC Function \n");
		goto end;
	}

	/* Read Panel POC (EBh 1nd~4th) */
	if (ss_get_cmds(vdd, RX_POC_STATUS)->count) {
		memset(poc_buffer, 0x00, sizeof(poc_buffer[0]) * MAX_POC);

		if (unlikely(vdd->is_factory_mode) &&
				vdd->dtsi_data.samsung_support_factory_panel_swap) {
			memset(i_poc_buffer, 0x00, sizeof(i_poc_buffer[0]) * MAX_POC);
		}

		if (i_poc_buffer[3] == 0) {
			ss_panel_data_read(vdd, RX_POC_STATUS,
					poc_buffer, LEVEL1_KEY);

			for (loop = 0; loop < MAX_POC; loop++)
				i_poc_buffer[loop] = (unsigned int)poc_buffer[loop];
		}

		LCD_DEBUG("[POC] DSI%d: %02x %02x %02x %02x\n",
				vdd->ndx,
				i_poc_buffer[0],
				i_poc_buffer[1],
				i_poc_buffer[2],
				i_poc_buffer[3]);

		/*
		 * Update REBh 4th param to 0xFF or 0x64
		 */
		ss_find_reg_offset(on_reg_list, on_cmd_list,
				sizeof(on_cmd_list) / sizeof(on_cmd_list[0]));

		if ((on_reg_list[0][1] != -EINVAL) &&\
			(vdd->octa_id_dsi[1] == 0x1)) {
			if (i_poc_buffer[3] == 0x33)
				i_poc_buffer[3] = 0x64;

			on_cmd_list[0]->cmds[on_reg_list[0][1]].msg.tx_buf[4] =
				i_poc_buffer[3];

			LCD_DEBUG("Update POC register, 0x%02x\n",
					i_poc_buffer[3]);
		}

		LCD_DEBUG("[POC] DSI%d: octa_id:%d, poc_buffer:%02x, index:%d\n",
				vdd->ndx,
				vdd->octa_id_dsi[1],
				i_poc_buffer[3],
				on_reg_list[0][1]);
	} else {
		LCD_ERR("DSI%d no poc_rx_cmds cmd\n", vdd->ndx);
	}

end:
	LCD_INFO("-: ndx=%d \n", vdd->ndx);

	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	self_mask_img_write();
	self_mask_on(true);

	return true;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	switch (ss_panel_rev_get(vdd)) {
	case 0x00:
		/* Pre. / Rev.A / Rev.B */
		vdd->panel_revision = 'B';
		break;
	case 0x01:
		/* Rev.C / Rev.D */
		vdd->panel_revision = 'D';
		break;
	case 0x02:
		/* Rev.E / Rev.F */
		vdd->panel_revision = 'F';
		break;
	case 0x03:
		vdd->panel_revision = 'G';
		break;
	default:
		vdd->panel_revision = 'H';
		LCD_ERR("Invalid panel_rev(default rev : %c)\n",
				vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';

	LCD_INFO_ONCE("panel_revision = %c %d \n",
					vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

static int ss_manufacture_date_read(struct samsung_display_driver_data *vdd)
{
	unsigned char date[4];
	int year, month, day;
	int hour, min;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (A1h 5,6 th) for manufacture date */
	if (ss_get_cmds(vdd, RX_MANUFACTURE_DATE)->count) {
		ss_panel_data_read(vdd, RX_MANUFACTURE_DATE, date, LEVEL1_KEY);

		year = date[0] & 0xf0;
		year >>= 4;
		year += 2011; // 0 = 2011 year
		month = date[0] & 0x0f;
		day = date[1] & 0x1f;
		hour = date[2] & 0x0f;
		min = date[3] & 0x1f;

		vdd->manufacture_date_dsi = year * 10000 + month * 100 + day;
		vdd->manufacture_time_dsi = hour * 100 + min;

		LCD_ERR("manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			vdd->ndx, vdd->manufacture_date_dsi, vdd->manufacture_time_dsi,
			year, month, day, hour, min);

	} else {
		LCD_ERR("DSI%d no manufacture_date_rx_cmds cmds(%d)", vdd->ndx, vdd->panel_revision);
		return false;
	}

	return true;
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	char ddi_id[5];
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for ddi id */
	if (ss_get_cmds(vdd, RX_DDI_ID)->count) {
		ss_panel_data_read(vdd, RX_DDI_ID, ddi_id, LEVEL1_KEY);

		for (loop = 0; loop < 5; loop++)
			vdd->ddi_id_dsi[loop] = ddi_id[loop];

		LCD_INFO("DSI%d : %02x %02x %02x %02x %02x\n", vdd->ndx,
			vdd->ddi_id_dsi[0], vdd->ddi_id_dsi[1],
			vdd->ddi_id_dsi[2], vdd->ddi_id_dsi[3],
			vdd->ddi_id_dsi[4]);
	} else {
		LCD_ERR("DSI%d no ddi_id_rx_cmds cmds", vdd->ndx);
		return false;
	}

	return true;
}

static int get_hbm_candela_value(int level)
{
	if (level == 13)
		return 443;
	else if (level == 6)
		return 465;
	else if (level == 7)
		return 488;
	else if (level == 8)
		return 510;
	else if (level == 9)
		return 533;
	else if (level == 10)
		return 555;
	else if (level == 11)
		return 578;
	else if (level == 12)
		return 600;
	else
		return 600;
}

static struct dsi_panel_cmd_set *ss_hbm_gamma(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *hbm_gamma_cmds = ss_get_cmds(vdd, TX_HBM_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_gamma_cmds)) {
		LCD_ERR("Invalid data  vdd : 0x%zx cmd : 0x%zx", (size_t)vdd, (size_t)hbm_gamma_cmds);
		return NULL;
	}

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi->generate_hbm_gamma)) {
		LCD_ERR("generate_hbm_gamma is NULL error");
		return NULL;
	} else {
		vdd->smart_dimming_dsi->generate_hbm_gamma(
			vdd->smart_dimming_dsi,
			vdd->auto_brightness,
			&hbm_gamma_cmds->cmds[0].msg.tx_buf[1]);

		*level_key = LEVEL1_KEY;
		return hbm_gamma_cmds;
	}
}

static struct dsi_panel_cmd_set *ss_hbm_etc(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *hbm_etc_cmds = ss_get_cmds(vdd, TX_HBM_ETC);

	/* hbm 11 steps */
	u8 hbm_elvss_3th_val_array_revA[] = {
		0x24,	/* 443nit */
		0x22,	/* 467nit */
		0x21,	/* 490nit */
		0x20,	/* 513nit */
		0x1E,	/* 537nit */
		0x1D,	/* 560nit */
		0x1C,	/* 583nit */
		0x1B,	/* 606nit */
		0x1A,	/* 630nit */
		0x19,	/* 653nit */
		0x17,	/* 676nit */
		0x16	/* 700nit */
	};

	/* hbm 60 steps after revG*/
	u8 hbm_elvss_3th_val_array_revG[] = {
		0x24, 0x24, 0x24, 0x24, 0x24, /* 425nit */
		0x22, 0x22, 0x22, 0x22, 0x22, /* 450nit */
		0x21, 0x21, 0x21, 0x21, 0x21, /* 475nit */
		0x20, 0x20, 0x20, 0x20, 0x20, /* 500nit */
		0x1E, 0x1E, 0x1E, 0x1E, 0x1E, /* 525nit */
		0x1D, 0x1D, 0x1D, 0x1D, 0x1D, /* 550nit */
		0x1C, 0x1C, 0x1C, 0x1C, 0x1C, /* 575nit */
		0x1B, 0x1B, 0x1B, 0x1B, 0x1B, /* 600nit */
		0x1A, 0x1A, 0x1A, 0x1A, 0x1A, /* 625nit */
		0x19, 0x19, 0x19, 0x19, 0x19, /* 650nit */
		0x17, 0x17, 0x17, 0x17, 0x17, /* 675nit */
		0x16, 0x16, 0x16, 0x16, 0x16  /* 700nit */
	};

	u8 *hbm_elvss;

	u8 vint;
	char panel_revision = 'A';

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_etc_cmds)) {
		LCD_ERR("Invalid data  vdd : 0x%zx cmd : 0x%zx", (size_t)vdd, (size_t)hbm_etc_cmds);
		return NULL;
	}

	panel_revision += vdd->panel_revision;

	/* 0xB5 2nd temperature */
	hbm_etc_cmds->cmds[1].msg.tx_buf[1] = vdd->temperature > 0 ?
			vdd->temperature : BIT(7) | (-1*vdd->temperature);

	/* ELVSS 0xB5 3th: elvss_dim_offset, B5 24th: elvss_cal_offset from OTP value */

	if (panel_revision < 'G')
		hbm_elvss = hbm_elvss_3th_val_array_revA;
	else
		hbm_elvss = hbm_elvss_3th_val_array_revG;

	hbm_etc_cmds->cmds[1].msg.tx_buf[3] =
		hbm_elvss[vdd->auto_brightness - HBM_MODE];
	hbm_etc_cmds->cmds[1].msg.tx_buf[23] =
		vdd->display_status_dsi.elvss_value2;

	/* VINT */
	if (panel_revision < 'G') {
		vint = vint_array_revA[0];
	} else {
		if (vdd->candela_level <= 600)
			vint = vint_array_revG[4];
		else if (vdd->candela_level <= 625)
			vint = vint_array_revG[3];
		else if (vdd->candela_level <= 650)
			vint = vint_array_revG[2];
		else if (vdd->candela_level <= 675)
			vint = vint_array_revG[1];
		else
			vint = vint_array_revG[0];
	}

	hbm_etc_cmds->cmds[2].msg.tx_buf[2] = vint;

	/* ACL percentage, ACL ON/OFF */
	if (vdd->gradual_acl_val) {
		hbm_etc_cmds->cmds[3].msg.tx_buf[6] = 0x26;	/* 8% */
		hbm_etc_cmds->cmds[4].msg.tx_buf[1] = 0x2;	/* ACL ON */
	} else {
		hbm_etc_cmds->cmds[3].msg.tx_buf[6] = 0;	/* 0% */
		hbm_etc_cmds->cmds[4].msg.tx_buf[1] = 0x0;	/* ACL OFF */
	}

	*level_key = LEVEL1_KEY;

	LCD_INFO("0xB5 3th: 0x%x 0xB5 elvss_23th_val(elvss val) 0x%x, acl=%x, per=%x, vint=%x\n",
			hbm_elvss[vdd->auto_brightness - HBM_MODE],
			vdd->display_status_dsi.elvss_value2,
			hbm_etc_cmds->cmds[4].msg.tx_buf[1],
			hbm_etc_cmds->cmds[3].msg.tx_buf[6],
			hbm_etc_cmds->cmds[2].msg.tx_buf[2]);

	return hbm_etc_cmds;
}

static int ss_elvss_read(struct samsung_display_driver_data *vdd)
{
	char elvss_b5[2];

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (B5h 23th,24th) for elvss*/
	ss_panel_data_read(vdd, RX_ELVSS, elvss_b5, LEVEL1_KEY);

	vdd->display_status_dsi.elvss_value1 = elvss_b5[0]; /*0xB5 23th OTP value*/
	vdd->display_status_dsi.elvss_value2 = elvss_b5[1]; /*0xB5 24th */

	return true;
}

#define NUM_IRC_OTP	17
static int ss_irc_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *set;
	int i;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	if (!vdd->display_status_dsi.irc_otp) {
		vdd->display_status_dsi.irc_otp	= kzalloc(NUM_IRC_OTP, GFP_KERNEL);
		if (!vdd->display_status_dsi.irc_otp) {
			LCD_ERR("fail to allocate irc_otp memory\n");
			return false;
		}
	}

	/* Read mtp (B5h 23th,24th) for elvss*/
	ss_panel_data_read(vdd, RX_IRC, vdd->display_status_dsi.irc_otp,
							LEVEL1_KEY);

	/* update irc packet with otp value */
	set = ss_get_cmds(vdd, TX_IRC_SUBDIVISION);
	for  (i = 0; i < set->count; i++)
		memcpy(&set->cmds[i].msg.tx_buf[2], vdd->display_status_dsi.irc_otp, NUM_IRC_OTP);

	set = ss_get_cmds(vdd, TX_HBM_IRC);
	for  (i = 0; i < set->count; i++)
		memcpy(&set->cmds[i].msg.tx_buf[2], vdd->display_status_dsi.irc_otp, NUM_IRC_OTP);

	return true;
}

static int ss_hbm_read(struct samsung_display_driver_data *vdd)
{
	char hbm_buffer[34];
	struct dsi_panel_cmd_set *hbm_gamma_cmds = ss_get_cmds(vdd, TX_HBM_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_gamma_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)hbm_gamma_cmds);
		return false;
	}

	/* Read mtp (B3h 3~35th) for hbm gamma */
	ss_panel_data_read(vdd, RX_HBM, hbm_buffer, LEVEL1_KEY);

	memcpy(&hbm_gamma_cmds->cmds[0].msg.tx_buf[1], hbm_buffer, 34);

	return true;
}

#define COORDINATE_DATA_SIZE 6
#define MDNIE_SCR_WR_ADDR	0x32
#define RGB_INDEX_SIZE 4
#define COEFFICIENT_DATA_SIZE 8

#define F1(x, y) (((y << 10) - (((x << 10) * 101) / 94) + (28 << 10)) >> 10)
#define F2(x, y) (((y << 10) + (((x << 10) * 106) / 29) - (14083 << 10)) >> 10)

static char coordinate_data_1[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfa, 0x00, 0xfa, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfb, 0x00, 0xfe, 0x00}, /* Tune_2 */
	{0xf9, 0x00, 0xf7, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfe, 0x00, 0xfa, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xf9, 0x00, 0xfb, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfc, 0x00, 0xff, 0x00, 0xf9, 0x00}, /* Tune_7 */
	{0xfa, 0x00, 0xff, 0x00, 0xfb, 0x00}, /* Tune_8 */
	{0xf9, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};

static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xf4, 0x00, 0xed, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xf5, 0x00, 0xf1, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xf5, 0x00, 0xf5, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xf6, 0x00, 0xec, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xf8, 0x00, 0xf0, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xf9, 0x00, 0xf5, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfb, 0x00, 0xf0, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xfc, 0x00, 0xf2, 0x00}, /* Tune_8 */
	{0xff, 0x00, 0xfe, 0x00, 0xf6, 0x00}, /* Tune_9 */
};

static char (*coordinate_data[MAX_MODE])[COORDINATE_DATA_SIZE] = {
	coordinate_data_2,
	coordinate_data_2,
	coordinate_data_2,
	coordinate_data_1,
	coordinate_data_1,
	coordinate_data_1
};

static int rgb_index[][RGB_INDEX_SIZE] = {
	{ 5, 5, 5, 5 }, /* dummy */
	{ 5, 2, 6, 3 },
	{ 5, 2, 4, 1 },
	{ 5, 8, 4, 7 },
	{ 5, 8, 6, 9 },
};

static int coefficient[][COEFFICIENT_DATA_SIZE] = {
	{       0,      0,      0,      0,      0,      0,      0,      0 }, /* dummy */
	{   85850,  68462, -22016, -26984,  4268,  34523, -6545, -6185 },
	{ -108292, -89259,  29166,  33669, -22116, 10322,  1741,  1705 },
	{  -89731, -73367,  23408,  28412, -44813, -73586,  20064,  18179 },
	{   86820,  70834, -22574, -27511,  19251, -12295,   -996,   -913 },
};

static int mdnie_coordinate_index(int x, int y)
{
	int tune_number = 0;

	if (F1(x, y) > 0) {
		if (F2(x, y) > 0) {
			tune_number = 1;
		} else {
			tune_number = 2;
		}
	} else {
		if (F2(x, y) > 0) {
			tune_number = 4;
		} else {
			tune_number = 3;
		}
	}

	return tune_number;
}

static int mdnie_coordinate_x(int x, int y, int index)
{
	int result = 0;

	result = (coefficient[index][0] * x) + (coefficient[index][1] * y) + (((coefficient[index][2] * x + 512) >> 10) * y) + (coefficient[index][3] * 10000);

	result = (result + 512) >> 10;

	if (result < 0)
		result = 0;
	if (result > 1024)
		result = 1024;

	return result;
}

static int mdnie_coordinate_y(int x, int y, int index)
{
	int result = 0;

	result = (coefficient[index][4] * x) + (coefficient[index][5] * y) + (((coefficient[index][6] * x + 512) >> 10) * y) + (coefficient[index][7] * 10000);

	result = (result + 512) >> 10;

	if (result < 0)
		result = 0;
	if (result > 1024)
		result = 1024;

	return result;
}

static int ss_mdnie_read(struct samsung_display_driver_data *vdd)
{
	char x_y_location[4];
	int x, y;
	int mdnie_tune_index = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (A1h 1~5th) for ddi id */
	if (ss_get_cmds(vdd, RX_MDNIE)->count) {
		ss_panel_data_read(vdd, RX_MDNIE, x_y_location, LEVEL1_KEY);

		vdd->mdnie_x = x_y_location[0] << 8 | x_y_location[1];	/* X */
		vdd->mdnie_y = x_y_location[2] << 8 | x_y_location[3];	/* Y */

		mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie_x, vdd->mdnie_y);

		if (((vdd->mdnie_x - 2983) * (vdd->mdnie_x - 2983) + (vdd->mdnie_y - 3178) * (vdd->mdnie_y - 3178)) <= 225) {
			x = 0;
			y = 0;
		} else {
			x = mdnie_coordinate_x(vdd->mdnie_x, vdd->mdnie_y, mdnie_tune_index);
			y = mdnie_coordinate_y(vdd->mdnie_x, vdd->mdnie_y, mdnie_tune_index);
		}

		coordinate_tunning_calculate(vdd, x, y, coordinate_data,
				rgb_index[mdnie_tune_index],
				MDNIE_SCR_WR_ADDR, COORDINATE_DATA_SIZE);

		LCD_INFO("DSI%d : X-%d Y-%d \n", vdd->ndx,
			vdd->mdnie_x, vdd->mdnie_y);
	} else {
		LCD_ERR("DSI%d no mdnie_read_rx_cmds cmds", vdd->ndx);
		return false;
	}

	return true;
}

static int ss_samart_dimming_init(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	vdd->smart_dimming_dsi = vdd->panel_func.samsung_smart_get_conf();
	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi)) {
		LCD_ERR("DSI%d smart_dimming_dsi is null", vdd->ndx);
		return false;
	}

	ss_panel_data_read(vdd, RX_SMART_DIM_MTP,
			vdd->smart_dimming_dsi->mtp_buffer, LEVEL1_KEY);

	/* Initialize smart dimming related things here */
	/* lux_tab setting for 350cd */
	vdd->smart_dimming_dsi->lux_tab = vdd->dtsi_data.candela_map_table[vdd->panel_revision].cd;
	vdd->smart_dimming_dsi->lux_tabsize = vdd->dtsi_data.candela_map_table[vdd->panel_revision].tab_size;
	vdd->smart_dimming_dsi->man_id = vdd->manufacture_id_dsi;
	if (vdd->panel_func.samsung_panel_revision)
		vdd->smart_dimming_dsi->panel_revision = vdd->panel_func.samsung_panel_revision(vdd);

	/* copy hbm gamma payload for hbm interpolation calc */
	pcmds = ss_get_cmds(vdd, TX_HBM_GAMMA);
	vdd->smart_dimming_dsi->hbm_payload = &pcmds->cmds[0].msg.tx_buf[1];

	/* Just a safety check to ensure smart dimming data is initialised well */
	vdd->smart_dimming_dsi->init(vdd->smart_dimming_dsi);

	vdd->temperature = 20; // default temperature

	vdd->smart_dimming_loaded_dsi = true;

	LCD_INFO("DSI%d : --\n", vdd->ndx);

	return true;
}

static int ss_cell_id_read(struct samsung_display_driver_data *vdd)
{
	char cell_id_buffer[MAX_CELL_ID] = {0,};
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read Panel Unique Cell ID (C8h 41~51th) */
	if (ss_get_cmds(vdd, RX_CELL_ID)->count) {
		memset(cell_id_buffer, 0x00, MAX_CELL_ID);

		ss_panel_data_read(vdd, RX_CELL_ID, cell_id_buffer, LEVEL1_KEY);

		for (loop = 0; loop < MAX_CELL_ID; loop++)
			vdd->cell_id_dsi[loop] = cell_id_buffer[loop];

		LCD_INFO("DSI%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			vdd->ndx, vdd->cell_id_dsi[0],
			vdd->cell_id_dsi[1],	vdd->cell_id_dsi[2],
			vdd->cell_id_dsi[3],	vdd->cell_id_dsi[4],
			vdd->cell_id_dsi[5],	vdd->cell_id_dsi[6],
			vdd->cell_id_dsi[7],	vdd->cell_id_dsi[8],
			vdd->cell_id_dsi[9],	vdd->cell_id_dsi[10]);

	} else {
		LCD_ERR("DSI%d no cell_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	return true;
}

static int ss_octa_id_read(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read Panel Unique OCTA ID (C9h 2nd~21th) */
	if (ss_get_cmds(vdd, RX_OCTA_ID)->count) {
		memset(vdd->octa_id_dsi, 0x00, MAX_OCTA_ID);

		ss_panel_data_read(vdd, RX_OCTA_ID,
				vdd->octa_id_dsi, LEVEL1_KEY);

		LCD_INFO("octa id: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			vdd->octa_id_dsi[0], vdd->octa_id_dsi[1],
			vdd->octa_id_dsi[2], vdd->octa_id_dsi[3],
			vdd->octa_id_dsi[4], vdd->octa_id_dsi[5],
			vdd->octa_id_dsi[6], vdd->octa_id_dsi[7],
			vdd->octa_id_dsi[8], vdd->octa_id_dsi[9],
			vdd->octa_id_dsi[10], vdd->octa_id_dsi[11],
			vdd->octa_id_dsi[12], vdd->octa_id_dsi[13],
			vdd->octa_id_dsi[14], vdd->octa_id_dsi[15],
			vdd->octa_id_dsi[16], vdd->octa_id_dsi[17],
			vdd->octa_id_dsi[18], vdd->octa_id_dsi[19]);

	} else {
		LCD_ERR("DSI%d no octa_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	return true;
}

static struct dsi_panel_cmd_set aid_cmd;
static struct dsi_panel_cmd_set *ss_aid(struct samsung_display_driver_data *vdd, int *level_key)
{
	int cd_index = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	if (vdd->pac)
		cd_index = vdd->pac_cd_idx;
	else
		cd_index = vdd->bl_level;

	aid_cmd.count = 1;
	aid_cmd.cmds = &(ss_get_cmds(vdd, TX_AID_SUBDIVISION)->cmds[cd_index]);
	LCD_DEBUG("[%d] level(%d), aid(%x %x)\n",
			cd_index, vdd->bl_level,
			aid_cmd.cmds->msg.tx_buf[1],
			aid_cmd.cmds->msg.tx_buf[2]);

	*level_key = LEVEL1_KEY;

	return &aid_cmd;
}

static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);

	/* ACL percentage */
	if (vdd->gradual_acl_val)
		pcmds->cmds[0].msg.tx_buf[6] = 0x48;	/* 15% */
	else
		pcmds->cmds[0].msg.tx_buf[6] = 0;	/* 0% */

	LCD_INFO("gradual_acl: %d, acl per: 0x%x",
			vdd->gradual_acl_val, pcmds->cmds[0].msg.tx_buf[6]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	return ss_get_cmds(vdd, TX_ACL_OFF);
}

#define SUPPORT_LOWTEMP_ELVSS	15
static struct dsi_panel_cmd_set elvss_cmd;
static struct dsi_panel_cmd_set *ss_elvss(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *elvss_cmds = ss_get_cmds(vdd, TX_ELVSS);
	int cd_index = 0;
	int cmd_idx = 0;
	u8 elvss_23th_val;
	char panel_revision = 'A';
	int idx_temp = 0;
	u8 elvss_3th_val_array[SUPPORT_LOWTEMP_ELVSS][MAX_TEMP] = {
		/* high mid low*/
		{0x12, 0x1b, 0x22},	/* 0 nit */
		{0x12, 0x1b, 0x22},	/* 1 nit */
		{0x12, 0x1b, 0x22},	/* 2 nit */
		{0x12, 0x1b, 0x22},	/* 3 nit */
		{0x13, 0x1b, 0x22},	/* 4 nit */
		{0x13, 0x1b, 0x22},	/* 5 nit */
		{0x14, 0x1b, 0x22},	/* 6 nit */
		{0x15, 0x1b, 0x22},	/* 7 nit */
		{0x16, 0x1b, 0x22},	/* 8 nit */
		{0x17, 0x1f, 0x22},	/* 9 nit */
		{0x18, 0x1f, 0x22},	/* 10 nit */
		{0x1a, 0x1f, 0x22},	/* 11 nit */
		{0x1c, 0x1f, 0x22},	/* 12 nit */
		{0x1e, 0x1f, 0x22},	/* 13 nit */
		{0x20, 0x1f, 0x22},	/* 14 nit */
	};

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(elvss_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)elvss_cmds);
		return NULL;
	}

	panel_revision += vdd->panel_revision;

	cd_index  = vdd->cd_idx;
	LCD_DEBUG("cd_index (%d)\n", cd_index);

	if (!vdd->dtsi_data.smart_acl_elvss_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data.smart_acl_elvss_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data.smart_acl_elvss_map_table[vdd->panel_revision].cmd_idx[cd_index];

	elvss_cmd.cmds = &(elvss_cmds->cmds[cmd_idx]);
	elvss_cmd.count = 1;
	*level_key = LEVEL1_KEY;

	/* 0xB5 1th TSET */
	elvss_cmd.cmds->msg.tx_buf[1] = vdd->temperature > 0 ?
			vdd->temperature : BIT(7) | (-1*vdd->temperature);

	/* 0xB5 2th MSP */
	if (vdd->candela_level > 39)
		elvss_cmd.cmds->msg.tx_buf[2] = 0xDC;
	else
		elvss_cmd.cmds->msg.tx_buf[2] = 0xCC;

	/* ELVSS Compensation for Low Temperature & Low Birghtness*/
	if (vdd->candela_level <= 14) {
		if (vdd->temperature > 0)
			idx_temp = HIGH_TEMP;
		else if (vdd->temperature > vdd->elvss_interpolation_temperature)
			idx_temp = MID_TEMP;
		else
			idx_temp = LOW_TEMP;

		elvss_cmd.cmds->msg.tx_buf[3] = elvss_3th_val_array[vdd->candela_level][idx_temp];
		LCD_DEBUG("temperature(%d) level(%d):B5 3th (0x%x)\n",
				vdd->temperature, vdd->candela_level,
				elvss_cmd.cmds->msg.tx_buf[3]);
	}

	/* 0xB5 elvss_23th_val elvss_cal_offset */
	elvss_23th_val = vdd->display_status_dsi.elvss_value1;
	elvss_cmd.cmds->msg.tx_buf[23] = elvss_23th_val;

	return &elvss_cmd;

end:
	LCD_ERR("error");
	return NULL;
}

static struct dsi_panel_cmd_set *ss_vint(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *vint_cmds = ss_get_cmds(vdd, TX_VINT);
	u8 vint;
	char panel_revision = 'A';

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vint_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)vint_cmds);
		return NULL;
	}

	panel_revision += vdd->panel_revision;
	if (panel_revision < 'G') {
		if (vdd->candela_level >= 15)
			vint = vint_array_revA[0];
		else
			vint = vint_array_revA[1];

	} else {
		vint = vint_array_revG[4];
	}

	vint_cmds->cmds->msg.tx_buf[2] = vint;

	/* TODO: implement xtalk_mode
	if (vdd->xtalk_mode)
		vint_cmds->cmds->msg.tx_buf[1] = 0x6B;	// VGH 6.2 V
	else
		vint_cmds->cmds->msg.tx_buf[1] = 0xEB;	// VGH 7.0 V
	 */

	*level_key = LEVEL1_KEY;

	return vint_cmds;
}

static struct dsi_panel_cmd_set irc_set;
static struct dsi_panel_cmd_set *ss_irc(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *set = ss_get_cmds(vdd, TX_IRC_SUBDIVISION);
	int cd_index = 0;
	char panel_revision = 'A';

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(set)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx",
				(size_t)vdd, (size_t)set);
		return NULL;
	}

	if (IS_ERR_OR_NULL(set->cmds)) {
		LCD_ERR("No irc_subdivision_tx_cmds\n");
		return NULL;
	}

	if (!vdd->samsung_support_irc)
		return NULL;

	/* IRC Subdivision works like as AID Subdivision */
	if (vdd->pac)
		cd_index = vdd->pac_cd_idx;
	else
		cd_index = vdd->bl_level;

	LCD_DEBUG("irc idx (%d)\n", cd_index);

	irc_set.cmds = &(set->cmds[cd_index]);
	irc_set.count = 1;

	/* set irc mode to moderato or flat gamma */
	panel_revision += vdd->panel_revision;
	if (panel_revision >= 'G') {
		if (vdd->irc_mode == IRC_MODERATO_MODE)
			irc_set.cmds[0].msg.tx_buf[20] = IRC_MODERATO_MODE_VAL;
		else if (vdd->irc_mode == IRC_FLAT_GAMMA_MODE)
			irc_set.cmds[0].msg.tx_buf[20] = IRC_FLAT_GAMMA_MODE_VAL;
		else
			LCD_ERR("invalid irc mode(%d)\n", vdd->irc_mode);
	}

	*level_key = LEVEL1_KEY;

	return &irc_set;
}

static struct dsi_panel_cmd_set hbm_irc_set;
static struct dsi_panel_cmd_set *ss_hbm_irc(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *set = ss_get_cmds(vdd, TX_HBM_IRC);
	int idx = 0;
	char panel_revision = 'A';

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(set)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)set);
		return NULL;
	}

	if (IS_ERR_OR_NULL(set->cmds)) {
		LCD_ERR("No irc_tx_cmds\n");
		return NULL;
	}

	if (!vdd->samsung_support_irc)
		return NULL;

	/* TODO: use table candela or candela map id instead of auto_brightness */
	idx = vdd->auto_brightness - HBM_MODE;

	/* copy irc default setting */
	hbm_irc_set.cmds = &(set->cmds[idx]);
	hbm_irc_set.count = 1;

	/* set irc mode to moderato or flat gamma */
	panel_revision += vdd->panel_revision;
//	if (panel_revision >= 'G') {
		if (vdd->irc_mode == IRC_MODERATO_MODE)
			hbm_irc_set.cmds[0].msg.tx_buf[20] = IRC_MODERATO_MODE_VAL;
		else if (vdd->irc_mode == IRC_FLAT_GAMMA_MODE)
			hbm_irc_set.cmds[0].msg.tx_buf[20] = IRC_FLAT_GAMMA_MODE_VAL;
		else
			LCD_ERR("invalid irc mode(%d)\n", vdd->irc_mode);
//	}

	*level_key = LEVEL1_KEY;

	return &hbm_irc_set;
}

static struct dsi_panel_cmd_set *ss_gamma(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set  *gamma_cmds = ss_get_cmds(vdd, TX_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(gamma_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)gamma_cmds);
		return NULL;
	}

	LCD_DEBUG("bl_level : %d candela : %dCD\n", vdd->bl_level, vdd->candela_level);

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi->generate_gamma)) {
		LCD_ERR("generate_gamma is NULL error");
		return NULL;
	} else {
		vdd->smart_dimming_dsi->generate_gamma(
			vdd->smart_dimming_dsi,
			vdd->candela_level,
			&gamma_cmds->cmds[0].msg.tx_buf[1]);

		*level_key = LEVEL1_KEY;

		return gamma_cmds;
	}
}

static int samsung_panel_off_pre(struct samsung_display_driver_data *vdd)
{
	int rc = 0;
	return rc;
}

static int samsung_panel_off_post(struct samsung_display_driver_data *vdd)
{
	int rc = 0;
	return rc;
}

// ========================
//			HMT
// ========================
static struct dsi_panel_cmd_set *ss_gamma_hmt(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set  *hmt_gamma_cmds = ss_get_cmds(vdd, TX_HMT_GAMMA);

	if (IS_ERR_OR_NULL(hmt_gamma_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)hmt_gamma_cmds);
		return NULL;
	}

	LCD_DEBUG("hmt_bl_level : %d candela : %dCD\n", vdd->hmt_stat.hmt_bl_level, vdd->hmt_stat.candela_level_hmt);

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi_hmt->generate_gamma)) {
		LCD_ERR("generate_gamma is NULL");
		return NULL;
	} else {
		vdd->smart_dimming_dsi_hmt->generate_gamma(
			vdd->smart_dimming_dsi_hmt,
			vdd->hmt_stat.candela_level_hmt,
			&hmt_gamma_cmds->cmds[0].msg.tx_buf[1]);

		*level_key = LEVEL1_KEY;

		return hmt_gamma_cmds;
	}
}

static struct dsi_panel_cmd_set hmt_aid_cmd;
static struct dsi_panel_cmd_set *ss_aid_hmt(
		struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set  *hmt_aid_cmds = ss_get_cmds(vdd, TX_HMT_AID);
	int cmd_idx = 0;

	if (!vdd->dtsi_data.hmt_reverse_aid_map_table[vdd->panel_revision].size ||
		vdd->hmt_stat.cmd_idx_hmt > vdd->dtsi_data.hmt_reverse_aid_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data.hmt_reverse_aid_map_table[vdd->panel_revision].cmd_idx[vdd->hmt_stat.cmd_idx_hmt];

	hmt_aid_cmd.cmds = &hmt_aid_cmds->cmds[cmd_idx];
	hmt_aid_cmd.count = 1;

	*level_key = LEVEL1_KEY;

	return &hmt_aid_cmd;

end:
	LCD_ERR("error");
	return NULL;
}

static struct dsi_panel_cmd_set *ss_elvss_hmt(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *elvss_cmds = ss_get_cmds(vdd, TX_ELVSS);
	int cd_index = 0;
	int cmd_idx = 0;
	u8 elvss_23th_val;
	char panel_revision = 'A';
	int idx_temp = 0;
	u8 elvss_3th_val_array[SUPPORT_LOWTEMP_ELVSS][MAX_TEMP] = {
		/* high mid low*/
		{0x13, 0x1c, 0x23},	/* 0 nit */
		{0x13, 0x1c, 0x23},	/* 1 nit */
		{0x13, 0x1c, 0x23},	/* 2 nit */
		{0x13, 0x1c, 0x23},	/* 3 nit */
		{0x14, 0x1c, 0x23},	/* 4 nit */
		{0x14, 0x1c, 0x23},	/* 5 nit */
		{0x15, 0x1c, 0x23},	/* 6 nit */
		{0x16, 0x1c, 0x23},	/* 7 nit */
		{0x17, 0x1c, 0x23},	/* 8 nit */
		{0x18, 0x20, 0x23},	/* 9 nit */
		{0x19, 0x20, 0x23},	/* 10 nit */
		{0x1b, 0x20, 0x23},	/* 11 nit */
		{0x1d, 0x20, 0x23},	/* 12 nit */
		{0x1f, 0x20, 0x23},	/* 13 nit */
		{0x21, 0x20, 0x23},	/* 14 nit */
	};

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(elvss_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)elvss_cmds);
		return NULL;
	}

	panel_revision += vdd->panel_revision;

	cd_index  = vdd->cd_idx;
	LCD_DEBUG("cd_index (%d)\n", cd_index);

	if (!vdd->dtsi_data.smart_acl_elvss_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data.smart_acl_elvss_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data.smart_acl_elvss_map_table[vdd->panel_revision].cmd_idx[cd_index];

	elvss_cmd.cmds = &(elvss_cmds->cmds[cmd_idx]);
	elvss_cmd.count = 1;
	*level_key = LEVEL1_KEY;

	/* 0xB5 1th TSET */
	elvss_cmd.cmds->msg.tx_buf[1] = vdd->temperature > 0 ?
			vdd->temperature : BIT(7) | (-1*vdd->temperature);

	/* ELVSS(MPS_CON) setting condition is equal to normal birghtness */ // B5 2nd para : MPS_CON
	if (vdd->hmt_stat.candela_level_hmt > 39)
		elvss_cmd.cmds->msg.tx_buf[2] = 0xDC;
	else
		elvss_cmd.cmds->msg.tx_buf[2] = 0xCC;

	/* ELVSS Compensation for Low Temperature & Low Birghtness*/
	if (vdd->hmt_stat.candela_level_hmt <= 14) {
		if (vdd->temperature > 0)
			idx_temp = HIGH_TEMP;
		else if (vdd->temperature > vdd->elvss_interpolation_temperature)
			idx_temp = MID_TEMP;
		else
			idx_temp = LOW_TEMP;

		elvss_cmd.cmds->msg.tx_buf[3] = elvss_3th_val_array[vdd->hmt_stat.candela_level_hmt][idx_temp];
		LCD_DEBUG("temperature(%d) level(%d):B5 3th (0x%x)\n",
				vdd->temperature, vdd->hmt_stat.candela_level_hmt,
				elvss_cmd.cmds->msg.tx_buf[3]);
	}

	/* 0xB5 elvss_23th_val elvss_cal_offset */
	elvss_23th_val = vdd->display_status_dsi.elvss_value1;
	elvss_cmd.cmds->msg.tx_buf[23] = elvss_23th_val;

	return &elvss_cmd;

end:
	LCD_ERR("error");
	return NULL;
}

static void ss_make_sdimconf_hmt(struct samsung_display_driver_data *vdd)
{
	/* Set the mtp read buffer pointer and read the NVM value*/
	ss_panel_data_read(vdd, RX_SMART_DIM_MTP,
			vdd->smart_dimming_dsi_hmt->mtp_buffer, LEVEL1_KEY);

	/* Initialize smart dimming related things here */
	/* lux_tab setting for 350cd */
	vdd->smart_dimming_dsi_hmt->lux_tab = vdd->dtsi_data.hmt_candela_map_table[vdd->panel_revision].cd;
	vdd->smart_dimming_dsi_hmt->lux_tabsize = vdd->dtsi_data.hmt_candela_map_table[vdd->panel_revision].tab_size;
	vdd->smart_dimming_dsi_hmt->man_id = vdd->manufacture_id_dsi;
	if (vdd->panel_func.samsung_panel_revision)
			vdd->smart_dimming_dsi_hmt->panel_revision = vdd->panel_func.samsung_panel_revision(vdd);

	/* Just a safety check to ensure smart dimming data is initialised well */
	vdd->smart_dimming_dsi_hmt->init(vdd->smart_dimming_dsi_hmt);

	LCD_INFO("[HMT] smart dimming done!\n");
}

static int ss_samart_dimming_init_hmt(struct samsung_display_driver_data *vdd)
{
	LCD_INFO("DSI%d : ++\n", vdd->ndx);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	vdd->smart_dimming_dsi_hmt = vdd->panel_func.samsung_smart_get_conf_hmt();

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi_hmt)) {
		LCD_ERR("DSI%d error", vdd->ndx);
		return false;
	} else {
		vdd->hmt_stat.hmt_on = 0;
		vdd->hmt_stat.hmt_bl_level = 0;
		vdd->hmt_stat.hmt_reverse = 0;
		vdd->hmt_stat.hmt_is_first = 1;

		ss_make_sdimconf_hmt(vdd);

		vdd->smart_dimming_hmt_loaded_dsi = true;
	}

	LCD_INFO("DSI%d : --\n", vdd->ndx);

	return true;
}

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *alpm_brightness[LPM_BRIGHTNESS_MAX_IDX] = {NULL, };
	struct dsi_panel_cmd_set *alpm_ctrl[MAX_LPM_CTRL] = {NULL, };
	struct dsi_panel_cmd_set *cmd_list[2];

	/*default*/
	int mode = ALPM_MODE_ON;
	int ctrl_index = ALPM_CTRL_2NIT;
	int bl_index = LPM_2NIT_IDX;

	/*
	 * Init reg_list and cmd list
	 * reg_list[X][0] is reg value
	 * reg_list[X][1] is offset for reg value
	 * cmd_list is the target cmds for searching reg value
	 */
	static int reg_list[2][2] = {
		{ALPM_REG, -EINVAL},
		{ALPM_CTRL_REG, -EINVAL} };

	LCD_DEBUG("%s++\n", __func__);

	cmd_list[0] = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	cmd_list[1] = ss_get_cmds(vdd, TX_LPM_BL_CMD);

	/* Init alpm_brightness and alpm_ctrl cmds */
	alpm_brightness[LPM_2NIT_IDX] = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
	alpm_brightness[LPM_10NIT_IDX] = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
	alpm_brightness[LPM_30NIT_IDX] = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
	alpm_brightness[LPM_60NIT_IDX] = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);

	alpm_ctrl[ALPM_CTRL_2NIT] = ss_get_cmds(vdd, TX_ALPM_2NIT_CMD);
	alpm_ctrl[ALPM_CTRL_10NIT] = ss_get_cmds(vdd, TX_ALPM_10NIT_CMD);
	alpm_ctrl[ALPM_CTRL_30NIT] = ss_get_cmds(vdd, TX_ALPM_30NIT_CMD);
	alpm_ctrl[ALPM_CTRL_60NIT] = ss_get_cmds(vdd, TX_ALPM_60NIT_CMD);
	alpm_ctrl[HLPM_CTRL_2NIT] = ss_get_cmds(vdd, TX_HLPM_2NIT_CMD);
	alpm_ctrl[HLPM_CTRL_10NIT] = ss_get_cmds(vdd, TX_HLPM_10NIT_CMD);
	alpm_ctrl[HLPM_CTRL_30NIT] = ss_get_cmds(vdd, TX_HLPM_30NIT_CMD);
	alpm_ctrl[HLPM_CTRL_60NIT] = ss_get_cmds(vdd, TX_HLPM_60NIT_CMD);

	mode = vdd->panel_lpm.mode;

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_10NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_10NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_10NIT : ALPM_CTRL_10NIT;
		bl_index = LPM_10NIT_IDX;
		break;
	case LPM_30NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_30NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_30NIT : ALPM_CTRL_30NIT;
		bl_index = LPM_30NIT_IDX;
		break;
	case LPM_60NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_60NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_60NIT : ALPM_CTRL_60NIT;
		bl_index = LPM_60NIT_IDX;
		break;
	case LPM_2NIT:
	default:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_2NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_2NIT : ALPM_CTRL_2NIT;
		bl_index = LPM_2NIT_IDX;
		break;
	}

	LCD_DEBUG("[Panel LPM]bl_index %d, ctrl_index %d, mode %d\n",
			 bl_index, ctrl_index, mode);

	/*
	 * Find offset for alpm_reg and alpm_ctrl_reg
	 * alpm_reg  : Control register for ALPM/HLPM on/off
	 * alpm_ctrl_reg : Control register for changing ALPM/HLPM mode
	 */
	if (unlikely((reg_list[0][1] == -EINVAL) ||
				(reg_list[1][1] == -EINVAL)))
		ss_find_reg_offset(reg_list, cmd_list, ARRAY_SIZE(cmd_list));

	if (reg_list[0][1] != -EINVAL) {
		/* Update parameter for ALPM_REG */
		memcpy(cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf,
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[0]->cmds[reg_list[0][1]].msg.tx_len);

		LCD_DEBUG("[Panel LPM] change brightness cmd : %x, %x\n",
				cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf[1],
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf[1]);
	}

	if (reg_list[1][1] != -EINVAL) {
		/* Initialize ALPM/HLPM cmds */
		/* Update parameter for ALPM_CTRL_REG */
		memcpy(cmd_list[1]->cmds[reg_list[1][1]].msg.tx_buf,
				alpm_ctrl[ctrl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[1]->cmds[reg_list[1][1]].msg.tx_len);

		LCD_DEBUG("[Panel LPM] update alpm ctrl reg\n");
	}

	//send lpm bl cmd
	ss_send_cmd(vdd, TX_LPM_BL_CMD);

	LCD_INFO("[Panel LPM] bl_level : %s\n",
				/* Check current brightness level */
				vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN");

	LCD_DEBUG("%s--\n", __func__);
}

/*
 * This function will update parameters for ALPM_REG/ALPM_CTRL_REG
 * Parameter for ALPM_REG : Control brightness for panel LPM
 * Parameter for ALPM_CTRL_REG : Change panel LPM mode for ALPM/HLPM
 * mode, brightness, hz are updated here.
 */
static void ss_update_panel_lpm_ctrl_cmd(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *alpm_brightness[LPM_BRIGHTNESS_MAX_IDX] = {NULL, };
	struct dsi_panel_cmd_set *alpm_ctrl[MAX_LPM_CTRL] = {NULL, };
	struct dsi_panel_cmd_set *alpm_off_ctrl[MAX_LPM_MODE] = {NULL, };
	struct dsi_panel_cmd_set *cmd_list[2];
	struct dsi_panel_cmd_set *off_cmd_list[1];

	/*default*/
	int mode = ALPM_MODE_ON;
	int ctrl_index = ALPM_CTRL_2NIT;
	int bl_index = LPM_2NIT_IDX;

	/*
	 * Init reg_list and cmd list
	 * reg_list[X][0] is reg value
	 * reg_list[X][1] is offset for reg value
	 * cmd_list is the target cmds for searching reg value
	 */
	static int reg_list[2][2] = {
		{ALPM_REG, -EINVAL},
		{ALPM_CTRL_REG, -EINVAL} };

	static int off_reg_list[1][2] = {
		{ALPM_CTRL_REG, -EINVAL} };

	LCD_ERR("%s++\n", __func__);

	cmd_list[0] = ss_get_cmds(vdd, TX_LPM_ON);
	cmd_list[1] = ss_get_cmds(vdd, TX_LPM_ON);
	off_cmd_list[0] = ss_get_cmds(vdd, TX_LPM_OFF);

	/* Init alpm_brightness and alpm_ctrl cmds */
	alpm_brightness[LPM_2NIT_IDX] = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
	alpm_brightness[LPM_10NIT_IDX] = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
	alpm_brightness[LPM_30NIT_IDX] = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
	alpm_brightness[LPM_60NIT_IDX] = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);

	alpm_ctrl[ALPM_CTRL_2NIT] = ss_get_cmds(vdd, TX_ALPM_2NIT_CMD);
	alpm_ctrl[ALPM_CTRL_10NIT] = ss_get_cmds(vdd, TX_ALPM_10NIT_CMD);
	alpm_ctrl[ALPM_CTRL_30NIT] = ss_get_cmds(vdd, TX_ALPM_30NIT_CMD);
	alpm_ctrl[ALPM_CTRL_60NIT] = ss_get_cmds(vdd, TX_ALPM_60NIT_CMD);
	alpm_ctrl[HLPM_CTRL_2NIT] = ss_get_cmds(vdd, TX_HLPM_2NIT_CMD);
	alpm_ctrl[HLPM_CTRL_10NIT] = ss_get_cmds(vdd, TX_HLPM_10NIT_CMD);
	alpm_ctrl[HLPM_CTRL_30NIT] = ss_get_cmds(vdd, TX_HLPM_30NIT_CMD);
	alpm_ctrl[HLPM_CTRL_60NIT] = ss_get_cmds(vdd, TX_HLPM_60NIT_CMD);
	alpm_off_ctrl[ALPM_MODE_ON] = ss_get_cmds(vdd, TX_ALPM_OFF);
	alpm_off_ctrl[HLPM_MODE_ON] = ss_get_cmds(vdd, TX_HLPM_OFF);

	mode = vdd->panel_lpm.mode;

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_10NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_10NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_10NIT : ALPM_CTRL_10NIT;
		bl_index = LPM_10NIT_IDX;
		break;
	case LPM_30NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_30NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_30NIT : ALPM_CTRL_30NIT;
		bl_index = LPM_30NIT_IDX;
		break;
	case LPM_60NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_60NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_60NIT : ALPM_CTRL_60NIT;
		bl_index = LPM_60NIT_IDX;
		break;
	case LPM_2NIT:
	default:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_2NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_2NIT : ALPM_CTRL_2NIT;
		bl_index = LPM_2NIT_IDX;
		break;
	}

	LCD_DEBUG("[Panel LPM] change brightness cmd :%d, %d, %d\n",
			 bl_index, ctrl_index, mode);

	/*
	 * Find offset for alpm_reg and alpm_ctrl_reg
	 * alpm_reg	 : Control register for ALPM/HLPM on/off
	 * alpm_ctrl_reg : Control register for changing ALPM/HLPM mode
	 */
	if (unlikely((reg_list[0][1] == -EINVAL) ||
				(reg_list[1][1] == -EINVAL)))
		ss_find_reg_offset(reg_list, cmd_list, ARRAY_SIZE(cmd_list));

	if (unlikely(off_reg_list[0][1] == -EINVAL))
		ss_find_reg_offset(off_reg_list, off_cmd_list,
						ARRAY_SIZE(off_cmd_list));

	if (reg_list[0][1] != -EINVAL) {
		/* Update parameter for ALPM_REG */
		memcpy(cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf,
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[0]->cmds[reg_list[0][1]].msg.tx_len);

		LCD_DEBUG("[Panel LPM] change brightness cmd : %x, %x\n",
				cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf[1],
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf[1]);
	}

	if (reg_list[1][1] != -EINVAL) {
		/* Initialize ALPM/HLPM cmds */
		/* Update parameter for ALPM_CTRL_REG */
		memcpy(cmd_list[1]->cmds[reg_list[1][1]].msg.tx_buf,
				alpm_ctrl[ctrl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[1]->cmds[reg_list[1][1]].msg.tx_len);

		LCD_DEBUG("[Panel LPM] update alpm ctrl reg\n");
	}

	if ((off_reg_list[0][1] != -EINVAL) &&\
			(mode != LPM_MODE_OFF)) {
		/* Update parameter for ALPM_CTRL_REG */
		memcpy(off_cmd_list[0]->cmds[off_reg_list[0][1]].msg.tx_buf,
				alpm_off_ctrl[mode]->cmds[0].msg.tx_buf,
				sizeof(char) * off_cmd_list[0]->cmds[off_reg_list[0][1]].msg.tx_len);
	}

	LCD_ERR("%s--\n", __func__);
}

static int ddi_hw_cursor(struct samsung_display_driver_data *vdd, int *input)
{
	struct dsi_panel_cmd_set *pcmds;
	u8 *payload;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return 0;
	}

	if (IS_ERR_OR_NULL(input)) {
		LCD_ERR("input is NULL\n");
		return 0;
	}

	pcmds = ss_get_cmds(vdd, TX_HW_CURSOR);
	payload = pcmds->cmds[0].msg.tx_buf;
	if (IS_ERR_OR_NULL(payload)) {
		LCD_ERR("hw_cursor_tx_cmds is NULL\n");
		return 0;
	}

	LCD_INFO("On/Off:(%d), Por/Land:(%d), On_Select:(%d), X:(%d), Y:(%d), Width:(%d), Length:(%d), Color:(0x%x), Period:(%x), TR_TIME(%x)\n",
		input[0], input[1], input[2], input[3], input[4], input[5],
		input[6], input[7], input[8], input[9]);

	/* Cursor On&Off (0:Off, 1:On) */
	payload[1] = input[0] & 0x1;

	/* 3rd bit : CURSOR_ON_SEL, 2nd bit : Port/Land, 1st bit : BLINK_ON(Default On)*/
	payload[2] = (input[2] & 0x1) << 2 | (input[1] & 0x1) << 1 | 0x1;

	/* Start X address */
	payload[3] = (input[3] & 0x700) >> 8;
	payload[4] = input[3] & 0xFF;

	/* Start Y address */
	payload[5] = (input[4] & 0x700) >> 8;
	payload[6] = input[4] & 0xFF;

	/* Width */
	payload[7] = input[5] & 0xF;

	/* Length */
	payload[8] = (input[6] & 0x100) >> 8;
	payload[9] = input[6] & 0xFF;

	/* Color */
	payload[10] = (input[7] & 0xFF0000) >> 16;
	payload[11] = (input[7] & 0xFF00) >> 8;
	payload[12] = input[7] & 0xFF;

	/* Period */
	payload[13] = input[8] & 0xFF;

	/* TR_TIME */
	payload[14] = input[9] & 0xFF;

	ss_send_cmd(vdd, TX_LEVEL1_KEY_ENABLE);
	ss_send_cmd(vdd, TX_HW_CURSOR);
	ss_send_cmd(vdd, TX_LEVEL1_KEY_DISABLE);

	return 1;
}

static void ss_send_colorweakness_ccb_cmd(struct samsung_display_driver_data *vdd, int mode)
{
	struct dsi_panel_cmd_set *pcmds;

	LCD_INFO("mode (%d) color_weakness_value (%x) \n", mode, vdd->color_weakness_value);

	if (mode) {
		pcmds = ss_get_cmds(vdd, TX_COLOR_WEAKNESS_ENABLE);
		pcmds->cmds[1].msg.tx_buf[1] = vdd->color_weakness_value;
		ss_send_cmd(vdd, TX_COLOR_WEAKNESS_ENABLE);
	} else {
		ss_send_cmd(vdd, TX_COLOR_WEAKNESS_DISABLE);
	}
}

static int dsi_update_mdnie_data(struct samsung_display_driver_data *vdd)
{
	struct mdnie_lite_tune_data *mdnie_data;

	mdnie_data = kzalloc(sizeof(struct mdnie_lite_tune_data), GFP_KERNEL);
	if (!mdnie_data) {
		LCD_ERR("fail to allocate mdnie_data memory\n");
		return -ENOMEM;
	}

	/* Update mdnie command */
	mdnie_data->DSI_COLOR_BLIND_MDNIE_1 = DSI_COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_1 = DSI_RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_2 = DSI_RGB_SENSOR_MDNIE_2;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_3 = DSI_RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_TRANS_DIMMING_MDNIE = DSI_RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE_2 = DSI_UI_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_UI_STANDARD_MDNIE_2 = DSI_UI_STANDARD_MDNIE_2;
	mdnie_data->DSI_UI_AUTO_MDNIE_2 = DSI_UI_AUTO_MDNIE_2;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE_2 = DSI_VIDEO_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE_2 = DSI_VIDEO_STANDARD_MDNIE_2;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE_2 = DSI_VIDEO_AUTO_MDNIE_2;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE_2 = DSI_CAMERA_AUTO_MDNIE_2;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE_2 = DSI_GALLERY_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE_2 = DSI_GALLERY_STANDARD_MDNIE_2;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE_2 = DSI_GALLERY_AUTO_MDNIE_2;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE_2 = DSI_BROWSER_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE_2 = DSI_BROWSER_STANDARD_MDNIE_2;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE_2 = DSI_BROWSER_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE_2 = DSI_EBOOK_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE_2 = DSI_EBOOK_STANDARD_MDNIE_2;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = DSI_EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE_2 = DSI_TDMB_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE_2 = DSI_TDMB_STANDARD_MDNIE_2;
	mdnie_data->DSI_TDMB_AUTO_MDNIE_2 = DSI_TDMB_AUTO_MDNIE_2;

	mdnie_data->DSI_BYPASS_MDNIE = DSI_BYPASS_MDNIE;
	mdnie_data->DSI_NEGATIVE_MDNIE = DSI_NEGATIVE_MDNIE;
	mdnie_data->DSI_COLOR_BLIND_MDNIE = DSI_COLOR_BLIND_MDNIE;
	mdnie_data->DSI_HBM_CE_MDNIE = DSI_HBM_CE_MDNIE;
	mdnie_data->DSI_RGB_SENSOR_MDNIE = DSI_RGB_SENSOR_MDNIE;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE = DSI_UI_DYNAMIC_MDNIE;
	mdnie_data->DSI_UI_STANDARD_MDNIE = DSI_UI_STANDARD_MDNIE;
	mdnie_data->DSI_UI_NATURAL_MDNIE = DSI_UI_NATURAL_MDNIE;
	mdnie_data->DSI_UI_AUTO_MDNIE = DSI_UI_AUTO_MDNIE;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE = DSI_VIDEO_DYNAMIC_MDNIE;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE = DSI_VIDEO_STANDARD_MDNIE;
	mdnie_data->DSI_VIDEO_NATURAL_MDNIE = DSI_VIDEO_NATURAL_MDNIE;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE = DSI_VIDEO_AUTO_MDNIE;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE = DSI_CAMERA_AUTO_MDNIE;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE = DSI_GALLERY_DYNAMIC_MDNIE;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE = DSI_GALLERY_STANDARD_MDNIE;
	mdnie_data->DSI_GALLERY_NATURAL_MDNIE = DSI_GALLERY_NATURAL_MDNIE;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE = DSI_GALLERY_AUTO_MDNIE;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE = DSI_BROWSER_DYNAMIC_MDNIE;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE = DSI_BROWSER_STANDARD_MDNIE;
	mdnie_data->DSI_BROWSER_NATURAL_MDNIE = DSI_BROWSER_NATURAL_MDNIE;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE = DSI_BROWSER_AUTO_MDNIE;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE = DSI_EBOOK_DYNAMIC_MDNIE;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE = DSI_EBOOK_STANDARD_MDNIE;
	mdnie_data->DSI_EBOOK_NATURAL_MDNIE = DSI_EBOOK_NATURAL_MDNIE;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE = DSI_EBOOK_AUTO_MDNIE;
	mdnie_data->DSI_EMAIL_AUTO_MDNIE = DSI_EMAIL_AUTO_MDNIE;
	mdnie_data->DSI_GAME_LOW_MDNIE = DSI_GAME_LOW_MDNIE;
	mdnie_data->DSI_GAME_MID_MDNIE = DSI_GAME_MID_MDNIE;
	mdnie_data->DSI_GAME_HIGH_MDNIE = DSI_GAME_HIGH_MDNIE;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE = DSI_TDMB_DYNAMIC_MDNIE;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE = DSI_TDMB_STANDARD_MDNIE;
	mdnie_data->DSI_TDMB_NATURAL_MDNIE = DSI_TDMB_NATURAL_MDNIE;
	mdnie_data->DSI_TDMB_AUTO_MDNIE = DSI_TDMB_AUTO_MDNIE;
	mdnie_data->DSI_GRAYSCALE_MDNIE = DSI_GRAYSCALE_MDNIE;
	mdnie_data->DSI_GRAYSCALE_NEGATIVE_MDNIE = DSI_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data->DSI_CURTAIN = DSI_SCREEN_CURTAIN_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE = DSI_NIGHT_MODE_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR = DSI_NIGHT_MODE_MDNIE_1;
	mdnie_data->DSI_COLOR_LENS_MDNIE = DSI_COLOR_LENS_MDNIE;
	mdnie_data->DSI_COLOR_LENS_MDNIE_SCR = DSI_COLOR_LENS_MDNIE_1;
	mdnie_data->DSI_COLOR_BLIND_MDNIE_SCR = DSI_COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_SCR = DSI_RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_AFC = DSI_AFC;
	mdnie_data->DSI_AFC_ON = DSI_AFC_ON;
	mdnie_data->DSI_AFC_OFF = DSI_AFC_OFF;

	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi;
	mdnie_data->hmt_color_temperature_tune_value_dsi = hmt_color_temperature_tune_value_dsi;

	mdnie_data->hdr_tune_value_dsi = hdr_tune_value_dsi;

	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value_dsi;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data->dsi_bypass_mdnie_size = ARRAY_SIZE(DSI_BYPASS_MDNIE);
	mdnie_data->mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data->mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP3] = MDNIE_STEP3_INDEX;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
	mdnie_data->dsi_rgb_sensor_mdnie_1_size = DSI_RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_2_size = DSI_RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_3_size = DSI_RGB_SENSOR_MDNIE_3_SIZE;

	mdnie_data->dsi_trans_dimming_data_index = MDNIE_TRANS_DIMMING_DATA_INDEX;

	mdnie_data->dsi_adjust_ldu_table = adjust_ldu_data;
	mdnie_data->dsi_max_adjust_ldu = 6;
	mdnie_data->dsi_night_mode_table = night_mode_data;
	mdnie_data->dsi_max_night_mode_index = 11;
	mdnie_data->dsi_color_lens_table = color_lens_data;
	mdnie_data->dsi_white_default_r = 0xff;
	mdnie_data->dsi_white_default_g = 0xff;
	mdnie_data->dsi_white_default_b = 0xff;
	mdnie_data->dsi_white_balanced_r = 0;
	mdnie_data->dsi_white_balanced_g = 0;
	mdnie_data->dsi_white_balanced_b = 0;
	mdnie_data->dsi_scr_step_index = MDNIE_STEP1_INDEX;
	mdnie_data->dsi_afc_size = 45;
	mdnie_data->dsi_afc_index = 33;

	vdd->mdnie_data = mdnie_data;

	return 0;
}

static int ss_gct_read(struct samsung_display_driver_data *vdd)
{
	u8 valid_checksum[4] = {0x66, 0x66, 0x66, 0x66};
	int res;

	if (!vdd->gct.is_support)
		return GCT_RES_CHECKSUM_NOT_SUPPORT;

	if (!vdd->gct.on)
		return GCT_RES_CHECKSUM_OFF;


	if (!memcmp(vdd->gct.checksum, valid_checksum, 4))
		res = GCT_RES_CHECKSUM_PASS;
	else
		res = GCT_RES_CHECKSUM_NG;

	return res;
}

static int ss_gct_write(struct samsung_display_driver_data *vdd)
{
	u8 *checksum;
	int i;
	/* vddm set, 0x0: 1.0V, 0x0D: 0.92V, 0x10: 0.9V, 0x2B: 1.08V, 0x30: 1.1V */
	u8 vddm_set[MAX_VDDM] = {0x0, 0x0D, 0x2B};
	int ret = 0;
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);

	LCD_INFO("+\n");

	/* prevent sw reset to trigger esd recovery */
	LCD_INFO("disable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd);

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	for (i = TX_GCT_ENTER; i <= TX_GCT_EXIT; i++)
		ss_set_exclusive_tx_packet(vdd, i, 1);
	ss_set_exclusive_tx_packet(vdd, RX_GCT_CHECKSUM, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);

	mdelay(10);

	checksum = vdd->gct.checksum;
	for (i = VDDM_LV; i < MAX_VDDM; i++) {
		struct dsi_panel_cmd_set *set;

		LCD_INFO("(%d) TX_GCT_ENTER\n", i);
		set = ss_get_cmds(vdd, TX_GCT_ENTER);
		set->cmds[11].msg.tx_buf[1] = vddm_set[i];
		ss_send_cmd(vdd, TX_GCT_ENTER);

		mdelay(150);

		ss_panel_data_read(vdd, RX_GCT_CHECKSUM, checksum++,
				LEVEL_KEY_NONE);
		LCD_INFO("(%d) read checksum: %x\n", i, *(checksum - 1));

		LCD_INFO("(%d) TX_GCT_MID\n", i);
		ss_send_cmd(vdd, TX_GCT_MID);

		mdelay(150);

		ss_panel_data_read(vdd, RX_GCT_CHECKSUM, checksum++,
				LEVEL_KEY_NONE);

		LCD_INFO("(%d) read checksum: %x\n", i, *(checksum - 1));

		LCD_INFO("(%d) TX_GCT_EXIT\n", i);
		ss_send_cmd(vdd, TX_GCT_EXIT);
	}

	vdd->gct.on = 1;

	LCD_INFO("checksum = {%x %x %x %x}\n",
			vdd->gct.checksum[0], vdd->gct.checksum[1],
			vdd->gct.checksum[2], vdd->gct.checksum[3]);

	/* exit exclusive mode*/
	for (i = TX_GCT_ENTER; i <= TX_GCT_EXIT; i++)
		ss_set_exclusive_tx_packet(vdd, i, 0);
	ss_set_exclusive_tx_packet(vdd, RX_GCT_CHECKSUM, 0);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 0);

	/* Reset panel to enter nornal panel mode.
	 * The on commands also should be sent before update new frame.
	 * Next new frame update is blocked by exclusive_tx.enable
	 * in ss_event_frame_update_pre(), and it will be released
	 * by wake_up exclusive_tx.ex_tx_waitq.
	 * So, on commands should be sent before wake up the waitq
	 * and set exclusive_tx.enable to false.
	 */
	dsi_panel_power_off(panel);
	dsi_panel_power_on(panel);

	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_PPS, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 1);

	ss_send_cmd(vdd, DSI_CMD_SET_ON);
	dsi_panel_update_pps_nolock(panel);

	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_PPS, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 0);

	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	ss_panel_on_post(vdd);

	/* enable esd interrupt */
	LCD_INFO("enable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd);

	return ret;
}

static int ss_self_display_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO("Self Display Panel Data init\n");

	/* SELF DISPLAY */
	vdd->self_disp.operation[FLAG_SELF_MASK].img_buf = self_mask_img_data;
	vdd->self_disp.operation[FLAG_SELF_MASK].img_size = ARRAY_SIZE(self_mask_img_data);
	make_self_dispaly_img_cmds(TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);
	vdd->self_disp.operation[FLAG_SELF_MASK].img_checksum = SELF_MASK_IMG_CHECKSUM;

	vdd->self_disp.operation[FLAG_SELF_ICON].img_buf = self_icon_img_data;
	vdd->self_disp.operation[FLAG_SELF_ICON].img_size = ARRAY_SIZE(self_icon_img_data);
	make_self_dispaly_img_cmds(TX_SELF_ICON_IMAGE, FLAG_SELF_ICON);

	vdd->self_disp.operation[FLAG_SELF_ACLK].img_buf = self_aclock_img_data;
	vdd->self_disp.operation[FLAG_SELF_ACLK].img_size = ARRAY_SIZE(self_aclock_img_data);
	make_self_dispaly_img_cmds(TX_SELF_ACLOCK_IMAGE, FLAG_SELF_ACLK);

	vdd->self_disp.operation[FLAG_SELF_DCLK].img_buf = self_dclock_img_data;
	vdd->self_disp.operation[FLAG_SELF_DCLK].img_size = ARRAY_SIZE(self_dclock_img_data);
	make_self_dispaly_img_cmds(TX_SELF_DCLOCK_IMAGE, FLAG_SELF_DCLK);

	vdd->self_disp.operation[FLAG_SELF_VIDEO].img_buf = self_video_img_data;
	vdd->self_disp.operation[FLAG_SELF_VIDEO].img_size = ARRAY_SIZE(self_video_img_data);
	make_self_dispaly_img_cmds(TX_SELF_VIDEO_IMAGE, FLAG_SELF_VIDEO);

	return 1;
}

static void samsung_panel_init(struct samsung_display_driver_data *vdd)
{
	LCD_ERR("%s\n", ss_get_panel_name(vdd));

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;
	vdd->panel_func.samsung_panel_off_pre = samsung_panel_off_pre;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_manufacture_date_read = ss_manufacture_date_read;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;

	vdd->panel_func.samsung_elvss_read = ss_elvss_read;
	vdd->panel_func.samsung_irc_read = ss_irc_read;
	vdd->panel_func.samsung_hbm_read = ss_hbm_read;
	vdd->panel_func.samsung_mdnie_read = ss_mdnie_read;
	vdd->panel_func.samsung_smart_dimming_init = ss_samart_dimming_init;
	vdd->panel_func.samsung_smart_get_conf = smart_get_conf_S6E3HA8_AMB622NP01;

	vdd->panel_func.samsung_cell_id_read = ss_cell_id_read;
	vdd->panel_func.samsung_octa_id_read = ss_octa_id_read;

	/* Brightness */
	vdd->panel_func.samsung_brightness_hbm_off = NULL;
	vdd->panel_func.samsung_brightness_aid = ss_aid;
	vdd->panel_func.samsung_brightness_acl_on = ss_acl_on;
	vdd->panel_func.samsung_brightness_acl_percent = NULL;
	vdd->panel_func.samsung_brightness_acl_off = ss_acl_off;
	vdd->panel_func.samsung_brightness_elvss = ss_elvss;
	vdd->panel_func.samsung_brightness_elvss_temperature1 = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature2 = NULL;
	vdd->panel_func.samsung_brightness_vint = ss_vint;
	vdd->panel_func.samsung_brightness_irc = ss_irc;
	vdd->panel_func.samsung_brightness_gamma = ss_gamma;

	/* HBM */
	vdd->panel_func.samsung_hbm_gamma = ss_hbm_gamma;
	vdd->panel_func.samsung_hbm_etc = ss_hbm_etc;
	vdd->panel_func.samsung_hbm_irc = ss_hbm_irc;
	vdd->panel_func.get_hbm_candela_value = get_hbm_candela_value;

	/* Event */
	vdd->panel_func.samsung_change_ldi_fps = NULL;

	/* HMT */
	vdd->panel_func.samsung_brightness_gamma_hmt = ss_gamma_hmt;
	vdd->panel_func.samsung_brightness_aid_hmt = ss_aid_hmt;
	vdd->panel_func.samsung_brightness_elvss_hmt = ss_elvss_hmt;
	vdd->panel_func.samsung_brightness_vint_hmt = NULL;
	vdd->panel_func.samsung_smart_dimming_hmt_init = ss_samart_dimming_init_hmt;
	vdd->panel_func.samsung_smart_get_conf_hmt = smart_get_conf_S6E3HA8_AMB622NP01_hmt;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = ss_update_panel_lpm_ctrl_cmd;
	vdd->panel_func.samsung_set_lpm_brightness = ss_set_panel_lpm_brightness;

	/* default brightness */
	vdd->bl_level = 25500;

	/* mdnie */
	vdd->support_mdnie_lite = true;

	vdd->support_mdnie_trans_dimming = true;
	vdd->mdnie_tune_size1 = sizeof(DSI_BYPASS_MDNIE_1);
	vdd->mdnie_tune_size2 = sizeof(DSI_BYPASS_MDNIE_2);
	vdd->mdnie_tune_size3 = sizeof(DSI_BYPASS_MDNIE_3);

	dsi_update_mdnie_data(vdd);

	/* send recovery pck before sending image date (for ESD recovery) */
	vdd->send_esd_recovery = false;

	/* Enable panic on first pingpong timeout */
//	vdd->debug_data->panic_on_pptimeout = true;

	/* Set IRC init value */
	vdd->irc_mode = IRC_MODERATO_MODE;

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off =  ss_send_colorweakness_ccb_cmd;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = ddi_hw_cursor;

	/* COVER Open/Close */
	vdd->panel_func.samsung_cover_control = NULL;

	/* COPR */
	vdd->copr.read_addr = COPR_READ_ADDR;
	vdd->copr.read_size = COPR_READ_SIZE;
	ss_copr_init();

	/* ACL default ON */
	vdd->acl_status = 1;

	/* ACL default status in acl on */
	vdd->gradual_acl_val = 1;

	/* Gram Checksum Test */
	vdd->panel_func.samsung_gct_write = ss_gct_write;
	vdd->panel_func.samsung_gct_read = ss_gct_read;

	/* Self display */
	vdd->self_disp.is_support = true;
	vdd->panel_func.samsung_self_display_init = ss_self_display_init;
}

static int __init samsung_panel_initialize(void)
{
	struct samsung_display_driver_data *vdd;
	enum ss_display_ndx ndx;
	char panel_string[] = "ss_dsi_panel_S6E3HA8_AMB622NP01_WQHD";
	char panel_name[MAX_CMDLINE_PARAM_LEN];
	char panel_secondary_name[MAX_CMDLINE_PARAM_LEN];

	ss_get_primary_panel_name_cmdline(panel_name);
	ss_get_secondary_panel_name_cmdline(panel_secondary_name);

	/* TODO: use component_bind with panel_func
	 * and match by panel_string, instead.
	 */
	if (!strncmp(panel_string, panel_name, strlen(panel_string)))
		ndx = PRIMARY_DISPLAY_NDX;
	else if (!strncmp(panel_string, panel_secondary_name,
				strlen(panel_string)))
		ndx = SECONDARY_DISPLAY_NDX;
	else
		return 0;

	vdd = &vdd_data[ndx];
	vdd->panel_func.samsung_panel_init = samsung_panel_init;

	return 0;
}

early_initcall(samsung_panel_initialize);
