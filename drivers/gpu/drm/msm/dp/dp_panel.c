/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include "dp_panel.h"
#ifdef CONFIG_SEC_DISPLAYPORT
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
#include <linux/displayport_bigdata.h>
#endif
#include "secdp.h"
#endif

#define DP_PANEL_DEFAULT_BPP 24
#define DP_MAX_DS_PORT_COUNT 1

enum dp_panel_hdr_pixel_encoding {
	RGB,
	YCbCr444,
	YCbCr422,
	YCbCr420,
	YONLY,
	RAW,
};

enum dp_panel_hdr_rgb_colorimetry {
	sRGB,
	RGB_WIDE_GAMUT_FIXED_POINT,
	RGB_WIDE_GAMUT_FLOATING_POINT,
	ADOBERGB,
	DCI_P3,
	CUSTOM_COLOR_PROFILE,
	ITU_R_BT_2020_RGB,
};

enum dp_panel_hdr_dynamic_range {
	VESA,
	CEA,
};

enum dp_panel_hdr_content_type {
	NOT_DEFINED,
	GRAPHICS,
	PHOTO,
	VIDEO,
	GAME,
};

enum dp_panel_hdr_state {
	HDR_DISABLED,
	HDR_ENABLED,
};

struct dp_panel_private {
	struct device *dev;
	struct dp_panel dp_panel;
	struct dp_aux *aux;
	struct dp_link *link;
	struct dp_catalog_panel *catalog;
	bool custom_edid;
	bool custom_dpcd;
	bool panel_on;
	enum dp_panel_hdr_state hdr_state;
	u8 spd_vendor_name[8];
	u8 spd_product_description[16];
};

static const struct dp_panel_info fail_safe = {
	.h_active = 640,
	.v_active = 480,
	.h_back_porch = 48,
	.h_front_porch = 16,
	.h_sync_width = 96,
	.h_active_low = 0,
	.v_back_porch = 33,
	.v_front_porch = 10,
	.v_sync_width = 2,
	.v_active_low = 0,
	.h_skew = 0,
	.refresh_rate = 60,
	.pixel_clk_khz = 25200,
	.bpp = 24,
};

#ifdef CONFIG_SEC_DISPLAYPORT
enum downstream_port_type {
	DSP_TYPE_DP = 0x00,
	DSP_TYPE_VGA,
	DSP_TYPE_DVI_HDMI_DPPP,
	DSP_TYPE_OTHER,
};

static inline char *mdss_dp_dsp_type_to_string(u32 dsp_type)
{
	switch (dsp_type) {
	case DSP_TYPE_DP:
		return DP_ENUM_STR(DSP_TYPE_DP);
	case DSP_TYPE_VGA:
		return DP_ENUM_STR(DSP_TYPE_VGA);
	case DSP_TYPE_DVI_HDMI_DPPP:
		return DP_ENUM_STR(DSP_TYPE_DVI_HDMI_DPPP);
	case DSP_TYPE_OTHER:
		return DP_ENUM_STR(DSP_TYPE_OTHER);
	default:
		return "unknown";
	}
}
#endif

/* OEM NAME */
static const u8 vendor_name[8] = {'S', 'E', 'C', '.', 'M', 'C', 'B', 0};

/* MODEL NAME */
static const u8 product_desc[16] = {'G', 'A', 'L', 'A', 'X', 'Y', 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0};

static int dp_panel_read_dpcd(struct dp_panel *dp_panel)
{
	int rlen, rc = 0;
	struct dp_panel_private *panel;
	struct drm_dp_link *link_info;
	u8 *dpcd, major = 0, minor = 0;
	u32 dfp_count = 0;
	unsigned long caps = DP_LINK_CAP_ENHANCED_FRAMING;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	pr_debug("+++\n");

	dpcd = dp_panel->dpcd;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	link_info = &dp_panel->link_info;

	if (!panel->custom_dpcd) {
		rlen = drm_dp_dpcd_read(panel->aux->drm_aux, DP_DPCD_REV,
			dp_panel->dpcd, (DP_RECEIVER_CAP_SIZE + 1));
		if (rlen < (DP_RECEIVER_CAP_SIZE + 1)) {
			pr_err("dpcd read failed, rlen=%d\n", rlen);
			if (rlen == -ETIMEDOUT)
				rc = rlen;
			else
				rc = -EINVAL;

			goto end;
		}

		print_hex_dump(KERN_DEBUG, "[drm-dp] SINK DPCD: ",
			DUMP_PREFIX_NONE, 8, 1, dp_panel->dpcd, rlen, false);
	}

	link_info->revision = dp_panel->dpcd[DP_DPCD_REV];

	major = (link_info->revision >> 4) & 0x0f;
	minor = link_info->revision & 0x0f;
	pr_debug("version: %d.%d\n", major, minor);

	link_info->rate =
		drm_dp_bw_code_to_link_rate(dp_panel->dpcd[DP_MAX_LINK_RATE]);
	pr_debug("link_rate=%d\n", link_info->rate);

	link_info->num_lanes = dp_panel->dpcd[DP_MAX_LANE_COUNT] &
				DP_MAX_LANE_COUNT_MASK;

	pr_debug("lane_count=%d\n", link_info->num_lanes);

	if (drm_dp_enhanced_frame_cap(dpcd))
		link_info->capabilities |= caps;

	dfp_count = dpcd[DP_DOWN_STREAM_PORT_COUNT] &
						DP_DOWN_STREAM_PORT_COUNT;

	if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_PRESENT)
		&& (dpcd[DP_DPCD_REV] > 0x10)) {
		rlen = drm_dp_dpcd_read(panel->aux->drm_aux,
			DP_DOWNSTREAM_PORT_0, dp_panel->ds_ports,
			DP_MAX_DOWNSTREAM_PORTS);
		if (rlen < DP_MAX_DOWNSTREAM_PORTS) {
			pr_err("ds port status failed, rlen=%d\n", rlen);
			rc = -EINVAL;
			goto end;
		}
	}

	if (dfp_count > DP_MAX_DS_PORT_COUNT)
		pr_debug("DS port count %d greater that max (%d) supported\n",
			dfp_count, DP_MAX_DS_PORT_COUNT);

#ifdef CONFIG_SEC_DISPLAYPORT
	dp_panel->dsp_type = (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_TYPE_MASK) >> 1;
	pr_info("dsp_type: <%s>\n", mdss_dp_dsp_type_to_string(dp_panel->dsp_type));
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_ADAPTER_TYPE, mdss_dp_dsp_type_to_string(dp_panel->dsp_type));
	secdp_bigdata_save_item(BD_MAX_LANE_COUNT, link_info->num_lanes);
	secdp_bigdata_save_item(BD_MAX_LINK_RATE, dp_panel->dpcd[DP_MAX_LINK_RATE]);

	secdp_bigdata_save_item(BD_CUR_LANE_COUNT, link_info->num_lanes);
	secdp_bigdata_save_item(BD_CUR_LINK_RATE, dp_panel->dpcd[DP_MAX_LINK_RATE]);
#endif
#endif

end:
	return rc;
}

static int dp_panel_set_default_link_params(struct dp_panel *dp_panel)
{
	struct drm_dp_link *link_info;
	const int default_bw_code = 162000;
	const int default_num_lanes = 1;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}
	link_info = &dp_panel->link_info;
	link_info->rate = default_bw_code;
	link_info->num_lanes = default_num_lanes;
	pr_debug("link_rate=%d num_lanes=%d\n",
		link_info->rate, link_info->num_lanes);

	return 0;
}

static int dp_panel_set_edid(struct dp_panel *dp_panel, u8 *edid)
{
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (edid) {
		dp_panel->edid_ctrl->edid = (struct edid *)edid;
		panel->custom_edid = true;
	} else {
		panel->custom_edid = false;
	}

	return 0;
}

static int dp_panel_set_dpcd(struct dp_panel *dp_panel, u8 *dpcd)
{
	struct dp_panel_private *panel;
	u8 *dp_dpcd;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp_dpcd = dp_panel->dpcd;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (dpcd) {
		memcpy(dp_dpcd, dpcd, DP_RECEIVER_CAP_SIZE + 1);
		panel->custom_dpcd = true;
	} else {
		panel->custom_dpcd = false;
	}

	return 0;
}

static int dp_panel_read_edid(struct dp_panel *dp_panel,
	struct drm_connector *connector)
{
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

#ifdef CONFIG_SEC_DISPLAYPORT
	secdp_dex_res_init();
#endif

	if (panel->custom_edid) {
		pr_debug("skip edid read in debug mode\n");
		return 0;
	}

	sde_get_edid(connector, &panel->aux->drm_aux->ddc,
		(void **)&dp_panel->edid_ctrl);
	if (!dp_panel->edid_ctrl->edid) {
		pr_err("EDID read failed\n");
	} else {
		u8 *buf = (u8 *)dp_panel->edid_ctrl->edid;
		u32 size = buf[0x7E] ? 256 : 128;

		print_hex_dump(KERN_DEBUG, "[drm-dp] SINK EDID: ",
			DUMP_PREFIX_NONE, 16, 1, buf, size, false);

		return 0;
	}

	return -EINVAL;
}

static int dp_panel_read_sink_caps(struct dp_panel *dp_panel,
	struct drm_connector *connector)
{
	int rc = 0;
	struct dp_panel_private *panel;
#ifdef CONFIG_SEC_DISPLAYPORT
	char monitor_name[14];
#endif

	if (!dp_panel || !connector) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

#ifdef CONFIG_SEC_DISPLAYPORT
	usleep_range(10000, 11000);
#endif

	rc = dp_panel_read_dpcd(dp_panel);
	if (rc || !is_link_rate_valid(drm_dp_link_rate_to_bw_code(
		dp_panel->link_info.rate)) || !is_lane_count_valid(
		dp_panel->link_info.num_lanes) ||
		((drm_dp_link_rate_to_bw_code(dp_panel->link_info.rate)) >
		dp_panel->max_bw_code)) {

#ifndef CONFIG_SEC_DISPLAYPORT
		if ((rc == -ETIMEDOUT) || (rc == -ENODEV)) {
			pr_err("DPCD read failed, return early\n");
			return rc;
		}
#else
		if (!secdp_get_hpd_status() || !secdp_get_cable_status()) {
			pr_info("hpd_low or cable_lost\n");
			return -EIO;
		}
#endif
		pr_err("panel dpcd read failed/incorrect, set default params\n");
		dp_panel_set_default_link_params(dp_panel);

#ifdef CONFIG_SEC_DISPLAYPORT
		if (rc < 0)
			return -EIO;
#endif
	}

	rc = dp_panel_read_edid(dp_panel, connector);
	if (rc) {
		pr_err("panel edid read failed, set failsafe mode\n");
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_inc_error_cnt(ERR_EDID);
#endif
		return rc;
	}

#ifdef CONFIG_SEC_DISPLAYPORT
	pr_info("hdr_supported: <%d>\n", dp_panel->edid_ctrl->hdr_supported);
	pr_info("vendor_id: <%s>\n", dp_panel->edid_ctrl->vendor_id);
	drm_edid_get_monitor_name(dp_panel->edid_ctrl->edid, monitor_name, 14);
	pr_info("monitor_name: <%s>\n", monitor_name);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_SINK_NAME, monitor_name);
	secdp_bigdata_save_item(BD_EDID, (char *)(dp_panel->edid_ctrl->edid));
#endif
#endif
	return 0;
}

static u32 dp_panel_get_supported_bpp(struct dp_panel *dp_panel,
		u32 mode_edid_bpp, u32 mode_pclk_khz)
{
	struct drm_dp_link *link_info;
	const u32 max_supported_bpp = 30, min_supported_bpp = 18;
	u32 bpp = 0, data_rate_khz = 0;

#ifndef CONFIG_SEC_DISPLAYPORT
	bpp = min_t(u32, mode_edid_bpp, max_supported_bpp);
#else
	/* 4Kp60hz + bpp30 does not output audio with DP2HDMI dongle connection because
	 * DP2HDMI dongle does not support HDR10 yet. It has bandwidth limitation
	 */
	bpp = min_t(u32, mode_edid_bpp,
		((dp_panel->dsp_type == DSP_TYPE_DP) ? max_supported_bpp : max_supported_bpp - 6));
#endif

	link_info = &dp_panel->link_info;
	data_rate_khz = link_info->num_lanes * link_info->rate * 8;

	while (bpp > min_supported_bpp) {
		if (mode_pclk_khz * bpp <= data_rate_khz)
			break;
		bpp -= 6;
	}

	return bpp;
}

static u32 dp_panel_get_mode_bpp(struct dp_panel *dp_panel,
		u32 mode_edid_bpp, u32 mode_pclk_khz)
{
	struct dp_panel_private *panel;
	u32 bpp = mode_edid_bpp;

	if (!dp_panel || !mode_edid_bpp || !mode_pclk_khz) {
		pr_err("invalid input\n");
		return 0;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (dp_panel->video_test)
		bpp = dp_link_bit_depth_to_bpp(
				panel->link->test_video.test_bit_depth);
	else
		bpp = dp_panel_get_supported_bpp(dp_panel, mode_edid_bpp,
				mode_pclk_khz);

#ifdef CONFIG_SEC_DISPLAYPORT
	pr_debug("video_test(%d), bpp(%d)\n", dp_panel->video_test, bpp);
#endif

	return bpp;
}

static void dp_panel_set_test_mode(struct dp_panel_private *panel,
		struct dp_display_mode *mode)
{
	struct dp_panel_info *pinfo = NULL;
	struct dp_link_test_video *test_info = NULL;

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}

	pinfo = &mode->timing;
	test_info = &panel->link->test_video;

	pinfo->h_active = test_info->test_h_width;
	pinfo->h_sync_width = test_info->test_hsync_width;
	pinfo->h_back_porch = test_info->test_h_start -
		test_info->test_hsync_width;
	pinfo->h_front_porch = test_info->test_h_total -
		(test_info->test_h_start + test_info->test_h_width);

	pinfo->v_active = test_info->test_v_height;
	pinfo->v_sync_width = test_info->test_vsync_width;
	pinfo->v_back_porch = test_info->test_v_start -
		test_info->test_vsync_width;
	pinfo->v_front_porch = test_info->test_v_total -
		(test_info->test_v_start + test_info->test_v_height);

	pinfo->bpp = dp_link_bit_depth_to_bpp(test_info->test_bit_depth);
	pinfo->h_active_low = test_info->test_hsync_pol;
	pinfo->v_active_low = test_info->test_vsync_pol;

	pinfo->refresh_rate = test_info->test_rr_n;
	pinfo->pixel_clk_khz = test_info->test_h_total *
		test_info->test_v_total * pinfo->refresh_rate;

	if (test_info->test_rr_d == 0)
		pinfo->pixel_clk_khz /= 1000;
	else
		pinfo->pixel_clk_khz /= 1001;

	if (test_info->test_h_width == 640)
		pinfo->pixel_clk_khz = 25170;
}

static int dp_panel_get_modes(struct dp_panel *dp_panel,
	struct drm_connector *connector, struct dp_display_mode *mode)
{
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (dp_panel->video_test) {
		dp_panel_set_test_mode(panel, mode);
		return 1;
	} else if (dp_panel->edid_ctrl->edid) {
		return _sde_edid_update_modes(connector, dp_panel->edid_ctrl);
	} else { /* fail-safe mode */
		memcpy(&mode->timing, &fail_safe,
			sizeof(fail_safe));
		return 1;
	}
}

static void dp_panel_handle_sink_request(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (panel->link->sink_request & DP_TEST_LINK_EDID_READ) {
		u8 checksum = sde_get_edid_checksum(dp_panel->edid_ctrl);

		panel->link->send_edid_checksum(panel->link, checksum);
		panel->link->send_test_response(panel->link);
	}
}

static void dp_panel_tpg_config(struct dp_panel *dp_panel, bool enable)
{
	u32 hsync_start_x, hsync_end_x;
	struct dp_catalog_panel *catalog;
	struct dp_panel_private *panel;
	struct dp_panel_info *pinfo;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;
	pinfo = &panel->dp_panel.pinfo;

	if (!panel->panel_on) {
		pr_debug("DP panel not enabled, handle TPG on next panel on\n");
		return;
	}

	if (!enable) {
		panel->catalog->tpg_config(catalog, false);
		return;
	}

	/* TPG config */
	catalog->hsync_period = pinfo->h_sync_width + pinfo->h_back_porch +
			pinfo->h_active + pinfo->h_front_porch;
	catalog->vsync_period = pinfo->v_sync_width + pinfo->v_back_porch +
			pinfo->v_active + pinfo->v_front_porch;

	catalog->display_v_start = ((pinfo->v_sync_width +
			pinfo->v_back_porch) * catalog->hsync_period);
	catalog->display_v_end = ((catalog->vsync_period -
			pinfo->v_front_porch) * catalog->hsync_period) - 1;

	catalog->display_v_start += pinfo->h_sync_width + pinfo->h_back_porch;
	catalog->display_v_end -= pinfo->h_front_porch;

	hsync_start_x = pinfo->h_back_porch + pinfo->h_sync_width;
	hsync_end_x = catalog->hsync_period - pinfo->h_front_porch - 1;

	catalog->v_sync_width = pinfo->v_sync_width;

	catalog->hsync_ctl = (catalog->hsync_period << 16) |
			pinfo->h_sync_width;
	catalog->display_hctl = (hsync_end_x << 16) | hsync_start_x;

	panel->catalog->tpg_config(catalog, true);
}

static int dp_panel_timing_cfg(struct dp_panel *dp_panel)
{
	int rc = 0;
	u32 data, total_ver, total_hor;
	struct dp_catalog_panel *catalog;
	struct dp_panel_private *panel;
	struct dp_panel_info *pinfo;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;
	pinfo = &panel->dp_panel.pinfo;

	pr_debug("width=%d hporch= %d %d %d\n",
		pinfo->h_active, pinfo->h_back_porch,
		pinfo->h_front_porch, pinfo->h_sync_width);

	pr_debug("height=%d vporch= %d %d %d\n",
		pinfo->v_active, pinfo->v_back_porch,
		pinfo->v_front_porch, pinfo->v_sync_width);

	total_hor = pinfo->h_active + pinfo->h_back_porch +
		pinfo->h_front_porch + pinfo->h_sync_width;

	total_ver = pinfo->v_active + pinfo->v_back_porch +
			pinfo->v_front_porch + pinfo->v_sync_width;

	data = total_ver;
	data <<= 16;
	data |= total_hor;

	catalog->total = data;

	data = (pinfo->v_back_porch + pinfo->v_sync_width);
	data <<= 16;
	data |= (pinfo->h_back_porch + pinfo->h_sync_width);

	catalog->sync_start = data;

	data = pinfo->v_sync_width;
	data <<= 16;
	data |= (pinfo->v_active_low << 31);
	data |= pinfo->h_sync_width;
	data |= (pinfo->h_active_low << 15);

	catalog->width_blanking = data;

	data = pinfo->v_active;
	data <<= 16;
	data |= pinfo->h_active;

	catalog->dp_active = data;

	panel->catalog->timing_cfg(catalog);
	panel->panel_on = true;
end:
	return rc;
}

static int dp_panel_edid_register(struct dp_panel_private *panel)
{
	int rc = 0;

	panel->dp_panel.edid_ctrl = sde_edid_init();
	if (!panel->dp_panel.edid_ctrl) {
		pr_err("sde edid init for DP failed\n");
		rc = -ENOMEM;
	}

	return rc;
}

static void dp_panel_edid_deregister(struct dp_panel_private *panel)
{
	sde_edid_deinit((void **)&panel->dp_panel.edid_ctrl);
}

static int dp_panel_init_panel_info(struct dp_panel *dp_panel)
{
	int rc = 0;
	struct dp_panel_info *pinfo;
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	char buf[20] = {0, };
#endif

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	pinfo = &dp_panel->pinfo;

	/*
	 * print resolution info as this is a result
	 * of user initiated action of cable connection
	 */
	pr_info("SET NEW RESOLUTION:\n");
	pr_info("%dx%d@%dfps\n", pinfo->h_active,
		pinfo->v_active, pinfo->refresh_rate);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	scnprintf(buf, 20, "%dx%d@%d",
		pinfo->h_active, pinfo->v_active, pinfo->refresh_rate);
	secdp_bigdata_save_item(BD_RESOLUTION, buf);
#endif
	pr_info("h_porches(back|front|width) = (%d|%d|%d)\n",
			pinfo->h_back_porch,
			pinfo->h_front_porch,
			pinfo->h_sync_width);
	pr_info("v_porches(back|front|width) = (%d|%d|%d)\n",
			pinfo->v_back_porch,
			pinfo->v_front_porch,
			pinfo->v_sync_width);
	pr_info("pixel clock (KHz)=(%d)\n", pinfo->pixel_clk_khz);
	pr_info("bpp = %d\n", pinfo->bpp);
	pr_info("active low (h|v)=(%d|%d)\n", pinfo->h_active_low,
		pinfo->v_active_low);
end:
	return rc;
}

static int dp_panel_deinit_panel_info(struct dp_panel *dp_panel)
{
	int rc = 0;
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (!panel->custom_edid)
		sde_free_edid((void **)&dp_panel->edid_ctrl);

	memset(&dp_panel->pinfo, 0, sizeof(dp_panel->pinfo));
	panel->panel_on = false;

	return rc;
}

static u32 dp_panel_get_min_req_link_rate(struct dp_panel *dp_panel)
{
	const u32 encoding_factx10 = 8;
	u32 min_link_rate_khz = 0, lane_cnt;
	struct dp_panel_info *pinfo;

	if (!dp_panel) {
		pr_err("invalid input\n");
		goto end;
	}

	lane_cnt = dp_panel->link_info.num_lanes;
	pinfo = &dp_panel->pinfo;

	/* num_lanes * lane_count * 8 >= pclk * bpp * 10 */
	min_link_rate_khz = pinfo->pixel_clk_khz /
				(lane_cnt * encoding_factx10);
	min_link_rate_khz *= pinfo->bpp;

	pr_debug("min lclk req=%d khz for pclk=%d khz, lanes=%d, bpp=%d\n",
		min_link_rate_khz, pinfo->pixel_clk_khz, lane_cnt,
		pinfo->bpp);
end:
	return min_link_rate_khz;
}

static bool dp_panel_is_validate_hdr_state(struct dp_panel_private *panel,
		struct drm_msm_ext_hdr_metadata *hdr_meta)
{
	struct drm_msm_ext_hdr_metadata *panel_hdr_meta =
			&panel->catalog->hdr_data.hdr_meta;

	if (!hdr_meta)
		goto end;

	/* bail out if HDR not active */
	if (hdr_meta->hdr_state == HDR_DISABLED &&
	    panel->hdr_state == HDR_DISABLED)
		goto end;

	/* bail out if same meta data is received */
	if (hdr_meta->hdr_state == HDR_ENABLED &&
		panel_hdr_meta->eotf == hdr_meta->eotf)
		goto end;

	return true;
end:
	return false;
}

static int dp_panel_setup_hdr(struct dp_panel *dp_panel,
		struct drm_msm_ext_hdr_metadata *hdr_meta)
{
	int rc = 0;
	struct dp_panel_private *panel;
	struct dp_catalog_hdr_data *hdr;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	if (!dp_panel_is_validate_hdr_state(panel, hdr_meta))
		goto end;

	panel->hdr_state = hdr_meta->hdr_state;

	hdr = &panel->catalog->hdr_data;

	hdr->ext_header_byte0 = 0x00;
	hdr->ext_header_byte1 = 0x04;
	hdr->ext_header_byte2 = 0x1F;
	hdr->ext_header_byte3 = 0x00;

	hdr->vsc_header_byte0 = 0x00;
	hdr->vsc_header_byte1 = 0x07;
	hdr->vsc_header_byte2 = 0x05;
	hdr->vsc_header_byte3 = 0x13;

	hdr->vscext_header_byte0 = 0x00;
	hdr->vscext_header_byte1 = 0x87;
	hdr->vscext_header_byte2 = 0x1D;
	hdr->vscext_header_byte3 = 0x13 << 2;

	/* VSC SDP Payload for DB16 */
	hdr->pixel_encoding = RGB;
	hdr->colorimetry = ITU_R_BT_2020_RGB;

	/* VSC SDP Payload for DB17 */
	hdr->dynamic_range = CEA;

	/* VSC SDP Payload for DB18 */
	hdr->content_type = GRAPHICS;

	hdr->bpc = dp_panel->pinfo.bpp / 3;

	hdr->version = 0x01;
	hdr->length = 0x1A;

	memcpy(&hdr->hdr_meta, hdr_meta, sizeof(hdr->hdr_meta));

	panel->catalog->config_hdr(panel->catalog, panel->hdr_state);
end:
	return rc;
}

static int dp_panel_spd_config(struct dp_panel *dp_panel)
{
	int rc = 0;
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	if (!dp_panel->spd_enabled) {
		pr_debug("SPD Infoframe not enabled\n");
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	panel->catalog->spd_vendor_name = panel->spd_vendor_name;
	panel->catalog->spd_product_description =
		panel->spd_product_description;
	panel->catalog->config_spd(panel->catalog);
end:
	return rc;
}

struct dp_panel *dp_panel_get(struct dp_panel_in *in)
{
	int rc = 0;
	struct dp_panel_private *panel;
	struct dp_panel *dp_panel;

	if (!in->dev || !in->catalog || !in->aux || !in->link) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	panel = devm_kzalloc(in->dev, sizeof(*panel), GFP_KERNEL);
	if (!panel) {
		rc = -ENOMEM;
		goto error;
	}

	panel->dev = in->dev;
	panel->aux = in->aux;
	panel->catalog = in->catalog;
	panel->link = in->link;

	dp_panel = &panel->dp_panel;
#ifndef CONFIG_SEC_DISPLAYPORT
	dp_panel->max_bw_code = DP_LINK_BW_8_1;
#else
	dp_panel->max_bw_code = DP_LINK_BW_5_4;
#endif
	dp_panel->spd_enabled = true;
	memcpy(panel->spd_vendor_name, vendor_name, (sizeof(u8) * 8));
	memcpy(panel->spd_product_description, product_desc, (sizeof(u8) * 16));

	dp_panel->init = dp_panel_init_panel_info;
	dp_panel->deinit = dp_panel_deinit_panel_info;
	dp_panel->timing_cfg = dp_panel_timing_cfg;
	dp_panel->read_sink_caps = dp_panel_read_sink_caps;
	dp_panel->get_min_req_link_rate = dp_panel_get_min_req_link_rate;
	dp_panel->get_mode_bpp = dp_panel_get_mode_bpp;
	dp_panel->get_modes = dp_panel_get_modes;
	dp_panel->handle_sink_request = dp_panel_handle_sink_request;
	dp_panel->set_edid = dp_panel_set_edid;
	dp_panel->set_dpcd = dp_panel_set_dpcd;
	dp_panel->tpg_config = dp_panel_tpg_config;
	dp_panel->spd_config = dp_panel_spd_config;
	dp_panel->setup_hdr = dp_panel_setup_hdr;

	dp_panel_edid_register(panel);

	return dp_panel;
error:
	return ERR_PTR(rc);
}

void dp_panel_put(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;

	if (!dp_panel)
		return;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	dp_panel_edid_deregister(panel);
	devm_kfree(panel->dev, panel);
}
